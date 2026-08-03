/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-17, "pet command port" pass, per user request)
	-- dedicated /companionguard command, the companion equivalent of the
	real Creature Handler pet's Guard order (PetGuardCommand.h): every
	summoned companion escorts a guarded target -- the owner's current
	FRIENDLY target if one is selected, otherwise the owner themselves --
	holding the owner's chosen formation around them. The existing
	owner-threat auto-defense (CompanionThreatObserver ->
	interceptThreatToOwner()) keeps protecting the OWNER regardless; true
	arbitrary-target threat observation is future work (see NOTES.md).

	2026-07-20 ("massive battlefield" pass, per user request): deliberately
	did NOT get /companionstay's new single-companion targeting (see
	CompanionStayCommand.h) -- Guard's target parameter already means
	"what/who the WHOLE squad guards," and a companion is itself a valid
	creature target, so reusing the same slot for "which companion gets
	this order" would collide with "guard this other companion of mine."
	Posting individual companions at individual spots is /companionstay's
	job; Guard stays whole-squad. Now records standingOrder (STAY for the
	guard-a-spot branch, since it already reuses STAY's homeLocation
	posting under the hood; GUARD + guardTarget for the guard-a-creature
	branch) so combat interruptions and the post-combat sweep can resume
	the right posture afterward -- see CompanionObject.idl and NOTES.md.
*/

#ifndef COMPANIONGUARDCOMMAND_H_
#define COMPANIONGUARDCOMMAND_H_

#include "server/zone/objects/creature/commands/QueueCommand.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/objects/cell/CellObject.h"
#include "server/zone/managers/combat/CombatManager.h"
#include "server/zone/managers/companion/CompanionChatter.h"

class CompanionGuardCommand : public QueueCommand {
public:

	CompanionGuardCommand(const String& name, ZoneProcessServer* server)
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

		// Guard the owner's current FRIENDLY target if valid, otherwise
		// guard the owner -- exactly PetGuardCommand.h's target resolution.
		// Companion System (2026-07-19, "guard objects, like a chair" per
		// user request): a NON-creature target now works too -- the squad
		// walks to the object and POSTS there (STAY anchored at its spot),
		// holding that position until re-ordered. The homeLocation anchor
		// plus OBLIVIOUS movement is what walks them there: the engine's
		// own path-toward-home logic (the exact behavior the follow fixes
		// had to defeat -- here it's precisely what we want).
		uint64 targetID = (target != 0) ? target : creature->getTargetID();

		ManagedReference<CreatureObject*> guardTarget = nullptr;
		ManagedReference<SceneObject*> guardSpot = nullptr;

		if (targetID != 0) {
			ManagedReference<SceneObject*> targetObject = creature->getZoneServer()->getObject(targetID, true);

			if (targetObject != nullptr && targetObject->isCreatureObject()) {
				CreatureObject* targetCreature = cast<CreatureObject*>(targetObject.get());

				if (targetCreature != nullptr && targetCreature != creature && !targetCreature->isAttackableBy(creature)) {
					guardTarget = targetCreature;
				}
			} else if (targetObject != nullptr && targetObject->getZone() != nullptr && targetObject->getZone() == creature->getZone()) {
				guardSpot = targetObject;
			}
		}

		if (guardTarget == nullptr && guardSpot == nullptr) {
			guardTarget = creature;
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

			if (guardSpot != nullptr) {
				// Post at the object: small per-companion ring offset so a
				// squad doesn't stack on one spot.
				Vector3 spot = guardSpot->getWorldPosition();
				float angle = (6.2832f * i) / companions.size();
				float offsetX = spot.getX() + cos(angle) * 2.0f;
				float offsetY = spot.getY() + sin(angle) * 2.0f;

				ManagedReference<CellObject*> spotCell = guardSpot->getParent().get().castTo<CellObject*>();

				companion->setCompanionState(CompanionObject::STAY);
				companion->setFollowObject(nullptr);
				companion->clearPatrolPoints();
				companion->setHomeLocation(offsetX, spot.getZ(), offsetY, spotCell);
				companion->setOblivious();

				companion->setStandingOrder(CompanionObject::STAY);
				companion->setEscortTarget(nullptr);
				companion->setGuardTarget(nullptr);
			} else {
				companion->setCompanionState(CompanionObject::GUARD);
				companion->setFollowObject(guardTarget);

				// genesis port: the isResting() guard has no equivalent on this base;
				// removed. Setting the follow state unconditionally is harmless -- it
				// is exactly what the guarded body did.
				companion->setFollowState(AiAgent::FOLLOWING);

				companion->setStandingOrder(CompanionObject::GUARD);
				companion->setGuardTarget(guardTarget);
				companion->setEscortTarget(nullptr);
			}
		}

		String targetName;

		if (guardSpot != nullptr) {
			targetName = "the " + guardSpot->getDisplayedName();
		} else {
			targetName = (guardTarget == creature) ? "me" : guardTarget->getFirstName();
		}

		// Companion System (2026-07-17, "command flair" pass) -- see
		// CompanionChatter.h.
		CompanionChatter::announceOrder(creature, "Squad -- guard " + targetName + "!", "guard", companions);

		return SUCCESS;
	}
};

#endif // COMPANIONGUARDCOMMAND_H_
