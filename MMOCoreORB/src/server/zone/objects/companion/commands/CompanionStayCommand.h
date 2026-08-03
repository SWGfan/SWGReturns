/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System -- dedicated /companionstay command. Modeled on
	server/zone/objects/creature/commands/pet/PetStayCommand.h -- see
	CompanionFollowCommand.h and docs/companion_system/NOTES.md.

	2026-07-20 ("massive battlefield" pass, per user request) -- single-
	companion targeting: target one of your own companions first (or just
	click it), then /companionstay posts JUST that one at its current spot
	instead of the whole squad -- lets you walk each companion to its own
	position and hold the line individually. No target (or a target that
	isn't your own companion) falls back to the original whole-squad
	behavior, unchanged. Also now records CompanionObject::STAY as the
	companion's standingOrder (see CompanionObject.idl's doc comment) so
	combat interruptions and the post-combat loot sweep know to send it
	back here afterward instead of defaulting to FOLLOW -- see NOTES.md.
*/

#ifndef COMPANIONSTAYCOMMAND_H_
#define COMPANIONSTAYCOMMAND_H_

#include "server/zone/objects/creature/commands/QueueCommand.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/managers/combat/CombatManager.h"
#include "server/zone/managers/companion/CompanionChatter.h"

class CompanionStayCommand : public QueueCommand {
public:

	CompanionStayCommand(const String& name, ZoneProcessServer* server)
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

	/**
	 * Companion System (2026-07-20, "massive battlefield" pass) -- if the
	 * resolved target (explicit command target, falling back to the
	 * player's current screen target) is one of THIS player's own living
	 * companions, returns it alone; otherwise returns nullptr so the
	 * caller falls back to the whole-squad behavior. Same helper shape
	 * reused by CompanionGuardCommand.h.
	 */
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

		Vector<ManagedReference<CompanionObject*>> companions;

		CompanionObject* singleTarget = resolveSingleTargetedCompanion(creature, target);

		if (singleTarget != nullptr) {
			companions.add(singleTarget);
		} else {
			resolveActiveCompanions(creature, companions);
		}

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
			// CompanionFollowCommand.h's identical guard for the full
			// explanation (orphaned vehicle shell / runaway updateTaxiTick()
			// if a movement command clobbers state mid-ride without tearing
			// the ride down first).
			if (companion->isTaxiActive()) {
				companion->stopTaxiRide(false);
			}

			Vector3 home = companion->getWorldPosition();
			companion->setHomeLocation(home.getX(), home.getZ(), home.getY());

			companion->setCompanionState(CompanionObject::STAY);
			companion->setFollowObject(nullptr);
			companion->setOblivious();

			// Companion System (2026-07-20, "massive battlefield" pass) --
			// record this as the standing order (see CompanionObject.idl)
			// and clear any other order's leftover target data, so combat
			// interruptions and the post-combat sweep know to send this
			// companion back HERE afterward instead of defaulting to FOLLOW.
			companion->setStandingOrder(CompanionObject::STAY);
			companion->setEscortTarget(nullptr);
			companion->setGuardTarget(nullptr);
		}

		// Companion System (2026-07-17, "command flair" pass) -- see
		// CompanionChatter.h. (2026-07-20: a single targeted companion gets
		// its own line instead of the "Squad --" chorus.)
		if (singleTarget != nullptr) {
			CompanionChatter::announceOrder(creature, singleTarget->getDisplayedName() + " -- hold position!", "stay", companions);
		} else {
			CompanionChatter::announceOrder(creature, "Squad -- hold position!", "stay", companions);
		}

		return SUCCESS;
	}
};

#endif // COMPANIONSTAYCOMMAND_H_
