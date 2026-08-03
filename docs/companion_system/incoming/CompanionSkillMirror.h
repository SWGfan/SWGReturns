/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-27, "skill mirror" pass) -- summon-scoped
	owner mirroring of companion ability commands.

	Every BASE ability command a *summoned* companion has genuinely learned
	(via its own learnedSkills ledger -- the same Skill::getAbilities() walk
	CompanionSpecialAttackCommand.h:82-104 and HpetCommand's
	companionHasUnlockedAbility() already treat as the authoritative unlock
	source) is mirrored onto the OWNER as the ability string
	"companion_<base>" (lowercase), which is simultaneously:

	  1. the characterAbility gate of the generated client command_table row
	     for the owner-callable "companion_<base>" command (client side is
	     produced separately by build_command_table_rows.py -- see the
	     PHASE1 sync block below), and
	  2. the registered server-side QueueCommand name of the matching
	     CompanionSkillProxyCommand instance (CommandConfigManager2.cpp).

	Grants live only while a companion that knows the base command is
	summoned:
	  - grantFor()  -- end of CompanionControlDeviceImplementation::
	                   spawnObject() success path, and again after
	                   CompanionSkillTrainer::trainSkill() success
	                   (mid-summon training).
	  - revokeFor() -- CompanionControlDeviceImplementation::storeObject(),
	                   which removes only the grants no OTHER still-summoned
	                   companion of the same owner justifies.
	(Exact one-line call sites with quoted surrounding context: see
	INTEGRATION.md shipped alongside this header.)

	Ability-grant mechanics (verified against this fork's source):
	  - PlayerObject::addAbility(Ability*, notifyClient=true)
	    (PlayerObject.idl:691, PlayerObjectImplementation.cpp:985-1017)
	    appends to the persisted abilityList AND the transient
	    activeAbilities list, and when notifyClient is true builds a
	    PlayerObjectDeltaMessage9 (startUpdate(0x0) -- the ability-list
	    slot), so the client receives the new ability LIVE, no relog
	    required. The prior "macro list" pass's in-game test
	    (docs/companion_system/NOTES.md, 2026-07-13 entry, "Test" section)
	    confirmed newly granted companion_* abilities surface on the
	    owner's command browser immediately after training.
	  - removeAbility(Ability*, true) (PlayerObjectImplementation.cpp:
	    1087-1113) removes from both lists and sends the matching delta.
	    Removal is index-based via AbilityList::find(ability); to stay
	    safe regardless of whether find() compares pointers or names, the
	    revoke path below always passes the EXACT Ability instance already
	    held in the owner's list (looked up by name from
	    ghost->getAbilityList() -- the same pointers live in both lists:
	    addAbility() adds one object to both, and the transient list is
	    rebuilt from abilityList with the same references on load,
	    PlayerObjectImplementation.cpp:119-122).

	LOCK DISCIPLINE (project iron rules):
	  - grantFor()/revokeFor() take NO lock on the owner themselves --
	    every documented call site already holds the owner's lock:
	      * spawnObject()/storeObject() run inside the callObject()
	        deferred task that takes Locker(player) FIRST, then cross-locks
	        the device and the companion against the player
	        (CompanionControlDeviceImplementation.cpp:116-145).
	      * trainSkill() runs from a SUI callback with the owner locked
	        (same context in which the existing
	        grantOwnerAbilitiesForSkill() already mutates the owner's
	        abilityList).
	    Callers MUST hold the owner's lock; the companion argument only
	    needs to be safely readable (it is locked at all three call
	    sites).
	  - The ONLY lock this helper takes itself is the per-iteration
	    cross-lock on OTHER summoned companions inside revokeFor()'s
	    still-justified scan -- Locker(other, owner), owner already locked
	    first, matching CompanionSpecialAttackCommand.h:159's identical
	    per-companion cross-lock discipline.
*/

#ifndef COMPANIONSKILLMIRROR_H_
#define COMPANIONSKILLMIRROR_H_

#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/objects/player/variables/Ability.h"
#include "server/zone/objects/player/variables/AbilityList.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/managers/skill/SkillManager.h"

class CompanionSkillMirror {
public:

	// -----------------------------------------------------------------
	// COMPANION-MIRROR-PHASE1-BEGIN  (EDITABLE BLOCK)
	//
	// Mirrored BASE command names, phase 1: 20 common low/mid-tier combat
	// specials across the ranged (rifle/carbine/pistol) and melee
	// (unarmed/1h/2h/polearm) trees -- deliberately EXCLUDING every base
	// command already mirrored by the 2026-07-13 "macro list" pass's 61
	// companion<Ability> rows (headShot3, warcry1, *Lunge1, ...), so no
	// duplicate mirrors are created.
	//
	// KEEP IN EXACT SYNC with the client generator's PHASE1 list --
	// docs/companion_system/tools/build_companion_skill_mirror.py's
	// PHASE1_BASE_COMMANDS ("EDIT PHASE LISTS HERE" block) and its
	// PHASE1_COMMANDS.txt sidecar: same names, same order, same authored
	// casing. Authored casing is kept because skills.iff's COMMANDS
	// column preserves it (CompanionSpecialAttackCommand.h:177-181);
	// everything is toLowerCase()'d at comparison/registration time.
	// -----------------------------------------------------------------
	static const Vector<String>& mirroredBaseCommands() {
		static const Vector<String> bases = [] {
			Vector<String> v;
			// rifle (headShot3 already mirrored by the macro-list pass)
			v.add("headShot1");
			v.add("headShot2");
			v.add("mindShot1");
			v.add("fullAutoSingle1");
			// carbine
			v.add("bodyShot1");
			v.add("bodyShot2");
			v.add("bodyShot3");
			// pistol
			v.add("legShot1");
			v.add("legShot2");
			v.add("legShot3");
			// aggro shouts, tier 2 (tier-1 versions already mirrored)
			v.add("warcry2");
			v.add("intimidate2");
			v.add("berserk2");
			// unarmed / melee 1h / melee 2h / polearm, tier-2 lunges
			v.add("unarmedLunge2");
			v.add("melee1hLunge2");
			v.add("melee2hLunge2");
			v.add("polearmLunge2");
			// carbine/pistol tier 2 (tier-1 versions already mirrored)
			v.add("pointBlankSingle2");
			v.add("pointBlankArea2");
			v.add("overchargeShot2");
			return v;
		}();
		return bases;
	}
	// COMPANION-MIRROR-PHASE1-END

	/** Owner-side ability string / proxy command name for a base command. */
	static String mirrorAbilityName(const String& baseCommand) {
		return "companion_" + baseCommand.toLowerCase();
	}

	/**
	 * Does this companion's own learnedSkills ledger grant the base
	 * command? Same skill -> Skill::getAbilities() walk as
	 * CompanionSpecialAttackCommand::resolveLearnedSpecial()
	 * (CompanionSpecialAttackCommand.h:82-104), the codebase's
	 * authoritative "what has this companion genuinely unlocked" source.
	 * Case-insensitive: skills.iff preserves authored casing while
	 * registered command names are lowercase.
	 */
	static bool companionGrantsBase(CompanionObject* companion, const String& baseCommand) {
		if (companion == nullptr) {
			return false;
		}

		String baseLower = baseCommand.toLowerCase();

		for (int i = 0; i < companion->getLearnedSkillCount(); ++i) {
			Skill* skill = SkillManager::instance()->getSkill(companion->getLearnedSkill(i));

			if (skill == nullptr) {
				continue;
			}

			const Vector<String>* abilities = skill->getAbilities();

			for (int j = 0; j < abilities->size(); ++j) {
				if (abilities->get(j).toLowerCase() == baseLower) {
					return true;
				}
			}
		}

		return false;
	}

	/**
	 * Grant the owner "companion_<base>" for every mirrored base command
	 * this companion's learnedSkills justify. Idempotent
	 * (hasAbility()-guarded per entry, same shape as the trainer's
	 * grantBaselineOwnerOrderAbilities()); cheap no-op when current.
	 *
	 * PRE: owner locked by the calling thread (see lock discipline in
	 * the file header); companion locked/readable.
	 */
	static void grantFor(CompanionObject* companion, CreatureObject* owner) {
		if (companion == nullptr || owner == nullptr) {
			return;
		}

		PlayerObject* ghost = owner->getPlayerObject();

		if (ghost == nullptr) {
			return;
		}

		bool grantedAny = false;
		const Vector<String>& bases = mirroredBaseCommands();

		for (int i = 0; i < bases.size(); ++i) {
			const String& base = bases.get(i);

			if (!companionGrantsBase(companion, base)) {
				continue;
			}

			String mirror = mirrorAbilityName(base);

			if (ghost->hasAbility(mirror)) {
				continue;
			}

			// notifyClient=true -> live PlayerObjectDeltaMessage9
			// (startUpdate(0x0)) so the client's ability list updates
			// without a relog (PlayerObjectImplementation.cpp:985-1017;
			// live refresh proven in-game by the 2026-07-13 macro-list
			// pass's test).
			ghost->addAbility(new Ability(mirror), true);

			grantedAny = true;
		}

		if (grantedAny) {
			owner->sendSystemMessage("New companion commands available.");
		}
	}

