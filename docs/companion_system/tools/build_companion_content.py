#!/usr/bin/env python3
"""
Authors the actual companion_master datatable/STF content and writes the
patched binary files to outputs/patched/. Does not touch the base .tre
archives -- only produces new file contents to be packaged into
companion_patch.tre by build_tre_patch.py.

IMPORTANT, discovered after the first in-game test (Companion Master's
skill screen showed the same 2-3 wrong/duplicated box labels everywhere):
GRAPH_TYPE isn't cosmetic. Inspecting the real skills.iff shows only values
1 (oneByFour, used exclusively for single-branch linear chains e.g.
force_title_jedi_rank_01..04), 4 (fourByFour, used for every real 4-branch
tree -- 1035 of 1068 base rows), and 5 (pyramid) ever appear -- 2 and 3 are
defined in the enum but never used by any real profession, so they're
unproven/likely unhandled by the client's tree-layout code. Every
"_novice"-rooted branch set in the base game has either exactly 1 child
(graphType=1) or exactly 4 (graphType=4); nothing in between. companion_master
originally shipped with 2 real branches (Husbandry, Resilience) under
GRAPH_TYPE=4, which is almost certainly what caused the client to duplicate
box labels across the "missing" 2 columns. Fixed by adding two more real
branches (Discipline, Vigilance) so the tree genuinely has 4, matching the
only proven multi-branch layout value.

IMPORTANT #2, discovered after the third in-game test (trainer conversation
now opens correctly but immediately says "There is nothing more I can
teach you!"): every companion_master_* row was authored with
XP_TYPE="companion_master_xp", a brand-new custom XP pool. Nothing anywhere
in the C++ server grants players this XP type (confirmed by grepping the
whole server source -- it only appears in CompanionSkillTrainer.cpp as a
SKILL_MOD string and in CompanionObject.idl, never as an
awardExperience(...,"companion_master_xp",...) call), so every player is
permanently stuck at 0 and SkillManager::fulfillsSkillPrerequisitesAndXp()
(SkillManager.cpp:902, `skill->getXpCost() > 0 && ... < skill->getXpCost()`)
rejects every single companion_master_* skill forever, including the novice
tier. The codebase already has an established precedent for treating
companion_master skills as free: SkillManager.cpp's
canLearnSkill()/fulfillsSkillPrerequisites() carries an explicit
`isCompanionMasterSkill = skillName.beginsWith("companion_master_")` bypass
for the skill-POINTS cost. Extending that same "free" design to XP cost --
by setting XP_COST/XP_CAP to 0 for every companion_master_* row here --
resolves this without adding a whole new XP-granting subsystem. A real
"Companion Mastery" XP economy (earned through some gameplay hook) remains
a legitimate future enhancement; see NOTES.md.

IMPORTANT #5 (this pass), root-caused why the tree's TOP/capstone box
(companion_master_master) kept rendering a stale, unrelated profession's data
(Master Brawler) even after IMPORTANT #3's row-position/SKILLS_REQUIRED_COUNT
fix and IMPORTANT #4's stat_n.stf/cmd_n.stf fix were both applied and
individually verified. Exhaustively re-diffed every one of companion_master's
and companion_master_master's 27 columns against combat_brawler/
combat_brawler_master column-by-column (not just the nonzero fields) -- every
single field matches real convention exactly (GRAPH_TYPE=4, matching PARENT-
chain topology including tier-1 branches sharing PARENT with novice/master,
IS_TITLE/IS_PROFESSION flags, SKILLS_REQUIRED_COUNT=0, skl_n.stf/skl_t.stf/
skl_d.stf key<->value-table cross-references individually re-verified
non-colliding) -- with exactly ONE structural difference found:
companion_master_master's SKILL_MODS and COMMANDS were BOTH emptied to ''
by IMPORTANT #4 (it correctly identified the *previous* content --
"companion_master_title=1"/"hpet_formup" -- as bogus with zero real mechanism
behind it, but replaced both with nothing rather than something real).

This project's own NOTES.md timeline supplies a controlled before/after:
the IMPORTANT #4 bug report's own screenshot -- taken AFTER the IMPORTANT #3
position fix but BEFORE SKILL_MODS/COMMANDS were emptied -- explicitly states
"the panel now correctly shows companion_master_master's own data" (rendering
as unresolved "stat_n:[companion_master_title]"/"cmd_n:[hpet_formup]" text,
but genuinely OUR OWN row's keys, not Brawler's). Only AFTER both fields were
emptied to '' did the Brawler-data symptom reappear (this pass's bug report).
That is hard, sourced, in-project evidence -- not speculation about closed
client internals -- that a completely empty SKILL_MODS+COMMANDS pair on this
specific row is what triggers the client to leave a stale previously-viewed
box's data on screen instead of refreshing to blank/real content.

Checked whether a fully-empty SKILL_MODS+COMMANDS master row has real
precedent at all: of 6 real *_master rows sampled (combat_brawler,
outdoors_creaturehandler, combat_bountyhunter, social_politician,
crafting_architect, social_entertainer), 5/6 have a real, non-empty
SKILL_MODS; the sole exception, social_politician_master, is Politician --
famously one of the lowest-traffic, closest-to-unused profession trees in
pre-CU SWG, so a client quirk that only surfaces on a fully-blank capstone
would plausibly have gone unreported there for two decades without
contradicting this finding. Given companion_master is meant to be a fully
real, actively-used profession (unlike Politician), and given 5/6 real
precedent says a capstone SHOULD grant a real stat bonus, the fix is to give
companion_master_master a real, non-bogus SKILL_MODS entry instead of leaving
it blank -- see the add("companion_master_master", ...) call below.
COMMANDS is left empty (real precedent: crafting_weaponsmith_master and
crafting_architect_master both have real SKILL_MODS with empty COMMANDS, so
COMMANDS-alone-empty is independently precedented and not implicated by the
before/after evidence above).

IMPORTANT #4 (an earlier pass this session), root-caused a second, distinct info-panel bug that
only appeared after IMPORTANT #3 was fixed: with the right row's data now
rendering, companion_master_master's panel showed the literal unresolved
strings "stat_n:[companion_master_title]" and "cmd_n:[hpet_formup]" instead
of real text. Root cause, confirmed by extracting the real client's
string/en/stat_n.stf (highest-priority stock copy: patch_12_00.tre) and
string/en/cmd_n.stf (patch_14_00.tre) and cross-checking every SKILL_MODS/
COMMANDS entry in the entire 1068-row base skills.iff against them:

- The "Skill Mods" panel resolves each SKILL_MODS key through stat_n.stf
  (case-sensitive, exact key match) and falls back to printing
  "stat_n:[key]" when the lookup misses. Every non-"private_"-prefixed
  SKILL_MODS key in the ENTIRE real base file (2055/2055, 100%) resolves
  via stat_n.stf -- "private_"-prefixed keys are the one real exception
  (never looked up/displayed at all, confirmed separately). There is zero
  real precedent anywhere in the base game for a "title"-granting SKILL_MODS
  entry of any kind -- real *_master rows (combat_bountyhunter_master,
  outdoors_creaturehandler_master, crafting_weaponsmith_master,
  combat_brawler_master, all individually inspected) grant only ordinary
  combat/utility stat buffs, never a title. Real SWG grants titles via
  PlayerObject::addSuffixTitle()/badges, never a skills.iff stat mod. So
  companion_master_master's authored companion_master_title=1 SKILL_MODS
  entry had no real mechanism behind it at all and was simply removed.
- The "Commands and Abilities Granted" panel resolves each COMMANDS entry
  through cmd_n.stf the same way (case-insensitively, and splitting on '+'
  for the real "basecommand+argument" convention seen in entertainer rows
  like startDance+basic) -- 727/727 non-"private_"/"cert_"-prefixed COMMANDS
  entries in the base file resolve this way (98.8%, with the only misses
  being genuine stock dead/test content: pilot_spacetest, a beta leftover).
  Real content NEVER concatenates a command and a chat argument with an
  underscore into one token -- the real separator (when one is used at all)
  is '+', and only for STF-resolvable style/argument names, not free chat
  text. companion_master_master's "hpet_formup" and jedi_teraskappa_01's
  "hpet_force_assist" are exactly this mistake: HpetCommand.h registers a
  single command, "hpet", and treats "formup"/"help"/"camp" purely as
  post-registration chat arguments dispatched inside doQueueCommand() --
  there is no registered command literally named "hpet_formup" or
  "hpet_force_assist" for cmd_n.stf (or the command factory) to ever resolve.
  (CompanionSkillTrainer.cpp's sendHelpSheet() already independently
  hardcodes both exact strings into its seenMacros skip-list from an earlier
  pass, i.e. the C++ side already knew these two tokens were not real
  invokable commands -- this pass fixes the same underlying authoring
  mistake at its source, the skills.iff data, so it stops surfacing in the
  client's own skill-tree UI too, not just the /hpet help sheet.)
  Additionally, no real *_master row ever re-lists a command already granted
  by its profession's novice row (crafting_weaponsmith_master's COMMANDS is
  empty; combat_brawler_master/combat_bountyhunter_master/
  outdoors_creaturehandler_master all list only NEW tier-2 abilities, never
  a repeat of a novice-tier grant) -- companion_master_novice already grants
  "hpet" itself, so companion_master_master (a pure title/stat-mod capstone,
  exactly like crafting_weaponsmith_master) and jedi_teraskappa_01 (a hidden
  flag-only bonus row, gated purely by hasLearnedSkill(), never by any
  command) both now carry an empty COMMANDS column, matching real precedent
  exactly instead of inventing a syntax with none.
- Separately, but discovered by the same exhaustive check: every OTHER
  companion_master_*/jedi_teraskappa_01 row's SKILL_MODS keys
  (companion_slots, companion_max_vitality, companion_heal_vitality,
  companion_heal_speed, companion_death_penalty_reduction,
  companion_incap_recovery, companion_xp_discount,
  companion_command_response, companion_threat_response,
  companion_combat_accuracy, companion_jedi_combat_assist) and
  companion_master_novice's real COMMANDS entries (hpet, companionfollow,
  companionstay, companionpatrol, companionstore, companionattack) are all
  genuinely-intended custom content, not authoring mistakes -- but NONE of
  them existed in the real stat_n.stf/cmd_n.stf either, so every single one
  would have shown the exact same "stat_n:[key]"/"cmd_n:[key]" bug the
  instant a player clicked any other companion box. Fixed the same way this
  project has fixed every other missing-STF-entry gap all session
  (skl_n.stf/skl_t.stf/skl_d.stf/exp_n.stf/creature_names.stf/
  skill_teacher.stf all got real added entries for new content rather than
  inventing a new lookup mechanism) -- see build_stat_n_stf()/
  build_cmd_n_stf() below, new files #11/#12 in companion_patch.tre.

IMPORTANT #3 (an earlier pass), root-caused the in-game "Skill Mods"/
"Commands and Abilities Granted" info panel showing a totally unrelated
profession's data (e.g. Master Brawler's Berserk/Intimidate/Melee Defense)
when clicking real companion_master_* boxes, even after mastering the
profession. Two concrete, exhaustively-verified structural deviations from
the real skills.iff were found and are fixed here -- see NOTES.md's dated
section for the full writeup:

1. Row PHYSICAL ORDER within a profession's block. Every one of the 54 real
   IS_PROFESSION=1 blocks in the base skills.iff (100%, exhaustively
   checked) is laid out NAME, NAME_novice, NAME_master, then all 4 branches
   in full (4 tiers each) -- i.e. the master/capstone row always
   *immediately* follows novice, physically, before any branch-tier rows.
   This file previously called add("companion_master_master", ...) *after*
   all four branch loops, so companion_master_master ended up at physical
   offset +18 from companion_master_novice instead of +2 -- a layout no
   real profession in the entire 1068-row base file ever uses. Fixed by
   moving the companion_master_master add() call back to immediately follow
   companion_master_novice, before the branch loops, matching the universal
   convention (forward name-references in SKILLS_REQUIRED to the not-yet-
   emitted branch tier-4 rows are fine -- every real *_master row does
   exactly the same forward reference, e.g. outdoors_creaturehandler_master
   lists outdoors_creaturehandler_support_04 while physically preceding it).

2. SKILLS_REQUIRED_COUNT. companion_master_master was authored with
   SKILLS_REQUIRED_COUNT=4 (one per branch capstone in SKILLS_REQUIRED).
   Exhaustively checked every one of the 1068 base rows: SKILLS_REQUIRED_COUNT
   is 0 on literally every single one, including every real *_master row
   that also lists 4 comma-separated capstones in SKILLS_REQUIRED (e.g.
   outdoors_creaturehandler_master, crafting_weaponsmith_master,
   combat_bountyhunter_master all have SKILLS_REQUIRED_COUNT=0 despite 4
   SKILLS_REQUIRED entries). The server itself loads this field
   (SkillInfo.h:103, column index 9) but never reads it anywhere in
   SkillManager.cpp's gating logic -- it's a client-facing-only field with
   zero real-world coverage of any nonzero value anywhere in the base game,
   exactly the same category of unproven/likely-mishandled value as the
   GRAPH_TYPE=2/3 finding above. Fixed by leaving it at the make_row()
   default of 0, matching literally 100% of real content.
"""
import sys, os, struct
sys.path.insert(0, os.path.dirname(__file__))
from iff_datatable import DataTable
from stf_codec import StfTable
from companion_ability_names import ALL_MIRROR_ABILITY_NAMES

