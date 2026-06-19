/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.*/

#ifndef UNDERHANDSHOTCOMMAND_H_
#define UNDERHANDSHOTCOMMAND_H_

#include "CombatQueueCommand.h"

class UnderHandShotCommand : public CombatQueueCommand {
public:

	UnderHandShotCommand(const String& name, ZoneProcessServer* server)
		: CombatQueueCommand(name, server) {
	}

	int doQueueCommand(CreatureObject* creature, const uint64& target, const UnicodeString& arguments) const {

		ManagedReference<WeaponObject*> weapon = creature->getWeapon();
		
		if (weapon == nullptr)
			return GENERALERROR;

		if (!weapon->isRangedWeapon())
			return INVALIDWEAPON;

		if (!checkStateMask(creature))
			return INVALIDSTATE;

		if (!checkInvalidLocomotions(creature))
			return INVALIDLOCOMOTION;

		return doCombatAction(creature, target);
	}

};

#endif //UNDERHANDSHOTCOMMAND_H_
