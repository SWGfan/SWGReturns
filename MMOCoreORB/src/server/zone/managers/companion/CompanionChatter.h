/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-17, "command flair" pass, per user request) --
	squad-radio chatter for companion order commands. When the owner issues
	any companion order, the owner BARKS the order in spatial chat ("Squad --
	form up! Wedge formation!") and every summoned companion answers with a
	randomized, profession-flavored acknowledgment line, staggered ~half a
	second apart so it reads like a squad responding rather than a chorus.

	Header-only static helper (no idl / no manager registration needed) --
	same self-contained pattern as the Companion*Command headers themselves.
	Spatial speech uses ChatManager::broadcastChatMessage(), the same call
	every talking NPC in the stock game uses (FactionRecruiter, contraband
	scanners, DroidMerchantBarkerTask). The staggered companion replies use
	the Core::getTaskManager()->scheduleTask() lambda pattern proven by
	scheduleCompanionTaxiTick() (CompanionObjectImplementation.cpp).
*/

#ifndef COMPANIONCHATTER_H_
#define COMPANIONCHATTER_H_

#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/chat/ChatManager.h"
#include "server/zone/ZoneServer.h"
#include <system/lang/StringBuffer.h>

class CompanionChatter {
public:

	/**
	 * ownerBark is spoken by the owner immediately; each companion in
	 * companions then speaks a random acknowledgment for orderKey (one of:
	 * follow, stay, patrol, store, attack, formup, guard, rangedattack,
	 * specialone, specialtwo, group, followother, friend, return),
	 * staggered.
	 */
	/**
	 * immediateReplies: companions answer synchronously instead of on the
	 * staggered timer -- REQUIRED for orders that despawn the companion
	 * right after (e.g. /companionstore: by the time a scheduled reply
	 * fires, the companion is out of the world and the reply is dropped).
	 */
	static void announceOrder(CreatureObject* owner, const String& ownerBark, const String& orderKey,
			const Vector<ManagedReference<CompanionObject*>>& companions, bool immediateReplies = false) {
		if (owner == nullptr) {
			return;
		}

		ZoneServer* zoneServer = owner->getZoneServer();

		if (zoneServer == nullptr) {
			return;
		}

		ChatManager* chatManager = zoneServer->getChatManager();

		if (chatManager == nullptr) {
			return;
		}

		if (!ownerBark.isEmpty()) {
			chatManager->broadcastChatMessage(owner, ownerBark, 0, 0, owner->getMoodID());
		}

		for (int i = 0; i < companions.size(); ++i) {
			ManagedReference<CompanionObject*> companionRef = companions.get(i);

			if (companionRef == nullptr) {
				continue;
			}

			String line = pickResponse(companionRef.get(), orderKey);

			if (line.isEmpty()) {
				continue;
			}

			if (immediateReplies) {
				CompanionObject* companion = companionRef.get();

				if (companion != nullptr && companion->getZone() != nullptr && !companion->isDead()) {
					chatManager->broadcastChatMessage(companion, line, 0, 0, 0);
				}

				continue;
			}

			// 600ms lead-in so the owner's bark lands first, then ~500ms
			// between each companion (+ a little jitter so repeated orders
			// don't sound mechanical).
			uint64 delay = 600 + (uint64) i * 500 + System::random(250);

			Core::getTaskManager()->scheduleTask([companionRef, line] () {
				CompanionObject* companion = companionRef.get();

				if (companion == nullptr || companion->getZone() == nullptr || companion->isDead()) {
					return;
				}

				ZoneServer* zs = companion->getZoneServer();

				if (zs == nullptr) {
					return;
				}

				ChatManager* cm = zs->getChatManager();

				if (cm == nullptr) {
					return;
				}

				Locker locker(companion);

				cm->broadcastChatMessage(companion, line, 0, 0, 0);
			}, "CompanionChatterReplyLambda", delay);
		}
	}

	/**
	 * Companion System (2026-07-30, "reaction chatter" pass). Speaks a
	 * random, in-character bark from `companion` reacting to a combat/
	 * crafting event -- reactionKey is one of: kill, heavydamage,
	 * lowhealth, outofstims, craftdone, wipe, flee. Cooldown-gated per
	 * (companion, reactionKey) pair (see reactionCooldowns() below) so a
	 * long fight doesn't spam the same line every tick. Unlike
	 * announceOrder(), there is no owner bark here -- only companions
	 * react.
	 */
	static void announceReaction(CompanionObject* companion, CreatureObject* owner, const String& reactionKey) {
		if (companion == nullptr || owner == nullptr) {
			return;
		}

		if (companion->getZone() == nullptr || companion->isDead()) {
			return;
		}

		ZoneServer* zoneServer = companion->getZoneServer();

		if (zoneServer == nullptr) {
			return;
		}

		ChatManager* chatManager = zoneServer->getChatManager();

		if (chatManager == nullptr) {
			return;
		}

		uint64 companionID = companion->getObjectID();
		uint64 now = System::getMiliTime();

		auto& outer = reactionCooldowns();

		if (!outer.contains(companionID)) {
			outer.put(companionID, VectorMap<String, uint64>());
		}

		VectorMap<String, uint64>& inner = outer.get(companionID);

		if (inner.contains(reactionKey) && now - inner.get(reactionKey) < REACTION_COOLDOWN_MS) {
			return;
		}

		inner.drop(reactionKey);
		inner.put(reactionKey, now);

		String line = pickReactionLine(companion, reactionKey);

		if (line.isEmpty()) {
			return;
		}

		chatManager->broadcastChatMessage(companion, line, 0, 0, 0);
	}

	/**
	 * Companion System (2026-07-30, "reaction chatter" pass). Level-ups
	 * are rare (unlike the other reaction keys above), so this speaks
	 * unconditionally -- no cooldown gate. Line is built with StringBuffer
	 * so the companion's own name and newLevel can both be substituted
	 * into whichever variant gets picked.
	 */
	static void announceLevelUp(CompanionObject* companion, CreatureObject* owner, int newLevel) {
		if (companion == nullptr || owner == nullptr) {
			return;
		}

		if (companion->getZone() == nullptr || companion->isDead()) {
			return;
		}

		ZoneServer* zoneServer = companion->getZoneServer();

		if (zoneServer == nullptr) {
			return;
		}

		ChatManager* chatManager = zoneServer->getChatManager();

		if (chatManager == nullptr) {
			return;
		}

		String name = companion->getDisplayedName();
		StringBuffer line;

		switch (System::random(3)) {
		case 0:
			line << name << " feels stronger! (Level " << newLevel << ")";
			break;
		case 1:
			line << name << " is now level " << newLevel << " -- and still standing.";
			break;
		case 2:
			line << "That's level " << newLevel << " for " << name << ".";
			break;
		default:
			line << name << " is getting sharper every fight. Level " << newLevel << " now.";
			break;
		}

		chatManager->broadcastChatMessage(companion, line.toString(), 0, 0, 0);
	}

private:

	/** Cooldown window for a single (companion, reactionKey) pair -- long
	 * enough that a companion doesn't repeat the same "kill"/"heavydamage"
	 * bark on every tick of a long fight, short enough that a genuinely new
	 * reaction a few seconds later still gets voiced. */
	static const int REACTION_COOLDOWN_MS = 10000;

	/** companion objectID -> (reactionKey -> last-spoken mili time), for
	 * announceReaction()'s cooldown gate. Function-local static -- same
	 * header-only-class pattern as CompanionFieldStation::deployedProps()
	 * / CompanionCraftTheater::activeCraftersByOwner() (see either for the
	 * full C++11 rationale: an inline function's local static is
	 * guaranteed shared across every translation unit that includes this
	 * header, so no class-level static data member -- and no .cpp -- is
	 * needed here). A nested VectorMap reuses that exact proven shape
	 * instead of hand-rolling a single compound integer key. In-memory
	 * only; resets on server restart, which just means the first reaction
	 * of each kind after a restart isn't cooldown-gated -- harmless. */
	static VectorMap<uint64, VectorMap<String, uint64> >& reactionCooldowns() {
		static VectorMap<uint64, VectorMap<String, uint64> > map;
		return map;
	}

	/**
	 * Personality-flavored override for a handful of the most
	 * personality-relevant reaction keys -- returns "" (meaning "no
	 * override, use the generic pool") for every other key/personality
	 * combination. PERSONALITY_* constants come from CompanionObject.idl.
	 */
	static String pickPersonalityReactionLine(int personality, const String& reactionKey) {
		if (reactionKey == "kill") {
			if (personality == CompanionObject::PERSONALITY_RECKLESS) {
				static const char* const lines[] = { "Ha! Too easy -- who's next?", "Didn't even break a sweat." };
				return pickFrom(lines, 2);
			} else if (personality == CompanionObject::PERSONALITY_CAUTIOUS) {
				static const char* const lines[] = { "Target down. Stay sharp -- it might not be alone.", "One threat clear. Eyes open." };
				return pickFrom(lines, 2);
			}
		} else if (reactionKey == "lowhealth") {
			if (personality == CompanionObject::PERSONALITY_BRAVE) {
				static const char* const lines[] = { "I'm hurt, but I'm not done yet!", "This won't stop me." };
				return pickFrom(lines, 2);
			} else if (personality == CompanionObject::PERSONALITY_CAUTIOUS || personality == CompanionObject::PERSONALITY_VIGILANT) {
				static const char* const lines[] = { "I'm hurt -- we should be careful.", "Taking heavy damage. Watch my back." };
				return pickFrom(lines, 2);
			}
		} else if (reactionKey == "flee") {
			if (personality == CompanionObject::PERSONALITY_RECKLESS) {
				static const char* const lines[] = { "Fine, falling back -- but I'll be right back in!", "This isn't over!" };
				return pickFrom(lines, 2);
			} else if (personality == CompanionObject::PERSONALITY_CAUTIOUS) {
				static const char* const lines[] = { "Pulling back -- this fight isn't worth it.", "Retreating. Live to fight another day." };
				return pickFrom(lines, 2);
			}
		}

		return "";
	}

	/**
	 * Picks the line announceReaction() actually speaks for reactionKey.
	 * ~25% of the time, defers to pickPersonalityReactionLine() first
	 * (same ratio pickResponse() below already uses for
	 * resolveProfessionFlavor()'s profession lines); falls through to the
	 * generic pool for reactionKey otherwise (or if personality gave no
	 * override). Returns "" for an unrecognized reactionKey, which
	 * announceReaction() treats as "say nothing".
	 */
	static String pickReactionLine(CompanionObject* companion, const String& reactionKey) {
		if (companion != nullptr && System::random(3) == 0) {
			String personalityLine = pickPersonalityReactionLine(companion->getPersonalityType(), reactionKey);

			if (!personalityLine.isEmpty()) {
				return personalityLine;
			}
		}

		if (reactionKey == "kill") {
			static const char* const lines[] = { "Target down!", "That's one for the squad.", "Got 'em.", "Clean kill.", "Down for good.", "One less problem." };
			return pickFrom(lines, 6);
		} else if (reactionKey == "heavydamage") {
			static const char* const lines[] = { "That one hurt.", "Took a hard hit there.", "Still standing -- barely.", "That'll leave a mark.", "Shook that one off." };
			return pickFrom(lines, 5);
		} else if (reactionKey == "lowhealth") {
			static const char* const lines[] = { "I'm running low -- could use a hand.", "Getting risky over here.", "I won't last much longer like this.", "Health's dropping fast.", "I need backup soon." };
			return pickFrom(lines, 5);
		} else if (reactionKey == "outofstims") {
			static const char* const lines[] = { "Out of stims -- I'm on my own now.", "No more stims left.", "That's the last of my supplies.", "I'm empty -- watch my back.", "Nothing left to patch myself with." };
			return pickFrom(lines, 5);
		} else if (reactionKey == "craftdone") {
			static const char* const lines[] = { "Finished up the work.", "That's done.", "Craft complete.", "All set -- ready when you are.", "Job's finished." };
			return pickFrom(lines, 5);
		} else if (reactionKey == "wipe") {
			static const char* const lines[] = { "We're down -- regroup and we'll come back stronger.", "That didn't go our way.", "We'll get them next time.", "Rough one. Let's regroup.", "That was a hard fight to lose." };
			return pickFrom(lines, 5);
		} else if (reactionKey == "flee") {
			static const char* const lines[] = { "Falling back!", "Retreating!", "Pulling out of this one!", "Not worth it -- disengaging!", "Breaking off!" };
			return pickFrom(lines, 5);
		} else if (reactionKey == "readytotrain") {
			// AUTO_SKILL_TRAIN_WALKUP_2026_07_30 -- companion has banked enough XP for a new
			// skill and is walking over to the owner to open the trainer
			// Skill Tree SUI (see CompanionObjectImplementation.cpp).
			static const char* const lines[] = { "I've learned enough for something new -- let me find you.", "Ready to train a new skill. Coming to you now.", "Got the experience I need -- just need a minute of your time.", "New skill's within reach. Let me catch up with you.", "I'm ready to learn something new -- heading your way." };
			return pickFrom(lines, 5);
		}

		return "";
	}

	/**
	 * Rough starter-profession read off the companion's isolated skill
	 * ledger (see CompanionObject::grantSkill()) -- returns "marksman",
	 * "medic", "brawler", "scout", "artisan", "entertainer", or "" if
	 * nothing recognizable is learned yet.
	 */
	static String resolveProfessionFlavor(CompanionObject* companion) {
		if (companion == nullptr) {
			return "";
		}

		for (int i = 0; i < companion->getLearnedSkillCount(); ++i) {
			String skill = companion->getLearnedSkill(i);

			if (skill.contains("marksman")) {
				return "marksman";
			} else if (skill.contains("medic")) {
				return "medic";
			} else if (skill.contains("brawler")) {
				return "brawler";
			} else if (skill.contains("scout")) {
				return "scout";
			} else if (skill.contains("artisan")) {
				return "artisan";
			} else if (skill.contains("entertainer")) {
				return "entertainer";
			}
		}

		return "";
	}

	static String pickFrom(const char* const* pool, int poolSize) {
		if (poolSize <= 0) {
			return "";
		}

		return pool[System::random(poolSize - 1)];
	}

	static String pickResponse(CompanionObject* companion, const String& orderKey) {
		// ~25% of the time, a companion with a recognizable profession
		// answers in its own voice instead of the generic squad pool.
		if (System::random(3) == 0) {
			String flavor = resolveProfessionFlavor(companion);

			if (flavor == "marksman") {
				static const char* const lines[] = { "Marksman ready.", "Scope's up, boss.", "I've got the range." };
				return pickFrom(lines, 3);
			} else if (flavor == "medic") {
				static const char* const lines[] = { "Medic standing by.", "I'll keep everyone patched up.", "Try not to bleed on my count." };
				return pickFrom(lines, 3);
			} else if (flavor == "brawler") {
				static const char* const lines[] = { "Fists ready.", "Let 'em come.", "Point me at something." };
				return pickFrom(lines, 3);
			} else if (flavor == "scout") {
				static const char* const lines[] = { "Trail's clear.", "Eyes on the horizon.", "I'll take point if you need me." };
				return pickFrom(lines, 3);
			} else if (flavor == "artisan") {
				static const char* const lines[] = { "On it -- and your gear could use a tune-up.", "Consider it done.", "Efficiency is my specialty." };
				return pickFrom(lines, 3);
			} else if (flavor == "entertainer") {
				static const char* const lines[] = { "With style, of course.", "And a one, and a two...", "For my next number..." };
				return pickFrom(lines, 3);
			}
		}

		if (orderKey == "follow") {
			static const char* const lines[] = { "On your six.", "Right behind you.", "Moving out.", "Copy that, falling in." };
			return pickFrom(lines, 4);
		} else if (orderKey == "stay") {
			static const char* const lines[] = { "Holding position.", "I'll be here.", "Standing fast.", "Not moving a muscle." };
			return pickFrom(lines, 4);
		} else if (orderKey == "patrol") {
			static const char* const lines[] = { "Beginning patrol sweep.", "Eyes open, moving out.", "Perimeter watch -- on it.", "Walking the line." };
			return pickFrom(lines, 4);
		} else if (orderKey == "store") {
			static const char* const lines[] = { "Powering down. Call if you need me.", "Heading home.", "Until next time, boss.", "Standing down." };
			return pickFrom(lines, 4);
		} else if (orderKey == "attack") {
			static const char* const lines[] = { "Engaging!", "Target acquired!", "Opening fire!", "For the squad!" };
			return pickFrom(lines, 4);
		} else if (orderKey == "formup") {
			static const char* const lines[] = { "Forming up!", "In position.", "Falling in.", "Dressing the line." };
			return pickFrom(lines, 4);
		} else if (orderKey == "guard") {
			static const char* const lines[] = { "Nobody touches them.", "On overwatch.", "I've got them covered.", "Guard duty -- understood." };
			return pickFrom(lines, 4);
		} else if (orderKey == "rangedattack") {
			static const char* const lines[] = { "Covering fire!", "Suppressing from here!", "Firing from position!", "Keep your head down!" };
			return pickFrom(lines, 4);
		} else if (orderKey == "specialone" || orderKey == "specialtwo") {
			static const char* const lines[] = { "Hitting them with everything!", "Special maneuver -- engaging!", "You'll want to watch this.", "No holding back!" };
			return pickFrom(lines, 4);
		} else if (orderKey == "group") {
			static const char* const lines[] = { "Joining the squad.", "Good to be aboard.", "Reporting for duty.", "Squad channel open." };
			return pickFrom(lines, 4);
		} else if (orderKey == "followother") {
			static const char* const lines[] = { "Escorting your friend.", "I've got them.", "They're in good hands.", "On escort detail." };
			return pickFrom(lines, 4);
		} else if (orderKey == "friend") {
			static const char* const lines[] = { "Understood -- they're one of us.", "Marked as a friend.", "I'll remember their face.", "Noted. No shooting that one." };
			return pickFrom(lines, 4);
		} else if (orderKey == "return") {
			// Companion System (2026-07-20, "massive battlefield" pass).
			static const char* const lines[] = { "Returning to post.", "Falling back to position.", "Back to my spot.", "Resuming the watch." };
			return pickFrom(lines, 4);
		}

		static const char* const lines[] = { "Copy that.", "Understood.", "On it.", "Acknowledged." };
		return pickFrom(lines, 4);
	}
};

#endif // COMPANIONCHATTER_H_