EXTRACTED = "extracted"
OUT = "patched"
os.makedirs(OUT, exist_ok=True)


def make_row(columns, **kwargs):
    row = []
    for name, t in columns:
        base = t[0]
        if name in kwargs:
            v = kwargs[name]
        else:
            v = "" if base == "s" else 0
        if base == "s":
            row.append(v)
        else:
            row.append(struct.pack("<i", v))
    return row


def build_skills_iff():
    dt, _ = DataTable.parse(open(os.path.join(EXTRACTED, "skills.iff"), "rb").read())
    cols = dt.columns
    existing = set(row[0] for row in dt.rows)

    new_rows = []

    def add(name, **kwargs):
        assert name not in existing, f"{name} already exists in base skills.iff!"
        new_rows.append(make_row(cols, NAME=name, **kwargs))

    add("companion_master",
        PARENT="outdoors", GRAPH_TYPE=4, IS_PROFESSION=1, SEARCHABLE=1)

    add("companion_master_novice",
        PARENT="companion_master", GRAPH_TYPE=4, IS_TITLE=1,
        XP_TYPE="companion_master_xp", XP_COST=0, XP_CAP=0,
        # Companion System (2026-07-14, "Form Up" macro pass): added
        # companionformup, the 7th baseline order command, giving /hpet
        # formup's existing functionality its own real, hotbar-draggable
        # owner ability -- same treatment as the other 6. See
        # CompanionFormupCommand.h / FormationManager.cpp / NOTES.md.
        # Companion System (2026-07-17, "pet command port" pass): appended
        # the seven ported Creature Handler pet-order equivalents (guard,
        # followother, rangedattack, specialone, specialtwo, group, friend)
        # -- same baseline treatment as the original seven. See
        # Companion{Guard,FollowOther,RangedAttack,SpecialAttack,Group,
        # Friend}Command.h / NOTES.md.
        # Companion System (2026-07-25, "Jenkins" pass, per user request) --
        # appended "jenkins" so the client's own ability/command browser
        # (which reads this COMMANDS column to know what a learned skill
        # grants -- separate from the server-side characterAbility/
        # hasAbility() runtime gate, which was already correct) actually
        # lists it as a hotbar-draggable macro. This is the real root cause
        # of "jenkins doesn't show up anywhere" -- the ability grant and
        # cmd_n.stf display name were both already correct, but the client
        # never knew companion_master_novice granted this command at all.
        # Companion System (2026-08-09, batch 40, REVERTED the v4 "cram
        # everything into one box" experiment): batch 35/36 believed
        # visible=2 was box-membership AND held-ability. It's not -- Nick
        # confirmed live that once ALL 203 mirror commands were added here,
        # EVERY one listed regardless of whether the companion had actually
        # learned it (batch 39: "every single command is in novice
        # companion handler, which is why im seeing them"). Box-membership
        # alone is apparently sufficient; the held-ability half of visible=2
        # never filtered anything for these cloned rows. (Silver lining:
        # execution is still correctly gated server-side -- "You do not have
        # sufficient abilities" -- so this was always cosmetic over-listing,
        # never a capability leak.)
        #
        # Real fix (batch 40): back to just the 16 baseline order commands
        # (always desired, owner-unconditional -- these don't need per-
        # ability gating) plus jenkins. The 203 companion mirror abilities
        # each get their OWN hidden gating skill instead (see
        # _build_companion_mirror_ability_skills() below, called from
        # build_skills_iff()) -- box-membership in THAT skill, individually
        # granted/revoked per ability by CompanionSkillTrainer.cpp's
        # syncOwnerMirrorAbilities(), is what now drives Command Browser
        # listing correctly, since box-membership is the mechanism that's
        # actually confirmed to work.
        COMMANDS="hpet,companionfollow,companionstay,companionpatrol,companionstore,companionattack,companionformup,companionguard,companionfollowother,companionrangedattack,companionspecialone,companionspecialtwo,companiongroup,companionfriend,companionreturn,jenkins",
        # Companion System (2026-07-15, "test 5 companions at once" pass --
        # see NOTES.md): bumped 1 -> 5 at the user's explicit request, to
        # test simultaneous multi-companion support end to end
        # (SkillManager.cpp's companion_master_novice grant block now reads
        # this live via creature->getSkillMod("companion_slots") to decide
        # how many companion control devices to create, rather than a
        # hardcoded count -- so this is the one and only place that number
        # needs to change). companion_master_master's own separate
        # companion_slots=1 below still stacks additively on top of this.
        # 2026-07-17, per user request: bumped 5 -> 50.
        # 2026-07-29, per user request: nerfed 50 -> 1 (novice Companion Handler).
        SKILL_MODS="companion_slots=1", SEARCHABLE=1)

    # Companion System fix (earlier pass): companion_master_master MUST be
    # emitted here, immediately after companion_master_novice, to match the
    # universal real-skills.iff physical row order (root, novice, master,
    # then branches) -- see module docstring "IMPORTANT #3" item 1.
    # SKILLS_REQUIRED_COUNT is deliberately left at its make_row() default
    # of 0 (matching every real profession's master row) -- see item 2.
    #
    # Companion System fix (IMPORTANT #4, an earlier pass): COMMANDS dropped
    # entirely. "hpet_formup" resolved to nothing in cmd_n.stf (no registered
    # command is ever named that -- "hpet" is the one real registered
    # command, "formup" is a chat argument dispatched inside
    # HpetCommand::doQueueCommand(), never a second command name). Leaving it
    # blank matches real precedent for a pure title/stat-mod capstone row
    # that grants no new command beyond what its novice tier already granted
    # (crafting_weaponsmith_master's/crafting_architect_master's real
    # COMMANDS columns are likewise empty; "hpet" itself is already granted
    # by companion_master_novice, so re-listing it here would be an empty
    # gesture no real *_master row ever makes either).
    #
    # Companion System fix (IMPORTANT #5, this pass): SKILL_MODS is no
    # longer left blank. IMPORTANT #4 also zeroed SKILL_MODS (it had held
    # the bogus "companion_master_title=1", which had no real mechanism
    # behind it and was correctly removed) -- but leaving BOTH SKILL_MODS
    # and COMMANDS completely empty on this specific row is what caused the
    # tree's top/capstone box to render a stale, unrelated profession's data
    # (Master Brawler) instead of its own -- see the module docstring's
    # "IMPORTANT #5" entry for the full before/after evidence from this
    # project's own NOTES.md timeline. companion_slots=1 is a real,
    # genuinely-intended stat (already resolvable via the patched
    # stat_n.stf/stat_d.stf -- see STAT_N_ENTRIES/STAT_D_ENTRIES below) that
    # stacks additively with companion_master_novice's own
    # SKILL_MODS="companion_slots=1", so a fully-mastered Companion Handler
    # ends up with 2 total companion slots -- a real, thematic "master"
    # reward (you've proven yourself capable of managing more than one
    # companion), not a placeholder value chosen just to make the field
    # non-empty.
    add("companion_master_master",
        PARENT="companion_master", GRAPH_TYPE=4, IS_TITLE=1,
        SKILLS_REQUIRED="companion_master_husbandry_04,companion_master_resilience_04,"
                         "companion_master_discipline_04,companion_master_vigilance_04",
        XP_TYPE="companion_master_xp", XP_COST=0, XP_CAP=0,
        SKILL_MODS="companion_slots=1",
        SEARCHABLE=1)

    # NOTE on PARENT vs SKILLS_REQUIRED (root-caused this session by diffing
    # against the real outdoors_creaturehandler rows): PARENT is NOT the
    # previous skill in the chain -- it's the tree-graph edge the client uses
    # to lay out the grid. Tier-1 of every real branch has PARENT set to the
    # PROFESSION itself (same PARENT as the novice/master title skills), and
    # SKILLS_REQUIRED is what gates it behind novice. Only tier-2+ chains
    # PARENT to the previous tier. The original version of this file set
    # PARENT=companion_master_novice on every tier-1 row, which malformed the
    # graph and was the actual cause of the garbled/leaking skill tree
    # (previously misattributed to row physical adjacency -- see NOTES.md).
    # Companion System -- branch content enriched to mirror the density of a
    # real Creature Handler tier row (e.g. outdoors_creaturehandler_taming_01
    # carries two stacked mods: "tame_non_aggro=5,tame_level=2"). Each of our
    # 4 branches now stacks two thematically-related mods per tier instead of
    # one, and every tier value is a per-row DELTA (SkillMods sum across every
    # learned skill box, same as the real game), so a player who has learned
    # all 4 tiers of a branch gets the full cumulative total, matching how
    # outdoors_creaturehandler_taming_01..04 all independently declare
    # "tame_non_aggro=5" so it sums to 20 at max taming.
    # NOTE: as of this pass, none of the companion_* SkillMod names below
    # (old or new) are read by any getSkillMod() call anywhere in the C++
    # source -- confirmed by repo-wide grep. They render correctly in the
    # in-game skill sheet as informational stat text, but do not yet affect
    # gameplay. Wiring them into CompanionObjectImplementation.cpp /
    # CompanionControlDeviceImplementation.cpp (mirroring the existing
    # Resilience hasSkill("companion_master_resilience_0N") tier-counting
    # pattern already used for the death-penalty scaling) is a legitimate
    # follow-up task; see NOTES.md.
    husbandry_costs = [(0, 0, 100, 10, 5), (0, 0, 200, 20, 10),
                        (0, 0, 300, 30, 15), (0, 0, 400, 40, 20)]
    prev = "companion_master_novice"
    for i, (cost, cap, maxvit, healmod, healspeed) in enumerate(husbandry_costs, start=1):
        name = f"companion_master_husbandry_0{i}"
        parent = "companion_master" if i == 1 else prev
        add(name,
            PARENT=parent, GRAPH_TYPE=4, IS_TITLE=(1 if i == 4 else 0),
            SKILLS_REQUIRED=prev, XP_TYPE="companion_master_xp",
            XP_COST=cost, XP_CAP=cap,
            SKILL_MODS=f"companion_max_vitality={maxvit},companion_heal_vitality={healmod},"
                       f"companion_heal_speed={healspeed}",
            SEARCHABLE=1)
        prev = name

    resilience_costs = [(0, 0, 25, 10), (0, 0, 50, 20),
                         (0, 0, 75, 30), (0, 0, 100, 40)]
    prev = "companion_master_novice"
    for i, (cost, cap, pct, incap) in enumerate(resilience_costs, start=1):
        name = f"companion_master_resilience_0{i}"
        parent = "companion_master" if i == 1 else prev
        add(name,
            PARENT=parent, GRAPH_TYPE=4, IS_TITLE=(1 if i == 4 else 0),
            SKILLS_REQUIRED=prev, XP_TYPE="companion_master_xp",
            XP_COST=cost, XP_CAP=cap,
            SKILL_MODS=f"companion_death_penalty_reduction={pct},"
                       f"companion_incap_recovery={incap}",
            SEARCHABLE=1)
        prev = name

    discipline_costs = [(0, 0, 5, 10), (0, 0, 10, 20),
                         (0, 0, 15, 30), (0, 0, 20, 40)]
    prev = "companion_master_novice"
    for i, (cost, cap, pct, cmdresp) in enumerate(discipline_costs, start=1):
        name = f"companion_master_discipline_0{i}"
        parent = "companion_master" if i == 1 else prev
        add(name,
            PARENT=parent, GRAPH_TYPE=4, IS_TITLE=(1 if i == 4 else 0),
            SKILLS_REQUIRED=prev, XP_TYPE="companion_master_xp",
            XP_COST=cost, XP_CAP=cap,
            SKILL_MODS=f"companion_xp_discount={pct},"
                       f"companion_command_response={cmdresp}",
            SEARCHABLE=1)
        prev = name

    vigilance_costs = [(0, 0, 10, 5), (0, 0, 20, 10),
                        (0, 0, 30, 15), (0, 0, 40, 20)]
    prev = "companion_master_novice"
    for i, (cost, cap, pct, acc) in enumerate(vigilance_costs, start=1):
        name = f"companion_master_vigilance_0{i}"
        parent = "companion_master" if i == 1 else prev
        add(name,
            PARENT=parent, GRAPH_TYPE=4, IS_TITLE=(1 if i == 4 else 0),
            SKILLS_REQUIRED=prev, XP_TYPE="companion_master_xp",
            XP_COST=cost, XP_CAP=cap,
            SKILL_MODS=f"companion_threat_response={pct},"
                       f"companion_combat_accuracy={acc}",
            SEARCHABLE=1)
        prev = name

    # Elite Companion Handler -- new profession (2026-07-27, per user
    # request "no jedi boxes in the companion profession ... add them to an
    # elite companion handler ... only unlocks when master companion
    # handler is mastered"). jedi_teraskappa_01 (the old single IS_HIDDEN
    # box bolted onto companion_master_master) is gone entirely, replaced
    # with a full standalone profession built to the one shape this
    # project's own 2026-07-14 "Jedi Companion" design research confirmed
    # every real top-level profession in the base game actually uses (all
    # 51 standalone professions surveyed, zero exceptions): root + novice +
    # 4 branches x4 tiers + master, GRAPH_TYPE=4 -- see NOTES.md. A smaller/
    # minimal shape was considered there and explicitly rejected as
    # unprecedented.
    #
    # Gating ("only unlocks once Master Companion Handler is mastered") is
    # NOT expressed via SKILLS_REQUIRED here -- no real profession in the
    # base game ever has one profession's SKILLS_REQUIRED point at a
    # different profession's tree (same finding NOTES.md already
    # documented), so that would be an unprecedented shape. It's instead a
    # plain C++ check in CompanionSkillTrainer::trainSkill()/sendTrainList()
    # (companion->hasLearnedSkill("companion_master_master")) -- see that
    # file. isJediEligible()'s old 11-combat-badge gate no longer applies
    # to anything in this tree (left in place, unused, in case some future
    # real-Jedi-adjacent feature wants it again).
    #
    # The 4 branches continue the exact same 4 stat pools as the base tree
    # (Husbandry/Resilience/Discipline/Vigilance), picking up numerically
    # where companion_master's own tier-4 rows leave off -- kept
    # deliberately plain/flavor, matching this project's own precedent for
    # a mostly-decorative branch set. The real payload lives on the
    # capstone: companion_master_elite_master inherits jedi_teraskappa_01's
    # exact former effect, renamed companion_jedi_combat_assist ->
    # companion_elite_combat_assist (see stat_n.stf/stat_d.stf below) --
    # "no jedi boxes" means no Jedi-named anything left in this profession,
    # including the stat mod's own display name/description, not just the
    # skill box name.
    add("companion_master_elite",
        PARENT="outdoors", GRAPH_TYPE=4, IS_PROFESSION=1, SEARCHABLE=1)

    add("companion_master_elite_novice",
        PARENT="companion_master_elite", GRAPH_TYPE=4, IS_TITLE=1,
        # Companion System (2026-07-28 FIX, sidebar categorization): every
        # real ELITE profession's novice row has a SKILLS_REQUIRED entry
        # pointing at its real prerequisite -- confirmed via direct
        # comparison against the base skills.iff (Bounty Hunter, Architect,
        # Creature Handler, Doctor all have one; every real BASIC profession
        # has none). This is what sorts a profession into the client's
        # advanced-profession sidebar list instead of the basic one --
        # matches Nick's explicit design ("above a starting class").
        SKILLS_REQUIRED="companion_master_master",
        # Companion System (2026-07-29, Elite novice slots zeroed): per
        # Nick's explicit request, Elite Companion Handler's NOVICE tier
        # itself should grant 0 additional Companion Slots -- companion_slots
        # was previously 1 here, stacking on top of the base Companion
        # Handler profession's own grants (companion_master_novice/
        # companion_master_master, a separate profession -- untouched by
        # this patch). Any later/master tier of Elite Companion Handler is
        # free to grant slots of its own; this box specifically should not.
        XP_TYPE="companion_master_xp", XP_COST=0, XP_CAP=0,
        SKILL_MODS="companion_slots=0", SEARCHABLE=1)

    # Companion System (2026-07-28 FIX, row-physical-order regression):
    # companion_master_elite_master previously sat AFTER all four branch
    # loops below (physical offset +18 from novice) -- the exact same
    # layout mistake already found and fixed for companion_master_master
    # (see this file's own "IMPORTANT #3" docstring section: every one of
    # the 54 real IS_PROFESSION=1 blocks in the base skills.iff places the
    # master/capstone row immediately after novice, physically, before any
    # branch-tier rows). Moved here to match. The forward references to
    # not-yet-emitted branch tier-4 rows in SKILLS_REQUIRED below are fine
    # -- every real *_master row does the same (see IMPORTANT #3 above).
    add("companion_master_elite_master",
        PARENT="companion_master_elite", GRAPH_TYPE=4, IS_TITLE=1,
        SKILLS_REQUIRED="companion_master_elite_husbandry_04,companion_master_elite_resilience_04,"
                         "companion_master_elite_discipline_04,companion_master_elite_vigilance_04",
        XP_TYPE="companion_master_xp", XP_COST=0, XP_CAP=0,
        SKILL_MODS="companion_elite_combat_assist=1",
        SEARCHABLE=1)

    elite_husbandry_costs = [(0, 0, 500, 50, 25), (0, 0, 600, 60, 30),
                              (0, 0, 700, 70, 35), (0, 0, 800, 80, 40)]
    prev = "companion_master_elite_novice"
    for i, (cost, cap, maxvit, healmod, healspeed) in enumerate(elite_husbandry_costs, start=1):
        name = f"companion_master_elite_husbandry_0{i}"
        parent = "companion_master_elite" if i == 1 else prev
        add(name,
            PARENT=parent, GRAPH_TYPE=4, IS_TITLE=(1 if i == 4 else 0),
            SKILLS_REQUIRED=prev, XP_TYPE="companion_master_xp",
            XP_COST=cost, XP_CAP=cap,
            SKILL_MODS=f"companion_max_vitality={maxvit},companion_heal_vitality={healmod},"
                       f"companion_heal_speed={healspeed}",
            SEARCHABLE=1)
        prev = name

    elite_resilience_costs = [(0, 0, 125, 50), (0, 0, 150, 60),
                               (0, 0, 175, 70), (0, 0, 200, 80)]
    prev = "companion_master_elite_novice"
    for i, (cost, cap, pct, incap) in enumerate(elite_resilience_costs, start=1):
        name = f"companion_master_elite_resilience_0{i}"
        parent = "companion_master_elite" if i == 1 else prev
        add(name,
            PARENT=parent, GRAPH_TYPE=4, IS_TITLE=(1 if i == 4 else 0),
            SKILLS_REQUIRED=prev, XP_TYPE="companion_master_xp",
            XP_COST=cost, XP_CAP=cap,
            SKILL_MODS=f"companion_death_penalty_reduction={pct},"
                       f"companion_incap_recovery={incap}",
            SEARCHABLE=1)
        prev = name

    elite_discipline_costs = [(0, 0, 25, 50), (0, 0, 30, 60),
                               (0, 0, 35, 70), (0, 0, 40, 80)]
    prev = "companion_master_elite_novice"
    for i, (cost, cap, pct, cmdresp) in enumerate(elite_discipline_costs, start=1):
        name = f"companion_master_elite_discipline_0{i}"
        parent = "companion_master_elite" if i == 1 else prev
        add(name,
            PARENT=parent, GRAPH_TYPE=4, IS_TITLE=(1 if i == 4 else 0),
            SKILLS_REQUIRED=prev, XP_TYPE="companion_master_xp",
            XP_COST=cost, XP_CAP=cap,
            SKILL_MODS=f"companion_xp_discount={pct},"
                       f"companion_command_response={cmdresp}",
            SEARCHABLE=1)
        prev = name

    elite_vigilance_costs = [(0, 0, 50, 25), (0, 0, 60, 30),
                              (0, 0, 70, 35), (0, 0, 80, 40)]
    prev = "companion_master_elite_novice"
    for i, (cost, cap, pct, acc) in enumerate(elite_vigilance_costs, start=1):
        name = f"companion_master_elite_vigilance_0{i}"
        parent = "companion_master_elite" if i == 1 else prev
        add(name,
            PARENT=parent, GRAPH_TYPE=4, IS_TITLE=(1 if i == 4 else 0),
            SKILLS_REQUIRED=prev, XP_TYPE="companion_master_xp",
            XP_COST=cost, XP_CAP=cap,
            SKILL_MODS=f"companion_threat_response={pct},"
                       f"companion_combat_accuracy={acc}",
            SEARCHABLE=1)
        prev = name

    # Companion System (2026-08-09, batch 40): one hidden, free, non-god
    # gating skill PER companion mirror ability (203 total), replacing the
    # reverted "cram everything into companion_master_novice" approach
    # above. Each row's only job is to make box-membership -- confirmed the
    # actual, sole Command Browser gate (batch 39) -- track exactly one
    # ability. CompanionSkillTrainer.cpp's syncOwnerMirrorAbilities()
    # grants/revokes membership in these via SkillManager::awardSkill()/
    # surrenderSkill() (notifyClient=false, noXpRequired=true) alongside its
    # existing companion_-ability grant/revoke, which stays unchanged and
    # remains the real server-side execution gate ("You do not have
    # sufficient abilities" already proved that gate works independently of
    # anything wrong with listing).
    #
    # IS_HIDDEN=1 so these never render in the player's own Skills screen or
    # any tree UI (real precedent: the Jedi force_discipline_* subtree uses
    # the same flag, though those are also GOD_ONLY -- deliberately NOT
    # copying GOD_ONLY=1 here, since SkillManager::awardSkill() unconditionally
    # sends a "you have gained the command..." system message for every
    # ability on any god-only skill regardless of notifyClient, which would
    # spam the owner on every sync). POINTS_REQUIRED/XP_COST/XP_CAP all left
    # at make_row()'s 0 default (real, not merely bypassed by a prefix
    # special-case elsewhere -- deliberately NOT named "companion_master_*"
    # to avoid colliding with the several other places in this codebase that
    # pattern-match that specific prefix for unrelated reasons). PARENT left
    # blank (make_row() default) -- these are mechanical leaves, not part of
    # any real tree, so they need no parent the way force_discipline (a real
    # tree root, also PARENT="") doesn't either.
    for _abilityName in ALL_MIRROR_ABILITY_NAMES:
        add("companion_hidden_" + _abilityName.lower(),
            IS_HIDDEN=1,
            COMMANDS="companion" + _abilityName,
            SEARCHABLE=0)

    # Companion System -- diagnostic experiment for the "wrong profession
    # boxes leak into the skill tree" bug. Base skills.iff has our block
    # appended at the absolute end (rows 1068-1087 of 1088), directly after
    # pilot_neutral/pilot_spacetest with nothing after -- leading theory is
    # the client's tree UI picks decorative prev/next preview text by
    # physical row-adjacency rather than PARENT. Testing that theory by
    # inserting the block right after outdoors_squadleader's block instead
    # (last real 'outdoors'-category profession, row 620 in the base file)
    # so if the leaked box changes to reflect Politician-related text
    # instead of Pilot text, that confirms row-adjacency is the mechanism.
    insertAt = None
    for i, row in enumerate(dt.rows):
        if row[0] == "outdoors_squadleader_support_04":
            insertAt = i + 1
            break
    assert insertAt is not None, "could not find outdoors_squadleader_support_04 insertion anchor"

    # Companion System (2026-07-27, Elite Companion Handler pass): split
    # companion_master_elite's rows off into their own, physically
    # SEPARATE splice (the new end of the table) rather than inserting
    # them alongside companion_master's own block -- matching NOTES.md's
    # 2026-07-14 "Jedi Companion" design research recommendation (test
    # isolation; row-adjacency was already exhaustively disproven as the
    # original leak mechanism, so this is not a correctness requirement).
    eliteStart = None
    for i, row in enumerate(new_rows):
        if row[0] == "companion_master_elite":
            eliteStart = i
            break
    assert eliteStart is not None, "could not find companion_master_elite row in new_rows"

    companionMasterRows = new_rows[:eliteStart]
    eliteRows = new_rows[eliteStart:]

    dt.rows = dt.rows[:insertAt] + companionMasterRows + dt.rows[insertAt:]
    dt.rows = dt.rows + eliteRows

    outPath = os.path.join(OUT, "skills.iff")
    with open(outPath, "wb") as f:
        f.write(dt.serialize())
    print(f"skills.iff: added {len(companionMasterRows)} companion_master rows at index {insertAt}, "
          f"{len(eliteRows)} companion_master_elite rows at end -> {outPath} ({os.path.getsize(outPath)} bytes)")
    return [r[0] for r in new_rows]


