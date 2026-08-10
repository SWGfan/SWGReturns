/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-13, "macro list" pass) -- generic dispatcher for
	every *learned-skill-granted* companion ability command (as opposed to the
	five always-available baseline order commands -- CompanionFollowCommand /
	CompanionStayCommand / CompanionPatrolCommand / CompanionStoreCommand /
	CompanionAttackCommand -- which each get their own dedicated class since
	they touch companion movement/AI state directly rather than just
	forwarding to the engine's per-ability combat action dispatch).

	One instance of this SAME class is registered under many different
	"companion<AbilityName>" command names (see CommandConfigManager2.cpp's
	registerCommands2(), and docs/companion_system/NOTES.md's "macro list"
	pass for the full generated list) -- e.g. "companionbodyshot1",
	"companionheadshot3", "companionapplypoison", etc. Rather than hand-write
	one class per ability (there are dozens across the 11 supported combat
	professions -- see resolveProfessionToken() in CompanionSkillTrainer),
	this class derives which underlying real ability action to invoke
	*dynamically*, at doQueueCommand() time, straight from its own registered
	command name: it strips the leading "companion" prefix (added purely to
	give the owner-facing macro/hotbar command a distinct, non-colliding
	name) and hashes what's left -- which, by construction (see the
	"companion" + ability generation step in
	docs/companion_system/tools/build_command_table_rows.py), is always
	exactly the same lowercase string the real ability's own QueueCommand was
	registered under (e.g. CommandConfigManager2.cpp:
	"commandFactory.registerCommand<BodyShot1Command>(String("bodyShot1").
	toLowerCase())"). Reusing that same lowercase hash to call
	executeObjectControllerAction() on the COMPANION (not the owner) means
	the companion runs through the engine's real, already-implemented
	per-ability combat pipeline (pool/HAM costs, cooldowns, damage, animation)
	exactly as CompanionAttackCommand.h already does for the base "attack"
	action -- no ability-specific logic is reimplemented here.

	Gating: unlike CompanionAttackCommand/CompanionFollowCommand/etc. (always
	available, no ability-unlock gate, matching how a stock Creature Handler
	pet can always be told to move/attack), these per-skill ability commands
	rely entirely on the command_table.iff "characterAbility" column set on
	each generated companion<Ability> row (characterAbility =
	"companion_<ability>") plus PlayerObject::hasAbility() -- the SAME
	central gate the real engine already uses for every player combat
	ability (see ObjectControllerImplementation::activateAbility(),
	checked before doQueueCommand() ever runs). CompanionSkillTrainer::
	trainSkill()/untrainSkill() is the only code that grants/revokes
	"companion_<ability>" on the OWNER's own PlayerObject::abilityList (see
	CompanionSkillTrainer.cpp, "macro list" pass) -- mirroring, at the
	owner-ability level, the same isolation principle already used for
	companion skills/badges: this class does not itself check
	companion->hasLearnedSkill() at all, exactly as a real ability command
	doesn't re-check the player's skill boxes -- the engine's central
	characterAbility gate is the single source of truth.
*/

#ifndef COMPANIONABILITYCOMMAND_H_
#define COMPANIONABILITYCOMMAND_H_

#include "server/zone/objects/creature/commands/QueueCommand.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"

class CompanionAbilityCommand : public QueueCommand {
public:

	CompanionAbilityCommand(const String& name, ZoneProcessServer* server)
		: QueueCommand(name, server) {

	}

	/**
	 * Companion System (2026-07-15, "test 5 companions at once" pass) --
	 * resolves EVERY summoned, living companion, not just the first --
	 * see CompanionFollowCommand.h's identical helper for the full
	 * rationale. Duplicated (rather than shared) from the other
	 * Companion*Command helpers to keep each command file self-contained.
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

	/**
	 * Strips the "companion" prefix this command's own registered name was
	 * generated with (see build_command_table_rows.py's
	 * make_companion_ability_command()), e.g. "companionbodyshot1" ->
	 * "bodyshot1". getQueueCommandName() is already lowercase (the factory
	 * key every command is registered under), so no further case handling
	 * is needed before hashing it against the real ability's own
	 * STRING_HASHCODE()-based action dispatch.
	 */
	String resolveUnderlyingAbilityName() const {
		String cmdName = getQueueCommandName();

		if (cmdName.beginsWith("companion")) {
			return cmdName.subString(9);
		}

		return cmdName;
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

		String abilityName = resolveUnderlyingAbilityName();

		if (abilityName.isEmpty()) {
			return GENERALERROR;
		}

		// Same target-resolution fallback as CompanionAttackCommand.h: accept
		// an explicit target (radial/macro invocation) or fall back to the
		// player's own current target -- most of these are combat abilities
		// that require one, a handful (e.g. healMind) are self/ally-targeted
		// and tolerate 0.
		uint64 targetID = (target != 0) ? target : creature->getTargetID();
		int abilityHash = abilityName.toLowerCase().hashCode();
		String argsString = arguments.toString();

		// Companion System (2026-07-15, "test 5 companions at once" pass):
		// every summoned companion performs this ability at once, squad-
		// order style -- matches CompanionFollowCommand.h/
		// CompanionAttackCommand.h's identical treatment. A dead/
		// incapacitated companion is silently skipped rather than aborting
		// the whole order for the others.
		for (int i = 0; i < companions.size(); ++i) {
			CompanionObject* companion = companions.get(i);

			if (companion->isDead() || companion->isIncapacitated()) {
				continue;
			}

			Locker clocker(companion, creature);

			// Bring the companion into an attacking posture for combat abilities
			// invoked this way, mirroring how CompanionAttackCommand primes
			// state before dispatch -- harmless no-op for non-combat abilities.
			if (targetID != 0) {
				ManagedReference<SceneObject*> targetObject = creature->getZoneServer()->getObject(targetID, true);

				if (targetObject != nullptr && targetObject->isCreatureObject()) {
					CreatureObject* hostileTarget = cast<CreatureObject*>(targetObject.get());

					if (hostileTarget != companion && hostileTarget != creature) {
						companion->addDefender(hostileTarget);
					}
				}
			}

			// Companion System (2026-08-10, per Nick: "companion is not
			// gathering loot if i dont attack"). Same gap CompanionAttackCommand.h
			// already hit and fixed 2026-07-29 (see its identical comment for the
			// full rationale), never propagated to this dispatcher: the
			// post-combat loot sweep is normally armed by CompanionThreatObserver
			// watching the OWNER's own combat events, which never fire if the
			// owner only issues ability orders (companionlegshot1, etc.) without
			// personally taking damage or attacking. Given every one of the 203
			// companion mirror abilities routes through this one class, this was
			// very likely THE primary way loot silently went ungathered --
			// confirmed live: ordering ability attacks and never engaging
			// personally left kills unlooted. Armed here, once per companion per
			// order (idempotent no-op via CompanionObject::isLootSweepActive()
			// if already armed/sweeping), same as CompanionAttackCommand.h's
			// own placement.
			companion->deferredStartPostCombatSweep();

			// Reuses the engine's real, already-implemented per-ability combat
			// pipeline (pool/HAM costs, cooldowns, damage, animation) for
			// whatever object this hashed action name resolves to -- exactly the
			// same mechanism CompanionAttackCommand.h already uses for "attack".
			companion->executeObjectControllerAction(abilityHash, targetID, argsString);
		}

		return SUCCESS;
	}
};

#endif // COMPANIONABILITYCOMMAND_H_
