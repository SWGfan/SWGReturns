/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-17, "pet command port" pass, per user request)
	-- dedicated /companionfollowother command, the companion equivalent of
	the real Creature Handler pet's Follow Other order: every summoned
	companion escorts the owner's current (non-hostile) target instead of
	the owner, holding the owner's chosen formation around that target
	(the formationOffset blackboard rotates around whatever the companion's
	followObject is -- see FormationManager.cpp).
*/

#ifndef COMPANIONFOLLOWOTHERCOMMAND_H_
#define COMPANIONFOLLOWOTHERCOMMAND_H_

#include "server/zone/objects/creature/commands/QueueCommand.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/managers/combat/CombatManager.h"
#include "server/zone/managers/companion/CompanionChatter.h"

class CompanionFollowOtherCommand : public QueueCommand {
public:

	CompanionFollowOtherCommand(const String& name, ZoneProcessServer* server)
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
			creature->sendSystemMessage("You must target the person your companions should follow.");
			return INVALIDPARAMETERS;
		}

		ManagedReference<SceneObject*> targetObject = creature->getZoneServer()->getObject(targetID, true);

		if (targetObject == nullptr || !targetObject->isCreatureObject()) {
			creature->sendSystemMessage("You must target the person your companions should follow.");
			return INVALIDPARAMETERS;
		}

		CreatureObject* followTarget = cast<CreatureObject*>(targetObject.get());

		if (followTarget == nullptr || followTarget == creature || followTarget->isAttackableBy(creature)) {
			// Escorting yourself is /companionfollow; escorting an enemy is
			// nonsense -- same friendly-only gate as PetFriendCommand.h.
			creature->sendSystemMessage("Your companions can only follow a friendly target.");
			return INVALIDPARAMETERS;
		}

		for (int i = 0; i < companions.size(); ++i) {
			CompanionObject* companion = companions.get(i);

			Locker clocker(companion, creature);

			if (companion->isInCombat()) {
				CombatManager::instance()->attemptPeace(companion);
			}

			// Companion Taxi interrupt fix (2026-07-15) -- see
			// CompanionFollowCommand.h's identical guard.
			if (companion->isTaxiActive()) {
				companion->stopTaxiRide(false);
			}

			companion->setCompanionState(CompanionObject::FOLLOW);
			companion->setFollowObject(followTarget);

			// Companion System (2026-07-20, "return to escort target after
			// the loot/harvest sweep" -- per user request): remember this as
			// the standing escort order so post-combat looting/harvesting
			// returns HERE afterward instead of always parking on the owner.
			companion->setEscortTarget(followTarget);

			// "Massive battlefield" pass, same date: FollowOther is a FOLLOW-
			// family order (see CompanionObject.idl's standingOrder doc
			// comment) and cancels any standing GUARD order.
			companion->setStandingOrder(CompanionObject::FOLLOW);
			companion->setGuardTarget(nullptr);

			// genesis port: the isResting() guard has no equivalent on this base;
			// removed. Setting the follow state unconditionally is harmless -- it
			// is exactly what the guarded body did.
			companion->setFollowState(AiAgent::FOLLOWING);
		}

		// Companion System (2026-07-17, "command flair" pass) -- see
		// CompanionChatter.h.
		CompanionChatter::announceOrder(creature, "Squad -- escort " + followTarget->getFirstName() + "!", "followother", companions);

		return SUCCESS;
	}
};

#endif // COMPANIONFOLLOWOTHERCOMMAND_H_