def build_xp_limits_iff():
    dt, _ = DataTable.parse(open(os.path.join(EXTRACTED, "xp_limits.iff"), "rb").read())
    existing = set(row[0] for row in dt.rows)
    assert "companion_master_xp" not in existing
    dt.rows.append(["companion_master_xp", struct.pack("<i", 200000)])
    outPath = os.path.join(OUT, "xp_limits.iff")
    with open(outPath, "wb") as f:
        f.write(dt.serialize())
    print(f"xp_limits.iff: added companion_master_xp -> {outPath} ({os.path.getsize(outPath)} bytes)")


def build_exp_n_stf():
    table = StfTable.parse(open(os.path.join(EXTRACTED, "exp_n.stf"), "rb").read())
    table.add("companion_master_xp", "Companion Mastery")
    outPath = os.path.join(OUT, "exp_n.stf")
    with open(outPath, "wb") as f:
        f.write(table.serialize())
    print(f"exp_n.stf: added companion_master_xp -> {outPath} ({os.path.getsize(outPath)} bytes)")


SKL_NAMES = {
    "companion_master": "Companion Handler",
    "companion_master_novice": "Novice Companion Handler",
    "companion_master_husbandry_01": "Companion Husbandry I",
    "companion_master_husbandry_02": "Companion Husbandry II",
    "companion_master_husbandry_03": "Companion Husbandry III",
    "companion_master_husbandry_04": "Master Companion Husbandry",
    "companion_master_resilience_01": "Companion Resilience I",
    "companion_master_resilience_02": "Companion Resilience II",
    "companion_master_resilience_03": "Companion Resilience III",
    "companion_master_resilience_04": "Master Companion Resilience",
    "companion_master_discipline_01": "Companion Discipline I",
    "companion_master_discipline_02": "Companion Discipline II",
    "companion_master_discipline_03": "Companion Discipline III",
    "companion_master_discipline_04": "Master Companion Discipline",
    "companion_master_vigilance_01": "Companion Vigilance I",
    "companion_master_vigilance_02": "Companion Vigilance II",
    "companion_master_vigilance_03": "Companion Vigilance III",
    "companion_master_vigilance_04": "Master Companion Vigilance",
    "companion_master_master": "Master Companion Handler",
    "companion_master_elite": "Elite Companion Handler",
    "companion_master_elite_novice": "Novice Elite Companion Handler",
    "companion_master_elite_husbandry_01": "Elite Companion Husbandry I",
    "companion_master_elite_husbandry_02": "Elite Companion Husbandry II",
    "companion_master_elite_husbandry_03": "Elite Companion Husbandry III",
    "companion_master_elite_husbandry_04": "Elite Master Companion Husbandry",
    "companion_master_elite_resilience_01": "Elite Companion Resilience I",
    "companion_master_elite_resilience_02": "Elite Companion Resilience II",
    "companion_master_elite_resilience_03": "Elite Companion Resilience III",
    "companion_master_elite_resilience_04": "Elite Master Companion Resilience",
    "companion_master_elite_discipline_01": "Elite Companion Discipline I",
    "companion_master_elite_discipline_02": "Elite Companion Discipline II",
    "companion_master_elite_discipline_03": "Elite Companion Discipline III",
    "companion_master_elite_discipline_04": "Elite Master Companion Discipline",
    "companion_master_elite_vigilance_01": "Elite Companion Vigilance I",
    "companion_master_elite_vigilance_02": "Elite Companion Vigilance II",
    "companion_master_elite_vigilance_03": "Elite Companion Vigilance III",
    "companion_master_elite_vigilance_04": "Elite Master Companion Vigilance",
    "companion_master_elite_master": "Master Elite Companion Handler",
}


