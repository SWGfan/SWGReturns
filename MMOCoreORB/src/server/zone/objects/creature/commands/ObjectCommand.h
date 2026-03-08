/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.*/

#ifndef OBJECTCOMMAND_H_
#define OBJECTCOMMAND_H_

#include "server/zone/objects/tangible/weapon/WeaponObject.h"
#include "server/zone/objects/scene/SceneObject.h"
#include "server/zone/managers/loot/LootManager.h"
#include "server/zone/managers/crafting/CraftingManager.h"
#include "server/zone/managers/crafting/ComponentMap.h"
#include "server/zone/objects/tangible/terminal/characterbuilder/CharacterBuilderTerminal.h"

class ObjectCommand : public QueueCommand {
public:

	ObjectCommand(const String& name, ZoneProcessServer* server)
		: QueueCommand(name, server) {

	}

	int doQueueCommand(CreatureObject* creature, const uint64& target, const UnicodeString& arguments) const {

		if (!checkStateMask(creature))
			return INVALIDSTATE;

		if (!checkInvalidLocomotions(creature))
			return INVALIDLOCOMOTION;

		StringTokenizer args(arguments.toString());

		try {
			String commandType;
			args.getStringToken(commandType);

			if (commandType.beginsWith("createitem")) {
				String objectTemplate;
				args.getStringToken(objectTemplate);

				ManagedReference<CraftingManager*> craftingManager = creature->getZoneServer()->getCraftingManager();
				if(craftingManager == nullptr) {
					return GENERALERROR;
				}

				Reference<SharedObjectTemplate*> shot = TemplateManager::instance()->getTemplate(objectTemplate.hashCode());

				if (shot == nullptr || !shot->isSharedTangibleObjectTemplate()) {
					creature->sendSystemMessage("Templates must be tangible objects, or descendants of tangible objects, only.");
					return INVALIDPARAMETERS;
				}

				ManagedReference<SceneObject*> inventory = creature->getSlottedObject("inventory");

				if (inventory == nullptr || inventory->isContainerFullRecursive()) {
					creature->sendSystemMessage("Your inventory is full, so the item could not be created.");
					return INVALIDPARAMETERS;
				}

				ManagedReference<TangibleObject*> object = (server->getZoneServer()->createObject(shot->getServerObjectCRC(), 1)).castTo<TangibleObject*>();

				if (object == nullptr) {
					creature->sendSystemMessage("The object '" + commandType + "' could not be created because the template could not be found.");
					return INVALIDPARAMETERS;
				}

				Locker locker(object);

				object->createChildObjects();

				String crafterName = creature->getFirstName() + " (Dev Spawn)";
				object->setCraftersName(crafterName);

				String serial = craftingManager->generateSerial();
				object->setSerialNumber(serial);

				int quantity = 1;

				if (args.hasMoreTokens())
					quantity = args.getIntToken();

				if(quantity > 1 && quantity <= 100)
					object->setUseCount(quantity);

				while (args.hasMoreTokens()) {
					String visName;
					args.getStringToken(visName);

					uint32 visId = visName.hashCode();
					if (ComponentMap::instance()->getFromID(visId).getId() == 0)
						continue;

					object->addVisibleComponent(visId, false);
				}

				if (inventory->transferObject(object, -1, true)) {
					inventory->broadcastObject(object, true);
					creature->sendSystemMessage("Created item with crafter tag: " + crafterName);
				} else {
					object->destroyObjectFromDatabase(true);
					creature->sendSystemMessage("Error transferring object to inventory.");
				}
			} else if (commandType.beginsWith("createloot")) {
				String lootGroup;
				args.getStringToken(lootGroup);

				int level = 1;

				if (args.hasMoreTokens())
					level = args.getIntToken();

				ManagedReference<SceneObject*> inventory = creature->getSlottedObject("inventory");

				if (inventory == nullptr || inventory->isContainerFullRecursive()) {
					creature->sendSystemMessage("Your inventory is full, so the item could not be created.");
					return INVALIDPARAMETERS;
				}

				ManagedReference<LootManager*> lootManager = creature->getZoneServer()->getLootManager();

				if (lootManager == nullptr)
					return INVALIDPARAMETERS;

				lootManager->createLoot(inventory, lootGroup, level);
			} else if (commandType.beginsWith("createresource")) {
				String resourceName;
				args.getStringToken(resourceName);

				int quantity = 100000;

				if (args.hasMoreTokens())
					quantity = args.getIntToken();

				ManagedReference<ResourceManager*> resourceManager = server->getZoneServer()->getResourceManager();
				resourceManager->givePlayerResource(creature, resourceName, quantity);
			} else if (commandType.beginsWith("createarealoot")) {
				String lootGroup;
				args.getStringToken(lootGroup);

				int range = 32;
				if (args.hasMoreTokens())
					range = args.getIntToken();

				if( range < 0 )
					range = 32;

				if( range > 128 )
					range = 128;

				int level = 1;
				if (args.hasMoreTokens())
					level = args.getIntToken();

				ManagedReference<LootManager*> lootManager = creature->getZoneServer()->getLootManager();
				if (lootManager == nullptr)
					return INVALIDPARAMETERS;

				Zone* zone = creature->getZone();
				if (zone == nullptr)
					return GENERALERROR;

				SortedVector<QuadTreeEntry*> closeObjects;
				CloseObjectsVector* closeObjectsVector = (CloseObjectsVector*) creature->getCloseObjects();
				if (closeObjectsVector == nullptr) {
					zone->getInRangeObjects(creature->getPositionX(), creature->getPositionY(), range, &closeObjects, true);
				} else {
					closeObjectsVector->safeCopyTo(closeObjects);
				}

				for (int i = 0; i < closeObjects.size(); i++) {
					SceneObject* targetObject = static_cast<SceneObject*>(closeObjects.get(i));

					if (targetObject->isPlayerCreature() && creature->isInRange(targetObject, range)) {

						CreatureObject* targetPlayer = cast<CreatureObject*>(targetObject);
						Locker tlock( targetPlayer, creature );

						ManagedReference<SceneObject*> inventory = targetPlayer->getSlottedObject("inventory");
						if (inventory != nullptr) {
							if( lootManager->createLoot(inventory, lootGroup, level) )
								targetPlayer->sendSystemMessage( "You have received a loot item!");
						}

						tlock.release();
					}
				}
			} else if (commandType.beginsWith("checklooted")) {
				ManagedReference<LootManager*> lootManager = creature->getZoneServer()->getLootManager();
				if (lootManager == nullptr)
					return INVALIDPARAMETERS;

				creature->sendSystemMessage("Number of Legendaries Looted: " + String::valueOf(lootManager->getLegendaryLooted()));
				creature->sendSystemMessage("Number of Exceptionals Looted: " + String::valueOf(lootManager->getExceptionalLooted()));
				creature->sendSystemMessage("Number of Magical Looted: " + String::valueOf(lootManager->getYellowLooted()));

			} else if (commandType.beginsWith("characterbuilder")) {
				ZoneServer* zserv = server->getZoneServer();

				String blueFrogTemplate = "object/tangible/terminal/terminal_character_builder.iff";
				ManagedReference<CharacterBuilderTerminal*> blueFrog = ( zserv->createObject(blueFrogTemplate.hashCode(), 0)).castTo<CharacterBuilderTerminal*>();

				if (blueFrog == nullptr)
					return GENERALERROR;

				Locker clocker(blueFrog, creature);

				float x = creature->getPositionX();
				float y = creature->getPositionY();
				float z = creature->getPositionZ();

				ManagedReference<SceneObject*> parent = creature->getParent().get();

				blueFrog->initializePosition(x, z, y);
				blueFrog->setDirection(creature->getDirectionW(), creature->getDirectionX(), creature->getDirectionY(), creature->getDirectionZ());

				if (parent != nullptr && parent->isCellObject())
					parent->transferObject(blueFrog, -1);
				else
					creature->getZone()->transferObject(blueFrog, -1, true);

				info("blue frog created", true);

			}
			else if (commandType.beginsWith("modify"))
			{
				String objID;
				args.getStringToken(objID);
				uint64 oid = UnsignedLong::valueOf(objID);

				if(server->getZoneServer()->getObject(oid) == nullptr)
				{
					creature->sendSystemMessage("Object couldn't be found, are you sure you entered the correct object ID?");
					return INVALIDPARAMETERS;
				}

				ManagedReference<TangibleObject*> object = server->getZoneServer()->getObject(oid).castTo<TangibleObject*>();
				if (object == nullptr) {
					creature->sendSystemMessage("Target object is not a tangible object.");
					return INVALIDPARAMETERS;
				}

				creature->sendSystemMessage("Found: " + String::valueOf(object->getObjectName()) + " with object id: " + String::valueOf(object->getObjectID()));
				creature->sendSystemMessage("Template: " + object->getObjectTemplate()->getTemplateFileName());

				String subCommand;
				args.getStringToken(subCommand);

				if(subCommand == "attributes")
				{
					String attributeName;
					args.getStringToken(attributeName);
					int attributeAmount = args.getIntToken();

					object->addSkillMod(SkillModManager::TEMPLATE, attributeName, attributeAmount);
					creature->sendSystemMessage("Added attribute " + attributeName + " amount " + String::valueOf(attributeAmount));
				}
				else if(subCommand == "uses")
				{
					int amount = args.getIntToken();
					object->setUseCount(amount, true);
					creature->sendSystemMessage("Set amount to " + String::valueOf(amount));
				}
				else if(subCommand == "clone")
				{
					ManagedReference<TangibleObject*> clonedObject = cast<TangibleObject*>(ObjectManager::instance()->cloneObject(object));
					ManagedReference<SceneObject*> inventory = creature->getSlottedObject("inventory");

					if (inventory == nullptr) {
						creature->sendSystemMessage("Inventory not found.");
						return INVALIDPARAMETERS;
					}

					inventory->broadcastObject(clonedObject, true);
					inventory->transferObject(clonedObject, -1, true);
					creature->sendSystemMessage("Object cloned.");
				}
				else if (subCommand == "template")
				{
					String newTemplate;
					args.getStringToken(newTemplate);

					object->setClientObjectCRC(newTemplate.hashCode());
					creature->sendSystemMessage("Client template changed to " + newTemplate);
				}
				else if (subCommand == "dot")
				{
					ManagedReference<WeaponObject*> weapon = cast<WeaponObject*>(object.get());

					if (weapon == nullptr) {
						creature->sendSystemMessage("Target object is not a weapon.");
						return INVALIDPARAMETERS;
					}

					int type = args.getIntToken();
					int attribute = args.getIntToken();
					int strength = args.getIntToken();
					int duration = args.getIntToken();
					int potency = args.getIntToken();
					int uses = args.getIntToken();

					weapon->addDotType(type);
					weapon->addDotAttribute(attribute);
					weapon->addDotStrength(strength);
					weapon->addDotDuration(duration);
					weapon->addDotPotency(potency);
					weapon->addDotUses(uses);

					creature->sendSystemMessage(
						"DOT added. Type=" + String::valueOf(type) +
						" Attribute=" + String::valueOf(attribute) +
						" Strength=" + String::valueOf(strength) +
						" Duration=" + String::valueOf(duration) +
						" Potency=" + String::valueOf(potency) +
						" Uses=" + String::valueOf(uses)
					);
				}
				else if (subCommand == "cleardots")
				{
					ManagedReference<WeaponObject*> weapon = cast<WeaponObject*>(object.get());

					if (weapon == nullptr) {
						creature->sendSystemMessage("Target object is not a weapon.");
						return INVALIDPARAMETERS;
					}

					while (weapon->getNumberOfDots() > 0)
						weapon->removeDot(0);

					creature->sendSystemMessage("All DOTs cleared.");
				}
				else if (subCommand == "setdamage")
				{
					ManagedReference<WeaponObject*> weapon = cast<WeaponObject*>(object.get());

					if (weapon == nullptr) {
						creature->sendSystemMessage("Target object is not a weapon.");
						return INVALIDPARAMETERS;
					}

					int minDamage = args.getIntToken();
					int maxDamage = args.getIntToken();

					weapon->setMinDamage(minDamage);
					weapon->setMaxDamage(maxDamage);

					creature->sendSystemMessage("Weapon damage set. Min=" + String::valueOf(minDamage) + " Max=" + String::valueOf(maxDamage));
				}
				else if (subCommand == "reset")
				{
					ManagedReference<SceneObject*> inventory = creature->getSlottedObject("inventory");
					if (inventory == nullptr) {
						creature->sendSystemMessage("Inventory not found.");
						return INVALIDPARAMETERS;
					}

					String templateFile = object->getObjectTemplate()->getTemplateFileName();
					ManagedReference<SceneObject*> parent = object->getParent().get();
					if (parent == nullptr || parent != inventory) {
						creature->sendSystemMessage("Reset currently only works on items in your inventory.");
						return INVALIDPARAMETERS;
					}

					ManagedReference<TangibleObject*> freshObject = (server->getZoneServer()->createObject(templateFile.hashCode(), 1)).castTo<TangibleObject*>();
					if (freshObject == nullptr) {
						creature->sendSystemMessage("Failed to recreate object from template.");
						return GENERALERROR;
					}

					Locker flocker(freshObject);
					freshObject->createChildObjects();
					String resetCrafter = creature->getFirstName() + " (Reset by Dev)";
					freshObject->setCraftersName(resetCrafter);

					if (!inventory->transferObject(freshObject, -1, true)) {
						freshObject->destroyObjectFromDatabase(true);
						creature->sendSystemMessage("Failed to place reset item into inventory.");
						return GENERALERROR;
					}

					inventory->broadcastObject(freshObject, true);

					Locker olocker(object, creature);
					object->destroyObjectFromWorld(true);
					object->destroyObjectFromDatabase(true);

					creature->sendSystemMessage("Item reset to a fresh template copy.");
				}
				else
				{
					creature->sendSystemMessage("Unknown Command");
					return INVALIDPARAMETERS;
				}
			}
		} catch (Exception& e) {
			creature->sendSystemMessage("SYNTAX: /object createitem <objectTemplatePath> [<quantity>]");
			creature->sendSystemMessage("SYNTAX: /object createresource <resourceName> [<quantity>]");
			creature->sendSystemMessage("SYNTAX: /object createloot <loottemplate> [<level>]");
			creature->sendSystemMessage("SYNTAX: /object createarealoot <loottemplate> [<range>] [<level>]");
			creature->sendSystemMessage("SYNTAX: /object checklooted");
			creature->sendSystemMessage("SYNTAX: /object characterbuilder");
			creature->sendSystemMessage("SYNTAX: /object modify <oid> attributes <attribute name> <amount>");
			creature->sendSystemMessage("SYNTAX: /object modify <oid> uses <amount>");
			creature->sendSystemMessage("SYNTAX: /object modify <oid> clone");
			creature->sendSystemMessage("SYNTAX: /object modify <oid> template <newTemplate>");
			creature->sendSystemMessage("SYNTAX: /object modify <oid> dot <type> <attribute> <strength> <duration> <potency> <uses>");
			creature->sendSystemMessage("SYNTAX: /object modify <oid> cleardots");
			creature->sendSystemMessage("SYNTAX: /object modify <oid> setdamage <min> <max>");
			creature->sendSystemMessage("SYNTAX: /object modify <oid> reset");
			return INVALIDPARAMETERS;
		}

		return SUCCESS;
	}

};

#endif //OBJECTCOMMAND_H_
