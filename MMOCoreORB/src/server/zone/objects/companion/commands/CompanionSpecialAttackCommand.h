/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-17, "pet command port" pass, per user request)
	-- /companionspecialone + /companionspecialtwo, the companion equivalent
	of the real Creature Handler pet's Special Attack One/Two orders
	(PetSpecialAttackCommand.h). One class registered under BOTH command
	names (the name suffix picks which learned special to use), exactly how
	the pet version distinguishes its two slots.

	"Special One" = the FIRST ability on the companion's own learned-skill
	ledger, "Special Two" = the SECOND (same skill->getAbilities() walk as
	HpetCommand::companionHasUnlockedAbility(), which is the authoritative
	source of what this companion has genuinely unlocked). A companion
	without enough learned specials falls back to the base "attack" action
	-- same graceful degradation as a stock pet that hasn't been trained its
	special attacks yet still obeying the order as a plain attack.
*/

#ifndef COMPANIONSPECIALATTACKCOMMAND_H_
#define COMPANIONSPECIALATTACKCOMMAND_H_

#include "server/zone/objects/creature/commands/QueueCommand.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/managers/skill/SkillManager.h"
#include "server/zone/managers/companion/CompanionChatter.h"

class CompanionSpecialAttackCommand : public QueueCommand {
public:

	CompanionSpecialAttackCommand(const String& name, ZoneProcessServer* server)
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

	/**
	 * The companion's specialIndex-th (0-based) learned ability, walked in
	 * ledger order -- same source of truth as HpetCommand's unlock gate.
	 * Empty string if the companion hasn't learned that many.
	 */
	String resolveLearnedSpecial(CompanionObject* companion, int specialIndex) const {
		int seen = 0;

		for (int i = 0; i < companion->getLearnedSkillCount(); ++i) {
			Skill* skill = SkillManager::instance()->getSkill(companion->getLearnedSkill(i));

			if (skill == nullptr) {
				continue;
			}

			const Vector<String>* abilities = skill->getAbilities();

			for (int j = 0; j < abilities->size(); ++j) {
				if (seen == specialIndex) {
					return abilities->get(j);
				}

				++seen;
			}
		}

		return "";
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

		// Registered under both names -- the suffix picks the slot.
		int specialIndex = getQueueCommandName().contains("two") ? 1 : 0;

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
			companion->setFollowObject(nullptr);

			// Companion System (2026-07-29 fix, per Nick: "sometimes they run
			// off and never return unless i press follow"). Same gap as
			// CompanionAttackCommand.h -- see its identical 2026-07-29 fix
			// comment for the full explanation. Arms the same post-combat
			// sweep interceptThreatToOwner() relies on, instead of leaving
			// this companion stuck at ATTACK/null-follow once the target's
			// dead and the owner never personally entered combat.
			companion->deferredStartPostCombatSweep();

			String special = resolveLearnedSpecial(companion, specialIndex);

			// The real registered QueueCommand names are lowercase (see
			// CommandConfigManager*.cpp's `String("healDamage").toLowerCase()`
			// pattern + HpetCommand.h's identical dispatch), while
			// skills.iff's COMMANDS column preserves authored casing.
			uint32 actionCRC = special.isEmpty() ? STRING_HASHCODE("attack") : special.toLowerCase().hashCode();

			companion->executeObjectControllerAction(actionCRC, targetID, "");

			++orderedCount;
		}

		if (orderedCount == 0) {
			creature->sendSystemMessage("@companion:no_valid_target");
			return INVALIDPARAMETERS;
		}

		// Companion System (2026-07-17, "command flair" pass) -- see
		// CompanionChatter.h. Immediate: it's combat.
		String orderKey = (specialIndex == 1) ? "specialtwo" : "specialone";
		CompanionChatter::announceOrder(creature, "Squad -- hit them with your specials!", orderKey, companions, true);

		return SUCCESS;
	}
};

#endif // COMPANIONSPECIALATTACKCOMMAND_H_