def build_skl_n_stf():
    table = StfTable.parse(open(os.path.join(EXTRACTED, "skl_n.stf"), "rb").read())
    for k, v in SKL_NAMES.items():
        table.add(k, v)
    outPath = os.path.join(OUT, "skl_n.stf")
    with open(outPath, "wb") as f:
        f.write(table.serialize())
    print(f"skl_n.stf: added {len(SKL_NAMES)} entries -> {outPath} ({os.path.getsize(outPath)} bytes)")


SKL_TITLES = {
    "companion_master_novice": "Companion Handler",
    "companion_master_husbandry_04": "Companion Husbandry Master",
    "companion_master_resilience_04": "Companion Resilience Master",
    "companion_master_discipline_04": "Companion Discipline Master",
    "companion_master_vigilance_04": "Companion Vigilance Master",
    "companion_master_master": "Master Companion Handler",
    "companion_master_elite_novice": "Elite Companion Handler",
    "companion_master_elite_husbandry_04": "Elite Companion Husbandry Master",
    "companion_master_elite_resilience_04": "Elite Companion Resilience Master",
    "companion_master_elite_discipline_04": "Elite Companion Discipline Master",
    "companion_master_elite_vigilance_04": "Elite Companion Vigilance Master",
    "companion_master_elite_master": "Master Elite Companion Handler",
}


