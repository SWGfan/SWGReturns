/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	BATTLE_THEATER_2026_07_30

	Companion System (2026-07-30, "Battle Theater" -- per Nick: "lets get
	the other plays coded in", full design per c3rr's research doc
	claude/design-battle-theater-2026-07-29.md). A staged 6-faction battle
	spectacle: ~180 NPCs (30 per faction, tunable -- see
	BATTLE_THEATER_FACTION_SIZE below) fighting in clusters around a point
	near the owner, plus two ship flyover passes, ~2.5 minutes, deployable
	anywhere (no fixed location -- Nick's explicit choice over the
	recommended fixed-location alternative).

	KEY DESIGN FACTS FROM THE RESEARCH DOC, confirmed against the live
	repo before writing this file:

	1. "Stock NPCs don't fight each other" -- combat AI here only ever
	   targets players. So this can't be free-for-all AI; it's
	   CHOREOGRAPHED. Every combatant is 1-to-1 paired (index i vs index
	   i + total/2 -- a simple fixed derangement, no shuffle needed) and a
	   director task (CompanionBattleTheaterDuelTask) enqueues a real
	   "attack" via CreatureObject::executeObjectControllerAction() on each
	   pair every tick -- the SAME enqueue-a-real-combat-action mechanism
	   CompanionSpecialAttackCommand.h already proves works on any
	   CreatureObject, companion or not.

	2. Nick's confirmed choice: scripted stagger/kneel, NEVER real death.
	   Two layers make this safe: (a) every spawned combatant gets
	   OptionBitmask::INVULNERABLE OR'd onto its options bitmask right
	   after spawn -- confirmed real via TangibleObject::setOptionsBitmask()
	   / getOptionsBitmask(), and confirmed to actually gate damage (not
	   just hide the HAM bar) via 5 real checks in
	   CombatManager.cpp (`if (defender->isInvulnerable())`). Real weapon
	   swings/blaster fire still render (the attack command genuinely
	   fires) -- nothing can die, but everything LOOKS like it's fighting
	   for real, exactly the "free" visual bonus the research doc predicted.
	   (b) the actual stagger/kneel PACING beat reuses the exact
	   CreaturePosture::CROUCHED/UPRIGHT toggle-on-a-timer mechanism
	   CompanionTheLandingCommand.h's own cover-fire beat already uses and
	   Nick has already seen working live.

	3. Faction templates -- confirmed real, already-registered
	   CreatureTemplates (none invented), one clear rank-and-file combat
	   mob per faction (not elites/bosses, appropriate for a 30-per-side
	   crowd): fbase_rebel_commando (Rebel), assault_trooper (Imperial),
	   tusken_avenger (Tusken Raider), nightsister_initiate (Nightsister),
	   bounty_hunter (Bounty Hunter), corsec_agent (CorSec). This is the
	   "Classic SWG 6" set (Nick's rejected AskUserQuestion round had this
	   as the recommended pick -- going with it since Nick said to proceed
	   with best judgment and go to bed).

	4. Ship flyover -- reuses the SAME lambda_shuttle.iff template and
	   posture-toggle hover mechanic CompanionJenkinsCommand.h's ship
	   cinematic already uses, but adds genuinely NEW code this project
	   didn't have before: SceneObject::teleport(x, z, y, parentID) is a
	   real, confirmed in-world reposition method (distinct from
	   initializePosition(), which only works pre-spawn) -- used here to
	   interpolate the shuttle in a straight line across the sky over the
	   arena, tick by tick. Two passes: one near the start, one near the
	   end.

	5. Laser bolt effect -- UNCONFIRMED and NOT invented. No laser/blaster
	   cosmetic effect file could be found in this repo's assets, matching
	   the research doc's own flagged unknown. Per the doc's own
	   recommended fallback, this build does NOT fake one: the ground
	   battle gets its "laser fire" for free from real enqueued attacks
	   (point 2 above); the ship flyover is a silent hover-and-pass with no
	   invented weapons-fire effect. If Nick finds a real effect name later,
	   that's a small follow-up, not a blocker.

	6. Deployable anywhere (Nick's explicit choice over a fixed location):
	   staged at a point BATTLE_THEATER_ARENA_RADIUS meters in front of
	   wherever the owner is standing when the command fires -- the exact
	   same owner-relative-offset spawn pattern already proven 3 times this
	   project (Jenkins' shuttle, The Landing's banner, Birthday Show's
	   props), just extended to 6 faction clusters arranged in a ring
	   around that point (camera-friendly per the research doc's own
	   production suggestion -- six small clumps read better on video than
	   one blob).

	7. ⚠ PERFORMANCE, read before changing the faction size: this project
	   has never run anywhere near 180 concurrent AI-driven objects, and
	   the research doc explicitly warned against building straight to 180
	   without a smaller proof-of-concept first. Nick's explicit choice was
	   to jump straight to the full 6-faction vision anyway. As a safety
	   valve for that choice: BATTLE_THEATER_FACTION_SIZE below is the
	   ONLY number to change (single #define) to dial back to a smaller
	   count (e.g. 10) with no other code changes, if the first live test
	   shows server strain. Watch tick time on first run.

	8. NOT YET LIVE-TESTED (flagging honestly, same discipline as every
	   other new build this project has shipped): whether these specific
	   faction templates' own AI behavior scripts do anything unexpected
	   (wander, flee-at-low-HAM) once spawned with no patrol points set --
	   same open question already flagged for The Landing's rebel_recruiter
	   filler and never yet confirmed either. Since every combatant here is
	   forced INVULNERABLE and NEVER takes real damage, a flee-at-low-HAM
	   behavior specifically should never trigger (HAM never drops) -- but
	   spontaneous wandering, if the template's script has any, has not
	   been watched live. Worth an eyeball on first run.

	Overlap guard: participates in the SAME shared
	"companion_theater_mode_busy" owner-level cooldown every other Theater
	Mode show uses (armed in start(), cleared in finishShow()) -- not
	optional, per the mailbox's own standing rule for new shows, even
	though this show (unlike the others) doesn't touch any real companion
	object at all.

	Header-only (all methods in-class => implicitly inline; no new .cpp).
*/

#ifndef COMPANIONBATTLETHEATERCOMMAND_H_
#define COMPANIONBATTLETHEATERCOMMAND_H_

#include "server/zone/objects/creature/commands/QueueCommand.h"
#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/creature/ai/AiAgent.h"
#include "server/zone/managers/creature/CreatureManager.h"
#include "server/zone/Zone.h"
#include "server/zone/ZoneServer.h"
#include "templates/params/creature/CreaturePosture.h"
#include "templates/params/OptionBitmask.h"

// BATTLE_THEATER_2026_07_30 -- tunable knobs. FACTION_SIZE is the ONE number
// to change if 180 concurrent AI objects chokes the server on first test --
// see file header point 7.
#define BATTLE_THEATER_FACTION_COUNT 6
#define BATTLE_THEATER_FACTION_SIZE 30
#define BATTLE_THEATER_CLUSTER_JITTER 6.f
#define BATTLE_THEATER_PAIR_RING_RADIUS 20.f // BATTLE_THEATER_PAIRING_PLACEMENT_HOTFIX_2026_07_30
#define BATTLE_THEATER_PAIR_SEPARATION 4.f // BATTLE_THEATER_PAIRING_PLACEMENT_HOTFIX_2026_07_30
#define BATTLE_THEATER_RING_RADIUS 18.f
#define BATTLE_THEATER_ARENA_RADIUS 35.f
#define BATTLE_THEATER_RUNTIME_MS 150000 // ~2.5 min, Nick's confirmed "short" pick
#define BATTLE_THEATER_DUEL_TICK_MS 4000
#define BATTLE_THEATER_SHUTTLE_TEMPLATE "object/creature/npc/theme_park/lambda_shuttle.iff"
#define BATTLE_THEATER_SHUTTLE_ALTITUDE 40.f
#define BATTLE_THEATER_SHUTTLE_TICKS 10
#define BATTLE_THEATER_SHUTTLE_TICK_MS 1500

struct BattleTheaterFactionDef {
	const char* templateName;
	const char* label;
};

// BATTLE_THEATER_2026_07_30 -- the "Classic SWG 6", all confirmed real,
// already-registered CreatureTemplates (file header point 3).
static const BattleTheaterFactionDef BATTLE_THEATER_FACTIONS[BATTLE_THEATER_FACTION_COUNT] = {
	{ "fbase_rebel_commando", "Rebel" },
	{ "assault_trooper", "Imperial" },
	{ "tusken_avenger", "Tusken Raider" },
	{ "nightsister_initiate", "Nightsister" },
	{ "bounty_hunter", "Bounty Hunter" },
	{ "corsec_agent", "CorSec" }
};

/** BATTLE_THEATER_2026_07_30 -- per-show state, kept alive across every
 * deferred task via a Reference. combatantIDs is a flat list across all
 * factions; opponentOffset is the fixed derangement (i fights
 * i + opponentOffset, wrapping) -- avoids any shuffle/RNG bookkeeping while
 * still guaranteeing no combatant is paired with itself as long as the
 * total combatant count is even (BATTLE_THEATER_FACTION_COUNT *
 * BATTLE_THEATER_FACTION_SIZE always is, by construction). */
class CompanionBattleTheaterState : public Object {
public:
	ManagedWeakReference<CreatureObject*> ownerRef;
	Vector<uint64> combatantIDs;
	int opponentOffset;
	uint64 shuttleID;
	float centerX, centerZ, centerY;
	int elapsedMs;

	CompanionBattleTheaterState() {
		opponentOffset = 0;
		shuttleID = 0;
		centerX = 0.f;
		centerZ = 0.f;
		centerY = 0.f;
		elapsedMs = 0;
	}
};

/** BATTLE_THEATER_2026_07_30 -- the director loop. Reschedules itself every
 * BATTLE_THEATER_DUEL_TICK_MS until the runtime budget is spent, then hands
 * off to finishShow(). Every tick: each still-resolvable pair gets a real
 * "attack" enqueued on each other (file header point 1/2 -- safe because
 * every combatant is INVULNERABLE), plus a per-combatant random chance of a
 * scripted stagger/kneel posture toggle (file header point 2b) so the
 * battle has visible pacing instead of being a static tableau. */
class CompanionBattleTheaterDuelTask : public Task {
	Reference<CompanionBattleTheaterState*> state;

public:
	CompanionBattleTheaterDuelTask(CompanionBattleTheaterState* state) {
		this->state = state;
	}

	void run();
};

/** BATTLE_THEATER_2026_07_30 -- moves the flyover shuttle in a straight line
 * across the sky above the arena over BATTLE_THEATER_SHUTTLE_TICKS ticks
 * using SceneObject::teleport() (file header point 4 -- genuinely new
 * movement code this project didn't have before), then despawns it.
 * Re-resolves the shuttle by ID fresh every tick, matching every other
 * deferred-task precedent in this project (never holds a raw pointer across
 * a delay). */
class CompanionBattleTheaterFlyoverTask : public Task {
	Reference<CompanionBattleTheaterState*> state;
	float startX, startY, endX, endY;
	int tick;

public:
	CompanionBattleTheaterFlyoverTask(CompanionBattleTheaterState* state, float startX, float startY, float endX, float endY, int tick) {
		this->state = state;
		this->startX = startX;
		this->startY = startY;
		this->endX = endX;
		this->endY = endY;
		this->tick = tick;
	}

	void run();
};

/** BATTLE_THEATER_2026_07_30 -- see file header for the full design. Static,
 * header-only, mirrors CompanionTheLandingShow/CompanionBirthdayShow's own
 * shape (state object + scheduleTask chain) even though this show has no
 * real companion involvement at all. */
class CompanionBattleTheaterShow {
public:
	static void say(CreatureObject* owner, const String& msg) {
		if (owner == nullptr) {
			return;
		}

		owner->sendSystemMessage(msg);
	}

	/** @pre { nothing locked -- locks the spawned NPC itself } */
	static uint64 spawnCombatant(ZoneServer* zoneServer, Zone* zone, const BattleTheaterFactionDef& faction, float x, float y) {
		if (zoneServer == nullptr || zone == nullptr) {
			return 0;
		}

		CreatureManager* creatureManager = zone->getCreatureManager();

		if (creatureManager == nullptr) {
			return 0;
		}

		uint32 templateCRC = String(faction.templateName).hashCode(); // BATTLE_THEATER_STRINGHASH_HOTFIX_2026_07_30 -- STRING_HASHCODE is compile-time-only; faction.templateName is a runtime struct field, use the runtime hashCode() instead
		float z = zone->getHeight(x, y);

		ManagedReference<CreatureObject*> creo = creatureManager->spawnCreatureWithAi(templateCRC, x, z, y, 0, false);

		if (creo == nullptr) {
			return 0;
		}

		ManagedReference<AiAgent*> agent = creo.castTo<AiAgent*>();

		if (agent == nullptr) {
			return 0;
		}

		Locker locker(agent);

		// BATTLE_THEATER_2026_07_30 -- force-invulnerable (file header
		// point 2a). These templates are real killable combat mobs by
		// default; this is the safety layer that makes real enqueued
		// attacks render visually without ever causing real death.
		// BATTLE_THEATER_AI_SAFETY_HOTFIX_2026_07_30 -- CRITICAL: INVULNERABLE alone only stops this NPC
		// from being damaged/killed -- it does NOT stop it from autonomously
		// attacking real nearby players/NPCs, which is exactly what happened
		// in the first live test (player incapped/killed by theater combatants).
		// AIENABLED (0x80) is what gates AiAgentImplementation::runBehaviorTree()/
		// activateAiBehavior() -- the AI's own autonomous perception+targeting
		// loop. Clearing it means this agent NEVER independently selects or
		// attacks any target on its own; the only attacks that ever fire are
		// the explicit, scripted executeObjectControllerAction() calls the
		// director task issues between paired combatants, which never target
		// the owner/player or any real-world object.
		uint32 combatantBitmask = agent->getOptionsBitmask();
		combatantBitmask |= OptionBitmask::INVULNERABLE;
		combatantBitmask &= ~((uint32) OptionBitmask::AIENABLED);
		agent->setOptionsBitmask(combatantBitmask, true);

		uint64 id = agent->getObjectID();

		return id;
	}

	/** @pre { nothing locked } */
	static void spawnFactions(ZoneServer* zoneServer, Zone* zone, Reference<CompanionBattleTheaterState*> state) {
		if (zoneServer == nullptr || zone == nullptr || state == nullptr) {
			return;
		}

		// BATTLE_THEATER_PAIRING_PLACEMENT_HOTFIX_2026_07_30 -- place each duel PAIR close together (a few
		// meters apart, facing across a short gap) instead of clustering
		// purely by faction. The original faction-ring layout put every
		// paired opponent in the DIAMETRICALLY OPPOSITE cluster (~30-36m
		// apart), which is beyond weapon range and routinely LOS-blocked by
		// the packed clusters -- confirmed as the real cause of every
		// enqueued "attack" silently failing TOOFAR/line-of-sight in
		// CombatQueueCommand::doCombatAction(). Pairs are still spread
		// around a ring across the arena so the show reads as one large
		// distributed battle, not a single scrum.
		int total = BATTLE_THEATER_FACTION_COUNT * BATTLE_THEATER_FACTION_SIZE;
		int totalPairs = total / 2;

		Vector<float> secondX;
		Vector<float> secondY;
		Vector<int> secondFactionIdx;

		for (int p = 0; p < totalPairs; ++p) {
			float pairAngle = (float) p * (2.f * (float) M_PI / (float) totalPairs);
			float slotX = state->centerX + sin(pairAngle) * BATTLE_THEATER_PAIR_RING_RADIUS;
			float slotY = state->centerY + cos(pairAngle) * BATTLE_THEATER_PAIR_RING_RADIUS;

			// perpendicular offset so the two combatants stand a short,
			// in-weapon-range gap apart rather than on top of each other
			float perpX = cos(pairAngle) * (BATTLE_THEATER_PAIR_SEPARATION / 2.f);
			float perpY = -sin(pairAngle) * (BATTLE_THEATER_PAIR_SEPARATION / 2.f);

			int firstFaction = p / BATTLE_THEATER_FACTION_SIZE;
			int secondFaction = (p + totalPairs) / BATTLE_THEATER_FACTION_SIZE;

			float jx1 = slotX + perpX + (((float) (System::random(1000)) / 1000.f) - 0.5f) * 1.f;
			float jy1 = slotY + perpY + (((float) (System::random(1000)) / 1000.f) - 0.5f) * 1.f;
			float jx2 = slotX - perpX + (((float) (System::random(1000)) / 1000.f) - 0.5f) * 1.f;
			float jy2 = slotY - perpY + (((float) (System::random(1000)) / 1000.f) - 0.5f) * 1.f;

			// spawn this pair's FIRST member now (fills combatantIDs[p],
			// matching the existing i <-> i+total/2 index math below) and
			// remember the SECOND member's slot to spawn once every first
			// member has claimed index 0..totalPairs-1 in order
			uint64 id1 = spawnCombatant(zoneServer, zone, BATTLE_THEATER_FACTIONS[firstFaction], jx1, jy1);

			if (id1 != 0) {
				state->combatantIDs.add(id1);
			}

			secondX.add(jx2);
			secondY.add(jy2);
			secondFactionIdx.add(secondFaction);
		}

		for (int p = 0; p < totalPairs; ++p) {
			uint64 id2 = spawnCombatant(zoneServer, zone, BATTLE_THEATER_FACTIONS[secondFactionIdx.get(p)], secondX.get(p), secondY.get(p));

			if (id2 != 0) {
				state->combatantIDs.add(id2);
			}
		}

		// Fixed derangement: combatant i fights combatant i + (total/2),
		// wrapping -- guarantees no self-pairing as long as the total is
		// even, which it always is (FACTION_COUNT * FACTION_SIZE). NOTE:
		// if any individual spawnCombatant() call above failed (id == 0),
		// combatantIDs.size() will be less than `total` and this offset
		// self-adjusts to the reduced list -- pairing stays internally
		// consistent, it just won't exactly match the geometric pairing
		// computed above for the few dropped slots.
		state->opponentOffset = state->combatantIDs.size() / 2;
	}

	/** @pre { nothing locked } */
	static void despawnAll(ZoneServer* zoneServer, Reference<CompanionBattleTheaterState*> state) {
		if (zoneServer == nullptr || state == nullptr) {
			return;
		}

		for (int i = 0; i < state->combatantIDs.size(); ++i) {
			ManagedReference<SceneObject*> obj = zoneServer->getObject(state->combatantIDs.get(i));

			if (obj == nullptr) {
				continue;
			}

			Locker locker(obj);
			obj->destroyObjectFromWorld(true);
		}

		if (state->shuttleID != 0) {
			ManagedReference<SceneObject*> shuttleObj = zoneServer->getObject(state->shuttleID);

			if (shuttleObj != nullptr) {
				Locker slocker(shuttleObj);
				shuttleObj->destroyObjectFromWorld(true);
			}

			state->shuttleID = 0;
		}
	}

	/** BATTLE_THEATER_2026_07_30 -- clears the shared overlap-guard cooldown
	 * (see file header, "Overlap guard") and despawns the whole cast.
	 * @pre { nothing locked } */
	static void finishShow(Reference<CompanionBattleTheaterState*> state) {
		if (state == nullptr) {
			return;
		}

		CreatureObject* owner = state->ownerRef.get();

		if (owner == nullptr) {
			// Owner logged out mid-show -- still need to clean up the cast.
			return;
		}

		Locker locker(owner);

		ZoneServer* zoneServer = owner->getZoneServer();

		despawnAll(zoneServer, state);

		owner->updateCooldownTimer("companion_theater_mode_busy", 0);

		say(owner, "The battle winds down and the field clears.");
	}

	static void scheduleFlyover(Reference<CompanionBattleTheaterState*> state, bool eastToWest) {
		if (state == nullptr) {
			return;
		}

		float halfSpan = BATTLE_THEATER_RING_RADIUS + BATTLE_THEATER_ARENA_RADIUS;
		float sx = eastToWest ? state->centerX - halfSpan : state->centerX + halfSpan;
		float sy = state->centerY;
		float ex = eastToWest ? state->centerX + halfSpan : state->centerX - halfSpan;
		float ey = state->centerY;

		Reference<CompanionBattleTheaterFlyoverTask*> task = new CompanionBattleTheaterFlyoverTask(state, sx, sy, ex, ey, 0);
		task->schedule(100);
	}

	/** @pre { nothing locked -- locks owner internally } */
	static void start(CreatureObject* owner) {
		if (owner == nullptr) {
			return;
		}

		if (owner->isInCombat()) {
			say(owner, "Not while you're in combat.");
			return;
		}

		if (!owner->checkCooldownRecovery("companion_theater_mode_busy")) {
			say(owner, "Another Theater Mode show is already running -- wait for it to finish.");
			return;
		}

		Zone* zone = owner->getZone();
		ZoneServer* zoneServer = owner->getZoneServer();

		if (zone == nullptr || zoneServer == nullptr) {
			return;
		}

		owner->updateCooldownTimer("companion_theater_mode_busy", 600000);

		Reference<CompanionBattleTheaterState*> state = new CompanionBattleTheaterState();
		state->ownerRef = owner;

		float angle = owner->getDirectionAngle() * (float) (M_PI / 180.0);
		state->centerX = owner->getPositionX() + sin(angle) * BATTLE_THEATER_ARENA_RADIUS;
		state->centerY = owner->getPositionY() + cos(angle) * BATTLE_THEATER_ARENA_RADIUS;
		state->centerZ = zone->getHeight(state->centerX, state->centerY);

		say(owner, "Six factions take the field. This won't be quiet.");

		spawnFactions(zoneServer, zone, state);

		Reference<CompanionBattleTheaterDuelTask*> duelTask = new CompanionBattleTheaterDuelTask(state);
		duelTask->schedule(2000);

		// Two flyover passes: one early, one near the end.
		scheduleFlyover(state, true);

		Core::getTaskManager()->scheduleTask([state] () {
			CompanionBattleTheaterShow::scheduleFlyover(state, false);
		}, "BattleTheaterSecondFlyover", BATTLE_THEATER_RUNTIME_MS - 30000);

		Core::getTaskManager()->scheduleTask([state] () {
			CompanionBattleTheaterShow::finishShow(state);
		}, "BattleTheaterFinish", BATTLE_THEATER_RUNTIME_MS);
	}
};

inline void CompanionBattleTheaterDuelTask::run() {
	if (state == nullptr) {
		return;
	}

	CreatureObject* owner = state->ownerRef.get();

	if (owner == nullptr) {
		return; // owner gone -- finishShow()'s own scheduled call still fires and cleans up
	}

	ZoneServer* zoneServer = owner->getZoneServer();

	if (zoneServer == nullptr) {
		return;
	}

	int total = state->combatantIDs.size();

	if (total > 0 && state->opponentOffset > 0) {
		for (int i = 0; i < total; ++i) {
			int opponentIdx = (i + state->opponentOffset) % total;

			if (opponentIdx == i) {
				continue;
			}

			ManagedReference<SceneObject*> selfObj = zoneServer->getObject(state->combatantIDs.get(i));
			ManagedReference<SceneObject*> oppObj = zoneServer->getObject(state->combatantIDs.get(opponentIdx));

			if (selfObj == nullptr || oppObj == nullptr) {
				continue;
			}

			CreatureObject* selfCreo = selfObj->asCreatureObject();
			CreatureObject* oppCreo = oppObj->asCreatureObject();

			if (selfCreo == nullptr || oppCreo == nullptr) {
				continue;
			}

			Locker locker(selfCreo);

			if (selfCreo->isDead() || selfCreo->isIncapacitated()) {
				locker.release();
				continue;
			}

			// Real attack, real weapon-fire visuals, zero real damage
			// possible (INVULNERABLE, file header point 2a).
			selfCreo->executeObjectControllerAction(STRING_HASHCODE("attack"), oppCreo->getObjectID(), "");

			// Occasional scripted stagger -- same mechanism The Landing's
			// cover-fire beat already uses, Nick's confirmed choice.
			if (System::random(100) < 20) {
				selfCreo->setPosture(CreaturePosture::CROUCHED, true, true);

				Core::getTaskManager()->scheduleTask([selfCreo] () {
					if (selfCreo == nullptr) {
						return;
					}

					Locker relocker(selfCreo);

					if (!selfCreo->isDead()) {
						selfCreo->setPosture(CreaturePosture::UPRIGHT, true, true);
					}
				}, "BattleTheaterStaggerRecover", 2000);
			}

			locker.release();
		}
	}

	state->elapsedMs += BATTLE_THEATER_DUEL_TICK_MS;

	if (state->elapsedMs < BATTLE_THEATER_RUNTIME_MS) {
		Reference<CompanionBattleTheaterDuelTask*> next = new CompanionBattleTheaterDuelTask(state);
		next->schedule(BATTLE_THEATER_DUEL_TICK_MS);
	}
}

inline void CompanionBattleTheaterFlyoverTask::run() {
	if (state == nullptr) {
		return;
	}

	CreatureObject* owner = state->ownerRef.get();

	if (owner == nullptr) {
		return;
	}

	ZoneServer* zoneServer = owner->getZoneServer();
	Zone* zone = owner->getZone();

	if (zoneServer == nullptr || zone == nullptr) {
		return;
	}

	float progress = (float) tick / (float) (BATTLE_THEATER_SHUTTLE_TICKS - 1);
	float px = startX + (endX - startX) * progress;
	float py = startY + (endY - startY) * progress;
	float pz = zone->getHeight(px, py) + BATTLE_THEATER_SHUTTLE_ALTITUDE;

	if (tick == 0) {
		ManagedReference<SceneObject*> shuttleObj = zoneServer->createObject(String(BATTLE_THEATER_SHUTTLE_TEMPLATE).hashCode(), 0);

		if (shuttleObj == nullptr) {
			return;
		}

		Locker shuttleLocker(shuttleObj);

		shuttleObj->initializePosition(px, pz, py);
		zone->transferObject(shuttleObj, -1, true);

		CreatureObject* shuttle = shuttleObj->asCreatureObject();

		if (shuttle != nullptr) {
			shuttle->setPosture(CreaturePosture::UPRIGHT, true, true);
		}

		state->shuttleID = shuttleObj->getObjectID();
	} else {
		ManagedReference<SceneObject*> shuttleObj = zoneServer->getObject(state->shuttleID);

		if (shuttleObj == nullptr) {
			return;
		}

		Locker shuttleLocker(shuttleObj);

		if (tick >= BATTLE_THEATER_SHUTTLE_TICKS - 1) {
			// Last tick -- final position, then despawn.
			shuttleObj->teleport(px, pz, py, 0);
			shuttleObj->destroyObjectFromWorld(true);
			state->shuttleID = 0;
			return;
		}

		shuttleObj->teleport(px, pz, py, 0);
	}

	Reference<CompanionBattleTheaterFlyoverTask*> next = new CompanionBattleTheaterFlyoverTask(state, startX, startY, endX, endY, tick + 1);
	next->schedule(BATTLE_THEATER_SHUTTLE_TICK_MS);
}

class CompanionBattleTheaterCommand : public QueueCommand {
public:

	CompanionBattleTheaterCommand(const String& name, ZoneProcessServer* server)
		: QueueCommand(name, server) {

	}

	int doQueueCommand(CreatureObject* creature, const uint64& target, const UnicodeString& arguments) const {
		if (!checkStateMask(creature)) {
			return INVALIDSTATE;
		}

		if (!checkInvalidLocomotions(creature)) {
			return INVALIDLOCOMOTION;
		}

		CompanionBattleTheaterShow::start(creature);

		return SUCCESS;
	}
};

#endif // COMPANIONBATTLETHEATERCOMMAND_H_
