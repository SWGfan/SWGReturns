/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System -- dedicated /companionfollow command. Modeled on
	server/zone/objects/creature/commands/pet/PetFollowCommand.h (the
	equivalent Creature Handler pet command), but resolves the caller's
	companion the same isolated way HpetCommand does (scanning the datapad
	for a CompanionControlDevice, never touching PlayerObject's active-pet
	list) -- see docs/companion_system/NOTES.md. Kept as a genuinely separate
	command class/name rather than routed through /hpet, per design
	direction: companion movement commands should mirror the real Creature
	Handler pet commands (/petfollow, /petstay, etc.) as their own isolated
	custom companion equivalents.
*/

#ifndef COMPANIONFOLLOWCOMMAND_H_
#define COMPANIONFOLLOWCOMMAND_H_

#include "server/zone/objects/creature/commands/QueueCommand.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/managers/combat/CombatManager.h"
#include "server/zone/managers/companion/FormationManager.h"
#include "server/zone/managers/companion/CompanionChatter.h"

class CompanionFollowCommand : public QueueCommand {
public:

	CompanionFollowCommand(const String& name, ZoneProcessServer* server)
		: QueueCommand(name, server) {

	}

	/**
	 * Companion System (2026-07-15, "test 5 companions at once" pass -- see
	 * NOTES.md). Scans the owner's datapad for EVERY summoned, living
	 * companion linked to them -- was resolveActiveCompanion() (singular,
	 * returned the first match only) before the user asked for movement/
	 * order commands to control every summoned companion at once, squad-
	 * order style (matching how FormationManager::formUp() already iterates
	 * the whole datapad). Duplicated (rather than shared) from HpetCommand's
	 * identical helper to keep each command file self-contained, matching
	 * how each individual Pet*Command resolves its own control device
	 * independently.
	 */
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
				// 2026-07-29 night #3 FIX (forcePeace fallback -- "press follow, they run back"): attemptPeace()
				// deliberately refuses to clear combat state if the companion's
				// current threat is still in range and still has this companion
				// as its mainDefender (CombatManager.cpp) -- this command used to
				// ignore that failure and set followObject(owner) anyway, which
				// AiAgentImplementation::setDefender() then silently overwrote
				// back onto the still-active threat on the very next AI tick.
				// Fall back to the same unconditional forcePeace() this codebase
				// already uses elsewhere for stuck-combat-state recovery (see
				// CompanionObjectImplementation.cpp's combatStuckPollCount fix) --
				// safe here specifically because this is a deliberate, explicit
				// owner recall order, not automatic disengagement.
				if (!CombatManager::instance()->attemptPeace(companion)) {
					CombatManager::instance()->forcePeace(companion);
				}
			}

			// Companion Taxi interrupt fix (2026-07-15, see NOTES.md "vehicle
			// left behind" bug): a manual movement command mid-ride used to
			// clobber the companion's state without ever calling
			// stopTaxiRide() -- the cosmetic vehicle shell was orphaned in the
			// world (never despawned) and updateTaxiTick() kept firing,
			// mirroring it onto wherever the companion wandered next since
			// taxiActive never got cleared. Tear the ride down first, same
			// shape as the isInCombat() peace-out just above.
			if (companion->isTaxiActive()) {
				companion->stopTaxiRide(false);
			}

			companion->setCompanionState(CompanionObject::FOLLOW);
			companion->setFollowObject(creature);

			// Companion System (2026-07-20, "return to escort target after
			// the loot/harvest sweep" -- per user request): an explicit
			// "follow me" cancels any standing /companionfollowother escort
			// order -- back onto the owner by default from here on.
			companion->setEscortTarget(nullptr);

			// genesis port: the isResting() guard has no equivalent on this base;
			// removed. Setting the follow state unconditionally is harmless -- it
			// is exactly what the guarded body did.
			companion->setFollowState(AiAgent::FOLLOWING);
		}

		// Companion System (2026-07-17, "militant formations" pass) -- a
		// plain "follow me" keeps the owner's last-chosen formation spacing
		// (blackboard offsets, no snap-teleport) instead of collapsing the
		// whole squad into a stack on the owner's position.
		FormationManager::instance()->applyFormationOffsets(creature);

		// Companion System (2026-07-17, "command flair" pass) -- squad-radio
		// chatter; see CompanionChatter.h.
		CompanionChatter::announceOrder(creature, "Squad -- on me, let's move!", "follow", companions);

		return SUCCESS;
	}
};

#endif // COMPANIONFOLLOWCOMMAND_H_