def build_skl_t_stf():
    """Companion System UX-parity fix: every real profession's IS_TITLE=1
    rows (novice, each branch's tier-4 box, and the profession-master box)
    get a player title in skl_t.stf -- ours never did, confirmed by
    checking the base file (patch_12_00.tre's copy, the only TRE that ships
    this file at all) for any companion_master_* key. Only the 6 IS_TITLE
    rows need an entry here (see build_skills_iff()'s IS_TITLE=(1 if i==4
    else 0) pattern and the companion_master_novice/companion_master_master
    rows)."""
    table = StfTable.parse(open(os.path.join(EXTRACTED, "skl_t.stf"), "rb").read())
    for k, v in SKL_TITLES.items():
        table.add(k, v)
    outPath = os.path.join(OUT, "skl_t.stf")
    with open(outPath, "wb") as f:
        f.write(table.serialize())
    print(f"skl_t.stf: added {len(SKL_TITLES)} entries -> {outPath} ({os.path.getsize(outPath)} bytes)")


SKL_DESCRIPTIONS = {
    "companion_master": (
        "The Companion Handler learns to recruit, train, and field a loyal "
        "companion of their own -- tending to its health and readiness, "
        "hardening it against the dangers of the field, sharpening its "
        "obedience, and honing how quickly it comes to its master's defense."
    ),
    "companion_master_novice": (
        "The Novice Companion Handler has just been assigned their first "
        "companion. They can call it to their side, order it to stay or "
        "patrol, and store it safely away when it is not needed."
    ),
    "companion_master_husbandry_01": (
        "Basic care techniques let a Handler nurse their companion's "
        "vitality back more effectively after a hard fight."
    ),
    "companion_master_husbandry_02": (
        "Continued study of companion physiology further improves how much "
        "vitality a companion can recover and how quickly."
    ),
    "companion_master_husbandry_03": (
        "Advanced husbandry techniques let a Handler push their companion's "
        "maximum vitality and recovery rate well beyond a novice's reach."
    ),
    "companion_master_husbandry_04": (
        "A Master of Companion Husbandry has learned every technique for "
        "keeping a companion at peak health, maximizing its vitality and "
        "how fast it recovers from harm."
    ),
    "companion_master_resilience_01": (
        "A Handler begins to learn how to soften the toll a companion's "
        "death takes on it, and how quickly it can recover from being "
        "incapacitated."
    ),
    "companion_master_resilience_02": (
        "Further training reduces how much a companion permanently suffers "
        "when it falls, and speeds its recovery from incapacitation."
    ),
    "companion_master_resilience_03": (
        "Advanced resilience training brings a companion's death penalty "
        "down close to the bare minimum a companion can ever suffer."
    ),
    "companion_master_resilience_04": (
        "A Master of Companion Resilience has minimized the lasting harm "
        "their companion suffers from falling in battle, and taught it to "
        "shake off incapacitation almost immediately."
    ),
    "companion_master_discipline_01": (
        "A Handler begins to teach their companion to respond to commands "
        "more crisply, and discovers ways to make its continued training "
        "less costly."
    ),
    "companion_master_discipline_02": (
        "Further discipline sharpens a companion's command response and "
        "further discounts the cost of training it."
    ),
    "companion_master_discipline_03": (
        "Advanced discipline techniques bring a companion's obedience and "
        "training efficiency close to their peak."
    ),
    "companion_master_discipline_04": (
        "A Master of Companion Discipline has trained their companion to "
        "answer every command instantly, at the lowest possible training "
        "cost."
    ),
    "companion_master_vigilance_01": (
        "A Handler begins teaching their companion to watch for danger to "
        "its master, so it reacts more readily when its master is "
        "threatened."
    ),
    "companion_master_vigilance_02": (
        "Further vigilance training makes a companion quicker to intervene "
        "and steadier in a fight once it does."
    ),
    "companion_master_vigilance_03": (
        "Advanced vigilance training leaves a companion watching its "
        "master's back almost constantly."
    ),
    "companion_master_vigilance_04": (
        "A Master of Companion Vigilance has trained their companion to "
        "intercept any threat to its master without hesitation, and to "
        "fight at its sharpest once it does."
    ),
    "companion_master_master": (
        "The Master Companion Handler has mastered every discipline of "
        "companion care -- husbandry, resilience, discipline, and "
        "vigilance -- and commands a companion as capable and steadfast as "
        "any in the field."
    ),
    "companion_master_elite": (
        "Elite Companion Handler is the advanced tier only a companion "
        "that has already mastered Companion Handler can begin -- further, "
        "harder training in the same fundamentals, pushed well past a "
        "master's reach."
    ),
    "companion_master_elite_novice": (
        "Having mastered the basics, a companion begins the harder work "
        "of becoming an Elite Companion Handler."
    ),
    "companion_master_elite_husbandry_01": (
        "Elite care techniques push a companion's vitality recovery well "
        "past what a master handler could ever manage."
    ),
    "companion_master_elite_husbandry_02": (
        "Further elite study continues to raise a companion's maximum "
        "vitality and how fast it recovers."
    ),
    "companion_master_elite_husbandry_03": (
        "Advanced elite husbandry techniques leave a companion recovering "
        "from harm faster than seems possible."
    ),
    "companion_master_elite_husbandry_04": (
        "An Elite Master of Companion Husbandry keeps a companion at the "
        "peak of vitality and recovery, beyond any ordinary master's reach."
    ),
    "companion_master_elite_resilience_01": (
        "Elite resilience training further softens the toll a companion's "
        "death takes on it, beyond a master's own training."
    ),
    "companion_master_elite_resilience_02": (
        "Further elite training reduces a companion's death penalty and "
        "incapacitation recovery time even more."
    ),
    "companion_master_elite_resilience_03": (
        "Advanced elite resilience training leaves a companion nearly "
        "unshaken by falling in battle."
    ),
    "companion_master_elite_resilience_04": (
        "An Elite Master of Companion Resilience shrugs off death's "
        "penalty and incapacitation alike, far past a master's training."
    ),
    "companion_master_elite_discipline_01": (
        "Elite discipline sharpens a companion's command response and "
        "training efficiency beyond a master's own limits."
    ),
    "companion_master_elite_discipline_02": (
        "Further elite discipline continues to sharpen a companion's "
        "obedience and discount its training cost."
    ),
    "companion_master_elite_discipline_03": (
        "Advanced elite discipline training brings a companion's "
        "responsiveness to near instant."
    ),
    "companion_master_elite_discipline_04": (
        "An Elite Master of Companion Discipline commands instant, "
        "flawless obedience at minimal training cost."
    ),
    "companion_master_elite_vigilance_01": (
        "Elite vigilance training makes a companion quicker to intervene "
        "than any master's own training allowed."
    ),
    "companion_master_elite_vigilance_02": (
        "Further elite vigilance training continues to sharpen a "
        "companion's reaction to danger and its steadiness in a fight."
    ),
    "companion_master_elite_vigilance_03": (
        "Advanced elite vigilance leaves a companion watching its "
        "master's back with almost no delay at all."
    ),
    "companion_master_elite_vigilance_04": (
        "An Elite Master of Companion Vigilance intercepts any threat to "
        "its master instantly, and fights at its absolute sharpest."
    ),
    "companion_master_elite_master": (
        "The Master Elite Companion Handler has pushed every discipline "
        "of companion care beyond mastery, and lends its full strength "
        "to any fight at its owner's side."
    ),
}


def build_skl_d_stf():
    """Companion System UX-parity fix: real professions have a description
    for every skill box, shown in the skills window; ours had none. Adds one
    for the profession row itself plus all 19 skill boxes (companion_master
    was checked and confirmed absent from the base file, same as skl_t.stf)."""
    table = StfTable.parse(open(os.path.join(EXTRACTED, "skl_d.stf"), "rb").read())
    for k, v in SKL_DESCRIPTIONS.items():
        table.add(k, v)
    outPath = os.path.join(OUT, "skl_d.stf")
    with open(outPath, "wb") as f:
        f.write(table.serialize())
    print(f"skl_d.stf: added {len(SKL_DESCRIPTIONS)} entries -> {outPath} ({os.path.getsize(outPath)} bytes)")


def build_creature_names_stf():
    table = StfTable.parse(open(os.path.join(EXTRACTED, "creature_names.stf"), "rb").read())
    table.add("trainer_companion_master", "a Companion Handler trainer")
    # Companion System (2026-07-20, "companion kill token" pass, per user
    # request) -- veteran_reward_vendor.lua's display name. See
    # VeteranRewardVendorSuiCallback.h.
    table.add("veteran_reward_vendor", "a Veteran Reward Vendor")
    outPath = os.path.join(OUT, "creature_names.stf")
    with open(outPath, "wb") as f:
        f.write(table.serialize())
    print(f"creature_names.stf: added trainer_companion_master, veteran_reward_vendor -> {outPath} ({os.path.getsize(outPath)} bytes)")


def build_skill_teacher_stf():
    """Companion System -- the generic trainer conversation handler
    (bin/scripts/screenplays/trainers/trainerConvHandler.lua) resolves the
    NPC's greeting text as "@skill_teacher:" .. trainerType, where trainerType
    is the second arg passed to createTrainerConversationTemplate() in
    trainer_conv.lua ("trainer_companion_master" for us). Every other
    profession trainer has a matching key in this file (e.g.
    "trainer_creaturehandler"); companion_master never got one, which left
    the greeting -- and with it the whole conversation window -- blank and
    non-interactive in-game. Fixed by adding the missing key."""
    table = StfTable.parse(open(os.path.join(EXTRACTED, "skill_teacher.stf"), "rb").read())
    table.add(
        "trainer_companion_master",
        "So you want to learn how to properly train and command a companion? "
        "I can teach you what I know. What do you need?",
    )
    outPath = os.path.join(OUT, "skill_teacher.stf")
    with open(outPath, "wb") as f:
        f.write(table.serialize())
    print(f"skill_teacher.stf: added trainer_companion_master -> {outPath} ({os.path.getsize(outPath)} bytes)")


