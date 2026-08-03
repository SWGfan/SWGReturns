/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-14, "player-side loadout backpack" redesign --
	see CompanionLoadoutContainerComponent.h and NOTES.md).
*/

#include "CompanionLoadoutContainerComponent.h"
#include "server/zone/objects/scene/SceneObject.h"
#include "server/zone/objects/scene/TransferErrorCode.h"
#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/objects/tangible/TangibleObject.h"
#include "server/zone/objects/tangible/weapon/WeaponObject.h"
#include "server/zone/objects/tangible/consumable/Consumable.h"
#include "server/zone/managers/objectcontroller/ObjectController.h"
#include "server/zone/ZoneServer.h"

namespace {

	// This backpack always lives inside a player's own inventory chain, so
	// its root ancestor is always the owning player CreatureObject.
	CreatureObject* resolveOwningPlayer(SceneObject* backpack) {
		if (backpack == nullptr) {
			return nullptr;
		}

		ManagedReference<SceneObject*> rootParent = backpack->getRootParent();

		if (rootParent == nullptr || !rootParent->isPlayerCreature()) {
			return nullptr;
		}

		return rootParent->asCreatureObject();
	}

	// Same datapad-scan pattern used by every /companion* command's own
	// local resolveActiveCompanion() helper (see e.g. CompanionStayCommand.h)
	// -- duplicated here rather than shared, matching this codebase's
	// existing convention of local per-file copies of this exact lookup.
	CompanionObject* resolveActiveCompanion(CreatureObject* player) {
		if (player == nullptr) {
			return nullptr;
		}

		ManagedReference<SceneObject*> datapad = player->getSlottedObject("datapad");

		if (datapad == nullptr) {
			return nullptr;
		}

		for (int i = 0; i < datapad->getContainerObjectsSize(); ++i) {
			ManagedReference<SceneObject*> obj = datapad->getContainerObject(i);

			if (obj == nullptr || !obj->isCompanionControlDevice()) {
				continue;
			}

			CompanionControlDevice* device = cast<CompanionControlDevice*>(obj.get());

			if (device->isCompanionDead()) {
				continue;
			}

			CompanionObject* companion = device->getCompanionObject();

			if (companion == nullptr || companion->getZone() == nullptr) {
				continue;
			}

			if (companion->getLinkedCreature().get() != player) {
				continue;
			}

			return companion;
		}

		return nullptr;
	}

