/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.*/

#ifndef DECLAREOVERTCOMMAND_H_
#define DECLAREOVERTCOMMAND_H_

#include "server/zone/objects/player/FactionStatus.h"

class DeclareOvertCommand : public QueueCommand {
public:

	DeclareOvertCommand(const String& name, ZoneProcessServer* server)
		: QueueCommand(name, server) {

	}

	int doQueueCommand(CreatureObject* creature, const uint64& target, const UnicodeString& arguments) const {

		if (!checkStateMask(creature))
			return INVALIDSTATE;

		if (!checkInvalidLocomotions(creature))
			return INVALIDLOCOMOTION;

		if (!creature->isPlayerCreature())
			return GENERALERROR;

		if (!creature->isRebel() && !creature->isImperial()) {
			creature->sendSystemMessage("You must be Rebel or Imperial to declare overt.");
			return GENERALERROR;
		}

		if (creature->getFactionStatus() == FactionStatus::OVERT) {
			creature->sendSystemMessage("You are already overt.");
			return SUCCESS;
		}

		creature->setFactionStatus(FactionStatus::OVERT);
		creature->sendSystemMessage("You are now overt and may be attacked by enemy faction players.");

		return SUCCESS;
	}

};

#endif //DECLAREOVERTCOMMAND_H_