COMPANION_STF_ENTRIES = [
    ("no_active_companion", "You have no active companion."),
    ("ability_locked", "Your companion has not learned that ability."),
    ("no_valid_target", "You must have a valid hostile target selected."),
    ("formup_no_followers", "You have no active pets, droids, or companions to form up."),
    ("formup_complete", "Your followers have assumed formation."),
    ("jedi_not_eligible", "This companion has not mastered every baseline combat profession."),
    ("missing_master_badge", "You must have the skill learned, or have the profession's Master Badge, to train your companion in it."),
    ("skill_trained", "Your companion has learned a new skill."),
    ("skill_untrained", "Your companion has forgotten a skill."),
    ("skill_sheet_title", "Companion Skill Sheet"),
    ("skill_sheet_text", "Skills this companion has learned:"),
    ("inspect_title", "Inspect Companion"),
    ("inspect_text", "Public equipment & skill summary:"),
    ("dialog_title", "Companion Options"),
    ("dialog_text", "What would you like your companion to do?"),
    ("dialog_follow", "Follow"),
    ("dialog_stay", "Stay"),
    ("dialog_patrol", "Patrol"),
    ("dialog_skill_sheet", "Skill Sheet"),
    ("dialog_train", "Train"),
    ("dialog_untrain", "Untrain"),
    ("dialog_help", "Contextual Help"),
    ("train_title", "Train Companion"),
    ("train_text", "Select a skill to train (0 Skill Points required):"),
    ("untrain_title", "Untrain Companion"),
    ("untrain_text", "Select a skill to remove:"),
    ("help_title", "Companion Macro Reference"),
    ("help_text", "/hpet <macro> -- readiness shown per row:"),
    ("camp_not_trained", "Your companion has not been trained in the Ranger or Scout skill trees."),
    ("no_camp_tent", "Your companion is not carrying a camp tent."),
    ("menu_open_inventory", "Open Companion Inventory"),
    ("menu_skill_sheet", "Companion Skill Sheet"),
    ("menu_revive", "Revive"),
    ("menu_talk", "Talk to Companion"),
    ("menu_inspect", "Inspect Companion"),
    ("no_companion_loaded", "This datapad has no companion loaded."),
    ("cant_summon_now", "You cannot summon or store your companion right now."),
    ("dead_summon_error", "Your companion has fallen and cannot be summoned. Revive it first."),
    ("insufficient_rank", "Your Companion Handler rank is not high enough to summon this companion."),
    ("summoned", "Your companion has been summoned."),
    ("cant_store_now", "You cannot store your companion while in combat."),
    ("stored", "Your companion has been stored."),
    ("companion_died", "Your companion has fallen and permanently lost vitality."),
    ("companion_granted", "A companion has been added to your datapad. Open your datapad and spawn your companion to summon it."),
    ("starter_profession_title", "Choose a Starting Profession"),
    ("starter_profession_text", "Select the profession your companion will start as. This choice can only be made once, on your companion's first launch."),
    ("starter_profession_chosen", "Your companion has chosen its starting profession."),
    # Companion System (2026-07-13, "custom companion name" pass) -- see
    # CompanionRenameSuiCallback.h / CompanionDialogMenuSuiCallback.h.
    ("rename_prompt", "Enter a new name for your companion."),
    ("rename_invalid", "That name is not valid. Companion names must be 1-40 characters."),
    ("rename_rejected", "That name was rejected by the name filter."),
    ("rename_success", "Your companion's name has been updated."),

    # Companion System (2026-07-13, "manual equip radial" pass) -- see
    # CompanionObjectImplementation.cpp::equipItemFromInventory() /
    # TangibleObjectMenuComponent.cpp (radial ID 82).
    ("equip_on_companion", "Equip on Companion"),
    ("equip_not_in_inventory", "That item isn't in your companion's inventory."),
    ("equip_not_equippable", "Your companion can't equip that item."),
    ("equip_slot_occupied", "Your companion already has something equipped in that slot. Unequip it first."),
    ("equipped", "Your companion equips the item."),

    # Companion System (2026-07-13, "Take Off Companion" radial pass;
    # relabeled "Pick Up" 2026-07-14 per user request, see NOTES.md) -- see
    # CompanionObjectImplementation.cpp::unequipItemToInventory() /
    # TangibleObjectMenuComponent.cpp (radial ID 83). Key names kept as
    # "unequip_*" (not renamed to "pick_up_*") to avoid unnecessary churn --
    # only the display text changed to reflect the new behavior (item goes
    # to the player's own inventory now, not the companion's nested bag).
    ("unequip_from_companion", "Pick Up"),
    ("unequip_not_equipped", "Your companion doesn't have that item equipped."),
    ("unequip_failed", "Couldn't pick up that item from your companion right now."),
    ("unequipped", "You take the item back from your companion."),

    # Companion System (2026-07-14, "player-side loadout backpack" redesign)
    # -- see companion_loadout_backpack.lua / CompanionLoadoutContainerComponent
    # and NOTES.md.
    ("loadout_backpack_name", "Companion Loadout"),

    # Companion System (2026-07-29, "in crafting range" indicator) -- see
    # CompanionCraftingRangeIndicator.h. showFlyText() aux key, fired
    # PRIVATELY to the owner (never broadcast) on entering crafting range
    # of a crafting-capable companion, re-pulsed ~every 10s while still in
    # range.
    #
    # RESTORED 2026-08-01: the original indicator patch carried this entry
    # but aborted on a drifted .cpp anchor before it reached the STF write,
    # and the re-anchor script only covered the .cpp half. The string has
    # been missing since, rendering in-game as the raw key
    # companion:[crafting_range_flytext].
    ("crafting_range_flytext", "\u2692 Crafting station in range"),

    # Companion System (2026-08-01) -- Jenkin's Survey Tool. objectName in
    # master_survey_tool.lua MUST be an STF reference (@companion:key); a
    # raw literal string never resolves and the client falls back to
    # printing the template path. Same pattern as loadout_backpack_name
    # directly above.
    ("master_survey_tool_name", "Jenkin's Survey Tool"),
    ("master_survey_tool_desc", "An advanced survey device that scans an entire planet at once and drops a waypoint at each of the five strongest resource concentrations found, at least 1000m apart, instead of the standard single-point survey. Requires Master Artisan to operate."),
]


STAT_N_ENTRIES = {
    # Companion System (IMPORTANT #4): real display names for every custom
    # companion_* SKILL_MODS key actually used by a companion_master_*/
    # jedi_teraskappa_01 row, so the "Skill Mods" info panel resolves them
    # instead of falling back to "stat_n:[key]" text. Short noun-phrase
    # style, matching real stat_n.stf entries (e.g. "Melee Defense",
    # "Force Power Max") rather than the longer sentence style used in
    # companion.stf. companion_master_title is deliberately NOT here -- it
    # was removed from skills.iff entirely, not given a display name, since
    # it has no real mechanism behind it at all (see module docstring).
    "companion_slots": "Companion Slots",
    "companion_max_vitality": "Companion Max Vitality",
    "companion_heal_vitality": "Companion Vitality Healing",
    "companion_heal_speed": "Companion Vitality Heal Speed",
    "companion_death_penalty_reduction": "Companion Death Penalty Reduction",
    "companion_incap_recovery": "Companion Incapacitation Recovery",
    "companion_xp_discount": "Companion Training Cost Discount",
    "companion_command_response": "Companion Command Response",
    "companion_threat_response": "Companion Threat Response",
    "companion_combat_accuracy": "Companion Combat Accuracy",
    "companion_elite_combat_assist": "Companion Elite Combat Mastery",
}


def build_stat_n_stf():
    """Companion System (IMPORTANT #4): the real string/en/stat_n.stf
    (highest-priority stock copy: patch_12_00.tre) is what the client's
    "Skill Mods" info panel actually resolves each SKILL_MODS key through,
    falling back to literal "stat_n:[key]" text on a miss -- confirmed by
    cross-checking every non-"private_"-prefixed SKILL_MODS key in the whole
    1068-row base skills.iff (2055/2055, 100%) against this file. Adds one
    entry per genuinely-intended custom companion_* stat key (see
    STAT_N_ENTRIES above)."""
    table = StfTable.parse(open(os.path.join(EXTRACTED, "stat_n.stf"), "rb").read())
    for k, v in STAT_N_ENTRIES.items():
        table.add(k, v)
    outPath = os.path.join(OUT, "stat_n.stf")
    with open(outPath, "wb") as f:
        f.write(table.serialize())
    print(f"stat_n.stf: added {len(STAT_N_ENTRIES)} entries -> {outPath} ({os.path.getsize(outPath)} bytes)")


STAT_D_ENTRIES = {
    # Companion System (Bug 2, this pass): real one-line descriptions for the
    # "Skill Mods" info panel's tooltip/detail text, the separate stat_d.stf
    # counterpart table to stat_n.stf (which only supplies the short display
    # NAME, e.g. "Companion Slots" -- confirmed missing here: build_stat_n_stf()
    # fixed the display names but this project never shipped a patched
    # stat_d.stf at all, so every companion stat mod's tooltip still fell back
    # to literal "stat_d:[key]" text). Style matches the real stat_d.stf's
    # "This mod ..." one-line convention (see e.g. "melee_defense" ->
    # "This mod improves your defense against melee attacks." in the base
    # file). Each description is grounded in the real mechanic it represents
    # in CompanionObjectImplementation.cpp/CompanionControlDeviceImplementation.cpp
    # (or, where the mechanic is an established but not-yet-wired design
    # target -- see NOTES.md's "none of the companion_* SkillMod names ... are
    # read by any getSkillMod() call" caveat -- the intended, documented
    # in-tree behavior, not invented flavor text).
    "companion_slots": (
        "This mod increases the number of companions you may have "
        "registered to your account at one time."
    ),
    "companion_max_vitality": (
        "This mod increases your companion's maximum vitality, the pool "
        "that is permanently reduced when your companion falls in battle."
    ),
    "companion_heal_vitality": (
        "This mod increases how much vitality your companion recovers "
        "each time it is treated with a vitality pack."
    ),
    "companion_heal_speed": (
        "This mod increases the rate at which your companion recovers "
        "vitality over time."
    ),
    "companion_death_penalty_reduction": (
        "This mod reduces how much of your companion's maximum vitality is "
        "permanently lost when it falls in battle."
    ),
    "companion_incap_recovery": (
        "This mod speeds up how quickly your companion recovers from being "
        "incapacitated."
    ),
    "companion_xp_discount": (
        "This mod reduces the experience cost of training your companion "
        "in new skills."
    ),
    "companion_command_response": (
        "This mod makes your companion respond more crisply and quickly "
        "to your commands."
    ),
    "companion_threat_response": (
        "This mod increases how readily your companion intercepts threats "
        "directed at you."
    ),
    "companion_combat_accuracy": (
        "This mod improves your companion's accuracy once it engages a "
        "threat in combat."
    ),
    "companion_elite_combat_assist": (
        "This mod marks a companion as an Elite Companion Handler's "
        "finest work -- hardened by training that goes well beyond "
        "mastery, lending its full strength to any fight at its owner's "
        "side."
    ),
}


