/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-29, "Entertainer Dance/Watch" -- per Nick,
	third of three independent ways to end a dance/watch session: the
	"Stop Dance" radial, automatic stop on range/line-of-sight loss, and
	this chat command). Modeled on CompanionRequestArmorCommand.h's shape
	(no target required -- acts on the OWNER's current session, not a
	specific companion, since CampDeploymentManager keys the session by
	owner, not by entertainer). See CampDeploymentManager.h/.cpp for the
	actual stop logic (weapon re-equip, dance-mind buff, spatial-chat
	line) -- this command is a thin, real dispatcher onto
	CampDeploymentManager::stopEntertainerDanceWatch(), same as the "Stop
	Dance" radial in CompanionMenuComponent.cpp.

	NAMING NOTE: NOT named /companionstopdance -- that name is already
	taken by the generic CompanionAbilityCommand dispatcher's real
	per-companion "stop your own performance" ability (see
	CommandConfigManager2.cpp's registerCommands2(), "companionstopdance"
	entry) -- an unrelated, pre-existing command. This is
	/companionenddance instead.
*/

#ifndef COMPANIONENDDANCECOMMAND_H_
#define COMPANIONENDDANCECOMMAND_H_

#include "server/zone/objects/creature/commands/QueueCommand.h"
#include "server/zone/managers/companion/CampDeploymentManager.h"

class CompanionEndDanceCommand : public QueueCommand {
public:

	CompanionEndDanceCommand(const String& name, ZoneProcessServer* server)
		: QueueCommand(name, server) {

	}

	int doQueueCommand(CreatureObject* creature, const uint64& target, const UnicodeString& arguments) const {
		if (!checkStateMask(creature)) {
			return INVALIDSTATE;
		}

		if (!checkInvalidLocomotions(creature)) {
			return INVALIDLOCOMOTION;
		}

		if (!CampDeploymentManager::instance()->hasActiveDanceSession(creature)) {
			// "Play Music" (2026-07-29): hasActiveDanceSession()/
			// stopEntertainerDanceWatch() already cover a Music session too --
			// shared one-session-per-owner slot, see CampDeploymentManager.h.
			// Wording only, generalized to cover either performance.
			creature->sendSystemMessage("None of your companions are performing right now.");
			return GENERALERROR;
		}

		CampDeploymentManager::instance()->stopEntertainerDanceWatch(creature->getObjectID());

		return SUCCESS;
	}
};

#endif // COMPANIONENDDANCECOMMAND_H_
