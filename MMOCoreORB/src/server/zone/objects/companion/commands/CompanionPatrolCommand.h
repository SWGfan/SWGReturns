/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System -- dedicated /companionpatrol command. Modeled on
	server/zone/objects/creature/commands/pet/PetPatrolCommand.h, but
	simplified: the real pet patrol command records a list of waypoints via
	PetControlDevice::getPatrolPointSize()/getPatrolPoint(), a full
	waypoint-recording system CompanionControlDevice does not have. Rather
	than build that out, this command just flips the companion into PATROL
	state (identical to CompanionDialogMenuSuiCallback.h's case 2, which is
	the proven-working reference for this behavior) -- see
	CompanionFollowCommand.h and docs/companion_system/NOTES.md.
*/

#ifndef COMPANIONPATROLCOMMAND_H_
#define COMPANIONPATROLCOMMAND_H_

#include "server/zone/objects/creature/commands/QueueCommand.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/managers/combat/CombatManager.h"
#include "server/zone/managers/companion/CompanionChatter.h"

class CompanionPatrolCommand : public QueueCommand {
public:

	CompanionPatrolCommand(const String& name, ZoneProcessServer* server)
		: QueueCommand(name, server) {

	}

	/** Companion System (2026-07-15, "test 5 companions at once" pass) --
	 * resolves EVERY summoned, living companion, not just the first --
	 * see CompanionFollowCommand.h's identical helper for the full
	 * rationale. */
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

		for (int i = 0; i < companions.size(); ++i) {
			CompanionObject* companion = companions.get(i);

			Locker clocker(companion, creature);

			if (companion->isInCombat()) {
				CombatManager::instance()->attemptPeace(companion);
			}

			// Companion Taxi interrupt fix (2026-07-15) -- see
			// CompanionFollowCommand.h's identical guard. Notable here
			// specifically: a taxi ride ALSO uses CompanionObject::PATROL
			// internally, so this guard still matters even when the target
			// state matches -- a manual /companionpatrol mid-ride is the
			// player asking for a normal free-roam patrol, not the ride's own
			// internal PATROL usage, and must still tear the ride down (stop
			// the vehicle, restore speed, clear the ride-specific patrol
			// point) before starting a real one.
			if (companion->isTaxiActive()) {
				companion->stopTaxiRide(false);
			}

			companion->setCompanionState(CompanionObject::PATROL);
			companion->setFollowObject(nullptr);

			// genesis port: the isResting() guard has no equivalent on this base;
			// removed. Setting the follow state unconditionally is harmless -- it
			// is exactly what the guarded body did.
			companion->setFollowState(AiAgent::PATROLLING);
		}

		// Companion System (2026-07-17, "command flair" pass) -- see
		// CompanionChatter.h.
		CompanionChatter::announceOrder(creature, "Squad -- patrol the area!", "patrol", companions);

		return SUCCESS;
	}
};

#endif // COMPANIONPATROLCOMMAND_H_