def build_stat_d_stf():
    """Companion System (Bug 2, this pass): the real string/en/stat_d.stf
    (highest-priority stock copy: patch_12_00.tre, same archive stat_n.stf
    was extracted from) is what the client's "Skill Mods" info panel
    resolves each SKILL_MODS key's tooltip/description text through,
    falling back to literal "stat_d:[key]" text on a miss -- exactly the
    same fallback-on-miss pattern already confirmed for stat_n.stf (names)
    and cmd_n.stf (commands), just for the separate description table
    build_stat_n_stf() didn't touch. Adds one entry per genuinely-intended
    custom companion_* stat key -- the same key list STAT_N_ENTRIES already
    established (see STAT_D_ENTRIES above)."""
    table = StfTable.parse(open(os.path.join(EXTRACTED, "stat_d.stf"), "rb").read())
    for k, v in STAT_D_ENTRIES.items():
        table.add(k, v)
    outPath = os.path.join(OUT, "stat_d.stf")
    with open(outPath, "wb") as f:
        f.write(table.serialize())
    print(f"stat_d.stf: added {len(STAT_D_ENTRIES)} entries -> {outPath} ({os.path.getsize(outPath)} bytes)")


CMD_N_ENTRIES = {
    # Companion System (IMPORTANT #4): real display names for every custom
    # command actually granted by a companion_master_* row's COMMANDS
    # column (currently only companion_master_novice grants any -- see
    # module docstring for why companion_master_master/jedi_teraskappa_01
    # now grant none). Style matches the real "Pet Command: <X>" convention
    # already used by pet_follow/pet_stay/pet_patrol/pet_release in the
    # stock cmd_n.stf.
    "hpet": "Companion Command: Menu",
    "companionfollow": "Companion Command: Follow",
    "companionstay": "Companion Command: Stay",
    "companionpatrol": "Companion Command: Patrol",
    "companionstore": "Companion Command: Store",
    "companionattack": "Companion Command: Attack",
    # Companion System (2026-07-14, "Form Up" macro pass): see
    # CompanionFormupCommand.h / build_skills_iff()'s companion_master_novice
    # COMMANDS entry.
    "companionformup": "Companion Command: Form Up",
    # Companion System (2026-07-17, "pet command port" pass, per user
    # request): the remaining Creature Handler pet orders ported as
    # companion equivalents -- same "Companion Command: <X>" convention.
    "companionguard": "Companion Command: Guard",
    "companionfollowother": "Companion Command: Follow Other",
    "companionrangedattack": "Companion Command: Ranged Attack",
    "companionspecialone": "Companion Command: Special Attack One",
    "companionspecialtwo": "Companion Command: Special Attack Two",
    "companiongroup": "Companion Command: Group",
    "companionfriend": "Companion Command: Friend",
    # Companion System (2026-07-20, "massive battlefield" pass, per user
    # request): recalls a posted companion back to its last Stay/Guard
    # position -- see CompanionReturnCommand.h.
    "companionreturn": "Companion Command: Return",
    # Companion System (2026-07-25, "Jenkins" pass, per user request) --
    # mass-summon-and-form-up order, same "Companion Command: <X>"
    # convention as every other baseline order command. See
    # CompanionJenkinsCommand.h.
    "jenkins": "Companion Command: Jenkins",
    # Companion System (2026-07-29, companion-ability cmd_n.stf description fix)
    # Adds real display names for all 61 companion-ability commands
    # (companion<Ability>, see build_command_table_rows.py's
    # _COMPANION_ABILITY_NAMES + _STARTER_ABILITY_NAMES) whose
    # command_table.iff stringId is deliberately blank. Text is the
    # REAL stock cmd_n.stf display name for each base ability
    # (verified via direct StfTable read, 61/61 found -- not
    # invented), same "Companion Command: <X>" convention as every
    # entry above.
    "companionapplydisease": "Companion Command: Apply Disease",
    "companionapplypoison": "Companion Command: Apply Poison",
    "companionbleedingshot": "Companion Command: Bleeding Shot",
    "companionconcealshot": "Companion Command: Conceal Shot",
    "companionconfusionshot": "Companion Command: Confusion Shot",
    "companioneyeshot": "Companion Command: Eye Shot",
    "companionfastblast": "Companion Command: Fast Blast",
    "companionfireacidcone1": "Companion Command: Acid Cone 1",
    "companionfireacidcone2": "Companion Command: Acid Cone 2",
    "companionfireacidsingle1": "Companion Command: Acid Single 1",
    "companionfireacidsingle2": "Companion Command: Acid Single 2",
    "companionfirelightningcone1": "Companion Command: Lightning Cone 1",
    "companionfirelightningcone2": "Companion Command: Lightning Cone 2",
    "companionfirelightningsingle1": "Companion Command: Lightning Single 1",
    "companionfirelightningsingle2": "Companion Command: Lightning Single 2",
    "companionflamecone1": "Companion Command: Flame Cone 1",
    "companionflamecone2": "Companion Command: Flame Cone 2",
    "companionflamesingle1": "Companion Command: Flame Single 1",
    "companionflamesingle2": "Companion Command: Flame Single 2",
    "companionflurryshot1": "Companion Command: Flurry Shot 1",
    "companionflurryshot2": "Companion Command: Flurry Shot 2",
    "companionflushingshot1": "Companion Command: Flushing Shot 1",
    "companionflushingshot2": "Companion Command: Flushing Shot 2",
    "companionheadshot3": "Companion Command: Head Shot 3",
    "companionhealmind": "Companion Command: Heal Mind",
    "companionknockdownfire": "Companion Command: Fire Knockdown",
    "companionmindshot2": "Companion Command: Mind Shot 2",
    "companionsnipershot": "Companion Command: Sniper Shot",
    "companionsprayshot": "Companion Command: Spray Shot",
    "companionstartleshot1": "Companion Command: Startle Shot 1",
    "companionstartleshot2": "Companion Command: Startle Shot 2",
    "companionstrafeshot1": "Companion Command: Strafe Shot 1",
    "companionstrafeshot2": "Companion Command: Strafe Shot 2",
    "companionsurpriseshot": "Companion Command: Surprise Shot",
    "companiontorsoshot": "Companion Command: Torso Shot",
    "companionunderhandshot": "Companion Command: Underhand Shot",
    "companionhealdamage": "Companion Command: Heal Damage",
    "companionhealwound": "Companion Command: Heal Wound",
    "companiontendwound": "Companion Command: Tend Wound",
    "companiontenddamage": "Companion Command: Tend Damage",
    "companiondiagnose": "Companion Command: Diagnose",
    "companionmedicalforage": "Companion Command: Medical Forage",
    "companionharvestcorpse": "Companion Command: Harvest Corpse",
    "companionstartdance": "Companion Command: Start Dancing",
    "companionstopdance": "Companion Command: Stop Dancing",
    "companionstartmusic": "Companion Command: Start Music",
    "companionstopmusic": "Companion Command: Stop Music",
    "companionsample": "Companion Command: Sample Resource",
    "companionsurvey": "Companion Command: Survey Resources",
    "companionwarcry1": "Companion Command: Warcry 1",
    "companionintimidate1": "Companion Command: Intimidate 1",
    "companionberserk1": "Companion Command: Berserk 1",
    "companiontaunt": "Companion Command: Taunt",
    "companionpolearmlunge1": "Companion Command: Polearm Lunge 1",
    "companionunarmedlunge1": "Companion Command: Unarmed Lunge 1",
    "companionmelee1hlunge1": "Companion Command: One-Hand Lunge 1",
    "companionmelee2hlunge1": "Companion Command: Two-Hand Lunge 1",
    "companioncenterofbeing": "Companion Command: Center Of Being",
    "companionpointblankarea1": "Companion Command: Point Blank Area 1",
    "companionpointblanksingle1": "Companion Command: Point Blank Single 1",
    "companionoverchargeshot1": "Companion Command: Overcharge Shot 1",

    # Companion System (2026-08-07, "full combat tree ability coverage" pass)
    # -- display names for the 142 additional companion-ability commands
    # added the same day to build_command_table_rows.py's
    # _COMPANION_ABILITY_NAMES (see that file's own comment for the full
    # rationale). Same sourcing discipline as the 61 entries above: every
    # name is the REAL stock cmd_n.stf display name for its base ability
    # (verified via direct StfTable read, 142/142 found), same "Companion
    # Command: <X>" convention.
    "companionactionshot1": "Companion Command: Action Shot 1",
    "companionactionshot2": "Companion Command: Action Shot 2",
    "companionaim": "Companion Command: Aim",
    "companionberserk2": "Companion Command: Berserk 2",
    "companionbodyshot1": "Companion Command: Body Shot 1",
    "companionbodyshot2": "Companion Command: Body Shot 2",
    "companionbodyshot3": "Companion Command: Body Shot 3",
    "companionboostmorale": "Companion Command: Boost Morale",
    "companionburstshot1": "Companion Command: Burst Shot 1",
    "companionburstshot2": "Companion Command: Burst Shot 2",
    "companionchargeshot1": "Companion Command: Charge Shot 1",
    "companionchargeshot2": "Companion Command: Charge Shot 2",
    "companioncripplingshot": "Companion Command: Crippling Shot",
    "companioncuredisease": "Companion Command: Cure Disease",
    "companioncurepoison": "Companion Command: Cure Poison",
    "companiondazzle": "Companion Command: Dazzle",
    "companiondisarmingshot1": "Companion Command: Disarming Shot 1",
    "companiondisarmingshot2": "Companion Command: Disarming Shot 2",
    "companiondiveshot": "Companion Command: Dive Shot",
    "companiondoubletap": "Companion Command: Double Tap",
    "companiondragincapacitatedplayer": "Companion Command: Drag Incapacitated Player",
    "companionextinguishfire": "Companion Command: Extinguish Fire",
    "companionfanshot": "Companion Command: Fan Shot",
    "companionfeigndeath": "Companion Command: Feign Death",
    "companionfirstaid": "Companion Command: First Aid",
    "companionforage": "Companion Command: Forage",
    "companionforceofwill": "Companion Command: Force of Will",
    "companionfullautoarea1": "Companion Command: Full Auto Area 1",
    "companionfullautoarea2": "Companion Command: Full Auto Area 2",
    "companionfullautosingle1": "Companion Command: Full Auto Single 1",
    "companionfullautosingle2": "Companion Command: Full Auto Single 2",
    "companionheadshot1": "Companion Command: Head Shot 1",
    "companionheadshot2": "Companion Command: Head Shot 2",
    "companionhealenhance": "Companion Command: Heal Enhance",
    "companionhealstate": "Companion Command: Heal State",
    "companionhealthshot1": "Companion Command: Health Shot 1",
    "companionhealthshot2": "Companion Command: Health Shot 2",
    "companionintimidate2": "Companion Command: Intimidate 2",
    "companionkipupshot": "Companion Command: Kip Up Shot",
    "companionlastditch": "Companion Command: Last Ditch",
    "companionlegshot1": "Companion Command: Leg Shot 1",
    "companionlegshot2": "Companion Command: Leg Shot 2",
    "companionlegshot3": "Companion Command: Leg Shot 3",
    "companionlowblow": "Companion Command: Low Blow",
    "companionmaskscent": "Companion Command: Mask Scent",
    "companionmeditate": "Companion Command: Meditate",
    "companionmelee1hblindhit1": "Companion Command: One-Hand Blind 1",
    "companionmelee1hblindhit2": "Companion Command: One-Hand Blind 2",
    "companionmelee1hbodyhit1": "Companion Command: One-Hand Body Hit 1",
    "companionmelee1hbodyhit2": "Companion Command: One-Hand Body Hit 2",
    "companionmelee1hbodyhit3": "Companion Command: One-Hand Body Hit 3",
    "companionmelee1hdizzyhit1": "Companion Command: One-Hand Dizzy 1",
    "companionmelee1hdizzyhit2": "Companion Command: One-Hand Dizzy 2",
    "companionmelee1hhealthhit1": "Companion Command: One-Hand Health Hit 1",
    "companionmelee1hhealthhit2": "Companion Command: One-Hand Health Hit 2",
    "companionmelee1hhit1": "Companion Command: One-Hand Hit 1",
    "companionmelee1hhit2": "Companion Command: One-Hand Hit 2",
    "companionmelee1hhit3": "Companion Command: One-Hand Hit 3",
    "companionmelee1hlunge2": "Companion Command: One-Hand Lunge 2",
    "companionmelee1hscatterhit1": "Companion Command: One-Hand Scatter Hit 1",
    "companionmelee1hscatterhit2": "Companion Command: One-Hand Scatter Hit 2",
    "companionmelee1hspinattack1": "Companion Command: One-Hand Spin Attack 1",
    "companionmelee1hspinattack2": "Companion Command: One-Hand Spin Attack 2",
    "companionmelee2harea1": "Companion Command: Two-Hand Area Attack 1",
    "companionmelee2harea2": "Companion Command: Two-Hand Area Attack 2",
    "companionmelee2harea3": "Companion Command: Two-Hand Area Attack 3",
    "companionmelee2hheadhit1": "Companion Command: Two-Hand Head Hit 1",
    "companionmelee2hheadhit2": "Companion Command: Two-Hand Head Hit 2",
    "companionmelee2hheadhit3": "Companion Command: Two-Hand Head Hit 3",
    "companionmelee2hhit1": "Companion Command: Two-Hand Hit 1",
    "companionmelee2hhit2": "Companion Command: Two-Hand Hit 2",
    "companionmelee2hhit3": "Companion Command: Two-Hand Hit 3",
    "companionmelee2hlunge2": "Companion Command: Two-Hand Lunge 2",
    "companionmelee2hmindhit1": "Companion Command: Two-Hand Mind Hit 1",
    "companionmelee2hmindhit2": "Companion Command: Two-Hand Mind Hit 2",
    "companionmelee2hspinattack1": "Companion Command: Two-Hand Spin Attack 1",
    "companionmelee2hspinattack2": "Companion Command: Two-Hand Spin Attack 2",
    "companionmelee2hsweep1": "Companion Command: Two-Hand Sweep 1",
    "companionmelee2hsweep2": "Companion Command: Two-Hand Sweep 2",
    "companionmindshot1": "Companion Command: Mind Shot 1",
    "companionmultitargetpistolshot": "Companion Command: Multi Target Pistol Shot",
    "companionoverchargeshot2": "Companion Command: Overcharge Shot 2",
    "companionpanicshot": "Companion Command: Panic Shot",
    "companionpistolmeleedefense1": "Companion Command: Pistol Melee Defense 1",
    "companionpistolmeleedefense2": "Companion Command: Pistol Melee Defense 2",
    "companionpointblankarea2": "Companion Command: Point Blank Area 2",
    "companionpointblanksingle2": "Companion Command: Point Blank Single 2",
    "companionpolearmactionhit1": "Companion Command: Polearm Action Hit 1",
    "companionpolearmactionhit2": "Companion Command: Polearm Action Hit 2",
    "companionpolearmarea1": "Companion Command: Polearm Area Attack 1",
    "companionpolearmarea2": "Companion Command: Polearm Area Attack 2",
    "companionpolearmhit1": "Companion Command: Polearm Hit 1",
    "companionpolearmhit2": "Companion Command: Polearm Hit 2",
    "companionpolearmhit3": "Companion Command: Polearm Hit 3",
    "companionpolearmleghit1": "Companion Command: Polearm Leg Hit 1",
    "companionpolearmleghit2": "Companion Command: Polearm Leg Hit 2",
    "companionpolearmleghit3": "Companion Command: Polearm Leg Hit 3",
    "companionpolearmlunge2": "Companion Command: Polearm Lunge 2",
    "companionpolearmspinattack1": "Companion Command: Polearm Spin Attack 1",
    "companionpolearmspinattack2": "Companion Command: Polearm Spin Attack 2",
    "companionpolearmstun1": "Companion Command: Polearm Stun 1",
    "companionpolearmstun2": "Companion Command: Polearm Stun 2",
    "companionpolearmsweep1": "Companion Command: Polearm Sweep 1",
    "companionpolearmsweep2": "Companion Command: Polearm Sweep 2",
    "companionpowerboost": "Companion Command: Power Boost",
    "companionquickheal": "Companion Command: Quick Heal",
    "companionrally": "Companion Command: Rally",
    "companionretreat": "Companion Command: Retreat!",
    "companionreviveplayer": "Companion Command: Revive Player",
    "companionrollshot": "Companion Command: Roll Shot",
    "companionscattershot1": "Companion Command: Scatter Shot 1",
    "companionscattershot2": "Companion Command: Scatter Shot 2",
    "companionsteadyaim": "Companion Command: Steady Aim",
    "companionstoppingshot": "Companion Command: Stopping Shot",
    "companionsuppressionfire1": "Companion Command: Suppression Fire 1",
    "companionsuppressionfire2": "Companion Command: Suppression Fire 2",
    "companiontakecover": "Companion Command: Take Cover",
    "companionthreatenshot": "Companion Command: Threaten Shot",
    "companiontumbletokneeling": "Companion Command: Tumble To Kneeling",
    "companiontumbletoprone": "Companion Command: Tumble To Prone",
    "companiontumbletostanding": "Companion Command: Tumble To Standing",
    "companionunarmedblind1": "Companion Command: Unarmed Blind 1",
    "companionunarmedbodyhit1": "Companion Command: Unarmed Body Hit 1",
    "companionunarmedcombo1": "Companion Command: Unarmed Combo 1",
    "companionunarmedcombo2": "Companion Command: Unarmed Combo 2",
    "companionunarmeddizzy1": "Companion Command: Unarmed Dizzy 1",
    "companionunarmedheadhit1": "Companion Command: Unarmed Head Hit 1",
    "companionunarmedhit1": "Companion Command: Unarmed Hit 1",
    "companionunarmedhit2": "Companion Command: Unarmed Hit 2",
    "companionunarmedhit3": "Companion Command: Unarmed Hit 3",
    "companionunarmedknockdown1": "Companion Command: Unarmed Knockdown 1",
    "companionunarmedknockdown2": "Companion Command: Unarmed Knockdown 2",
    "companionunarmedleghit1": "Companion Command: Unarmed Leg Hit 1",
    "companionunarmedlunge2": "Companion Command: Unarmed Lunge 2",
    "companionunarmedspinattack1": "Companion Command: Unarmed Spin Attack 1",
    "companionunarmedspinattack2": "Companion Command: Unarmed Spin Attack 2",
    "companionunarmedstun1": "Companion Command: Unarmed Stun 1",
    "companionvolleyfire": "Companion Command: Volley Fire",
    "companionwarcry2": "Companion Command: Warcry 2",
    "companionwarningshot": "Companion Command: Warning Shot",
    "companionwildshot1": "Companion Command: Wild Shot 1",
    "companionwildshot2": "Companion Command: Wild Shot 2",
}


