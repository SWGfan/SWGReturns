/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-17, "pet command port" pass, per user request)
	-- dedicated /companionfriend command, the companion equivalent of the
	real Creature Handler pet's Friend order (PetFriendCommand.h): toggles
	the owner's current targeted PLAYER on/off every summoned companion's
	friend list. A friend is never auto-attacked by the companions'
	owner-threat auto-defense, even when they attack the owner (the
	duel-with-a-buddy case) -- see CompanionObjectImplementation::
	interceptThreatToOwner()'s friend gate and CompanionObject.idl's
	friendIds field.
*/

#ifndef COMPANIONFRIENDCOMMAND_H_
#define COMPANIONFRIENDCOMMAND_H_

#include "server/zone/objects/creature/commands/QueueCommand.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/managers/companion/CompanionChatter.h"

class CompanionFriendCommand : public QueueCommand {
public:

	CompanionFriendCommand(const String& name, ZoneProcessServer* server)
		: QueueCommand(name, server) {

	}

	/** See CompanionFollowCommand.h's identical helper for the rationale. */
	void resolveActiveCompanions(CreatureObject* player, Vector<ManagedReference<CompanionObject*>>& companions) const {
		if (player == nullptr) {
			return;
		}

		ManagedReference<SceneObject*> datapad = player->getSlottedObject("datapad");

		if (datapad == nullptr) {
			return;
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

			companions.add(companion);
		}
	}

	int doQueueCommand(CreatureObject* creature, const uint64& target, const UnicodeString& arguments) const {
		if (!checkStateMask(creature)) {
			return INVALIDSTATE;
		}

		if (!checkInvalidLocomotions(creature)) {
			return INVALIDLOCOMOTION;
		}

		Vector<ManagedReference<CompanionObject*>> companions;
		resolveActiveCompanions(creature, companions);

		if (companions.size() == 0) {
			creature->sendSystemMessage("@companion:no_active_companion"); // You have no active companion.
			return GENERALERROR;
		}

		uint64 targetID = (target != 0) ? target : creature->getTargetID();

		if (targetID == 0) {
			creature->sendSystemMessage("You must target the player your companions should treat as a friend.");
			return INVALIDPARAMETERS;
		}

		ManagedReference<SceneObject*> targetObject = creature->getZoneServer()->getObject(targetID, true);

		if (targetObject == nullptr || !targetObject->isPlayerCreature()) {
			creature->sendSystemMessage("You must target the player your companions should treat as a friend.");
			return INVALIDPARAMETERS;
		}

		CreatureObject* friendPlayer = cast<CreatureObject*>(targetObject.get());

		if (friendPlayer == nullptr || friendPlayer == creature) {
			creature->sendSystemMessage("You must target the player your companions should treat as a friend.");
			return INVALIDPARAMETERS;
		}

		bool nowFriend = false;

		for (int i = 0; i < companions.size(); ++i) {
			CompanionObject* companion = companions.get(i);

			Locker clocker(companion, creature);

			// Same result for every companion (all toggled together, squad
			// style) -- record the final state from the last toggle.
			nowFriend = companion->toggleCompanionFriend(friendPlayer->getObjectID());
		}

		if (nowFriend) {
			creature->sendSystemMessage("Your companions now consider " + friendPlayer->getFirstName() + " a friend and will not attack them.");

			// Companion System (2026-07-17, "command flair" pass) -- see
			// CompanionChatter.h.
			CompanionChatter::announceOrder(creature, "Squad -- " + friendPlayer->getFirstName() + " is a friend. Stand easy.", "friend", companions);
		} else {
			creature->sendSystemMessage("Your companions no longer consider " + friendPlayer->getFirstName() + " a friend.");
		}

		return SUCCESS;
	}
};

#endif // COMPANIONFRIENDCOMMAND_H_
