/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.*/

#ifndef GRANTJEDISLOTCOMMAND_H_
#define GRANTJEDISLOTCOMMAND_H_

#include "server/zone/objects/player/PlayerObject.h"
#include "server/login/account/Account.h"

class GrantJediSlotCommand : public QueueCommand {
public:

	GrantJediSlotCommand(const String& name, ZoneProcessServer* server) :
		QueueCommand(name, server) {

	}

	int doQueueCommand(CreatureObject* creature, const uint64& target,
			const UnicodeString& arguments) const {

		if (!checkStateMask(creature))
			return INVALIDSTATE;

		if (!checkInvalidLocomotions(creature))
			return INVALIDLOCOMOTION;

		ManagedReference<PlayerObject*> adminGhost = creature->getPlayerObject();

		if (adminGhost == nullptr || !adminGhost->isPrivileged())
			return INVALIDPARAMETERS;

		ManagedReference<CreatureObject*> targetCreature = nullptr;
		ManagedReference<PlayerManager*> playerManager = server->getPlayerManager();

		StringTokenizer args(arguments.toString());

		if (args.hasMoreTokens()) {
			String character;
			args.getStringToken(character);

			targetCreature = playerManager->getPlayer(character);
		} else {
			targetCreature = server->getZoneServer()->getObject(target).castTo<CreatureObject*>();
		}

		if (targetCreature == nullptr || !targetCreature->isPlayerCreature()) {
			creature->sendSystemMessage("Usage: /grantJediSlot <player name>");
			return GENERALERROR;
		}

		ManagedReference<PlayerObject*> targetGhost = targetCreature->getPlayerObject();

		if (targetGhost == nullptr) {
			creature->sendSystemMessage("Player Ghost not found");
			return SUCCESS;
		}

		ManagedReference<Account*> account = targetGhost->getAccount();

		if (account == nullptr) {
			creature->sendSystemMessage("Account not found for that character");
			return SUCCESS;
		}

		Locker alocker(account);

		account->grantJediSlot();

		creature->sendSystemMessage(targetCreature->getFirstName() + "'s account has been granted the bonus Jedi character slot.");

		return SUCCESS;
	}

};

#endif //GRANTJEDISLOTCOMMAND_H_
