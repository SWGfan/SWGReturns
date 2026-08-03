/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-20, "crafting theater" pass, per user request)
	-- /companioncraft <draftSchematicServerScriptPath>: target a single
	companion first, then issue this with the schematic's server script path
	(same convention the admin-only /GenerateCraftedItem command accepts,
	e.g. "weapon/melee/sword/sword_generic" or a full
	"object/draft_schematic/..." path). The targeted companion gathers real
	resources/components (see CompanionCraftingManager.h for the full
	acquisition chain) and hands the finished item to YOUR inventory.

	Deliberately single-companion-target only in this pass (no "whole squad"
	fan-out like CompanionReturnCommand.h) -- crafting one specific item is
	inherently a one-companion job in this build; multi-companion
	collaboration on a single item is the next build pass (see NOTES.md).

	This is a first testable entry point for CompanionCraftingManager, not
	the final player-facing UX -- no walking/theater movement yet, no
	station/factory integration yet, no dedicated command_table icon/macro
	yet (typeable via /companioncraft only for now, same bar
	/companionreturn started at before its icon pass).
*/

#ifndef COMPANIONCRAFTCOMMAND_H_
#define COMPANIONCRAFTCOMMAND_H_

#include "server/zone/objects/creature/commands/QueueCommand.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/managers/companion/CompanionCraftingManager.h"

class CompanionCraftCommand : public QueueCommand {
public:

	CompanionCraftCommand(const String& name, ZoneProcessServer* server)
		: QueueCommand(name, server) {

	}

	/** See CompanionReturnCommand.h's identical helper for the rationale. */
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
			creature->sendSystemMessage("Target an active companion first, then use /companioncraft <schematic>.");
			return GENERALERROR;
		}

		String schematicPath = arguments.toString().trim();

		if (schematicPath.isEmpty()) {
			creature->sendSystemMessage("Usage: /companioncraft <draft schematic path>");
			return GENERALERROR;
		}

		if (companion->isInCombat()) {
			creature->sendSystemMessage(companion->getDisplayedName() + " can't craft while in combat.");
			return GENERALERROR;
		}

		// CompanionCraftingManager::craftItem() locks resources/components it
		// gathers using `companion` as the cross-lock partner (the same
		// two-argument Locker(obj, cross) pattern used throughout this
		// codebase, e.g. CompanionReturnCommand.h's `Locker clocker(companion,
		// creature)`), which requires companion to already be locked by this
		// thread before entering. `creature` itself is already locked by the
		// object controller before doQueueCommand() runs.
		Locker clocker(companion, creature);

		String errorMessage;
		bool success = CompanionCraftingManager::instance()->craftItem(creature, companion, schematicPath, errorMessage);

		if (!success) {
			creature->sendSystemMessage(errorMessage.isEmpty() ? "Crafting failed." : errorMessage);
			return GENERALERROR;
		}

		creature->sendSystemMessage(companion->getDisplayedName() + " finishes crafting and hands you the result.");

		return SUCCESS;
	}
};

#endif // COMPANIONCRAFTCOMMAND_H_
