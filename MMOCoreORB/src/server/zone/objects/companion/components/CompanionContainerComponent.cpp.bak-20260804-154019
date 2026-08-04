/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System -- see CompanionContainerComponent.h and NOTES.md.

	checkContainerPermission(SceneObject*, CreatureObject*, uint16) is the one
	and only actor-aware hook ContainerComponent exposes (transferObject() /
	removeObject() are not passed an acting CreatureObject at all -- they are
	purely mechanical). SceneObjectImplementation::checkContainerPermission
	(server/zone/objects/scene/SceneObject.idl) delegates straight into this
	method before any client-issued MOVEIN/MOVEOUT/OPEN request is allowed to
	proceed, so denying access here is equivalent to "drop the packet": the
	transfer never happens and nothing is mutated.

	2026-07-12 -- Auto-Equip. See docs/companion_system/NOTES.md, "Companion
	Auto-Equip", for the full writeup. Summary:

	  * canAddObject() adds one special case on top of the inherited,
	    unmodified PlayerContainerComponent::canAddObject(): containmentType
	    == -1 (a loose item landing directly in the companion's own
	    inventory, not an equip-slot transfer) is allowed through even though
	    the companion's underlying SceneObject template containerType is
	    inherited SLOTTED (1) from the shared NPC "dressed" appearance
	    template it reuses for its look (object/mobile/companion_actor.lua).
	    A real player never hits this because misc loot always targets their
	    separate "inventory" bag child object (a VOLUME container), never
	    the player CreatureObject itself. (2026-07-13 update: the companion
	    now gets a real separate "inventory" bag child object too -- see the
	    note below this list -- but a loose item still lands here, directly
	    on the companion, first.)
	  * notifyObjectInserted() branches on the just-inserted object's
	    containmentType (already set by ContainerComponent::transferObject()
	    before this callback fires): >=4 (a real equip-slot insertion) reuses
	    the inherited PlayerContainerComponent::notifyObjectInserted() as-is
	    (skill mods, armor encumbrance, wearables-vector tracking, jedi
	    visibility -- everything a real player equip gets); anything else
	    (a loose item landing in inventory) skips all of that and instead
	    attempts to auto-equip the item via the same validated
	    canAddObject()-gated slot transfer a player's "Wear" radial/command
	    uses (see attemptAutoEquip() below), reusing
	    ObjectController::transferObject() -- the identical call
	    TransferItemArmorCommand / TransferItemWeaponCommand make.

	  2026-07-13 update -- real "inventory" bag child object. The companion
	  now gets a real separate VOLUME-type "inventory" bag child object at
	  spawn time (CompanionControlDeviceImplementation.cpp::spawnObject(),
	  object/tangible/inventory/creature_inventory.iff -- the same template
	  a real loot-bearing NPC uses), fixing an item-loss bug where taking a
	  loose item back out of the companion's own inherited-SLOTTED top-level
	  container could silently fail client-side (see NOTES.md, "item
	  vanishes when taken back out of companion inventory"). Loose items
	  still land on the companion directly first (canAddObject()'s -1
	  special case above is unchanged, and still needed as the landing
	  spot), but attemptAutoEquip() below now relocates anything that didn't
	  get equipped into the real bag instead of leaving it sitting directly
	  on the companion. Equipped gear (containmentType >= 4) is unaffected
	  and still lives directly on the companion, exactly like a real
	  player's own worn gear. Any code that used to assume a companion's own
	  containerObjects list *is* its entire loose inventory (e.g.
	  CampDeploymentManager::deployCamp()'s camp-tent scan) needs to also
	  check the "inventory" bag now -- see that file's own updated comment.
*/

#include "CompanionContainerComponent.h"
#include "server/zone/objects/scene/SceneObject.h"
#include "server/zone/objects/scene/TransferErrorCode.h"
#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/objects/tangible/TangibleObject.h"
#include "server/zone/objects/tangible/weapon/WeaponObject.h"
#include "server/zone/managers/objectcontroller/ObjectController.h"
#include "server/zone/ZoneServer.h"

namespace {

	// Walks up the container hierarchy to find the CompanionObject that owns
	// this container slot set (works whether sceneObject IS the companion, or
	// is a nested slot/appearance container hanging off it).
	CompanionObject* resolveCompanion(SceneObject* sceneObject) {
		SceneObject* current = sceneObject;

		for (int i = 0; i < 8 && current != nullptr; ++i) {
			if (current->isCompanionObject()) {
				return cast<CompanionObject*>(current);
			}

			current = current->getParent().get();
		}

		return nullptr;
	}

	// Companion System -- Auto-Equip. Reuses the exact slot-resolution +
	// transfer call TransferItemArmorCommand::doQueueCommand() /
	// TransferItemWeaponCommand::doQueueCommand() use for a real player
	// "Wear" action (server/zone/objects/creature/commands/
	// TransferItem{Armor,Weapon}Command.h) -- canAddObject() for validation,
	// ObjectController::transferObject() for the actual move. Deliberately
	// does NOT replicate those commands' "swap the currently-equipped item
	// back to inventory and equip this one instead" behavior: this runs as a
	// passive reaction to an item merely landing in inventory (not an
	// explicit player "Wear" click), and forcibly un-equipping already-worn
	// gear as a side effect of an unrelated inventory drop would be a
	// surprising, unwanted mutation -- and would also self-trigger through
	// this very same notifyObjectInserted() hook (the swapped-out item would
	// immediately try to auto-equip itself right back into the slot it was
	// just removed from). If every matching slot is already occupied, the
	// new item is simply left sitting in the companion's inventory.
	// Companion System (2026-07-13, "item vanishes when taken back out of
	// companion inventory" fix -- see NOTES.md). Returns true if the item
	// actually got equipped onto the companion (containmentType >= 4).
	// Split out of attemptAutoEquip() below so both the equip attempt and
	// the "didn't equip, so relocate it into the real inventory bag"
	// fallback can share one deferred, locked task instead of needing two.
	bool tryEquipOntoCompanion(CompanionObject* companion, TangibleObject* item, ObjectController* objectController) {
		// transferType 4 is the base arrangement-group offset every real
		// wearable/weapon "Wear" transfer starts from -- see the identical
		// constant in TransferItemArmorCommand.h / TransferItemWeaponCommand.h.
		int transferType = 4;
		String errorDescription;

		int precheck = companion->canAddObject(item, transferType, errorDescription);

		if (precheck == TransferErrorCode::SLOTOCCUPIED) {
			int arrangementSize = item->getArrangementDescriptorSize();
			int arrangementGroupToUse = -1;

			// Probe every arrangement group this item could go into (mirrors
			// TransferItemArmorCommand's multi-arrangement probing, e.g. an
			// item that can go in either of two equivalent slots) for one
			// that's entirely free.
			for (int i = 0; i < arrangementSize && arrangementGroupToUse == -1; ++i) {
				const Vector<String>* descriptors = item->getArrangementDescriptor(i);
				bool allFree = true;

				for (int j = 0; j < descriptors->size(); ++j) {
					if (companion->getSlottedObject(descriptors->get(j)) != nullptr) {
						allFree = false;
						break;
					}
				}

				if (allFree) {
					arrangementGroupToUse = i;
				}
			}

			if (arrangementGroupToUse == -1) {
				// Companion System (2026-07-15, "always swap out the occupied
				// slot" per user request -- supersedes the original
				// never-force-swap policy in the file header comment): every
				// matching slot is occupied, so displace whatever occupies
				// this item's FIRST arrangement group into the OWNER's own
				// inventory and equip into the freed slots -- the same policy
				// the loadout backpack and the "Equip on Companion" radial
				// now share. Same destroy-first / silent-transfer /
				// deferred-re-create client resync as unequipItemToInventory().
				CreatureObject* owner = companion->getLinkedCreature().get();

				if (owner == nullptr) {
					return false;
				}

				// Displaced gear goes into THIS companion's own storage bag
				// (2026-07-15, per user request), falling back to the owner's
				// inventory if the bag is missing/full.
				SceneObject* ownerInventory = companion->getSlottedObject("inventory");

				if (ownerInventory == nullptr || ownerInventory->isContainerFull()) {
					ownerInventory = owner->getSlottedObject("inventory");
				}

				if (ownerInventory == nullptr || ownerInventory->isContainerFull()) {
					// No room to displace -- fall back to the old behavior
					// (item goes to the storage bag) rather than orphan gear.
					return false;
				}

				const Vector<String>* descriptors = item->getArrangementDescriptor(0);

				SortedVector<ManagedReference<SceneObject*> > occupants;
				occupants.setNoDuplicateInsertPlan();

				for (int j = 0; j < descriptors->size(); ++j) {
					SceneObject* slotted = companion->getSlottedObject(descriptors->get(j));

					if (slotted != nullptr) {
						occupants.put(slotted);
					}
				}

				Locker ownerInventoryLocker(ownerInventory, companion);

				for (int j = 0; j < occupants.size(); ++j) {
					SceneObject* occupant = occupants.get(j);

					Locker occupantLocker(occupant, companion);

					bool wasCurrentWeapon = occupant->isWeaponObject() && occupant == companion->getWeapon().get();

					occupant->broadcastDestroy(occupant, true);

					if (!objectController->transferObject(occupant, ownerInventory, -1, false)) {
						companion->broadcastObject(occupant, true);
						return false;
					}

					if (wasCurrentWeapon) {
						companion->setWeapon(nullptr, true);
						companion->refreshCombatAttacks(nullptr);
					}

					Reference<SceneObject*> occupantRef = occupant;
					Reference<CreatureObject*> ownerRef = owner;

					Core::getTaskManager()->scheduleTask([occupantRef, ownerRef] () {
						SceneObject* displaced = occupantRef.get();
						CreatureObject* displacedOwner = ownerRef.get();

						if (displaced == nullptr || displacedOwner == nullptr) {
							return;
						}

						Locker locker(displaced);

						displaced->sendTo(displacedOwner, true);
					}, "CompanionAutoEquipDisplaceResendLambda", 400);
				}

				arrangementGroupToUse = 0;
			}

			transferType += arrangementGroupToUse;
		} else if (precheck != 0) {
			// Not equippable onto this companion right now (insufficient
			// wearable skill certification, armor encumbrance, jedi-only
			// weapon restriction, etc. -- all reused, unmodified, from
			// PlayerContainerComponent::canAddObject() via
			// CompanionContainerComponent::canAddObject() below). This is a
			// passive auto-action, not a player-initiated command, so it
			// fails quietly: the item just gets relocated into the
			// inventory bag by the caller, no error message spam to the
			// owner.
			return false;
		}

		if (!objectController->transferObject(item, companion, transferType, true)) {
			return false;
		}

		if (item->isWeaponObject()) {
			WeaponObject* weapon = cast<WeaponObject*>(item);

			if (weapon != nullptr) {
				companion->setWeapon(weapon, true);

				// Bug fix (2026-07-13, "companion doesn't fire its
				// equipped weapon" -- see NOTES.md): setWeapon() above
				// only updates the base CreatureObject `weapon` field --
				// never AiAgent's own attack-selection state
				// (primaryAttackMap/currentWeapon), which companions can
				// never populate the normal way (setupAttackMaps()
				// requires a real npcTemplate, which companions never
				// have). refreshCombatAttacks() builds a real attack map
				// for this weapon directly and wires up
				// currentWeapon/primaryWeapon so the AI can actually use
				// it in combat.
				companion->refreshCombatAttacks(weapon);
			}
		}

		return true;
	}

	// Companion System (2026-07-13, "item vanishes when taken back out of
	// companion inventory" fix -- see NOTES.md). Moves a loose item that
	// didn't get auto-equipped off the companion's own inherited-SLOTTED
	// top-level container and into its real "inventory" child bag (created
	// in CompanionControlDeviceImplementation::spawnObject()) -- the same
	// kind of separate VOLUME-type bag object a real player keeps loose
	// items in, instead of forcing them to live directly alongside the
	// companion's equipped gear. If the bag doesn't exist (a companion
	// summoned before this fix shipped, or bag creation failed at spawn),
	// falls back to leaving the item exactly where it already is -- no
	// worse than this feature's prior behavior, and self-healing the next
	// time the companion is re-summoned.
	void relocateLooseItemToInventoryBag(CompanionObject* companion, TangibleObject* item, ObjectController* objectController) {
		SceneObject* bag = companion->getSlottedObject("inventory");

		if (bag == nullptr) {
			return;
		}

		if (item->getParent().get() != companion) {
			return;
		}

		Locker bagLocker(bag, companion);

		objectController->transferObject(item, bag, -1, true);
	}

	void attemptAutoEquip(SceneObject* sceneObject, SceneObject* object) {
		CompanionObject* companion = resolveCompanion(sceneObject);

		if (companion == nullptr) {
			return;
		}

		// Only while summoned & alive -- a stored/despawned companion has no
		// live AI/combat state to react to a "new weapon" with, and
		// getZone() == nullptr during store/teardown transfers (which also
		// flow through this same transferObject()/notifyObjectInserted()
		// path) must not trigger equip logic mid-teardown.
		if (companion->getZone() == nullptr || companion->isDead()) {
			return;
		}

		if (object == nullptr || !object->isTangibleObject()) {
			return;
		}

		TangibleObject* item = object->asTangibleObject();

		if (item == nullptr) {
			return;
		}

		// Locking fix (2026-07-12): the actual equip work is deferred to a
		// separate task instead of running inline. This hook fires from
		// ContainerComponent::transferObject() on whatever thread performed
		// the original MOVEIN -- for a player dragging loot onto the
		// companion, that's the player's queue-command thread, which holds
		// the *player's* object lock and NOT the companion's. The nested
		// equip-slot transfer below ends in
		// PlayerContainerComponent::notifyObjectInserted() ->
		// tano->addTemplateSkillMods(companion), whose autogen proxy hard-
		// asserts `targetObject->isLockedByCurrentThread()` -- so running it
		// inline crashed the server (SIGABRT, TangibleObject.cpp autogen
		// assert; see NOTES.md "Companion Auto-Equip locking crash").
		// Deferring to a task that takes the companion's lock first (with
		// the item cross-locked to it) is this codebase's own proven fix
		// pattern for exactly this class of bug -- see the identical
		// treatment in CompanionControlDeviceImplementation.cpp
		// (spawnObject/storeObject lambda).
		Reference<CompanionObject*> companionRef = companion;
		Reference<TangibleObject*> itemRef = item;

		Core::getTaskManager()->executeTask([companionRef, itemRef] () {
			Locker locker(companionRef);
			Locker itemLocker(itemRef, companionRef);

			CompanionObject* companion = companionRef.get();
			TangibleObject* item = itemRef.get();

			// Re-validate under lock -- state may have changed between the
			// insert notification and this task running (companion stored,
			// item moved/equipped/deleted, etc.).
			if (companion->getZone() == nullptr || companion->isDead()) {
				return;
			}

			if (item->getParent().get() != companion || item->getContainmentType() != -1) {
				return;
			}

			ZoneServer* zoneServer = companion->getZoneServer();

			if (zoneServer == nullptr) {
				return;
			}

			ObjectController* objectController = zoneServer->getObjectController();

			if (objectController == nullptr) {
				return;
			}

			// Only weapons, armor, and clothing -- exactly the set of
			// "equippable" types TransferItemArmorCommand (isWearableObject(),
			// which covers both ArmorObject and plain clothing) and
			// TransferItemWeaponCommand (isWeaponObject()) act on -- are
			// even attempted for auto-equip. Everything else (resources,
			// food, schematics, generic tangibles, camp tents, etc.) skips
			// straight to relocating into the inventory bag below.
			bool eligibleForEquip = (item->isWeaponObject() || item->isWearableObject())
					&& item->getArrangementDescriptorSize() > 0;

			bool equipped = eligibleForEquip && tryEquipOntoCompanion(companion, item, objectController);

			if (!equipped) {
				relocateLooseItemToInventoryBag(companion, item, objectController);
			}
		}, "CompanionAutoEquipLambda");
	}

}

bool CompanionContainerComponent::checkContainerPermission(SceneObject* sceneObject, CreatureObject* creature, uint16 permission) const {
	CompanionObject* companion = resolveCompanion(sceneObject);

	if (companion == nullptr || creature == nullptr) {
		return false;
	}

	if (!companion->isAuthorizedActor(creature)) {
		// Spec 2B: "If the actor performing the transaction is not the owner,
		// immediately drop the network packet and log an exploitation attempt."
		Logger::console.warning("CompanionSystem: rejected container access attempt on companion "
				+ String::valueOf(companion->getObjectID()) + " by non-owner actor "
				+ String::valueOf(creature->getObjectID()) + " (permission=" + String::valueOf(permission) + ")");

		return false;
	}

	return permission == ContainerPermissions::MOVEIN || permission == ContainerPermissions::MOVEOUT || permission == ContainerPermissions::OPEN;
}

int CompanionContainerComponent::canAddObject(SceneObject* sceneObject, SceneObject* object, int containmentType, String& errorDescription) const {
	if (containmentType == -1 && sceneObject != nullptr && object != nullptr && sceneObject->isCompanionObject()) {
		if (sceneObject == object) {
			errorDescription = "@container_error_message:container02"; // You cannot add something to itself.

			return TransferErrorCode::CANTADDTOITSELF;
		}

		// Deliberately does not replicate the no-trade/vendor/building-
		// ownership matrix ContainerComponent::canAddObject() runs for the
		// general case -- none of those scenarios (factory hoppers, civic
		// structure ownership, etc.) are reachable for a personal companion
		// pet's own inventory. See NOTES.md, "Companion Auto-Equip", for the
		// full list of accepted limitations.
		if (sceneObject->getContainerObjectsSize() >= sceneObject->getContainerVolumeLimit()) {
			errorDescription = "@container_error_message:container03"; // This container is full.

			return TransferErrorCode::CONTAINERFULL;
		}

		return 0;
	}

	return PlayerContainerComponent::canAddObject(sceneObject, object, containmentType, errorDescription);
}

int CompanionContainerComponent::notifyObjectInserted(SceneObject* sceneObject, SceneObject* object) const {
	// Bug fix (2026-07-13, see NOTES.md "notifyObjectInserted/Removed
	// unsigned containmentType" -- found chasing a live SIGABRT):
	// getContainmentType() returns unsigned int, but the loose-item sentinel
	// used throughout this file is -1 (ContainerComponent::transferObject()'s
	// own containmentType parameter is a plain int). Once -1 is stored into
	// the unsigned field it reads back as 0xFFFFFFFF, and comparing that
	// directly against a small positive literal (">= 4") is ALWAYS true --
	// so every loose (-1) insert, from any caller (GiveItemCommand included --
	// it never resolves to a real slot, always passes -1 literally), was
	// being misrouted into the "real equip" branch below instead of the
	// loose-item/attemptAutoEquip() branch. Cast back to a signed int first
	// so -1 compares correctly again.
	int containmentType = object != nullptr ? (int) object->getContainmentType() : -1;

	if (object != nullptr && containmentType >= 4) {
		// Real equip-slot insertion -- reuse the exact same side effects a
		// player equip gets (skill mods, encumbrance, wearables vector,
		// jedi visibility) via the inherited, unmodified
		// PlayerContainerComponent logic. Deferred to a locked task for the
		// same reason attemptAutoEquip() below already is: this hook fires
		// on whatever thread performed the original transfer -- which does
		// NOT guarantee the companion is locked (e.g. GiveItemCommand only
		// takes `Locker objLocker(giveObject)` on the *item*, never on the
		// target companion) -- and PlayerContainerComponent::
		// notifyObjectInserted() ends in tano->addTemplateSkillMods()/
		// applyEncumbrancies()->inflictDamage(), both hard-asserting the
		// target is already locked by the current thread.
		Reference<SceneObject*> sceneObjectRef = sceneObject;
		Reference<SceneObject*> objectRef = object;

		Core::getTaskManager()->executeTask([this, sceneObjectRef, objectRef] () {
			Locker locker(sceneObjectRef);
			Locker itemLocker(objectRef, sceneObjectRef);

			SceneObject* sceneObject = sceneObjectRef.get();
			SceneObject* object = objectRef.get();

			// Re-validate under lock -- state may have changed between the
			// insert notification and this task running.
			if (object->getParent().get() != sceneObject || (int) object->getContainmentType() < 4) {
				return;
			}

			this->PlayerContainerComponent::notifyObjectInserted(sceneObject, object);
		}, "CompanionEquipSideEffectsLambda");

		return 0;
	}

	// Loose item landing directly in the companion's own inventory
	// (containmentType == -1) -- NOT equipped, so skip
	// PlayerContainerComponent's slot-insertion side effects entirely (they
	// don't gate on containmentType at all -- they'd otherwise incorrectly
	// apply skill mods / encumbrance / wearables-vector tracking to
	// something that's merely sitting in the bag). Run the base, no-side-
	// effect notifyObjectInserted(), then attempt auto-equip.
	int result = ContainerComponent::notifyObjectInserted(sceneObject, object);

	attemptAutoEquip(sceneObject, object);

	return result;
}

int CompanionContainerComponent::notifyObjectRemoved(SceneObject* sceneObject, SceneObject* object, SceneObject* destination) const {
	// Mirrors notifyObjectInserted() above, and for the same reason:
	// ContainerComponent::removeObject() never resets an object's
	// containmentType before calling this hook, so object->getContainmentType()
	// still reflects whatever it was while still contained in sceneObject --
	// >=4 means it was a real equipped slot item being un-equipped/removed.
	// Same unsigned-wraparound bug fix as notifyObjectInserted() above: cast
	// to a signed int before comparing, since a stored -1 sentinel otherwise
	// always satisfies ">= 4" as an unsigned value.
	int containmentType = object != nullptr ? (int) object->getContainmentType() : -1;

	if (object != nullptr && containmentType >= 4) {
		// Reuse the real un-equip side effects (skill mod / encumbrance
		// removal, wearables vector, jedi force power recalculation) via the
		// inherited, unmodified PlayerContainerComponent logic. Deferred to a
		// locked task for the same reason as the insert branch above --
		// nothing upstream of this hook guarantees the companion is locked
		// (ContainerComponent::removeObject() releases its own container
		// lock before calling this), and PlayerContainerComponent::
		// notifyObjectRemoved() ends in tano->removeTemplateSkillMods()/
		// removeEncumbrancies(), both hard-asserting the target is already
		// locked.
		Reference<SceneObject*> sceneObjectRef = sceneObject;
		Reference<SceneObject*> objectRef = object;
		Reference<SceneObject*> destinationRef = destination;

		Core::getTaskManager()->executeTask([this, sceneObjectRef, objectRef, destinationRef] () {
			Locker locker(sceneObjectRef);
			Locker itemLocker(objectRef, sceneObjectRef);

			this->PlayerContainerComponent::notifyObjectRemoved(sceneObjectRef.get(), objectRef.get(), destinationRef.get());

			// Companion System (2026-07-15): CENTRAL current-weapon clear.
			// The dedicated companion removal paths (Retrieve Gear,
			// unequipItemToInventory, the equip-swap displacement) each
			// clear the companion's current-weapon pointer themselves --
			// but the CLIENT's own built-in "Pick Up" radial
			// (TransferItemMiscCommand) works too since the loot-permission
			// fix, and that path never knew about companions. Clearing here
			// covers every removal path in one place; the dedicated paths'
			// own clears just become harmless no-ops by the time this runs.
			SceneObject* sceneObj = sceneObjectRef.get();
			SceneObject* removed = objectRef.get();

			if (sceneObj != nullptr && removed != nullptr && sceneObj->isCompanionObject() && removed->isWeaponObject()) {
				CompanionObject* companion = cast<CompanionObject*>(sceneObj);

				if (companion != nullptr && companion->getWeapon().get() == removed) {
					companion->setWeapon(nullptr, true);
					companion->refreshCombatAttacks(nullptr);
				}
			}
		}, "CompanionUnequipSideEffectsLambda");

		return 0;
	}

	// Was a loose inventory item -- notifyObjectInserted() above never ran
	// PlayerContainerComponent's insertion side effects on it (only real
	// equip-slot insertions do), so running the matching *removal* side
	// effects here would incorrectly subtract skill mods / encumbrance /
	// wearables-vector membership that was never added in the first place.
	// Skip straight to the base, no-side-effect notifyObjectRemoved().
	return ContainerComponent::notifyObjectRemoved(sceneObject, object, destination);
}
