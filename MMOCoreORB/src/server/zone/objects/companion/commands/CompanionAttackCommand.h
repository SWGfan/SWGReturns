/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System -- dedicated /companionattack command. The missing piece
	of "use it like a pet": the four movement commands
	(CompanionFollowCommand/CompanionStayCommand/CompanionPatrolCommand/
	CompanionStoreCommand) already let an owner move their companion around,
	and CompanionThreatObserver already makes the companion auto-intercept
	threats TO its owner, but until this command there was no way for the
	OWNER to directly order the companion onto their own current target --
	the /hpet <macro> "Direct attack pipeline" (HpetCommand.h) only works for
	named, unlocked special abilities (companionHasUnlockedAbility() gate),
	so a companion whose starting profession granted no combat abilities at
	all had no way to be told to fight anything on command. This command is
	the baseline "just attack what I'm attacking" action every real
	Creature Handler pet has (PetAttackCommand.h) regardless of trained
	tricks -- no ability-unlock gate, routes through the same base "attack"
	action CompanionObjectImplementation::interceptThreatToOwner() already
	uses for auto-defense (STRING_HASHCODE("attack")), so it reuses the
	engine's normal pool/HAM/cooldown-aware combat pipeline rather than
	reimplementing anything. See docs/companion_system/NOTES.md.
*/

#ifndef COMPANIONATTACKCOMMAND_H_
#define COMPANIONATTACKCOMMAND_H_

#include "server/zone/objects/creature/commands/QueueCommand.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/managers/companion/CompanionChatter.h"

class CompanionAttackCommand : public QueueCommand {
public:

	CompanionAttackCommand(const String& name, ZoneProcessServer* server)
		: QueueCommand(name, server) {

	}

	/**
	 * Scans the owner's datapad for a summoned, living companion linked to
	 * them. Duplicated (rather than shared) from HpetCommand's identical
	 * helper to keep each command file self-contained, matching how each
	 * individual Companion*Command resolves its own control device
	 * independently.
	 */
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

		// Accept either an explicit /companionattack <target> radial/macro
		// invocation (target arg populated by the client) or the plain
		// typed command with no target, in which case we fall back to
		// whatever the player currently has selected -- matches
		// HpetCommand's own direct-attack-pipeline behavior.
		uint64 targetID = (target != 0) ? target : creature->getTargetID();

		ManagedReference<SceneObject*> targetObject = creature->getZoneServer()->getObject(targetID, true);

		// Companion System (2026-07-17 bug fix, per user report) -- was
		// isCreatureObject(), which silently rejected lairs/structures/
		// turrets (a mission "destroy this lair" target is a BuildingObject,
		// never a CreatureObject). The real player attack pipeline gates on
		// isTangibleObject() for exactly this reason -- see
		// CombatQueueCommand.h's doCombatAction(). CreatureObject IS-A
		// TangibleObject, so ordinary creature targets still work unchanged.
		if (targetObject == nullptr || !targetObject->isTangibleObject()) {
			creature->sendSystemMessage("@companion:no_valid_target"); // You must have a valid hostile target selected.
			return INVALIDPARAMETERS;
		}

		TangibleObject* hostileTarget = cast<TangibleObject*>(targetObject.get());

		if (hostileTarget == creature) {
			creature->sendSystemMessage("@companion:no_valid_target");
			return INVALIDPARAMETERS;
		}

		// Companion System (2026-07-15, "test 5 companions at once" pass):
		// every summoned companion piles onto the same target, squad-order
		// style. Per-companion validity (hostileTarget == companion,
		// attackable checks) is now checked inside the loop instead of once
		// up front, since a target could in principle be attackable by some
		// companions and not others (e.g. faction/duel state differences).
		// A companion that fails its own check is silently skipped rather
		// than aborting the whole order -- same partial-failure precedent
		// RallyCommand.h/FormupCommand.h already establish for multi-target
		// group commands.
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

			if (companion->isDead() || companion->isIncapacitated() || companion->getCompanionState() == CompanionObject::THEATER) {
				continue;
			}

			// Companion Taxi interrupt fix (2026-07-15) -- see
			// CompanionFollowCommand.h's identical guard (orphaned vehicle
			// shell if combat is ordered mid-ride without tearing it down
			// first).
			if (companion->isTaxiActive()) {
				companion->stopTaxiRide(false);
			}

			companion->setCompanionState(CompanionObject::ATTACK);
			companion->addDefender(hostileTarget);
			companion->setFollowObject(nullptr);

			// Companion System (2026-07-29 fix, per Nick: "sometimes they run
			// off and never return unless i press follow"). Unlike
			// interceptThreatToOwner()'s auto-intercept, nothing here ever
			// armed the post-combat sweep -- it's normally armed by
			// CompanionThreatObserver watching the OWNER's own combat
			// events, which never fire if the owner just orders the
			// companion onto a target without taking damage/fighting
			// personally. Arming it directly here means the same sweep
			// that already restores FOLLOW/STAY/GUARD once the fight ends
			// covers this order too, instead of leaving the companion
			// stuck at ATTACK/null-follow forever.
			companion->deferredStartPostCombatSweep();

			// Same base "attack" action CompanionObjectImplementation::
			// interceptThreatToOwner() already uses for auto-defense -- routes
			// through the engine's standard combat pipeline (pool/HAM
			// consumption, weapon cooldowns) rather than a bespoke one, and
			// requires no trained/unlocked special ability, matching how a
			// stock Creature Handler pet can always be told to attack even
			// before it's learned any tricks.
			companion->executeObjectControllerAction(STRING_HASHCODE("attack"), targetID, "");

			++orderedCount;
		}

		if (orderedCount == 0) {
			creature->sendSystemMessage("@companion:no_valid_target");
			return INVALIDPARAMETERS;
		}

		// Companion System (2026-07-17, "command flair" pass) -- see
		// CompanionChatter.h. immediateReplies: mid-combat, a half-second
		// stagger reads as lag rather than drama.
		CompanionChatter::announceOrder(creature, "Squad -- take that target down!", "attack", companions, true);

		return SUCCESS;
	}
};

#endif // COMPANIONATTACKCOMMAND_H_
