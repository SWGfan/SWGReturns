/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-17, "pet command port" pass, per user request)
	-- dedicated /companionrangedattack command, the companion spin on the
	real Creature Handler pet's Ranged Attack order. Companions carry real
	weapons rather than a pet's toggled special, so this is implemented as
	"covering fire": every summoned companion engages the owner's current
	target (same base "attack" pipeline as /companionattack) but KEEPS
	FOLLOWING THE OWNER instead of breaking formation to chase -- they shoot
	from their formation slots. Known v1 caveat (documented in NOTES.md):
	a companion whose equipped weapon is melee-range will mostly fail range
	checks in this mode and should be ordered with /companionattack instead.
*/

#ifndef COMPANIONRANGEDATTACKCOMMAND_H_
#define COMPANIONRANGEDATTACKCOMMAND_H_

#include "server/zone/objects/creature/commands/QueueCommand.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/managers/companion/CompanionChatter.h"

class CompanionRangedAttackCommand : public QueueCommand {
public:

	CompanionRangedAttackCommand(const String& name, ZoneProcessServer* server)
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

		ManagedReference<SceneObject*> targetObject = creature->getZoneServer()->getObject(targetID, true);

		// Companion System (2026-07-17 bug fix, per user report) -- was
		// isCreatureObject(), which silently rejected lairs/structures (a
		// mission "destroy this lair" target is a BuildingObject). See
		// CompanionAttackCommand.h's identical fix for the full rationale.
		if (targetObject == nullptr || !targetObject->isTangibleObject()) {
			creature->sendSystemMessage("@companion:no_valid_target"); // You must have a valid hostile target selected.
			return INVALIDPARAMETERS;
		}

		TangibleObject* hostileTarget = cast<TangibleObject*>(targetObject.get());

		if (hostileTarget == creature) {
			creature->sendSystemMessage("@companion:no_valid_target");
			return INVALIDPARAMETERS;
		}

		int orderedCount = 0;

		for (int i = 0; i < companions.size(); ++i) {
			CompanionObject* companion = companions.get(i);

			if (hostileTarget == companion) {
				continue;
			}

			if (!companion->isAttackableBy(hostileTarget) && !hostileTarget->isAttackableBy(companion)) {
				continue;
			}

			Locker clocker(companion, creature);

			if (companion->isDead() || companion->isIncapacitated()) {
				continue;
			}

			// Companion Taxi interrupt fix (2026-07-15) -- see
			// CompanionFollowCommand.h's identical guard.
			if (companion->isTaxiActive()) {
				companion->stopTaxiRide(false);
			}

			companion->setCompanionState(CompanionObject::ATTACK);
			companion->addDefender(hostileTarget);

			// THE deliberate difference from /companionattack: keep the owner
			// as the follow object so the companion holds its formation slot
			// and fires from position instead of charging the target.
			companion->setFollowObject(creature);

			companion->executeObjectControllerAction(STRING_HASHCODE("attack"), targetID, "");

			++orderedCount;
		}

		if (orderedCount == 0) {
			creature->sendSystemMessage("@companion:no_valid_target");
			return INVALIDPARAMETERS;
		}

		// Companion System (2026-07-17, "command flair" pass) -- see
		// CompanionChatter.h. Immediate: it's combat.
		CompanionChatter::announceOrder(creature, "Squad -- covering fire, hold your positions!", "rangedattack", companions, true);

		return SUCCESS;
	}
};

#endif // COMPANIONRANGEDATTACKCOMMAND_H_
