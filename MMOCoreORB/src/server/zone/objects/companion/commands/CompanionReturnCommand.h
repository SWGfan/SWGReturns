/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-20, "massive battlefield" pass, per user
	request) -- dedicated /companionreturn command: sends a companion back
	to whatever it was last EXPLICITLY told to hold (a /companionstay spot,
	a /companionguard-on-an-object spot, or a /companionguard-on-a-creature
	target) WITHOUT capturing a new position the way re-issuing
	/companionstay would. Companion equivalent of a rally-point recall:
	useful after a skirmish drags a posted companion off its mark, or any
	time you just want to snap a battlefield line back into shape without
	walking each companion back by hand.

	Same single-companion-or-whole-squad targeting as CompanionStayCommand.h
	(target one companion first to recall just that one; no target recalls
	every summoned companion that actually has a standing STAY/GUARD post).
	Reads CompanionObject::standingOrder/guardTarget/homeLocation -- all
	written by CompanionStayCommand.h / CompanionGuardCommand.h and left
	untouched by combat and the post-combat loot sweep -- see
	CompanionObject.idl's standingOrder doc comment and NOTES.md.

	For a STAY post: reuses the engine's own native "walk home" behavior
	(AiAgent::LEASHING -- the exact case AiAgentImplementation::setDestination()
	already uses to walk a leashed creature back to homeLocation, then
	auto-transitions to OBLIVIOUS on arrival) rather than re-implementing
	pathing with patrol points.

	For a GUARD-a-creature order: there's no fixed spot to walk back to --
	"return" just means resume guarding that target, identical to what
	endSweep() does automatically after a fight.

	A companion whose standingOrder is plain FOLLOW (never given a Stay/
	Guard order at all) has nothing to "return" to -- skipped, with a
	message only if NONE of the resolved companions had anything to do.
*/

#ifndef COMPANIONRETURNCOMMAND_H_
#define COMPANIONRETURNCOMMAND_H_

#include "server/zone/objects/creature/commands/QueueCommand.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/managers/combat/CombatManager.h"
#include "server/zone/managers/companion/CompanionChatter.h"

class CompanionReturnCommand : public QueueCommand {
public:

	CompanionReturnCommand(const String& name, ZoneProcessServer* server)
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

	/** See CompanionStayCommand.h's identical helper for the rationale. */
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

		Vector<ManagedReference<CompanionObject*>> recalled;

		for (int i = 0; i < companions.size(); ++i) {
			CompanionObject* companion = companions.get(i);

			Locker clocker(companion, creature);

			int standing = companion->getStandingOrder();

			if (standing != CompanionObject::STAY && standing != CompanionObject::GUARD) {
				// Nothing ordered -- plain FOLLOW has no post to return to.
				continue;
			}

			if (companion->isInCombat()) {
				CombatManager::instance()->attemptPeace(companion);
			}

			if (companion->isTaxiActive()) {
				companion->stopTaxiRide(false);
			}

			if (standing == CompanionObject::GUARD) {
				CreatureObject* guardTarget = companion->getGuardTarget().get();

				if (guardTarget != nullptr && guardTarget->getZone() != nullptr) {
					companion->setCompanionState(CompanionObject::GUARD);
					companion->setFollowObject(guardTarget);
					companion->setMovementState(AiAgent::FOLLOWING);
					recalled.add(companion);
					continue;
				}

				// Guarded creature is gone -- nothing to resume guarding,
				// and no fixed spot either (guard-a-creature never sets
				// homeLocation). Nothing sensible to do; skip it.
				continue;
			}

			// STAY: walk back to the stored homeLocation using the engine's
			// own native leash-home behavior instead of hand-rolled patrol
			// points -- see AiAgentImplementation::setDestination()'s
			// LEASHING case (paths to homeLocation, then auto-switches to
			// OBLIVIOUS once within 4m and marks it reached).
			companion->setCompanionState(CompanionObject::STAY);
			companion->setFollowObject(nullptr);
			companion->clearPatrolPoints();
			companion->setMovementState(AiAgent::LEASHING);
			recalled.add(companion);
		}

		if (recalled.size() == 0) {
			creature->sendSystemMessage("None of those companions have a Stay or Guard position to return to.");
			return GENERALERROR;
		}

		if (singleTarget != nullptr) {
			CompanionChatter::announceOrder(creature, singleTarget->getDisplayedName() + " -- return to your post!", "return", recalled);
		} else {
			CompanionChatter::announceOrder(creature, "Squad -- return to your posts!", "return", recalled);
		}

		return SUCCESS;
	}
};

#endif // COMPANIONRETURNCOMMAND_H_
