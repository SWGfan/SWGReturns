/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-20, per user request "I want ALL crafting to be
	a theater -- see them interact every time, chat saying what they traded
	and how much, and only the amount they needed, not a whole stack").

	This is the visible-collaboration layer that wraps the Companion chat's
	headless CompanionCraftingManager::craftItem(). Before the actual craft
	runs, this reads the schematic's real resource slots (class + quantity),
	works out what the crafting companion is short on, and for each shortfall
	finds a squad-mate carrying that resource. Each donor then WALKS to the
	crafter and hands over EXACTLY the number of units needed (ResourceContainer
	::split(amount, crafter) -- a partial transfer, never the whole stack),
	announced in spatial chat ("traded 45 units of Ipee (metal)"), finished
	with a high five. Only once every trade is done does craftItem() run --
	which still tops up any remaining gap from harvesters / resource deeds.

	Header-only, all-static (same shape as CompanionFireworksShow). The
	walk-to-meet + high-five pattern mirrors the fireworks/camp fetch.

	COMPLETION CALLBACK (2026-07-23, FIXED same day -- live bug report:
	"Armorsmith has finished the whole bone suit!" was announced right after
	a piece had actually failed to craft, e.g. "is missing a component it
	can't make: shared_armor_segment_bone.iff"). The first pass of this
	callback used std::function<void()> with NO success/failure signal, so
	CompanionArmorTypeSuiCallback.h's "craft whole suit" chain always
	continued to the next piece (and always announced full success at the
	end) no matter what craftItem() actually returned. Fixed by changing the
	signature to std::function<void(bool)> -- the bool is the real
	craftItem() success/failure result, fired from finishCraft(). Every
	existing call site still compiles: the default is an empty
	std::function<void(bool)>, and `if (onComplete) onComplete(success);` is
	a safe no-op for any caller that doesn't supply one. Callers that chain
	multiple crafts MUST check the bool before continuing -- see
	CompanionArmorPieceSuiCallback::craftAllPieces() for the corrected
	pattern.
*/

#ifndef COMPANIONCRAFTTHEATER_H_
#define COMPANIONCRAFTTHEATER_H_

#include <functional>

#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/objects/resource/ResourceContainer.h"
#include "server/zone/objects/resource/ResourceSpawn.h"
#include "server/zone/objects/draftschematic/DraftSchematic.h"
#include "server/zone/objects/creature/ai/PatrolPoint.h"
#include "templates/crafting/draftslot/DraftSlot.h"
#include "server/zone/objects/manufactureschematic/ingredientslots/IngredientSlot.h"
#include "server/zone/managers/companion/CompanionCraftingManager.h"
#include "server/chat/ChatManager.h"
#include "server/zone/Zone.h"
#include "server/zone/ZoneServer.h"
#include "server/zone/objects/cell/CellObject.h"

class CompanionCraftTrade : public Object {
public:
	uint64 donorID = 0;
	String resourceClass;
	int amount = 0;
};

class CompanionCraftTheaterState : public Object {
public:
	uint64 crafterID = 0;
	String schematicPath;
	Vector<Reference<CompanionCraftTrade*> > trades;
	int tradeIndex = 0;
	int steps = 0;
	// 2026-07-23: fired exactly once, from finishCraft(), with the real
	// craftItem() success/failure result. Empty std::function is falsy, so
	// "if (state->onComplete)" is always safe even when nothing was passed.
	std::function<void(bool)> onComplete;
};

class CompanionCraftTheater {
public:

	static void say(CompanionObject* companion, const String& text) {
		if (companion == nullptr) {
			return;
		}

		ZoneServer* zoneServer = companion->getZoneServer();

		if (zoneServer == nullptr) {
			return;
		}

		ChatManager* chatManager = zoneServer->getChatManager();

		if (chatManager != nullptr) {
			chatManager->broadcastChatMessage(companion, UnicodeString(text), 0, 0, 0, 0, 1);
		}
	}

	static int countResource(CompanionObject* companion, const String& classToken) {
		int total = 0;

		auto scan = [&](SceneObject* container) {
			if (container == nullptr) {
				return;
			}

			for (int i = 0; i < container->getContainerObjectsSize(); ++i) {
				ManagedReference<SceneObject*> obj = container->getContainerObject(i);

				if (obj == nullptr || !obj->isResourceContainer()) {
					continue;
				}

				ResourceContainer* rc = cast<ResourceContainer*>(obj.get());

				if (rc == nullptr || rc->getQuantity() <= 0) {
					continue;
				}

				ManagedReference<ResourceSpawn*> spawn = rc->getSpawnObject();

				if (spawn != nullptr && spawn->isType(classToken)) {
					total += rc->getQuantity();
				}
			}
		};

		scan(companion);
		scan(companion->getSlottedObject("inventory"));
		return total;
	}

	static void resumeFollow(CompanionObject* companion, CreatureObject* owner) {
		if (companion == nullptr || companion->isDead() || companion->getZone() == nullptr) {
			return;
		}

		// COMPANION_TAXI_ARRIVAL_WAIT_2026_08_05 -- this was the one restore path in the whole
		// companion system that forced FOLLOW without checking standingOrder
		// first (every other one, restoreStandingPosture() included, already
		// has this branch). A companion explicitly parked with STAY -- most
		// concretely, one waiting at a taxi destination for its owner -- could
		// get yanked back to the owner by an unrelated periodic gear-exchange
		// check that happens to call this when it gives up. Respecting STAY
		// here matches every other helper and closes that gap.
		if (companion->getStandingOrder() == CompanionObject::STAY) {
			companion->setCompanionState(CompanionObject::STAY);
			companion->setFollowObject(nullptr);
			companion->setOblivious();
			return;
		}

		companion->setCompanionState(CompanionObject::FOLLOW);
		companion->setFollowObject(owner);
		companion->setFollowState(AiAgent::FOLLOWING); // genesis port: was setMovementState()
		companion->clearPatrolPoints();
	}

	/** Recover any of the owner's companions left stranded in PATROL by a
	 * previous interrupted/failed theater (2026-07-20, user: "if a companion
	 * fails at crafting or setting up a tent, they should go back to what
	 * they were doing"). A companion in PATROL that ISN'T in an active taxi
	 * ride or loot sweep is a straggler -> send it back to following the
	 * owner. Skips STAY/GUARD/FOLLOW/ATTACK (legit states). @pre nothing
	 * locked. */
	static void recoverStragglers(CreatureObject* owner, CompanionObject* excluding) {
		if (owner == nullptr) {
			return;
		}

		// Deferred so it never nests companion locks inside the caller's
		// already-held crafter/player locks (the deadlock lesson).
		ManagedReference<CreatureObject*> ownerRef = owner;
		uint64 excludeID = excluding != nullptr ? excluding->getObjectID() : 0;

		Core::getTaskManager()->executeTask([ownerRef, excludeID] () {
			CreatureObject* owner = ownerRef.get();

			if (owner == nullptr) {
				return;
			}

			// Lock the owner (root) so the per-companion cross-locks below
			// are valid (crash-lesson: cross needs to be held).
			Locker ownerLocker(owner);

			ManagedReference<SceneObject*> datapad = owner->getSlottedObject("datapad");

			if (datapad == nullptr) {
				return;
			}

			for (int i = 0; i < datapad->getContainerObjectsSize(); ++i) {
				ManagedReference<SceneObject*> obj = datapad->getContainerObject(i);

				if (obj == nullptr || !obj->isCompanionControlDevice()) {
					continue;
				}

				CompanionControlDevice* device = cast<CompanionControlDevice*>(obj.get());

				if (device == nullptr || device->isCompanionDead()) {
					continue;
				}

				CompanionObject* comp = device->getCompanionObject();

				if (comp == nullptr || comp->getObjectID() == excludeID || comp->getZone() == nullptr
						|| comp->getLinkedCreature().get() != owner) {
					continue;
				}

				if (comp->isInCombat() || comp->isTaxiActive() || comp->isLootSweepActive()) {
					continue; // legitimately busy
				}

				if (comp->getCompanionState() == CompanionObject::PATROL) {
					Locker clocker(comp, owner);
					resumeFollow(comp, owner);
				}
			}
		}, "CompanionRecoverStragglersLambda");
	}

	/** Entry point: build the trade plan, then either run the theater or
	 * craft straight away. onComplete (2026-07-23) fires exactly once, with
	 * the real craftItem() success/failure result -- optional, defaults to
	 * a no-op so every pre-existing caller is unaffected.
	 * @pre { companion crosslocked to player } */
	static void begin(CreatureObject* owner, CompanionObject* crafter, const String& schematicPath, std::function<void(bool)> onComplete = std::function<void(bool)>()) {
		if (owner == nullptr || crafter == nullptr) {
			if (onComplete) {
				onComplete(false);
			}
			return;
		}

		ZoneServer* zoneServer = owner->getZoneServer();
		Zone* zone = crafter->getZone();

		if (zoneServer == nullptr || zone == nullptr) {
			if (onComplete) {
				onComplete(false);
			}
			return;
		}

		// Heal any companions stranded by a previous interrupted theater
		// before starting this one (not the crafter -- it's about to work).
		recoverStragglers(owner, crafter);

		// Read the schematic's resource needs.
		String file = schematicPath;

		if (file.indexOf("draft_schematic") == -1) {
			file = "object/draft_schematic/" + file;
		}

		if (file.indexOf(".iff") == -1) {
			file = file + ".iff";
		}

		ManagedReference<DraftSchematic*> schematic = zoneServer->createObject(file.hashCode(), 0).castTo<DraftSchematic*>();

		if (schematic == nullptr) {
			craftNow(owner, crafter, schematicPath, onComplete);
			return;
		}

		// Per resource class: how many units the schematic needs vs the
		// crafter's own stock -> the shortfall we try to trade for.
		VectorMap<String, int> shortfalls; // class -> units still needed

		for (int i = 0; i < schematic->getDraftSlotCount(); ++i) {
			DraftSlot* slot = schematic->getDraftSlot(i);

			if (slot == nullptr || slot->getSlotType() != IngredientSlot::RESOURCESLOT) {
				continue;
			}

			String resourceClass = slot->getResourceType();
			int needed = (int) slot->getQuantity();
			int have = countResource(crafter, resourceClass);
			int shortHere = needed - have;

			if (shortHere <= 0) {
				continue;
			}

			int existing = shortfalls.contains(resourceClass) ? shortfalls.get(resourceClass) : 0;
			shortfalls.drop(resourceClass);
			shortfalls.put(resourceClass, existing + shortHere);
		}

		Reference<CompanionCraftTheaterState*> state = new CompanionCraftTheaterState();
		state->crafterID = crafter->getObjectID();
		state->schematicPath = schematicPath;
		state->onComplete = onComplete;

		// For each shortfall, find a sibling donor carrying that class and
		// queue a trade for the EXACT number of units still needed (capped
		// at what the donor actually holds).
		ManagedReference<SceneObject*> datapad = owner->getSlottedObject("datapad");

		if (datapad != nullptr) {
			for (int s = 0; s < shortfalls.size(); ++s) {
				String resourceClass = shortfalls.elementAt(s).getKey();
				int stillNeeded = shortfalls.get(resourceClass);

				for (int i = 0; i < datapad->getContainerObjectsSize() && stillNeeded > 0; ++i) {
					ManagedReference<SceneObject*> obj = datapad->getContainerObject(i);

					if (obj == nullptr || !obj->isCompanionControlDevice()) {
						continue;
					}

					CompanionControlDevice* device = cast<CompanionControlDevice*>(obj.get());

					if (device == nullptr || device->isCompanionDead()) {
						continue;
					}

					CompanionObject* sibling = device->getCompanionObject();

					if (sibling == nullptr || sibling == crafter || sibling->getZone() != zone
							|| sibling->getLinkedCreature().get() != owner) {
						continue;
					}

					int siblingHas = countResource(sibling, resourceClass);

					if (siblingHas <= 0) {
						continue;
					}

					int tradeAmount = siblingHas < stillNeeded ? siblingHas : stillNeeded;

					Reference<CompanionCraftTrade*> trade = new CompanionCraftTrade();
					trade->donorID = sibling->getObjectID();
					trade->resourceClass = resourceClass;
					trade->amount = tradeAmount;
					state->trades.add(trade);

					stillNeeded -= tradeAmount;
				}
			}
		}

		if (state->trades.size() == 0) {
			// Nobody to trade with -> craft from own stock + harvesters + deeds.
			craftNow(owner, crafter, schematicPath, onComplete);
			return;
		}

		say(crafter, "Alright team -- I need a few materials for this. Who's got what?");

		Reference<CreatureObject*> ownerRef = owner;
		scheduleStep(zoneServer, ownerRef, state, 400);
	}

	static void scheduleStep(ZoneServer* zoneServer, Reference<CreatureObject*> ownerRef, Reference<CompanionCraftTheaterState*> state, int delayMs) {
		Core::getTaskManager()->scheduleTask([zoneServer, ownerRef, state] () {
			runStep(zoneServer, ownerRef, state);
		}, "CompanionCraftTheaterStepLambda", delayMs);
	}

	static void runStep(ZoneServer* zoneServer, Reference<CreatureObject*> ownerRef, Reference<CompanionCraftTheaterState*> state) {
		CreatureObject* owner = ownerRef.get();

		if (owner == nullptr || state == nullptr || zoneServer == nullptr) {
			return;
		}

		ManagedReference<SceneObject*> crafterObj = zoneServer->getObject(state->crafterID);
		CompanionObject* crafter = crafterObj != nullptr ? crafterObj.castTo<CompanionObject*>().get() : nullptr;

		if (crafter == nullptr) {
			if (state->onComplete) {
				state->onComplete(false);
			}
			return;
		}

		Locker clocker(crafter);

		if (crafter->getZone() == nullptr || crafter->isDead() || crafter->isInCombat() || owner->isInCombat()) {
			resumeFollow(crafter, owner);

			if (state->onComplete) {
				state->onComplete(false);
			}
			return;
		}

		if (++state->steps > 300) { // 2-minute hard cap
			resumeFollow(crafter, owner);
			craftAfterTrades(owner, crafter, state);
			return;
		}

		// All trades done -> craft.
		if (state->tradeIndex >= state->trades.size()) {
			craftAfterTrades(owner, crafter, state);
			return;
		}

		CompanionCraftTrade* trade = state->trades.get(state->tradeIndex).get();

		ManagedReference<SceneObject*> donorObj = zoneServer->getObject(trade->donorID);
		CompanionObject* donor = donorObj != nullptr ? donorObj.castTo<CompanionObject*>().get() : nullptr;

		if (donor == nullptr || donor->getZone() != crafter->getZone() || donor->isDead() || donor->isInCombat()) {
			// Skip this trade -- move on; craftItem's deed fallback covers it.
			++state->tradeIndex;
			scheduleStep(zoneServer, ownerRef, state, 400);
			return;
		}

		Locker dlocker(donor, crafter);

		// Walk the donor to the crafter.
		if (donor->getDistanceTo(crafter) > 5.f) {
			donor->setCompanionState(CompanionObject::PATROL);
			donor->setFollowObject(nullptr);

			if (donor->getPatrolPointSize() == 0) {
				PatrolPoint point(crafter->getPositionX(), crafter->getPositionZ(), crafter->getPositionY());
				// genesis port: setMovementState() -> setFollowState(); genesis
				// setFollowState() calls clearPatrolPoints(), so the state must be
				// set BEFORE the point is queued (see PetPatrolCommand.h).
				donor->setFollowState(AiAgent::PATROLLING);
				donor->addPatrolPoint(point);
			}

			scheduleStep(zoneServer, ownerRef, state, 400);
			return;
		}

		// At the crafter: split off EXACTLY the needed amount, container by
		// container, into the crafter -- with a per-container chat line
		// naming the resource and the units traded.
		int remaining = trade->amount;

		auto tradeFrom = [&](SceneObject* container) {
			if (container == nullptr || remaining <= 0) {
				return;
			}

			for (int i = 0; i < container->getContainerObjectsSize() && remaining > 0; ++i) {
				ManagedReference<SceneObject*> obj = container->getContainerObject(i);

				if (obj == nullptr || !obj->isResourceContainer()) {
					continue;
				}

				ResourceContainer* rc = cast<ResourceContainer*>(obj.get());

				if (rc == nullptr || rc->getQuantity() <= 0) {
					continue;
				}

				ManagedReference<ResourceSpawn*> spawn = rc->getSpawnObject();

				if (spawn == nullptr || !spawn->isType(trade->resourceClass)) {
					continue;
				}

				int give = remaining < rc->getQuantity() ? remaining : rc->getQuantity();
				String spawnName = rc->getSpawnName();

				Locker rlocker(rc, donor);

				// split(amount, creature) hands exactly `give` units to the
				// crafter -- a partial transfer, never the whole stack.
				rc->split(give, crafter);

				remaining -= give;

				say(donor, "Here's " + String::valueOf(give) + " units of " + spawnName + " (" + trade->resourceClass + ").");
			}
		};

		tradeFrom(donor);
		tradeFrom(donor->getSlottedObject("inventory"));

		donor->faceObject(crafter, true);
		crafter->faceObject(donor, true);
		// 2026-07-20 (user request, "better theater"): the GIVER bows, the
		// RECEIVER kowtows -- a little courtesy exchange over the handoff.
		donor->doAnimation("bow");
		crafter->doAnimation("kowtow");
		say(crafter, "Thanks -- that's just what I needed!");

		// 2026-07-20 (user request): after handing off, the donor steps
		// 5m AWAY from the crafter (rather than snapping back to follow),
		// then resumes follow a few seconds later.
		stepAwayThenFollow(donor, crafter, owner, 5.f);

		++state->tradeIndex;
		// Slower pacing (user request "they work too fast") -- 2.5s between
		// trades so each interaction is watchable.
		scheduleStep(zoneServer, ownerRef, state, 2500);
	}

	/** Companion walks `dist` metres away from `from`, then resumes follow
	 * after a short delay. @pre walker locked. */
	static void stepAwayThenFollow(CompanionObject* walker, SceneObject* from, CreatureObject* owner, float dist) {
		if (walker == nullptr || from == nullptr || walker->getZone() == nullptr) {
			return;
		}

		// Direction from `from` to the walker, extended by `dist`.
		float dx = walker->getPositionX() - from->getPositionX();
		float dy = walker->getPositionY() - from->getPositionY();
		float len = Math::sqrt(dx * dx + dy * dy);

		if (len < 0.5f) {
			// On top of each other -- pick an arbitrary direction.
			dx = 1.f;
			dy = 0.f;
			len = 1.f;
		}

		float tx = walker->getPositionX() + (dx / len) * dist;
		float ty = walker->getPositionY() + (dy / len) * dist;
		float tz = walker->getZone()->getHeight(tx, ty);

		walker->setCompanionState(CompanionObject::PATROL);
		walker->setFollowObject(nullptr);
		walker->clearPatrolPoints();
		PatrolPoint awayPoint(tx, tz, ty);
		// genesis port: setMovementState() -> setFollowState(); genesis
		// setFollowState() calls clearPatrolPoints(), so the state must be
		// set BEFORE the point is queued (see PetPatrolCommand.h).
		walker->setFollowState(AiAgent::PATROLLING);
		walker->addPatrolPoint(awayPoint);

		ManagedReference<CompanionObject*> walkerRef = walker;
		ManagedReference<CreatureObject*> ownerRef = owner;

		Core::getTaskManager()->scheduleTask([walkerRef, ownerRef] () {
			CompanionObject* w = walkerRef.get();
			CreatureObject* o = ownerRef.get();

			if (w == nullptr || o == nullptr) {
				return;
			}

			Locker locker(w);
			resumeFollow(w, o);
		}, "CompanionStepAwayFollowLambda", 4000);
	}

	/** Final craft once trades are done -- @pre crafter locked (from runStep). */
	static void craftAfterTrades(CreatureObject* owner, CompanionObject* crafter, Reference<CompanionCraftTheaterState*> state) {
		say(crafter, "Now to put it all together...");
		beginCraftShimmer(owner, crafter, state->schematicPath, state->onComplete);
	}

	/** No trades needed -- craft immediately (still a bit of flair). @pre
	 * companion crosslocked to player. */
	static void craftNow(CreatureObject* owner, CompanionObject* crafter, const String& schematicPath, std::function<void(bool)> onComplete = std::function<void(bool)>()) {
		say(crafter, "I've got everything I need -- crafting now.");
		beginCraftShimmer(owner, crafter, schematicPath, onComplete);
	}

	// ---- "Force-ghost" crafting shimmer (2026-07-20, user request) -------
	// While a companion is actually crafting, it blinks on and off with a
	// force shimmer so you can see WHICH companions are busy crafting right
	// now. True alpha-translucency isn't networkable (the object-create
	// packet has no scale/alpha field -- same wall as factory scaling), so
	// the "see-through, fading" look is done by rapidly toggling the
	// companion's visibility (broadcastDestroy/broadcastObject) plus the
	// force-meditate client effect. Runs for ~6s, then the item is actually
	// produced and the companion is restored fully visible.
	// Craft time (2026-07-20, user request "each item should take 10
	// seconds to make"): the visible craft/glow window. Tick every 1s so
	// the glow re-pulses and the manipulate animation stays alive.
	static const int CRAFT_TICKS = 10;
	static const int CRAFT_TICK_MS = 1000;

	// ---- Simultaneous-craft spacing (2026-07-24, user request: "the
	// companions are too close together when we are crafting, can we
	// spread everyone out a bit more, maybe give them another 2 meter
	// buffer from each other until they need to interact") -----------------
	// Only kicks in when 2+ companions are crafting AT THE SAME TIME (e.g.
	// the "craft ALL pieces" auto-chain, or the player queuing a second
	// companion while the first is still mid-craft). Does NOT touch
	// FormationManager's DEFAULT_SPACING or any combat/travel/camp
	// formation -- this is purely a one-off nudge for the crafter that's
	// JOINING an already-in-progress craft, using the exact same
	// direction-and-extend math as stepAwayThenFollow() just above. Once
	// the craft finishes (finishCraft() -> resumeFollow()) or a handoff/
	// interaction is needed (the existing converge patterns in
	// CompanionFieldStation.h), the companion naturally comes back in --
	// nothing here is persistent.
	static const int CRAFT_SPACING_BONUS = 2;

	/** Owner objectID -> companion objectIDs currently between
	 * beginCraftShimmer() starting and finishCraft() finishing. Function-
	 * local static -- same header-only-class pattern as
	 * CompanionFieldStation::deployedProps(), see that file for the
	 * C++11 rationale. */
	static VectorMap<uint64, Vector<uint64> >& activeCraftersByOwner() {
		static VectorMap<uint64, Vector<uint64> > map;
		return map;
	}

	static void registerActiveCrafter(CreatureObject* owner, CompanionObject* crafter) {
		if (owner == nullptr || crafter == nullptr) {
			return;
		}

		auto& table = activeCraftersByOwner();
		uint64 ownerID = owner->getObjectID();

		if (!table.contains(ownerID)) {
			table.put(ownerID, Vector<uint64>());
		}

		Vector<uint64>& crafters = table.get(ownerID);
		uint64 crafterID = crafter->getObjectID();

		if (crafters.find(crafterID) == -1) {
			crafters.add(crafterID);
		}
	}

	static void unregisterActiveCrafter(CreatureObject* owner, CompanionObject* crafter) {
		if (owner == nullptr || crafter == nullptr) {
			return;
		}

		auto& table = activeCraftersByOwner();
		uint64 ownerID = owner->getObjectID();

		if (!table.contains(ownerID)) {
			return;
		}

		Vector<uint64>& crafters = table.get(ownerID);
		int idx = crafters.find(crafter->getObjectID());

		if (idx != -1) {
			crafters.remove(idx);
		}
	}

	static int countActiveCrafters(CreatureObject* owner) {
		if (owner == nullptr) {
			return 0;
		}

		auto& table = activeCraftersByOwner();
		uint64 ownerID = owner->getObjectID();

		if (!table.contains(ownerID)) {
			return 0;
		}

		return table.get(ownerID).size();
	}

	/** Companion System (2026-07-29, Gear Exchange hardening batch): true if
	 * `companionID` (belonging to `owner`) is currently registered as an
	 * active crafter -- i.e. somewhere between beginCraftShimmer() starting
	 * and finishCraft() ending. Read-only query over the existing
	 * activeCraftersByOwner() table; adds no new tracking state. Used by
	 * other companion systems (e.g. CompanionGearExchangeManager) to skip a
	 * smith/candidate that's mid-craft-glow right now rather than starting a
	 * hand-off theater on top of it. */
	static bool isCompanionCrafting(CreatureObject* owner, uint64 companionID) {
		if (owner == nullptr) {
			return false;
		}

		auto& table = activeCraftersByOwner();
		uint64 ownerID = owner->getObjectID();

		if (!table.contains(ownerID)) {
			return false;
		}

		return table.get(ownerID).find(companionID) != -1;
	}

	/** Pushes `crafter` CRAFT_SPACING_BONUS metres further from the owner
	 * along its own current bearing, so a companion joining an
	 * already-in-progress craft doesn't visually stack on the one(s)
	 * already crafting. @pre crafter locked (called from
	 * beginCraftShimmer, which inherits its lock from runStep/craftNow). */
	static void widenForSimultaneousCraft(CreatureObject* owner, CompanionObject* crafter) {
		if (owner == nullptr || crafter == nullptr || crafter->getZone() == nullptr) {
			return;
		}

		float dx = crafter->getPositionX() - owner->getPositionX();
		float dy = crafter->getPositionY() - owner->getPositionY();
		float len = Math::sqrt(dx * dx + dy * dy);

		if (len < 0.5f) {
			// On top of the owner -- pick an arbitrary direction.
			dx = 1.f;
			dy = 0.f;
			len = 1.f;
		}

		float tx = crafter->getPositionX() + (dx / len) * CRAFT_SPACING_BONUS;
		float ty = crafter->getPositionY() + (dy / len) * CRAFT_SPACING_BONUS;
		float tz = crafter->getZone()->getHeight(tx, ty);

		ManagedReference<SceneObject*> parent = crafter->getParent().get();
		uint64 parentID = (parent != nullptr) ? parent->getObjectID() : 0;

		crafter->teleport(tx, tz, ty, parentID);
		crafter->setNextPosition(tx, tz, ty, parent.castTo<CellObject*>());
	}

	static void beginCraftShimmer(CreatureObject* owner, CompanionObject* crafter, const String& schematicPath, std::function<void(bool)> onComplete = std::function<void(bool)>()) {
		// If another companion of this owner is already mid-craft, THIS
		// crafter is the one joining in progress -- give it the spacing
		// bump rather than disturbing whoever's already crafting.
		if (countActiveCrafters(owner) >= 1) {
			widenForSimultaneousCraft(owner, crafter);
		}

		registerActiveCrafter(owner, crafter);

		crafter->doAnimation("manipulate_low");
		crafter->playEffect("clienteffect/pl_force_meditate_self.cef", "");
		crafter->playEffect("clienteffect/healing_healenhance.cef", "");

		ZoneServer* zoneServer = crafter->getZoneServer();

		if (zoneServer == nullptr) {
			// No glow possible -- just craft.
			finishCraft(owner, crafter, schematicPath, onComplete);
			return;
		}

		// Factory prop theater (2026-07-20, user request): a small
		// factory-style prop appears 15m from the owner while the
		// companion "manufactures," then vanishes when the craft finishes.
		// Backed by a REAL factory deed the squad must hold -- see
		// hasFactoryDeed(); the deed itself is only required/checked and
		// never consumed (it "redeeds" back to inventory automatically
		// because it was never actually deployed -- the prop is cosmetic,
		// since a real deployed factory can't be the small stand-in the
		// user wants). ~6.5s to outlast the shimmer.
		spawnFactoryProp(owner, crafter, zoneServer);

		Reference<CreatureObject*> ownerRef = owner;
		uint64 crafterID = crafter->getObjectID();
		String path = schematicPath;

		scheduleShimmer(zoneServer, ownerRef, crafterID, path, CRAFT_TICKS, onComplete);
	}

	/** True if the crafter or any of the owner's other summoned companions
	 * is carrying a factory deed. */
	static bool hasFactoryDeed(CreatureObject* owner, CompanionObject* crafter) {
		auto scan = [](SceneObject* container) -> bool {
			if (container == nullptr) {
				return false;
			}

			for (int i = 0; i < container->getContainerObjectsSize(); ++i) {
				ManagedReference<SceneObject*> obj = container->getContainerObject(i);

				if (obj == nullptr || obj->getObjectTemplate() == nullptr) {
					continue;
				}

				if (obj->getObjectTemplate()->getFullTemplateString().indexOf("factory_deed") != -1) {
					return true;
				}
			}

			return false;
		};

		if (scan(crafter) || scan(crafter->getSlottedObject("inventory"))) {
			return true;
		}

		ManagedReference<SceneObject*> datapad = owner->getSlottedObject("datapad");

		if (datapad == nullptr) {
			return false;
		}

		for (int i = 0; i < datapad->getContainerObjectsSize(); ++i) {
			ManagedReference<SceneObject*> obj = datapad->getContainerObject(i);

			if (obj == nullptr || !obj->isCompanionControlDevice()) {
				continue;
			}

			CompanionControlDevice* device = cast<CompanionControlDevice*>(obj.get());

			if (device == nullptr || device->isCompanionDead()) {
				continue;
			}

			CompanionObject* sibling = device->getCompanionObject();

			if (sibling == nullptr || sibling->getLinkedCreature().get() != owner) {
				continue;
			}

			if (scan(sibling) || scan(sibling->getSlottedObject("inventory"))) {
				return true;
			}
		}

		return false;
	}

	static void spawnFactoryProp(CreatureObject* owner, CompanionObject* crafter, ZoneServer* zoneServer) {
		Zone* zone = owner->getZone();

		if (zone == nullptr) {
			return;
		}

		if (!hasFactoryDeed(owner, crafter)) {
			say(crafter, "I'd set up a factory for the parts, but nobody's got a factory deed -- a plain workbench will have to do.");
			// Still show the prop as a generic workbench flourish.
		} else {
			say(crafter, "Setting up the factory -- give me a moment.");
		}

		// 15m in front of the owner.
		float angle = owner->getDirectionAngle() * (M_PI / 180.f);
		float px = owner->getPositionX() + sin(angle) * 15.f;
		float py = owner->getPositionY() + cos(angle) * 15.f;
		float pz = zone->getHeight(px, py);

		ManagedReference<SceneObject*> prop = zoneServer->createObject(STRING_HASHCODE("object/static/installation/mockup_factory_item_style_1.iff"), 0);

		if (prop == nullptr) {
			return;
		}

		Locker propLocker(prop);

		prop->initializePosition(px, pz, py);
		zone->transferObject(prop, -1, true);

		// Auto-remove after the shimmer window (independent task -- the prop
		// isn't tied to a companion lock).
		ManagedReference<SceneObject*> propRef = prop;

		Core::getTaskManager()->scheduleTask([propRef] () {
			SceneObject* p = propRef.get();

			if (p == nullptr) {
				return;
			}

			Locker locker(p);
			p->destroyObjectFromWorld(true);
		}, "CompanionFactoryPropRemoveLambda", 6600);
	}

	static void scheduleShimmer(ZoneServer* zoneServer, Reference<CreatureObject*> ownerRef, uint64 crafterID, const String& schematicPath, int ticksLeft, std::function<void(bool)> onComplete = std::function<void(bool)>()) {
		Core::getTaskManager()->scheduleTask([zoneServer, ownerRef, crafterID, schematicPath, ticksLeft, onComplete] () {
			CreatureObject* owner = ownerRef.get();
			ManagedReference<SceneObject*> crafterObj = zoneServer != nullptr ? zoneServer->getObject(crafterID) : nullptr;
			CompanionObject* crafter = crafterObj != nullptr ? crafterObj.castTo<CompanionObject*>().get() : nullptr;

			if (owner == nullptr || crafter == nullptr) {
				if (onComplete) {
					onComplete(false);
				}
				return;
			}

			// Companion System (2026-07-28 FIX, found during adversarial review
			// of the crafting-quality patch): this deferred task previously only
			// locked the crafter, never the owner, even though finishCraft()
			// below reads/writes the owner's inventory and sends the owner
			// messages via craftItem() -- the exact crash-lesson
			// recoverStragglers() already applies correctly a few functions
			// above in this same file ("Lock the owner (root) so the
			// per-companion cross-locks below are valid"). Lock owner first,
			// then cross-lock the crafter to it -- same two-line pattern.
			Locker ownerLocker(owner);
			Locker clocker(crafter, owner);

			// Combat/despawn -> craft immediately.
			if (crafter->getZone() == nullptr || crafter->isDead() || crafter->isInCombat()) {
				finishCraft(owner, crafter, schematicPath, onComplete);
				return;
			}

			if (ticksLeft <= 0) {
				finishCraft(owner, crafter, schematicPath, onComplete);
				return;
			}

			// 2026-07-20 (user: "the ghost blink isn't happening; make them
			// GLOW instead"): dropped the broadcastDestroy/broadcastObject
			// visibility blink (never rendered, risked desync -- it was also
			// implicated in the form-up lock issues). Now a steady GLOW:
			// re-pulse the force + heal-enhance effects and keep the
			// crafting animation alive each second, for ~10 seconds (per the
			// "each item takes 10 seconds" request).
			crafter->doAnimation("manipulate_low");
			crafter->playEffect("clienteffect/pl_force_meditate_self.cef", "");
			crafter->playEffect("clienteffect/healing_healenhance.cef", "");

			scheduleShimmer(zoneServer, ownerRef, crafterID, schematicPath, ticksLeft - 1, onComplete);
		}, "CompanionCraftShimmerLambda", CRAFT_TICK_MS);
	}

	static void finishCraft(CreatureObject* owner, CompanionObject* crafter, const String& schematicPath, std::function<void(bool)> onComplete = std::function<void(bool)>()) {
		// Craft is over one way or another past this point (success,
		// failure, combat interrupt, or despawn) -- release this
		// crafter's slot in the simultaneous-craft spacing tracker. Safe
		// no-op if it was never registered.
		unregisterActiveCrafter(owner, crafter);

		String errorMessage;
		bool success = CompanionCraftingManager::instance()->craftItem(owner, crafter, schematicPath, errorMessage);

		if (!success) {
			owner->sendSystemMessage("Your companion couldn't finish the craft: " + (errorMessage.isEmpty() ? String("unknown reason.") : errorMessage));
			say(crafter, "Hmm -- couldn't finish that one.");
		} else {
			say(crafter, "Done -- a fine piece of work, if I say so myself!");
		}

		resumeFollow(crafter, owner);

		// 2026-07-23: signal the real result back to the caller -- see the
		// file header for why this matters (the "craft whole suit" chain
		// must NOT continue past a failed piece).
		if (onComplete) {
			onComplete(success);
		}
	}

};

#endif // COMPANIONCRAFTTHEATER_H_