	/**
	 * Revoke every mirrored "companion_<base>" grant this companion was
	 * justifying, UNLESS another still-summoned companion linked to the
	 * same owner also grants the same base command -- mirrors the
	 * trainer's revokeOwnerAbilitiesForSkillIfUnused() "keep while any
	 * grantor remains" semantics, but scoped to summoned companions.
	 *
	 * PRE: owner locked by the calling thread. `companion` is the one
	 * being stored -- it is excluded from the still-justified scan both
	 * by pointer identity and (when called after despawn) by its
	 * zone==nullptr, so the hook works either side of
	 * destroyObjectFromWorld().
	 */
	static void revokeFor(CompanionObject* companion, CreatureObject* owner) {
		if (companion == nullptr || owner == nullptr) {
			return;
		}

		PlayerObject* ghost = owner->getPlayerObject();

		if (ghost == nullptr) {
			return;
		}

		const Vector<String>& bases = mirroredBaseCommands();

		for (int i = 0; i < bases.size(); ++i) {
			const String& base = bases.get(i);

			// Only touch strings THIS companion was justifying -- never
			// sweep grants it had nothing to do with.
			if (!companionGrantsBase(companion, base)) {
				continue;
			}

			String mirror = mirrorAbilityName(base);

			if (!ghost->hasAbility(mirror)) {
				continue;
			}

			if (anotherSummonedCompanionGrants(owner, companion, base)) {
				continue;
			}

			// Pass the exact held instance so
			// removeAbility()'s find() succeeds under either pointer- or
			// name-equality semantics (see file header).
			Ability* held = findHeldAbility(ghost, mirror);

			if (held != nullptr) {
				ghost->removeAbility(held, true);
			}
		}
	}

private:

	/**
	 * Any OTHER summoned, living-in-world companion linked to this owner
	 * that also grants the base command? Datapad walk identical to
	 * CompanionSpecialAttackCommand::resolveActiveCompanions()
	 * (CompanionSpecialAttackCommand.h:39-75); each candidate is
	 * cross-locked against the (already locked) owner before its ledger
	 * is read, per the project's cross-lock iron rule.
	 */
	static bool anotherSummonedCompanionGrants(CreatureObject* owner, CompanionObject* excluded, const String& baseCommand) {
		ManagedReference<SceneObject*> datapad = owner->getSlottedObject("datapad");

		if (datapad == nullptr) {
			return false;
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

			CompanionObject* other = device->getCompanionObject();

			if (other == nullptr || other == excluded || other->getZone() == nullptr) {
				continue;
			}

			if (other->getLinkedCreature().get() != owner) {
				continue;
			}

			Locker clocker(other, owner);

			if (companionGrantsBase(other, baseCommand)) {
				return true;
			}
		}

		return false;
	}

	/**
	 * The exact Ability instance the owner currently holds under this
	 * name, from the transient active list (getAbilityList(),
	 * PlayerObject.idl:1495-1497) -- the same object also sits in the
	 * persisted abilityList (addAbility() adds one instance to both;
	 * the transient list is rebuilt from abilityList with the same
	 * references on load, PlayerObjectImplementation.cpp:119-122).
	 */
	static Ability* findHeldAbility(PlayerObject* ghost, const String& abilityName) {
		auto* list = ghost->getAbilityList();

		if (list == nullptr) {
			return nullptr;
		}

		for (int i = 0; i < list->size(); ++i) {
			Ability* ability = list->get(i);

			if (ability != nullptr && ability->getAbilityName() == abilityName) {
				return ability;
			}
		}

		return nullptr;
	}
};

#endif // COMPANIONSKILLMIRROR_H_
