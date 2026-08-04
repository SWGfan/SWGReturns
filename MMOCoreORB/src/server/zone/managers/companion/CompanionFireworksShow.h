/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion Fireworks Show (2026-07-19, per user request + uploaded design
	notes -- see NOTES.md). The companion puts on a REAL fireworks show using
	Core3's own firework system: it walks to several random spots around the
	owner and fires genuine FireworkObject items from its inventory --
	FireworkObject::launch() already handles the crouch, the manipulate
	animation, the launcher prop, the client particle effect, and the item's
	use count, and it accepts ANY CreatureObject as the launcher.

	Resources rule (user spec: "check for the resources needed or ask
	around"): the show consumes real firework items from the companion's
	bag. If it has none, the owner's OTHER companions are scanned -- a
	sibling carrying fireworks walks over, hands them across (high five),
	and the show starts. Nobody has any -> the companion says exactly what
	it needs.

	The uploaded notes' night-sky override is intentionally NOT attempted:
	the client derives time-of-day from the global server time sync -- no
	per-player override exists in the protocol (same client-authoritative
	wall family as NOTES 2026-07-16; flagged as a c3r research topic).

	Header-only (all methods in-class => implicitly inline; no new .cpp).
*/

#ifndef COMPANIONFIREWORKSSHOW_H_
#define COMPANIONFIREWORKSSHOW_H_

#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/objects/tangible/firework/FireworkObject.h"
#include "server/zone/objects/resource/ResourceContainer.h"
#include "server/zone/objects/resource/ResourceSpawn.h"
#include "server/zone/objects/creature/ai/PatrolPoint.h"
#include "server/zone/managers/companion/CompanionCraftingManager.h"
#include "templates/params/creature/CreaturePosture.h"
#include "server/chat/ChatManager.h"
#include "server/zone/Zone.h"
#include "server/zone/ZoneServer.h"

class CompanionFireworksState : public Object {
public:
	uint64 companionID = 0;
	uint64 donorID = 0;
	Vector<uint64> donorFireworkIDs;
	int phase = 0; // 0 = donor delivery walk, 1 = the show
	// 2026-07-20: after a resource-container delivery (not finished
	// fireworks), the companion CRAFTS the batch before the show instead
	// of jumping straight to phase 1.
	bool craftAfterDelivery = false;
	float targetX = 0;
	float targetY = 0;
	bool hasTarget = false;
	int launched = 0;
	int steps = 0;
};

class CompanionFireworksShow {
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

	static void collectFireworks(CompanionObject* companion, Vector<uint64>& out) {
		if (companion == nullptr) {
			return;
		}

		auto scan = [&out](SceneObject* container) {
			if (container == nullptr) {
				return;
			}

			for (int i = 0; i < container->getContainerObjectsSize(); ++i) {
				ManagedReference<SceneObject*> obj = container->getContainerObject(i);

				if (obj != nullptr && obj->isFireworkObject()) {
					out.add(obj->getObjectID());
				}
			}
		};

		scan(companion);
		scan(companion->getSlottedObject("inventory"));
	}

	// ---- Craft-your-own fallback (2026-07-20, live report: "companion has
	// crates of free resource deeds but still says it needs resources") ----
	// The show only ever LOOKED for finished firework items; deeds never
	// entered the picture. Now: no fireworks + no donor -> the companion
	// CRAFTS a batch from real resources (camp-recipe style), claiming
	// resource deeds through the (fixed) CompanionCraftingManager path for
	// any class it's short on. Recipe: 5 fireworks per batch, each 20
	// chemical + 10 metal.

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

	static void consumeResource(CompanionObject* companion, const String& classToken, int amount) {
		auto drain = [&](SceneObject* container) {
			if (container == nullptr || amount <= 0) {
				return;
			}

			for (int i = 0; i < container->getContainerObjectsSize() && amount > 0; ++i) {
				ManagedReference<SceneObject*> obj = container->getContainerObject(i);

				if (obj == nullptr || !obj->isResourceContainer()) {
					continue;
				}

				ResourceContainer* rc = cast<ResourceContainer*>(obj.get());

				if (rc == nullptr || rc->getQuantity() <= 0) {
					continue;
				}

				ManagedReference<ResourceSpawn*> spawn = rc->getSpawnObject();

				if (spawn == nullptr || !spawn->isType(classToken)) {
					continue;
				}

				int take = Math::min(amount, rc->getQuantity());

				Locker rlocker(rc, companion);
				// genesis port: dropped the 4th argument (destroyEmpty = true) -- genesis's
				// ResourceContainer::setQuantity(quantity, notifyClient, ignoreMax) has only 3
				// parameters. Nothing is lost: the newer base's destroyEmpty defaults to true and
				// this call passed true, and genesis unconditionally destroys the container when
				// stackQuantity drops below 1 -- identical behaviour.
				rc->setQuantity(rc->getQuantity() - take, true, false);
				amount -= take;
			}
		};

		drain(companion);
		drain(companion->getSlottedObject("inventory"));
	}

	static bool companionHasCraftingTool(CompanionObject* companion) {
		bool found = false;

		auto scan = [&found](SceneObject* container) {
			if (container == nullptr || found) {
				return;
			}

			for (int i = 0; i < container->getContainerObjectsSize(); ++i) {
				ManagedReference<SceneObject*> obj = container->getContainerObject(i);

				if (obj != nullptr && obj->isCraftingTool()) {
					found = true;
					return;
				}
			}
		};

		scan(companion);
		scan(companion->getSlottedObject("inventory"));
		return found;
	}

	/** Finds the owner's first OTHER summoned companion carrying resource
	 * containers of a class this companion is short on, returning that
	 * sibling's id + the matching container ids to hand over. */
	static void findResourceDonor(CreatureObject* owner, CompanionObject* companion, Zone* zone, uint64& donorIDOut, Vector<uint64>& containerIDsOut) {
		static const char* CLASSES[2] = { "chemical", "metal" };
		const int needed[2] = { 100, 50 }; // BATCH totals (5 * 20 / 5 * 10)

		bool shortAny = false;

		for (int c = 0; c < 2; ++c) {
			if (countResource(companion, CLASSES[c]) < needed[c]) {
				shortAny = true;
			}
		}

		if (!shortAny) {
			return;
		}

		ManagedReference<SceneObject*> datapad = owner->getSlottedObject("datapad");

		if (datapad == nullptr) {
			return;
		}

		for (int i = 0; i < datapad->getContainerObjectsSize() && donorIDOut == 0; ++i) {
			ManagedReference<SceneObject*> obj = datapad->getContainerObject(i);

			if (obj == nullptr || !obj->isCompanionControlDevice()) {
				continue;
			}

			CompanionControlDevice* device = cast<CompanionControlDevice*>(obj.get());

			if (device == nullptr || device->isCompanionDead()) {
				continue;
			}

			CompanionObject* sibling = device->getCompanionObject();

			if (sibling == nullptr || sibling == companion || sibling->getZone() != zone
					|| sibling->getLinkedCreature().get() != owner) {
				continue;
			}

			Vector<uint64> ids;

			auto collect = [&ids](SceneObject* container) {
				if (container == nullptr) {
					return;
				}

				for (int j = 0; j < container->getContainerObjectsSize(); ++j) {
					ManagedReference<SceneObject*> item = container->getContainerObject(j);

					if (item == nullptr || !item->isResourceContainer()) {
						continue;
					}

					ResourceContainer* rc = cast<ResourceContainer*>(item.get());

					if (rc == nullptr || rc->getQuantity() <= 0) {
						continue;
					}

					ManagedReference<ResourceSpawn*> spawn = rc->getSpawnObject();

					if (spawn != nullptr && (spawn->isType("chemical") || spawn->isType("metal"))) {
						ids.add(item->getObjectID());
					}
				}
			};

			collect(sibling);
			collect(sibling->getSlottedObject("inventory"));

			if (ids.size() > 0) {
				donorIDOut = sibling->getObjectID();
				containerIDsOut = ids;
			}
		}
	}

	/** @returns true if a batch of fireworks was crafted into the bag. */
	static bool tryCraftFireworks(CreatureObject* owner, CompanionObject* companion, ZoneServer* zoneServer) {
		constexpr int BATCH = 5;
		constexpr int CHEMICAL_PER = 20;
		constexpr int METAL_PER = 10;

		static const char* CLASSES[2] = { "chemical", "metal" };
		const int needed[2] = { CHEMICAL_PER * BATCH, METAL_PER * BATCH };

		// Need a crafting tool aboard.
		bool hasTool = false;

		auto toolScan = [&hasTool](SceneObject* container) {
			if (container == nullptr || hasTool) {
				return;
			}

			for (int i = 0; i < container->getContainerObjectsSize(); ++i) {
				ManagedReference<SceneObject*> obj = container->getContainerObject(i);

				if (obj != nullptr && obj->isCraftingTool()) {
					hasTool = true;
					return;
				}
			}
		};

		toolScan(companion);
		toolScan(companion->getSlottedObject("inventory"));

		if (!hasTool) {
			say(companion, "No fireworks and no crafting tool to make any -- hand me a tool!");
			return false;
		}

		// Top up shortfalls via resource deeds (the fixed claim path picks
		// the server's best in-spawn resource of each class).
		String missing;

		for (int c = 0; c < 2; ++c) {
			if (countResource(companion, CLASSES[c]) < needed[c]) {
				CompanionCraftingManager::instance()->claimResourceDeedForClass(owner, companion, CLASSES[c]);
			}

			int have = countResource(companion, CLASSES[c]);

			if (have < needed[c]) {
				if (!missing.isEmpty()) {
					missing += ", ";
				}

				missing += String::valueOf(needed[c] - have) + " more " + CLASSES[c];
			}
		}

		if (!missing.isEmpty()) {
			say(companion, "I can't mix fireworks yet -- I still need " + missing + " (a crate of resource deeds works too!).");
			return false;
		}

		for (int c = 0; c < 2; ++c) {
			consumeResource(companion, CLASSES[c], needed[c]);
		}

		// The batch -- real firework items into the bag.
		static const char* FIREWORK_TEMPLATES[5] = {
			"object/tangible/firework/firework_s01.iff",
			"object/tangible/firework/firework_s02.iff",
			"object/tangible/firework/firework_s03.iff",
			"object/tangible/firework/firework_s04.iff",
			"object/tangible/firework/firework_s05.iff"
		};

		SceneObject* bag = companion->getSlottedObject("inventory");
		SceneObject* destination = bag != nullptr ? bag : static_cast<SceneObject*>(companion);
		int made = 0;

		for (int i = 0; i < BATCH; ++i) {
			ManagedReference<SceneObject*> firework = zoneServer->createObject(String(FIREWORK_TEMPLATES[i % 5]).hashCode(), 1);

			if (firework == nullptr) {
				continue;
			}

			Locker flocker(firework, companion);

			if (destination->transferObject(firework, -1, true)) {
				++made;
			} else {
				firework->destroyObjectFromDatabase(true);
			}
		}

		if (made == 0) {
			say(companion, "Couldn't stow the fireworks -- never mind.");
			return false;
		}

		say(companion, "Mixed up " + String::valueOf(made) + " fresh fireworks -- now THAT'S chemistry!");
		return true;
	}

	static void resumeFollow(CompanionObject* companion, CreatureObject* owner) {
		if (companion == nullptr || companion->isDead() || companion->getZone() == nullptr) {
			return;
		}

		companion->setPosture(CreaturePosture::UPRIGHT);
		companion->setCompanionState(CompanionObject::FOLLOW);
		companion->setFollowObject(owner);
		companion->setFollowState(AiAgent::FOLLOWING); // genesis port: was setMovementState()
		companion->clearPatrolPoints();
	}

	/** Dialog entry point ("Fun: Fireworks Show"). @pre { companion crosslocked, player locked } */
	static void start(CreatureObject* owner, CompanionObject* companion) {
		if (owner == nullptr || companion == nullptr) {
			return;
		}

		Zone* zone = companion->getZone();
		ZoneServer* zoneServer = companion->getZoneServer();

		if (zone == nullptr || zoneServer == nullptr || owner->getZone() != zone) {
			owner->sendSystemMessage("Your companion must be summoned and on this planet for a show.");
			return;
		}

		if (owner->isInCombat() || companion->isInCombat()) {
			owner->sendSystemMessage("Not while there's a fight going on!");
			return;
		}

		if (!companion->checkCooldownRecovery("companion_fireworks")) {
			owner->sendSystemMessage("Your companion is already busy with a show.");
			return;
		}

		Vector<uint64> ownFireworks;
		collectFireworks(companion, ownFireworks);

		Reference<CompanionFireworksState*> state = new CompanionFireworksState();
		state->companionID = companion->getObjectID();

		if (ownFireworks.size() == 0) {
			// Ask around: another companion carrying fireworks delivers them.
			ManagedReference<SceneObject*> datapad = owner->getSlottedObject("datapad");
			CompanionObject* donor = nullptr;
			Vector<uint64> donorFireworks;

			if (datapad != nullptr) {
				for (int i = 0; i < datapad->getContainerObjectsSize() && donor == nullptr; ++i) {
					ManagedReference<SceneObject*> obj = datapad->getContainerObject(i);

					if (obj == nullptr || !obj->isCompanionControlDevice()) {
						continue;
					}

					CompanionControlDevice* device = cast<CompanionControlDevice*>(obj.get());

					if (device == nullptr || device->isCompanionDead()) {
						continue;
					}

					CompanionObject* sibling = device->getCompanionObject();

					if (sibling == nullptr || sibling == companion || sibling->getZone() != zone
							|| sibling->getLinkedCreature().get() != owner) {
						continue;
					}

					Vector<uint64> siblingFireworks;
					collectFireworks(sibling, siblingFireworks);

					if (siblingFireworks.size() > 0) {
						donor = sibling;
						donorFireworks = siblingFireworks;
					}
				}
			}

			if (donor == nullptr) {
				// No finished fireworks anywhere -> CRAFT them. First the
				// theater: if THIS companion is short on materials and a
				// sibling is carrying the resources, that sibling walks over
				// and hands them across (high five), THEN this one crafts.
				// (Only meaningful if this companion actually has a tool.)
				bool hasTool = companionHasCraftingTool(companion);

				uint64 resourceDonorID = 0;
				Vector<uint64> resourceContainerIDs;

				if (hasTool) {
					findResourceDonor(owner, companion, zone, resourceDonorID, resourceContainerIDs);
				}

				if (resourceDonorID != 0 && resourceContainerIDs.size() > 0) {
					ManagedReference<SceneObject*> rdObj = zoneServer->getObject(resourceDonorID);
					CompanionObject* resourceDonor = rdObj != nullptr ? rdObj.castTo<CompanionObject*>().get() : nullptr;

					say(companion, "I've got the tool -- who's carrying materials for fireworks?");

					if (resourceDonor != nullptr) {
						say(resourceDonor, "I've got what you need -- bringing it over!");
					}

					state->phase = 0;
					state->donorID = resourceDonorID;
					state->donorFireworkIDs = resourceContainerIDs; // resource containers reuse the same delivery slot
					state->craftAfterDelivery = true;

					companion->updateCooldownTimer("companion_fireworks", 60000);

					Reference<CreatureObject*> ownerRef = owner;
					scheduleStep(zoneServer, ownerRef, state, 400);
					return;
				}

				// No sibling materials -> craft from own stock + resource deeds.
				if (tryCraftFireworks(owner, companion, zoneServer)) {
					say(companion, "Ladies and gentlebeings -- find a good seat, the show's about to start!");
					state->phase = 1;
					companion->updateCooldownTimer("companion_fireworks", 60000);

					Reference<CreatureObject*> ownerRef = owner;
					scheduleStep(zoneServer, ownerRef, state, 400);
					return;
				}

				owner->sendSystemMessage("Your companion needs firework items (or a crafting tool + resources/resource deeds) for a show.");
				return;
			}

			say(companion, "Anyone got fireworks for the show?");
			say(donor, "I've got a whole pocketful -- on my way!");

			state->phase = 0;
			state->donorID = donor->getObjectID();
			state->donorFireworkIDs = donorFireworks;
		} else {
			say(companion, "Ladies and gentlebeings -- find a good seat, the show's about to start!");
			state->phase = 1;
		}

		companion->updateCooldownTimer("companion_fireworks", 60000);

		Reference<CreatureObject*> ownerRef = owner;
		scheduleStep(zoneServer, ownerRef, state, 400);
	}

	static void scheduleStep(ZoneServer* zoneServer, Reference<CreatureObject*> ownerRef, Reference<CompanionFireworksState*> state, int delayMs) {
		Core::getTaskManager()->scheduleTask([zoneServer, ownerRef, state] () {
			runStep(zoneServer, ownerRef, state);
		}, "CompanionFireworksStepLambda", delayMs);
	}

	static void runStep(ZoneServer* zoneServer, Reference<CreatureObject*> ownerRef, Reference<CompanionFireworksState*> state) {
		CreatureObject* owner = ownerRef.get();

		if (owner == nullptr || state == nullptr || zoneServer == nullptr) {
			return;
		}

		ManagedReference<SceneObject*> companionObj = zoneServer->getObject(state->companionID);
		CompanionObject* companion = companionObj != nullptr ? companionObj.castTo<CompanionObject*>().get() : nullptr;

		if (companion == nullptr) {
			return;
		}

		Locker clocker(companion);

		Zone* zone = companion->getZone();

		if (zone == nullptr || companion->isDead() || companion->isInCombat() || owner->isInCombat()) {
			resumeFollow(companion, owner);
			return;
		}

		if (++state->steps > 225) { // 90-second cap
			resumeFollow(companion, owner);
			return;
		}

		// ---- Phase 0: the donor walks its fireworks over ----
		if (state->phase == 0) {
			ManagedReference<SceneObject*> donorObj = zoneServer->getObject(state->donorID);
			CompanionObject* donor = donorObj != nullptr ? donorObj.castTo<CompanionObject*>().get() : nullptr;

			if (donor == nullptr || donor->getZone() != zone || donor->isDead() || donor->isInCombat()) {
				say(companion, "Guess the fireworks aren't coming -- show's off.");
				resumeFollow(companion, owner);
				return;
			}

			Locker dlocker(donor, companion);

			if (donor->getDistanceTo(companion) > 5.f) {
				donor->setCompanionState(CompanionObject::PATROL);
				donor->setFollowObject(nullptr);

				if (donor->getPatrolPointSize() == 0) {
					PatrolPoint point(companion->getPositionX(), companion->getPositionZ(), companion->getPositionY());
					// genesis port: setMovementState() -> setFollowState(); genesis
					// setFollowState() calls clearPatrolPoints(), so the state must be
					// set BEFORE the point is queued (see PetPatrolCommand.h).
					donor->setFollowState(AiAgent::PATROLLING);
					donor->addPatrolPoint(point);
				}

				scheduleStep(zoneServer, ownerRef, state, 400);
				return;
			}

			// Hand-off + high five.
			SceneObject* bag = companion->getSlottedObject("inventory");
			SceneObject* destination = bag != nullptr ? bag : static_cast<SceneObject*>(companion);
			int handed = 0;

			for (int i = 0; i < state->donorFireworkIDs.size(); ++i) {
				ManagedReference<SceneObject*> item = zoneServer->getObject(state->donorFireworkIDs.get(i));

				if (item == nullptr || item->getRootParent() != donor) {
					continue;
				}

				Locker itemLocker(item, companion);

				if (destination->transferObject(item, -1, true)) {
					++handed;
				}
			}

			donor->faceObject(companion, true);
			companion->faceObject(donor, true);

			if (handed > 0) {
				say(donor, state->craftAfterDelivery ? "Here -- these should do it!" : "Here -- light 'em up!");
				// 2026-07-20 (user request): giver bows, receiver kowtows.
				donor->doAnimation("bow");
				companion->doAnimation("kowtow");
			}

			resumeFollow(donor, owner);

			if (handed == 0) {
				say(companion, state->craftAfterDelivery ? "The materials are gone -- show's off." : "Huh, the fireworks are gone. Show's off.");
				resumeFollow(companion, owner);
				return;
			}

			// Resource delivery -> now CRAFT the fireworks (still topping up
			// with resource deeds if the sibling's share didn't cover it).
			if (state->craftAfterDelivery) {
				say(companion, "Perfect -- let me mix these up!");

				if (!tryCraftFireworks(owner, companion, zoneServer)) {
					resumeFollow(companion, owner);
					return;
				}
			}

			say(companion, "Ladies and gentlebeings -- find a good seat, the show's about to start!");
			state->phase = 1;
			scheduleStep(zoneServer, ownerRef, state, 400);
			return;
		}

		// ---- Phase 1: the show ----
		if (!state->hasTarget) {
			Vector<uint64> remaining;
			collectFireworks(companion, remaining);

			if (state->launched >= 5 || remaining.size() == 0) {
				// Finale.
				say(companion, state->launched > 0 ? "That's the show! Thank you, thank you!" : "No fireworks left -- next time!");
				companion->doAnimation("bow");
				resumeFollow(companion, owner);

				// 2026-07-20 fix ("companion stays kneeling after the
				// show"): FireworkObject::launch() applies its CROUCHED
				// posture on the firework's own DELAYED launch event, so
				// the final firework re-kneels the companion AFTER the
				// resumeFollow above already stood it up -- and a crouched
				// creature won't walk. Re-assert stand+follow once more
				// after every launch delay has certainly fired.
				ManagedReference<CompanionObject*> companionRef = companion;
				ManagedReference<CreatureObject*> standOwnerRef = owner;

				Core::getTaskManager()->scheduleTask([companionRef, standOwnerRef] () {
					CompanionObject* comp = companionRef.get();
					CreatureObject* standOwner = standOwnerRef.get();

					if (comp == nullptr || standOwner == nullptr) {
						return;
					}

					Locker locker(comp);
					resumeFollow(comp, standOwner);
				}, "CompanionFireworksStandUpLambda", 6000);

				return;
			}

			// Pick the next launch spot: 8-15m from the OWNER, random angle.
			Vector3 ownerWorld = owner->getWorldPosition();
			float angle = (6.2832f * System::random(628)) / 628.f;
			float distance = 8.f + System::random(7);

			state->targetX = ownerWorld.getX() + cos(angle) * distance;
			state->targetY = ownerWorld.getY() + sin(angle) * distance;
			state->hasTarget = true;

			companion->setCompanionState(CompanionObject::PATROL);
			companion->setFollowObject(nullptr);
			companion->clearPatrolPoints();

			PatrolPoint point(state->targetX, zone->getHeight(state->targetX, state->targetY), state->targetY);
			// genesis port: setMovementState() -> setFollowState(); genesis
			// setFollowState() calls clearPatrolPoints(), so the state must be
			// set BEFORE the point is queued (see PetPatrolCommand.h).
			companion->setFollowState(AiAgent::PATROLLING);
			companion->addPatrolPoint(point);

			scheduleStep(zoneServer, ownerRef, state, 400);
			return;
		}

		// Stand back up between spots -- each launch's delayed event
		// re-crouches the companion, and a crouched creature won't walk.
		if (companion->getPosture() == CreaturePosture::CROUCHED) {
			companion->setPosture(CreaturePosture::UPRIGHT);
		}

		float dx = companion->getPositionX() - state->targetX;
		float dy = companion->getPositionY() - state->targetY;

		if ((dx * dx + dy * dy) > 9.f) { // not there yet (3m)
			if (companion->getPatrolPointSize() == 0) {
				PatrolPoint point(state->targetX, zone->getHeight(state->targetX, state->targetY), state->targetY);
				// genesis port: setMovementState() -> setFollowState(); genesis
				// setFollowState() calls clearPatrolPoints(), so the state must be
				// set BEFORE the point is queued (see PetPatrolCommand.h).
				companion->setFollowState(AiAgent::PATROLLING);
				companion->addPatrolPoint(point);
			}

			scheduleStep(zoneServer, ownerRef, state, 400);
			return;
		}

		// At the spot: rig and fire a REAL firework (launch() does the
		// crouch, animation, prop, effect, and use-count bookkeeping).
		Vector<uint64> remaining;
		collectFireworks(companion, remaining);

		if (remaining.size() > 0) {
			ManagedReference<SceneObject*> fireworkObj = zoneServer->getObject(remaining.get(0));
			FireworkObject* firework = fireworkObj != nullptr ? fireworkObj.castTo<FireworkObject*>().get() : nullptr;

			if (firework != nullptr) {
				Locker flocker(firework, companion);
				firework->launch(companion, 10);
				++state->launched;

				// 2026-07-20 (user request): don't kneel forever -- 0.5s after
				// lighting the fuse the companion SCREAMS and bolts ~10m away in
				// fear, then carries on to the next spot.
				ManagedReference<CompanionObject*> companionRef2 = companion;
				ManagedReference<CreatureObject*> ownerRef2 = owner;
				float fx = companion->getPositionX();
				float fy = companion->getPositionY();

				Core::getTaskManager()->scheduleTask([companionRef2, ownerRef2, fx, fy] () {
					CompanionObject* comp = companionRef2.get();
					CreatureObject* o = ownerRef2.get();

					if (comp == nullptr || o == nullptr || comp->getZone() == nullptr) {
						return;
					}

					Locker locker(comp);

					comp->setPosture(CreaturePosture::UPRIGHT, true);
					comp->doAnimation("scared");
					comp->playEffect("clienteffect/holoemote_shocked.cef", "head");

					Zone* fzone = comp->getZone();
					float dx = comp->getPositionX() - fx;
					float dy = comp->getPositionY() - fy;
					float len = Math::sqrt(dx * dx + dy * dy);

					if (len < 0.5f) { dx = 1.f; dy = 0.f; len = 1.f; }

					float rx = comp->getPositionX() + (dx / len) * 10.f;
					float ry = comp->getPositionY() + (dy / len) * 10.f;

					comp->setCompanionState(CompanionObject::PATROL);
					comp->setFollowObject(nullptr);
					comp->clearPatrolPoints();
					PatrolPoint fleePoint(rx, fzone->getHeight(rx, ry), ry);
					// genesis port: setMovementState() -> setFollowState(); genesis
					// setFollowState() calls clearPatrolPoints(), so the state must be
					// set BEFORE the point is queued (see PetPatrolCommand.h).
					comp->setFollowState(AiAgent::PATROLLING);
					comp->addPatrolPoint(fleePoint);
				}, "CompanionFireworkFleeLambda", 500);
			}
		}

		state->hasTarget = false;
		scheduleStep(zoneServer, ownerRef, state, 2500); // savor each burst
	}

};

#endif // COMPANIONFIREWORKSSHOW_H_