	void attemptAutoEquipOntoActiveCompanion(SceneObject* backpack, SceneObject* object) {
		if (backpack == nullptr || object == nullptr || !object->isTangibleObject()) {
			return;
		}

		TangibleObject* item = object->asTangibleObject();

		if (item == nullptr) {
			return;
		}

		// Weapons/wearables auto-equip; food/drink auto-consumes (Companion
		// System 2026-07-14 consume path -- see Consumable.idl's
		// consumeByCreature()). Anything else (resources, schematics, camp
		// tents, ...) is legitimate ordinary loadout-backpack storage and is
		// simply left where it landed.
		bool eligibleForEquip = (item->isWeaponObject() || item->isWearableObject())
				&& item->getArrangementDescriptorSize() > 0;
		bool eligibleForConsume = item->isConsumable();

		if (!eligibleForEquip && !eligibleForConsume) {
			return;
		}

		CreatureObject* player = resolveOwningPlayer(backpack);

		if (player == nullptr) {
			return;
		}

		CompanionObject* companion = resolveActiveCompanion(player);

		if (companion == nullptr || companion->getZone() == nullptr || companion->isDead()) {
			return;
		}

		Reference<CompanionObject*> companionRef = companion;
		Reference<TangibleObject*> itemRef = item;
		Reference<CreatureObject*> playerRef = player;

		Core::getTaskManager()->executeTask([companionRef, itemRef, playerRef] () {
			Locker locker(companionRef);
			Locker itemLocker(itemRef, companionRef);
			Locker playerLocker(playerRef, companionRef);

			CompanionObject* companion = companionRef.get();
			TangibleObject* item = itemRef.get();
			CreatureObject* player = playerRef.get();

			if (companion == nullptr || item == nullptr || player == nullptr) {
				return;
			}

			// Re-validate under lock -- state may have changed between the
			// insert notification and this task running (companion stored,
			// item already moved/equipped/deleted by something else, etc.).
			if (companion->getZone() == nullptr || companion->isDead()) {
				return;
			}

			if (item->getParent().get() == nullptr || (int) item->getContainmentType() != -1) {
				return;
			}

			// Consume path: food/drink dropped into the loadout backpack is
			// fed straight to the companion. consumeByCreature() applies the
			// buff/heal to the companion, messages the player, and burns a
			// use-count charge (destroying the item at 0 charges); a stacked
			// consumable keeps its remaining charges sitting in the backpack.
			// Unsupported consumable types (spice, instant, delayed) return
			// false and the item just stays stored -- same quiet-failure
			// policy as the equip path below.
			if (item->isConsumable()) {
				Consumable* consumable = cast<Consumable*>(item);

				if (consumable != nullptr) {
					consumable->consumeByCreature(companion, player);
				}

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

			int transferType = 4;
			String errorDescription;

			int precheck = companion->canAddObject(item, transferType, errorDescription);

			if (precheck == TransferErrorCode::SLOTOCCUPIED) {
				if (item->getArrangementDescriptorSize() == 0) {
					return;
				}

				// Companion System (2026-07-14, "player-side loadout
				// backpack" redesign): unlike CompanionContainerComponent's
				// attemptAutoEquip(), which deliberately never force-swaps
				// a currently-worn item, dropping something into this
				// dedicated loadout backpack IS an explicit "equip this"
				// action by the player -- so whatever currently occupies
				// this item's arrangement slot gets displaced into the
				// player's own MAIN inventory (never back into this
				// backpack, which would just re-trigger this same
				// auto-equip hook in a loop) to make room, exactly as
				// requested.
				const Vector<String>* descriptors = item->getArrangementDescriptor(0);
				ManagedReference<SceneObject*> occupant = nullptr;

				for (int j = 0; j < descriptors->size(); ++j) {
					SceneObject* slotted = companion->getSlottedObject(descriptors->get(j));

					if (slotted != nullptr) {
						occupant = slotted;
						break;
					}
				}

				if (occupant != nullptr) {
					// Displaced gear goes into THIS companion's own storage
					// bag (2026-07-15, per user request -- each companion's
					// stuff stays with the companion), falling back to the
					// player's inventory if the bag is missing/full.
					SceneObject* playerInventory = companion->getSlottedObject("inventory");

					if (playerInventory == nullptr || playerInventory->isContainerFull()) {
						playerInventory = player->getSlottedObject("inventory");
					}

					if (playerInventory == nullptr) {
						return;
					}

					Locker occupantLocker(occupant, companion);
					Locker inventoryLocker(playerInventory, companion);

					String displaceError;
					int displaceType = -1;
					int displacePrecheck = playerInventory->canAddObject(occupant, displaceType, displaceError);

					// canAddObject() does NOT catch a full inventory for
					// containment -1 (fullness is only enforced inside
					// ContainerComponent::transferObject(), line ~304) --
					// check it explicitly so the swap fails LOUDLY instead
					// of silently leaving the new item in the backpack
					// (2026-07-14 live-test follow-up, see NOTES.md).
					if (displacePrecheck != 0 || playerInventory->isContainerFull()) {
						// No room in the player's own inventory -- abort the
						// whole swap rather than orphan the currently
						// equipped item; the new item just stays sitting in
						// the backpack for now.
						player->sendSystemMessage("Your inventory is too full to swap out your companion's currently equipped item.");
						return;
					}

					bool wasCurrentWeapon = occupant->isWeaponObject() && occupant == companion->getWeapon().get();

					// Same destroy-first/transfer-silently/re-create ordering as
					// unequipItemToInventory() (2026-07-14, "retrieved item
					// invisible until relog" fix, take 2 -- see NOTES.md and
					// that method's comment): a worn-on-creature -> player
					// inventory move desyncs the client if it sees the
					// cross-creature containment-link broadcast first.
					occupant->broadcastDestroy(occupant, true);

					if (!objectController->transferObject(occupant, playerInventory, displaceType, false)) {
						companion->broadcastObject(occupant, true);
						player->sendSystemMessage("Could not move your companion's currently equipped item to your inventory -- the swap was cancelled.");
						return;
					}

					// Deferred re-create, same reasoning as
					// unequipItemToInventory() take 3 (see NOTES.md): an
					// immediate re-create of a just-destroyed object ID gets
					// silently eaten by the client.
					Reference<SceneObject*> occupantRef = occupant.get();
					Reference<CreatureObject*> displaceOwnerRef = player;

					Core::getTaskManager()->scheduleTask([occupantRef, displaceOwnerRef] () {
						SceneObject* displaced = occupantRef.get();
						CreatureObject* owner = displaceOwnerRef.get();

						if (displaced == nullptr || owner == nullptr) {
							return;
						}

						Locker locker(displaced);

						displaced->sendTo(owner, true);
					}, "CompanionLoadoutDisplaceResendLambda", 400);

					if (wasCurrentWeapon) {
						companion->setWeapon(nullptr, true);
						companion->refreshCombatAttacks(nullptr);
					}
				}
			} else if (precheck != 0) {
				// Not equippable onto this companion at all right now
				// (certification, encumbrance, jedi-only restriction,
				// etc.) -- leave the item sitting in the backpack; this is
				// a passive auto-action, not an explicit command, so it
				// fails quietly rather than spamming an error.
				return;
			}

			if (!objectController->transferObject(item, companion, transferType, true)) {
				return;
			}

			if (item->isWeaponObject()) {
				WeaponObject* weapon = cast<WeaponObject*>(item);

				if (weapon != nullptr) {
					companion->setWeapon(weapon, true);
					companion->refreshCombatAttacks(weapon);
				}
			}
		}, "CompanionLoadoutAutoEquipLambda");
	}

}

int CompanionLoadoutContainerComponent::notifyObjectInserted(SceneObject* sceneObject, SceneObject* object) const {
	// This backpack is a plain VOLUME container -- every insert into it is
	// a loose item (containmentType -1), never a real equip-slot insertion,
	// so (unlike CompanionContainerComponent, which also separately governs
	// the companion's own SLOTTED top-level container) there's no >=4
	// branch to consider here. Run the base, no-side-effect
	// notifyObjectInserted() first -- PlayerContainerComponent's own
	// insertion side effects (skill mods, encumbrance, wearables-vector
	// tracking) are for real equip-slot wear, not loose bag contents, so
	// skip straight to ContainerComponent's version, same reasoning
	// CompanionContainerComponent's own loose-item branch already uses --
	// then attempt the auto-equip-onto-companion side effect.
	int result = ContainerComponent::notifyObjectInserted(sceneObject, object);

	attemptAutoEquipOntoActiveCompanion(sceneObject, object);

	return result;
}
