/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-17, "pet command port" pass, per user request)
	-- dedicated /companiongroup command, the companion equivalent of the
	real Creature Handler pet's Group order (PetGroupCommand.h): toggles
	every summoned companion into/out of the owner's group. Same
	GroupManager::inviteToGroup()/leaveGroup() calls the pet version uses
	(companions already have real group membership -- storeObject() and
	handleCompanionDeath() both already tear it down on despawn).
*/

#ifndef COMPANIONGROUPCOMMAND_H_
#define COMPANIONGROUPCOMMAND_H_

#include "server/zone/objects/creature/commands/QueueCommand.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/objects/group/GroupObject.h"
#include "server/zone/managers/group/GroupManager.h"
#include "server/zone/managers/companion/CompanionChatter.h"

class CompanionGroupCommand : public QueueCommand {
public:

	CompanionGroupCommand(const String& name, ZoneProcessServer* server)
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

		int joined = 0;
		int left = 0;

		for (int i = 0; i < companions.size(); ++i) {
			CompanionObject* companion = companions.get(i);

			Locker clocker(companion, creature);

			ManagedReference<GroupObject*> group = companion->getGroup();

			if (group == nullptr) {
				// Same call shape as PetGroupCommand.h -- owner is the
				// inviter (owner is already locked as the command executor,
				// companion crosslocked above, matching the pet version's
				// two-object locking).
				GroupManager::instance()->inviteToGroup(creature, companion);
				++joined;
			} else {
				GroupManager::instance()->leaveGroup(group, companion);
				++left;
			}
		}

		// Companion System (2026-07-17, "command flair" pass) -- see
		// CompanionChatter.h. Only chatter for the JOIN direction; leaving
		// silently matches the pet version's quiet toggle-off.
		if (joined >= left) {
			CompanionChatter::announceOrder(creature, "Squad -- fall in with the group!", "group", companions);
		}

		return SUCCESS;
	}
};

#endif // COMPANIONGROUPCOMMAND_H_