def build_cmd_n_stf():
    """Companion System (IMPORTANT #4): the real string/en/cmd_n.stf
    (highest-priority stock copy: patch_14_00.tre) is what the client's
    "Commands and Abilities Granted" info panel resolves each COMMANDS
    entry through (case-insensitively, splitting on '+' for the real
    "basecommand+argument" convention), falling back to literal
    "cmd_n:[key]" text on a miss -- confirmed by cross-checking 727/727
    non-"private_"/"cert_"-prefixed COMMANDS entries in the base skills.iff
    (98.8%, with the few remaining misses being genuine stock dead/test
    content). Adds one entry per genuinely-intended custom companion
    command (see CMD_N_ENTRIES above)."""
    table = StfTable.parse(open(os.path.join(EXTRACTED, "cmd_n.stf"), "rb").read())
    for k, v in CMD_N_ENTRIES.items():
        table.add(k, v)
    outPath = os.path.join(OUT, "cmd_n.stf")
    with open(outPath, "wb") as f:
        f.write(table.serialize())
    print(f"cmd_n.stf: added {len(CMD_N_ENTRIES)} entries -> {outPath} ({os.path.getsize(outPath)} bytes)")


def build_companion_stf():
    table = StfTable()
    for k, v in COMPANION_STF_ENTRIES:
        table.add(k, v)
    outPath = os.path.join(OUT, "companion.stf")
    with open(outPath, "wb") as f:
        f.write(table.serialize())
    print(f"companion.stf: created new with {len(COMPANION_STF_ENTRIES)} entries -> {outPath} ({os.path.getsize(outPath)} bytes)")


if __name__ == "__main__":
    newSkillNames = build_skills_iff()
    build_xp_limits_iff()
    build_exp_n_stf()
    build_skl_n_stf()
    build_skl_t_stf()
    build_skl_d_stf()
    build_creature_names_stf()
    build_companion_stf()
    build_skill_teacher_stf()
    build_stat_n_stf()
    build_stat_d_stf()
    build_cmd_n_stf()

    missing = [n for n in newSkillNames if n not in SKL_NAMES]
    if missing:
        print("WARNING: no skl_n.stf display name for:", missing)

    missingDesc = [n for n in newSkillNames if n not in SKL_DESCRIPTIONS]
    if missingDesc:
        print("WARNING: no skl_d.stf description for:", missingDesc)

    statMismatch = sorted(set(STAT_N_ENTRIES) ^ set(STAT_D_ENTRIES))
    if statMismatch:
        print("WARNING: stat_n.stf/stat_d.stf key set mismatch:", statMismatch)
