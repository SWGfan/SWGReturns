/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System -- spec 3A ("Leveling, Skill Trees, and Gated Gearing"),
	3B ("Unachievable Skill Box Auto-Grant Bypass"), 3C ("Jedi Transformation
	requirements"), and 4D ("Dialogue Interface & Contextual Help Sheet").

	CompanionSkillTrainer is a self-contained singleton (mirrors
	server/zone/managers/skill/SkillManager.h's shape) that is the ONLY code
	path allowed to call CompanionObject::grantSkill()/removeSkill(). It never
	touches the real SkillManager, PlayerObject skill boxes, or player skill
	points -- see docs/companion_system/NOTES.md, "Skill point isolation".
*/

#ifndef COMPANIONSKILLTRAINER_H_
#define COMPANIONSKILLTRAINER_H_

#include "engine/engine.h"

namespace server {
namespace zone {
namespace objects {
namespace creature {
	class CreatureObject;
}
namespace companion {
	class CompanionObject;
}
}
}
}

using namespace server::zone::objects::creature;
using namespace server::zone::objects::companion;

namespace server {
namespace zone {
namespace managers {
namespace companion {

class CompanionSkillTrainer : public Singleton<CompanionSkillTrainer>, public Logger, public Object {

	/** Non-combat xp types a companion can never accrue on its own (spec 3B) --
	 * any skill box gated behind one of these is auto-granted for free. */
	Vector<String> autoGrantXpTypes;

	/** Mission/terminal-gated career branches that are likewise unreachable by
	 * a companion (spec 3B), matched by prefix (e.g.
	 * "combat_bountyhunter_investigation_" matches _01.._04). */
	Vector<String> autoGrantPrefixes;

	/** The eleven baseline master combat profession skill strings required
	 * before jedi_* choices are offered (spec 3C). Companion System
	 * (2026-07-12, "badge tracking" pass): isJediEligible() no longer checks
	 * these strings directly against learnedSkills -- each is resolved via
	 * resolveProfessionToken() to the companion's own earned badge instead.
	 * This list still serves as the canonical "which 11 professions gate
	 * Jedi" reference. */
	Vector<String> jediGateMasterSkills;

public:
	CompanionSkillTrainer();

	/**
	 * Returns true if learning/prerequisite-resolving skillName should
	 * completely bypass XP and Skill Point checks and be granted for free
	 * (spec 3B).
	 */
	bool isAutoGrantable(const String& skillName) const;

	/**
	 * Resolves a combat skill string down to its profession token (e.g.
	 * "combat_bountyhunter_novice" -> "bountyhunter"), the same token used to
	 * build both the owner-side "<profession>_master" PlayerObject badge
	 * lookup key (ownerHasRequiredMasterBadge()) and the companion-side
	 * badge the companion itself earns via grantCompanionBadge() the moment
	 * it masters that profession (trainSkill(), "badge tracking" pass).
	 * Returns an empty String for skills with no mapped profession (jedi_*,
	 * companion_master_*, or anything unmapped).
	 *
	 * NOTE: the exact profession tokens below are asserted from naming
	 * convention and should be cross-checked against the live badges table/
	 * SQL dump -- see NOTES.md, "Badge string keys".
	 */
	String resolveProfessionToken(const String& skillName) const;

	/**
	 * Checks PlayerObject::hasBadge() for the owner's "<profession>_master"
	 * badge (spec 3A), via resolveProfessionToken(). Returns true immediately
	 * for skills belonging to the companion_master tree itself (no badge
	 * gate) and for auto-grantable skills.
	 */
	bool ownerHasRequiredMasterBadge(CreatureObject* owner, const String& skillName) const;

	/** Companion System (2026-07-12, "badge tracking" pass) -- true once the
	 * companion itself holds every baseline Master Combat Profession badge
	 * (awarded by trainSkill() the moment each corresponding "_master" skill
	 * tier is granted -- see resolveProfessionToken()). Replaces the earlier
	 * raw learnedSkills-string check: holding the badge is proof mastery
	 * happened at least once, even if the companion is later untrained out
	 * of that skill (spec 3C). */
	bool isJediEligible(CompanionObject* companion) const;

	/**
	 * Full training entry point used by both the /hpet train dialogue flow and
	 * the SUI train-list callback. Performs the badge check, the jedi_ prefix
	 * gate, and the auto-grant bypass, then (and only then) calls
	 * companion->grantSkill() -- always at 0 Skill Points, per spec.
	 * @returns true if the skill was granted.
	 */
	bool trainSkill(CreatureObject* owner, CompanionObject* companion, const String& skillName);

	bool untrainSkill(CreatureObject* owner, CompanionObject* companion, const String& skillName);

	/** Companion System (2026-07-13, "macro list" pass) -- grants the owner's
	 * own PlayerObject::abilityList a "companion_<ability>" entry for every
	 * real, invokable ability skillName's real Skill::getAbilities() lists
	 * (see server/zone/objects/companion/commands/CompanionAbilityCommand.h
	 * for the matching generic QueueCommand dispatcher, and
	 * docs/companion_system/tools/build_command_table_rows.py for the
	 * generated command_table.iff rows/characterAbility values these
	 * strings must exactly match). Called by trainSkill() and by
	 * CompanionStarterProfessionSuiCallback (the one skill grant that
	 * bypasses trainSkill() entirely). No-ops harmlessly for skills that
	 * grant no abilities, or abilities already present. Idempotent/safe to
	 * call more than once. */
	void grantOwnerAbilitiesForSkill(CreatureObject* owner, const String& skillName) const;

	/** Companion System (2026-07-13, "macro list" pass) -- the untrainSkill()
	 * counterpart: revokes each "companion_<ability>" skillName granted
	 * UNLESS some other skill still in the companion's learnedSkills also
	 * grants that same real ability (mirrors how a real player keeps a
	 * shared ability as long as any one skill box granting it is still
	 * held). Must be called while companion->hasLearnedSkill(skillName) is
	 * still true (i.e. before companion->removeSkill()) -- it explicitly
	 * skips skillName itself when scanning for other grantors, so call
	 * order relative to removeSkill() doesn't actually matter, but calling
	 * it first keeps the intent obvious at each call site. */
	void revokeOwnerAbilitiesForSkillIfUnused(CreatureObject* owner, CompanionObject* companion, const String& skillName) const;

	/** Companion System (2026-07-13, "macro list" pass) -- grants the five
	 * always-available baseline companion order abilities (companion_follow/
	 * _stay/_patrol/_store/_attack -- see CompanionFollowCommand.h and
	 * siblings) to the owner's own abilityList exactly once, the first time
	 * they ever complete the starter-profession first-launch flow (see
	 * CompanionStarterProfessionSuiCallback). Never revoked afterward --
	 * matches the always-available, no-skill-gate design of those five
	 * commands themselves. Idempotent/safe to call more than once. */
	void grantBaselineOwnerOrderAbilities(CreatureObject* owner) const;

	/** COMPANION_SKILLMOD_RESYNC_HOTFIX_2026_07_31 -- self-healing fixup for companions whose
	 * skills were granted before CompanionObjectImplementation::
	 * grantSkill() ever called addSkillMod() (see
	 * patch_companion_skillmod_grant_2026-07-31, docs/companion_system/
	 * NOTES.md 2026-07-31): any companion that learned a skill before
	 * that fix landed is still carrying a getSkillMod() of 0 for
	 * everything it "knows" -- untraining and retraining the same
	 * skill does NOT correct this on its own (removeSkill() now
	 * subtracts the skill's real modifiers unconditionally, so a stale
	 * companion goes negative on untrain and nets right back to its
	 * original, wrong total once retrained -- the round trip cancels
	 * itself out by design).
	 *
	 * Recomputes the CORRECT total for every skill modifier this
	 * companion should have from its current learnedSkills list,
	 * compares against what's actually applied via getSkillMod(), and
	 * nudges by the delta -- safe to call on every summon regardless of
	 * whether a given companion is already fully correct (delta lands
	 * on 0, a no-op), was never touched at all (delta == the full
	 * correct value), or is sitting in some partially-corrected state
	 * from an earlier untrain/retrain cycle. Called from
	 * CompanionControlDeviceImplementation::spawnObject() alongside
	 * this deployment's other "self-heal on every summon" calls (see
	 * the grantBaselineOwnerOrderAbilities() call site there). */
	void resyncSkillMods(CompanionObject* companion) const;

	/** Companion System (2026-07-15, "return to the trainer to claim
	 * additional companions" -- see the .cpp doc comment). Hands out ONE
	 * new companion device per trainer conversation while the player's
	 * datapad holds fewer companions than their companion_slots skill mod
	 * allows. No-op for non-Companion-Masters. @pre { player locked } */
	void claimAdditionalCompanion(CreatureObject* player) const;

	/** Companion System (2026-07-15, "test everything from novice" pass --
	 * see NOTES.md and SkillManager.cpp's companion_master_novice grant
	 * block). Directly grants every one of the 60 "companion_<ability>"
	 * macro strings this deliverable ever built a command_table.iff row
	 * for -- both the 35 badge-gated combat-profession abilities and the
	 * 25 starter-profession abilities (see docs/companion_system/tools/
	 * build_command_table_rows.py's _COMPANION_ABILITY_NAMES/
	 * _STARTER_ABILITY_NAMES lists, which this list must exactly mirror)
	 * -- onto the owner's own PlayerObject::abilityList in one pass,
	 * bypassing trainSkill()/ownerHasRequiredMasterBadge() entirely.
	 *
	 * Why this exists: CompanionAbilityCommand.h's own doc comment already
	 * establishes that these 60 macros are gated ONLY by
	 * PlayerObject::hasAbility("companion_<ability>") -- the companion
	 * itself never needs hasLearnedSkill() for the ability to function.
	 * The *intended* way to earn each one -- training the companion in
	 * the matching real profession skill via trainSkill() ->
	 * grantOwnerAbilitiesForSkill() -- turns out to be unreachable through
	 * any UI in this deployment: sendTrainList()'s own header comment
	 * explicitly flags real profession skill-tree candidate enumeration
	 * as "an integration TODO... not part of this deliverable's scope",
	 * and confirms only companion_master_ or jedi_ prefixed skills ever
	 * appear as candidates. So without this method, none of the 60 ability macros
	 * could ever be unlocked by anyone, in any deployment, today.
	 *
	 * This is a deliberate testing/dev-convenience shortcut, not a design
	 * change to the intended slow per-skill unlock -- called once, the
	 * same moment companion_master_novice is first granted, so a fresh
	 * companion is immediately fully testable without needing the
	 * still-unbuilt real profession-training UI. Idempotent (hasAbility()
	 * guard per entry, matching grantOwnerAbilitiesForSkill()'s own
	 * pattern) -- safe to call more than once. */
	void grantAllAbilitiesForTesting(CreatureObject* owner) const;

	/** Spec 2B: owner-only skill sheet (also reachable from spec 4D's dialogue
	 * menu). */
	void sendSkillSheet(CreatureObject* player, CompanionObject* companion);

	/** Companion System (2026-07-20, per user request): the companion's
	 * skill TREE as a colored SUI list -- every skill box in each
	 * profession the owner can teach, GREEN if the companion has it, grey
	 * if not (the "highlighted-green vs not-highlighted" of the real Skills
	 * window), indented by branch. Tapping a box trains it (+ its whole
	 * prerequisite chain, via trainSkill's prereq recursion). */
	void sendSkillTree(CreatureObject* player, CompanionObject* companion);

	/** Companion System (2026-07-12, "stats window" pass) -- owner-only stats
	 * sheet (reachable from spec 4D's dialogue menu, same as the skill
	 * sheet): HAM, companion vitality, resistances/armor, XP, and an
	 * aggregate, clearly-labeled-informational summary of the skill-mod
	 * bonuses the companion's learned skills list (not yet wired into real
	 * combat math -- see NOTES.md). */
	void sendStatsSheet(CreatureObject* player, CompanionObject* companion);

	/** Spec 2B: public read-only equipment/skills view for non-owners. */
	void sendInspectionSheet(CreatureObject* player, CompanionObject* companion);

	/** Spec 4D: dialogue root menu (movement commands / skill sheet / train /
	 * untrain / help sheet). */
	void sendDialogMenu(CreatureObject* player, CompanionObject* companion);

	/** Spec 4D: the "meticulously organized, scannable Contextual Help SUI
	 * ListBox" -- registers the full macro list across all professions with
	 * prerequisites, /hpet syntax, and current readiness (learned vs. not). */
	void sendHelpSheet(CreatureObject* player, CompanionObject* companion);

	/** Spec 4D: train / untrain candidate lists. */
	void sendTrainList(CreatureObject* player, CompanionObject* companion);
	void sendUntrainList(CreatureObject* player, CompanionObject* companion);

	/** Companion System -- one-time first-launch starter profession picker
	 * (SUI list box, same style as a real trainer). See
	 * CompanionObject::hasCompletedFirstLaunch()/firstLaunchComplete and
	 * CompanionControlDeviceImplementation::spawnObject(), which calls this
	 * exactly once per companion the first time it's ever summoned. */
	void sendStarterProfessionChoice(CreatureObject* player, CompanionObject* companion);

	/**
	 * Companion System (2026-07-20, "Master Jedi companion" pass, per user
	 * request) -- bypasses the normal trainSkill()/badge-gate flow entirely
	 * (there is no real "companion_master" path into the Jedi tree today --
	 * see the module doc comment on grantAllAbilitiesForTesting() for the
	 * same "the intended UI doesn't reach here yet" situation) and directly
	 * grants a brand-new, otherwise-untrained companion FULL real Jedi
	 * mastery: jedi_padawan_master, the light- or dark-side journeyman
	 * master tier, and every rung of the matching force_rank_light/dark
	 * ladder plus the full force_title_jedi chain (real stock skills.iff
	 * rows -- not invented). Also sets CompanionObject::isDarkSideJedi,
	 * awards a "jedi_master" companion badge, recalculates combat level, and
	 * equips the companion with a real legendary lightsaber
	 * (sword_lightsaber_vader.iff). Intended as the veteran reward vendor's
	 * "Master Jedi Companion" purchase handler -- see
	 * VeteranRewardVendorSuiCallback.h. Does NOT touch the owner's own
	 * PlayerObject in any way beyond the normal grantOwnerAbilitiesForSkill()
	 * calls already used by every other trainSkill() grant.
	 * @pre { companion locked }
	 */
	void grantMasterJediMastery(CreatureObject* owner, CompanionObject* companion, bool darkSide) const;

	/**
	 * Companion System (2026-07-20, "Master Jedi companion" pass) -- the
	 * veteran reward vendor's actual purchase handler: checks the player's
	 * remaining companion_slots capacity (same check/message shape as
	 * claimAdditionalCompanion()), creates a brand-new device+companion
	 * pair (same object-creation idiom as claimAdditionalCompanion()),
	 * names it, adds it to the datapad, calls grantMasterJediMastery() on
	 * it, and marks firstLaunchComplete so summoning it skips the normal
	 * starter-profession picker (it's already fully built). @returns true
	 * on success (tokens should only be spent by the caller if this
	 * returns true).
	 */
	bool recruitMasterJediCompanion(CreatureObject* player, bool darkSide) const;
};

}
}
}
}

using namespace server::zone::managers::companion;

#endif // COMPANIONSKILLTRAINER_H_
