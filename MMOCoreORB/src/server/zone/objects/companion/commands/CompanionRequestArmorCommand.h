/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-23, "full suit" pass, per user request) --
	/companionrequestarmor: target an armorsmith companion first, then use
	this command (or its hotbar icon, once one exists -- typeable only for
	now, same bar CompanionCraftCommand.h started at). Opens the armor-type
	picker (CompanionArmorTypeSuiCallback.h); picking a type then walks you
	through crafting each piece via the existing CompanionCraftTheater.

	The squad-wide armor NEEDS-CHECK ("does anyone need armor, or better
	armor?") is a SEPARATE, related feature -- see
	CompanionGearExchangeManager::checkAndExchangeArmor(), which also runs
	automatically on a timer per armorsmith companion (no command needed for
	that half). This command is specifically "build ME a suit."
*/

#ifndef COMPANIONREQUESTARMORCOMMAND_H_
#define COMPANIONREQUESTARMORCOMMAND_H_

#include "server/zone/objects/creature/commands/QueueCommand.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/managers/companion/CompanionGearExchangeManager.h"
#include "server/zone/managers/companion/callbacks/CompanionArmorTypeSuiCallback.h"

class CompanionRequestArmorCommand : public QueueCommand {
public:

	CompanionRequestArmorCommand(const String& name, ZoneProcessServer* server)
		: QueueCommand(name, server) {

	}

	/** Same targeted-companion resolution every single-target Companion*Command
	 * duplicates locally (see CompanionCraftCommand.h's identical helper). */
	CompanionObject* resolveSingleTargetedCompanion(CreatureObject* player, const uint64& target) const {
		uint64 targetID = (target != 0) ? target : player->getTargetID();

		if (targetID == 0) {
			return nullptr;
		}

		ManagedReference<SceneObject*> targetObject = player->getZoneServer()->getObject(targetID, true);

		if (targetObject == nullptr || !targetObject->isCompanionObject()) {
			return nullptr;
		}

		CompanionObject* companion = cast<CompanionObject*>(targetObject.get());

		if (companion->getZone() == nullptr || companion->getLinkedCreature().get() != player) {
			return nullptr;
		}

		ManagedReference<CompanionControlDevice*> device = companion->getCompanionControlDevice();

		if (device != nullptr && device->isCompanionDead()) {
			return nullptr;
		}

		return companion;
	}

	int doQueueCommand(CreatureObject* creature, const uint64& target, const UnicodeString& arguments) const {
		if (!checkStateMask(creature)) {
			return INVALIDSTATE;
		}

		if (!checkInvalidLocomotions(creature)) {
			return INVALIDLOCOMOTION;
		}

		CompanionObject* companion = resolveSingleTargetedCompanion(creature, target);

		if (companion == nullptr) {
			creature->sendSystemMessage("Target an active armorsmith companion first, then use /companionrequestarmor.");
			return GENERALERROR;
		}

		if (!CompanionGearExchangeManager::isArmorsmith(companion)) {
			creature->sendSystemMessage(companion->getDisplayedName() + " isn't an armorsmith.");
			return GENERALERROR;
		}

		if (companion->isInCombat()) {
			creature->sendSystemMessage(companion->getDisplayedName() + " can't take orders while in combat.");
			return GENERALERROR;
		}

		CompanionArmorTypeSuiCallback::sendTypeList(creature, companion);

		return SUCCESS;
	}
};

#endif // COMPANIONREQUESTARMORCOMMAND_H_
