/*
 * TangibleObjectMenuComponent.cpp
 *
 *  Created on: 26/05/2011
 *      Author: victor
 */

#include "TangibleObjectMenuComponent.h"
#include "server/zone/objects/player/sessions/SlicingSession.h"
#include "server/zone/packets/object/ObjectMenuResponse.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/managers/stringid/StringIdManager.h"
#include "server/zone/ZoneServer.h"
#include "server/zone/objects/scene/SceneObject.h"
#include "server/zone/managers/loot/LootManager.h"
#include "server/zone/managers/loot/LootGroupMap.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"

namespace {
	// Companion System (2026-07-13, "manual equip radial" pass) -- walks up
	// the container hierarchy to find a CompanionObject ancestor, same
	// resolveCompanion() helper shape as CompanionContainerComponent.cpp's
	// (kept as a separate local copy since that one is file-local too).
	CompanionObject* resolveCompanionAncestor(SceneObject* sceneObject) {
		SceneObject* current = sceneObject;

		for (int i = 0; i < 8 && current != nullptr; ++i) {
			if (current->isCompanionObject()) {
				return cast<CompanionObject*>(current);
			}

			current = current->getParent().get();
		}

		return nullptr;
	}
}

void TangibleObjectMenuComponent::fillObjectMenuResponse(SceneObject* sceneObject, ObjectMenuResponse* menuResponse, CreatureObject* player) const {
	ObjectMenuComponent::fillObjectMenuResponse(sceneObject, menuResponse, player);

	uint32 gameObjectType = sceneObject->getGameObjectType();

	if (!sceneObject->isTangibleObject())
		return;

	TangibleObject* tano = cast<TangibleObject*>( sceneObject);

	// Figure out what the object is and if its able to be Sliced.
	if(tano->isSliceable() && !tano->isSecurityTerminal()) { // Check to see if the player has the correct skill level

		bool hasSkill = true;
		ManagedReference<SceneObject*> inventory = player->getSlottedObject("inventory");

		if ((gameObjectType == SceneObjectType::PLAYERLOOTCRATE) && !player->hasSkill("combat_smuggler_novice"))
			hasSkill = false;
		else if (sceneObject->isContainerObject())
			hasSkill = false; // Let the container handle our slice menu
		else if (sceneObject->isMissionTerminal() && !player->hasSkill("combat_smuggler_slicing_01"))
			hasSkill = false;
		else if (sceneObject->isWeaponObject() && (!inventory->hasObjectInContainer(sceneObject->getObjectID()) || !player->hasSkill("combat_smuggler_slicing_02")))
			hasSkill = false;
		else if (sceneObject->isArmorObject() && (!inventory->hasObjectInContainer(sceneObject->getObjectID()) || !player->hasSkill("combat_smuggler_slicing_03")))
			hasSkill = false;

		if(hasSkill)
			menuResponse->addRadialMenuItem(69, 3, "@slicing/slicing:slice"); // Slice
	}

	if(player->getPlayerObject() != nullptr && player->getPlayerObject()->isPrivileged()) {
		/// Viewing components used to craft item, for admins
		ManagedReference<SceneObject*> container = tano->getSlottedObject("crafted_components");
		if(container != nullptr) {

			if(container->getContainerObjectsSize() > 0) {

				SceneObject* satchel = container->getContainerObject(0);

				if(satchel != nullptr && satchel->getContainerObjectsSize() > 0) {
					menuResponse->addRadialMenuItem(79, 3, "@ui_radial:ship_manage_components"); // View Components
				}
			}
		}
	}

	WearableObject* wearable = cast<WearableObject*>(tano);
	if (wearable != nullptr)
	if (wearable->hasSeaRemovalTool(player, false) ==  true)
	if (wearable->isWearableObject() || wearable->isArmorObject()){
		VectorMap<String, int>* mods = wearable->getWearableSkillMods();
			if (mods->size() > 0)
				menuResponse->addRadialMenuItem(89,3,"Extract Skill Mods");
		}
	

	ManagedReference<SceneObject*> parent = tano->getParent().get();
	if (parent != nullptr && parent->getGameObjectType() == SceneObjectType::STATICLOOTCONTAINER) {
		menuResponse->addRadialMenuItem(10, 3, "@ui_radial:item_pickup"); //Pick up
	}

	// Companion System (2026-07-15, "loadout backpack should show what the
	// companion has equipped" -- see NOTES.md): equipped items are children
	// of the companion CREATURE, so they can never be listed inside the
	// backpack container itself -- instead the backpack gets a one-click
	// radial that opens the companion's equipment view. Plain-text label
	// (no TRE change), radial 84 (82/83 are the companion equip/pickup pair
	// below).
	if (tano->getServerObjectCRC() == STRING_HASHCODE("object/tangible/inventory/companion_loadout_backpack.iff")) {
		menuResponse->addRadialMenuItem(84, 3, "View Companion Equipment");
		menuResponse->addRadialMenuItem(85, 3, "Retrieve Companion Gear"); // 2026-07-15, per user request: unequip-all straight from the backpack radial
	}

	// Companion System -- "Equip on Companion" manual override. Only offered
	// for weapons/wearables sitting loose in a companion's own inventory, and
	// only to that companion's owner -- mirrors the auto-equip eligibility
	// check in CompanionContainerComponent.cpp's attemptAutoEquip(), but as
	// an explicit player-triggered action instead of an automatic reaction,
	// so the player can override whatever auto-equip already picked (e.g.
	// swap out an auto-equipped starter weapon for a better one they gave
	// the companion).
	//
	// Bug fix (2026-07-14, see NOTES.md): this used to show regardless of
	// the item's current containmentType, including on an item that's
	// ALREADY equipped (>= 4) -- clicking it there always failed
	// equipItemFromInventory()'s isLooseInCompanionInventory check with the
	// confusing "@companion:equip_not_in_inventory" ("that item isn't in
	// your companion's inventory") message, even though the item was
	// plainly visible and equipped. Gated to containmentType == -1 (loose)
	// so this radial and "Pick Up" (>= 4, below) are now mutually
	// exclusive on any one item, exactly mirroring how equip/unequip work
	// for a real player's own gear.
	if ((tano->isWeaponObject() || tano->isWearableObject()) && tano->getArrangementDescriptorSize() > 0
			&& (int) tano->getContainmentType() == -1) {
		CompanionObject* companion = resolveCompanionAncestor(parent);

		if (companion != nullptr && companion->isAuthorizedActor(player)) {
			menuResponse->addRadialMenuItem(82, 3, "@companion:equip_on_companion"); // Equip on Companion
		}
	}

	// Companion System (2026-07-13, "Take Off Companion" radial pass;
	// relabeled "Pick Up" 2026-07-14 per user request -- see NOTES.md) --
	// the missing counterpart: an item currently equipped (a real slot,
	// containmentType >= 4) directly on a companion. Unsigned
	// getContainmentType(), so cast to signed before comparing (same fix
	// already applied elsewhere in this feature -- a stored -1 sentinel
	// would otherwise always satisfy ">= 4" as an unsigned value). Equipped
	// gear always lives directly on the companion (never in its
	// "inventory" bag), so parent here IS the companion itself in the
	// normal case -- resolveCompanionAncestor() still handles it correctly
	// either way via its walk-up loop. Still radial ID 83 and still backed
	// by unequipItemToInventory() (name unchanged -- see that method's own
	// doc comment for why) -- only the STF label text and the destination
	// the item lands in changed, not the wiring here.
	if ((int) tano->getContainmentType() >= 4) {
		CompanionObject* companion = resolveCompanionAncestor(parent);

		if (companion != nullptr && companion->isAuthorizedActor(player)) {
			// Relabeled from the @companion:unequip_from_companion STF key
			// ("Pick Up") to distinguish it from the CLIENT's own built-in
			// Pick Up radial, which shows on the same items and (since the
			// loot-permission fix) also works -- two identical labels were
			// confusing (2026-07-15 live report). Plain text, no TRE change.
			menuResponse->addRadialMenuItem(83, 3, "Take From Companion");
		}
	}
}

