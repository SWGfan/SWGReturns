/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System -- spec 4B ("Unified Control Syntax (/hpet) & Direct
	Attack Pipeline") and 4C ("Multi-Entity Coordination Mechanics / Form Up
	Formations"). Modeled on
	server/zone/objects/creature/commands/TellpetCommand.h (the equivalent
	unified control command for Creature Handler pets), but resolves its
	target companion entirely without touching PlayerObject's active-pet
	list -- see docs/companion_system/NOTES.md, "Active companion
	resolution".
*/

#ifndef HPETCOMMAND_H_
#define HPETCOMMAND_H_

#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/managers/companion/CompanionSkillTrainer.h"
#include "server/zone/managers/companion/FormationManager.h"
#include "server/zone/managers/companion/CampDeploymentManager.h"
#include "server/zone/managers/skill/SkillManager.h"
#include "server/zone/objects/creature/variables/Skill.h"

class HpetCommand : public QueueCommand {
public:

	HpetCommand(const String& name, ZoneProcessServer* server)
		: QueueCommand(name, server) {

	}

	/**
	 * Scans the owner's datapad for a summoned, living companion linked to
	 * them. Deliberately does not use PlayerObject::getActivePet() (that list
	 * belongs to the isolated Creature Handler pet system).
	 */
	CompanionObject* resolveActiveCompanion(CreatureObject* player) const {
		if (player == nullptr) {
			return nullptr;
		}

		ManagedReference<SceneObject*> datapad = player->getSlottedObject("datapad");

		if (datapad == nullptr) {
			return nullptr;
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

			return companion;
		}

		return nullptr;
	}

	bool companionHasUnlockedAbility(CompanionObject* companion, const String& macro) const {
		// Companion System (2026-07-12) bug fix: `macro` here is always
		// already-lowercased by the caller (doQueueCommand() below lowercases
		// the whole argument string before ever splitting out a subcommand),
		// but skills.iff's COMMANDS column -- and therefore
		// Skill::getAbilities() -- preserves each ability's real authored
		// casing (e.g. "healDamage", "harvestCorpse", "polearmLunge1").
		// Vector<String>::contains() is a case-sensitive element compare
		// (String::operator==), so the old `abilities->contains(macro)` could
		// never match any mixed-case ability -- a player typing exactly what
		// CompanionSkillTrainer::sendHelpSheet() shows them (necessarily
		// lowercase, since chat input is lowercased before it ever reaches
		// here) would always be told "Your companion has not learned that
		// ability," even for an ability it genuinely had. The real registered
		// QueueCommand names are lowercase regardless (see
		// CommandConfigManager*.cpp's `String("healDamage").toLowerCase()`
		// pattern), so the fix is a case-insensitive compare here, not a
		// case change to the dispatch hash below.
		for (int i = 0; i < companion->getLearnedSkillCount(); ++i) {
			Skill* skill = SkillManager::instance()->getSkill(companion->getLearnedSkill(i));

			if (skill == nullptr) {
				continue;
			}

			// Skill::commands is protected; getAbilities() is the established
			// public accessor for this exact vector (see SkillManager.cpp's
			// own addAbilities() call sites).
			const Vector<String>* abilities = skill->getAbilities();

			for (int j = 0; j < abilities->size(); ++j) {
				if (abilities->get(j).toLowerCase() == macro) {
					return true;
				}
			}
		}

		return false;
	}

	int doQueueCommand(CreatureObject* creature, const uint64& target, const UnicodeString& arguments) const {
		if (!checkStateMask(creature)) {
			return INVALIDSTATE;
		}

		if (!checkInvalidLocomotions(creature)) {
			return INVALIDLOCOMOTION;
		}

		ManagedReference<PlayerObject*> ghost = creature->getPlayerObject();

		if (ghost == nullptr) {
			return GENERALERROR;
		}

		ManagedReference<CompanionObject*> companion = resolveActiveCompanion(creature);

		if (companion == nullptr) {
			creature->sendSystemMessage("@companion:no_active_companion"); // You have no active companion.
			return GENERALERROR;
		}

		Locker clocker(companion, creature);

		String args = arguments.toString().trim().toLowerCase();

		if (args.isEmpty()) {
			// A bare /hpet opens the full dialogue/options menu (spec 4D)
			// rather than jumping straight to the help sheet -- Contextual
			// Help is still one click away as option 6 of that menu
			// (CompanionDialogMenuSuiCallback), and this is also the only
			// way the dialogue menu is reachable outside of targeting a
			// live, non-incapacitated companion and using its radial menu.
			CompanionSkillTrainer::instance()->sendDialogMenu(creature, companion);
			return SUCCESS;
		}

		int firstSpace = args.indexOf(' ');
		String subcommand = (firstSpace < 0) ? args : args.subString(0, firstSpace);
		String remainder = (firstSpace < 0) ? "" : args.subString(firstSpace + 1).trim();

		// --- /hpet formup <line|wedge|box> (spec 4C) ---
		if (subcommand == "formup") {
			if (remainder != "line" && remainder != "wedge" && remainder != "box") {
				CompanionSkillTrainer::instance()->sendHelpSheet(creature, companion);
				return INVALIDPARAMETERS;
			}

			FormationManager::instance()->formUp(creature, remainder);
			return SUCCESS;
		}

		// --- /hpet camp (spec 4E, reachable via the unified syntax too) ---
		if (subcommand == "camp") {
			CampDeploymentManager::instance()->deployCamp(creature, companion);
			return SUCCESS;
		}

		// --- /hpet help ---
		if (subcommand == "help") {
			CompanionSkillTrainer::instance()->sendHelpSheet(creature, companion);
			return SUCCESS;
		}

		// --- /hpet attack (baseline, no trained ability required) ---
		// Real gap found in the first in-game pass: the direct-attack
		// pipeline below only works for a NAMED, unlocked special ability,
		// so a companion whose starting profession granted no combat
		// abilities at all had no way to be ordered to fight anything.
		// "attack" is special-cased here, bypassing
		// companionHasUnlockedAbility() entirely and routing through the
		// same base "attack" action CompanionObjectImplementation::
		// interceptThreatToOwner() already uses for auto-defense -- see
		// CompanionAttackCommand.h (the equivalent dedicated
		// /companionattack command; this is the /hpet unified-syntax
		// alias for the same logic).
		if (subcommand == "attack") {
			uint64 targetID = creature->getTargetID();

			ManagedReference<SceneObject*> targetObject = creature->getZoneServer()->getObject(targetID, true);

			if (targetObject == nullptr || !targetObject->isCreatureObject()) {
				creature->sendSystemMessage("@companion:no_valid_target"); // You must have a valid hostile target selected.
				return INVALIDPARAMETERS;
			}

			CreatureObject* hostileTarget = cast<CreatureObject*>(targetObject.get());

			if (hostileTarget == companion.get() || hostileTarget == creature) {
				creature->sendSystemMessage("@companion:no_valid_target");
				return INVALIDPARAMETERS;
			}

			if (!companion->isAttackableBy(hostileTarget) && !hostileTarget->isAttackableBy(companion)) {
				creature->sendSystemMessage("@companion:no_valid_target");
				return INVALIDPARAMETERS;
			}

			if (companion->isDead() || companion->isIncapacitated()) {
				return GENERALERROR;
			}

			companion->setCompanionState(CompanionObject::ATTACK);
			companion->addDefender(hostileTarget);
			companion->setFollowObject(nullptr);
			companion->executeObjectControllerAction(STRING_HASHCODE("attack"), targetID, "");

			return SUCCESS;
		}

		// --- Direct pipeline for named special abilities (spec 4B) ---
		// Bug fix: this used to hard-require a hostile target for EVERY
		// named ability, which would silently block any heal/buff-type
		// ability a companion's starting profession might grant (e.g.
		// science_medic_novice's heal commands) -- a heal was never
		// attackable by/to its target, so it always failed the old check
		// before ever reaching the real ability. Fixed by not
		// pre-validating hostility at all here: executeObjectControllerAction()
		// routes through ObjectController::activateCommand(), the exact
		// same dispatch a player's own typed command goes through, so the
		// REAL registered command for `macro` already has its own correct
		// target-type validation (a heal command accepts a friendly
		// target, an attack command requires a hostile one) -- we just
		// need to get it a target, not second-guess what kind it should
		// be. Only the ATTACK companionState/addDefender bookkeeping below
		// is conditional on hostility, so a heal aimed at the owner/self
		// doesn't incorrectly flag the companion as attacking its own
		// patient.
		String macro = subcommand;

		if (!companionHasUnlockedAbility(companion, macro)) {
			creature->sendSystemMessage("@companion:ability_locked"); // Your companion has not learned that ability.
			return INVALIDPARAMETERS;
		}

		uint64 targetID = creature->getTargetID();

		if (targetID == 0) {
			// No target selected -- valid for self-heals/buffs, so default
			// to the companion targeting itself rather than hard-failing
			// the way a required-target attack would.
			targetID = companion->getObjectID();
		}

		ManagedReference<SceneObject*> targetObject = creature->getZoneServer()->getObject(targetID, true);

		if (targetObject == nullptr || !targetObject->isCreatureObject()) {
			creature->sendSystemMessage("@companion:no_valid_target"); // You must have a valid target selected.
			return INVALIDPARAMETERS;
		}

		CreatureObject* abilityTarget = cast<CreatureObject*>(targetObject.get());

		if (companion->isAttackableBy(abilityTarget) || abilityTarget->isAttackableBy(companion)) {
			companion->setCompanionState(CompanionObject::ATTACK);
			companion->addDefender(abilityTarget);
		}

		// Interrupts the companion's current auto-attack/idle loop and
		// forces execution of the named ability against the target --
		// routed through the standard combat pipeline so pool/HAM
		// consumption and cooldown timers are handled by the same code a
		// player-issued command would use (spec 4B, step 3).
		companion->executeObjectControllerAction(macro.hashCode(), targetID, "");

		return SUCCESS;
	}

};

#endif // HPETCOMMAND_H_