int TangibleObjectMenuComponent::handleObjectMenuSelect(SceneObject* sceneObject, CreatureObject* player, byte selectedID) const {
	if (!sceneObject->isTangibleObject())
		return 0;

	TangibleObject* tano = cast<TangibleObject*>( sceneObject);


	if (selectedID == 69 && player->hasSkill("combat_smuggler_novice") ) { // Slice [PlayerLootCrate]
		if (player->containsActiveSession(SessionFacadeType::SLICING)) {
			player->sendSystemMessage("@slicing/slicing:already_slicing");
			return 0;
		}

		//Create Session
		ManagedReference<SlicingSession*> session = new SlicingSession(player);
		session->initalizeSlicingMenu(player, tano);

		return 0;
	} else if (selectedID == 79) { // See components (admin)
		if(player->getPlayerObject() != nullptr && player->getPlayerObject()->isPrivileged()) {

			SceneObject* container = tano->getSlottedObject("crafted_components");
			if(container != nullptr) {

				if(container->getContainerObjectsSize() > 0) {

					SceneObject* satchel = container->getContainerObject(0);

					if(satchel != nullptr) {

						satchel->sendWithoutContainerObjectsTo(player);
						satchel->openContainerTo(player);

					} else {
						player->sendSystemMessage("There is no satchel this container");
					}
				} else {
					player->sendSystemMessage("There are no items in this container");
				}
			} else {
				player->sendSystemMessage("There is no component container in this object");
			}
		}

		return 0;
	} else if (selectedID == 89) { //Remove SEA Mods from wearable
		
		WearableObject* wearable = cast<WearableObject*>(tano);
		ManagedReference<SceneObject*> sea = NULL;
		bool convertedMods = false;
		ManagedReference<SceneObject*> inventory = player->getSlottedObject("inventory");
			if (wearable != nullptr && inventory != nullptr) { //safety Checks

			if (wearable->hasSeaRemovalTool(player, false) ==  false){ //They need the tool
				player->sendSystemMessage("This requires a specialzied skill enhancing attachment removal tool.");
				return 0;
			}

			if (wearable->isWearableObject() || wearable->isArmorObject()){
				if (wearable->isEquipped()){
				player->sendSystemMessage("You must first un-equip the item.");
				return 0;
				}
				VectorMap<String, int>* mods = wearable->getWearableSkillMods();
					if (mods->size() > 0){	//If the item has no mods we're done
						ManagedReference<LootManager*> lootManager = player->getZoneServer()->getLootManager();		
						int i,j;
						LootGroupMap* lootGroupMap = LootGroupMap::instance();
						Reference<const LootItemTemplate*> itemTemplate = NULL;
						String objectTemplate = "";
						objectTemplate = sceneObject->getObjectTemplate()->getFullTemplateString();
						
						//error("ObjectTempate = " + objectTemplate);
						if (wearable->isArmorObject() || 
						 objectTemplate == "object/tangible/wearables/armor/padded/armor_padded_s01_belt.iff"  || 
						 objectTemplate == "object/tangible/wearables/armor/zam/armor_zam_wesell_belt.iff"){
							//error("Detected as armor or belt");
							itemTemplate = lootGroupMap->getLootItemTemplate("attachment_armor");
						}
						else{
							//error("detect as clothing");
							itemTemplate = lootGroupMap->getLootItemTemplate("attachment_clothing");
						}
						if (lootGroupMap == nullptr){
						error("Invalid loot template");
						return 0;
						}
						for (i=0;i<mods->size();i++){//Remove skill mods from item and create tapes
							
							String modKey = mods->elementAt(i).getKey();

							sea = lootManager->createLootAttachment(itemTemplate,modKey, mods->elementAt(i).getValue()); 

							if (sea != nullptr){
								Attachment* attachment = cast<Attachment*>(sea.get());
								
								if (attachment != nullptr){
									Locker objLocker(attachment);
									if (inventory->transferObject(sea, -1, true, true)) { //Transfer tape to player inventory
										inventory->broadcastObject(sea, true);
									} else {
										sea->destroyObjectFromDatabase(true);
										error("Unable to place Skill Attachment in player's inventory!");
										return false;
									}
									
								}
								
							}
						}
						//Destroy item now that tapes have been generated
						if (wearable->hasSeaRemovalTool(player,true) ==  true)
							player->sendSystemMessage("Your SEA Tool has been consumed in the process.");
						wearable->destroyObjectFromWorld(true);
						wearable->destroyObjectFromDatabase(true);		
						player->sendSystemMessage("Removing SEA");
						if (convertedMods)
							player->sendSystemMessage("Old skill mods were converted to new skill mods.");
					}	
			}
		}

		
	return 0;
	} else if (selectedID == 82) { // Equip on Companion
		ManagedReference<SceneObject*> parent = tano->getParent().get();
		CompanionObject* companion = resolveCompanionAncestor(parent);

		if (companion != nullptr && companion->isAuthorizedActor(player)) {
			companion->equipItemFromInventory(tano, player);
		}

		return 0;
	} else if (selectedID == 83) { // Pick Up (was "Take Off Companion")
		ManagedReference<SceneObject*> parent = tano->getParent().get();
		CompanionObject* companion = resolveCompanionAncestor(parent);

		if (companion != nullptr && companion->isAuthorizedActor(player)) {
			companion->unequipItemToInventory(tano, player);
		}

		return 0;
	} else if (selectedID == 84 || selectedID == 85) { // View Companion Equipment / Retrieve Companion Gear (loadout backpack shortcuts, 2026-07-15)
		if (tano->getServerObjectCRC() != STRING_HASHCODE("object/tangible/inventory/companion_loadout_backpack.iff")) {
			return 0;
		}

		// The backpack lives in the PLAYER's inventory, so the companion is
		// found via the same datapad scan every /companion* command uses.
		ManagedReference<SceneObject*> datapad = player->getSlottedObject("datapad");

		if (datapad == nullptr) {
			return 0;
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

			CompanionObject* companion = device->getCompanionObject();

			if (companion == nullptr || companion->getZone() == nullptr) {
				continue;
			}

			if (companion->getLinkedCreature().get() != player) {
				continue;
			}

			if (selectedID == 84) { // View Companion Equipment
				companion->openContainerTo(player);
				return 0;
			}

			// 85: Retrieve Companion Gear -- unequip everything onto the
			// owner, per item through the proven unequipItemToInventory()
			// path (same collection loop as the companion's own Retrieve
			// Gear radial in CompanionMenuComponent.cpp).
			Locker companionLocker(companion, player);

			SceneObject* bag = companion->getSlottedObject("inventory");

			// Port note (genesis base): this base has no
			// CreatureObject::getDefaultWeapon() -- the innate weapon is
			// resolved straight out of the "default_weapon" slot, exactly
			// as CompanionObjectImplementation.cpp already does on this
			// base. Same object, API that actually exists here.
			SceneObject* defaultWeapon = companion->getSlottedObject("default_weapon");

			SortedVector<ManagedReference<SceneObject*> > gear;
			gear.setNoDuplicateInsertPlan();

			for (int j = 0; j < companion->getSlottedObjectsSize(); ++j) {
				SceneObject* slotted = companion->getSlottedObject(j);

				if (slotted == nullptr || slotted == bag || slotted == defaultWeapon) {
					continue;
				}

				if (!slotted->isWeaponObject() && !slotted->isWearableObject()) {
					continue;
				}

				gear.put(slotted);
			}

			if (gear.size() == 0) {
				player->sendSystemMessage("Your companion has no removable gear equipped.");
				return 0;
			}

			for (int j = 0; j < gear.size(); ++j) {
				TangibleObject* gearItem = gear.get(j)->asTangibleObject();

				if (gearItem != nullptr) {
					companion->unequipItemToInventory(gearItem, player);
				}
			}

			return 0;
		}

		player->sendSystemMessage("@companion:no_active_companion"); // You have no active companion.

		return 0;
	} else
		return ObjectMenuComponent::handleObjectMenuSelect(sceneObject, player, selectedID);

}

