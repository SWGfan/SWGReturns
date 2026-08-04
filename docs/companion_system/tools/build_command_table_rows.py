#!/usr/bin/env python3
"""
Appends 7 new baseline rows (originally 6; /companionformup added 2026-07-14,
"Form Up" macro pass) to the client's datatables/command/command_table.iff so
that /hpet, /companionattack, /companionfollow, /companionstay,
/companionpatrol, /companionstore, /companionformup are recognized by the
CLIENT's own local chat-command gate (which currently rejects them with "No
such command, mood, chat type: X" before they're ever sent to the server --
confirmed this session to be a client-side-only string, not present anywhere
in Core3's own source).

Base file: extracted/command_table.iff (771 rows x 75 cols), extracted from
SWGEmu/patch_14_00.tre and round-trip-verified byte-identical via
iff_datatable.py's own verify_roundtrip() before this script trusts it.

Column layout matches CommandConfigManager.h's COMMANDNAME=0,
DEFAULTPRIORITY=1, ... constants exactly (verified against dt.columns).

New rows are built by cloning real precedent rows (attack / tellpet) and
overriding only the fields that need to differ, rather than hand-building
75 columns from scratch -- see inline comments per command for exactly what
changed vs. its template and why.
"""
import sys, os, struct
sys.path.insert(0, os.path.dirname(__file__))
from iff_datatable import DataTable, verify_roundtrip

SRC = "extracted/command_table.iff"
OUT = "patched/command_table.iff"

with open(SRC, "rb") as f:
    data = f.read()

dt, end = DataTable.parse(data)
assert end == len(data)
# Base row count is NOT fixed: it depends which client content the base
# command_table.iff came from. SWGEmu stock has 771 rows; aftermath_1.tre's
# has 779 (8 extra commands). What must hold is the COLUMN layout -- 75
# columns -- because every row this script appends is built positionally
# against that schema. Guard the schema, not the row count.
assert len(dt.columns) == 75, f"unexpected column count {len(dt.columns)} (expected 75)"
assert len(dt.rows) >= 771, f"suspiciously small base table: {len(dt.rows)} rows"
BASE_ROWS = len(dt.rows)
print(f"base command_table.iff: {BASE_ROWS} rows, {len(dt.columns)} columns")

colnames = [c[0] for c in dt.columns]
baseTypes = [t[0] for (_, t) in dt.columns]
colIndex = {name: i for i, name in enumerate(colnames)}


def enc(colname, pyval):
    """Encode a python value into this table's raw 4-byte (or string) cell
    representation for the given column, matching its declared base type."""
    i = colIndex[colname]
    bt = baseTypes[i]
    if bt == "s":
        return pyval
    elif bt == "f":
        return struct.pack("<f", float(pyval))
    else:  # 'i', 'e' (enum-as-int32), 'b' (bool-as-int32), 'h' (hash-as-uint32)
        return struct.pack("<i", int(pyval))


def get_row(name):
    for r in dt.rows:
        if r[0] == name:
            return r
    raise KeyError(name)


attack_row = get_row("attack")
tellpet_row = get_row("tellpet")


def clone(template_row):
    return list(template_row)


def set_col(row, colname, pyval):
    row[colIndex[colname]] = enc(colname, pyval)


new_rows = []

# ---------------------------------------------------------------------
# /hpet -- cloned from `tellpet` (closest real precedent for a
# chat-typed, optionally-targeted instruction issued to a pet/companion,
# usable in almost every locomotion/state). Deviations from tellpet:
#   - commandName -> "hpet"
#   - scriptHook cleared (tellpet's is Lua-side "cmdTellPet"; ours is a
#     pure C++ QueueCommand already registered via commandFactory, and
#     CPPHOOK/SCRIPTHOOK are confirmed dead columns server-side --
#     CommandConfigManager.cpp's loadCommandData() never reads either
#     one, it dispatches purely by looking up commandName in
#     commandFactory)
#   - tempScript cleared: tellpet's tempScript="player.skill.taming"
#     gates it behind the Creature Handler taming skill. The whole
#     companion system is deliberately isolated from the CH skill tree
#     (see NOTES.md "Skill point isolation"); copying that value would
#     silently re-couple /hpet to a skill tree it has nothing to do
#     with, so it's blanked instead.
#   - S:stunned flipped 1 -> 0 per spec (command must NOT be usable
#     while stunned/dead/incapacitated). tellpet otherwise already has
#     L:incapacitated=0 and L:dead=0, so only stunned needed changing.
# ---------------------------------------------------------------------
hpet = clone(tellpet_row)
set_col(hpet, "commandName", "hpet")
set_col(hpet, "defaultPriority", 0)  # immediate, matches tellpet
set_col(hpet, "scriptHook", "")
set_col(hpet, "failScriptHook", "")
set_col(hpet, "cppHook", "")
set_col(hpet, "failCppHook", "")
set_col(hpet, "defaultTime", 1.0)  # matches tellpet
set_col(hpet, "characterAbility", "")
set_col(hpet, "S:stunned", 0)
set_col(hpet, "targetType", 2)  # optional -- usable with or without a selected target
set_col(hpet, "tempScript", "")
set_col(hpet, "stringId", "")
set_col(hpet, "visible", 0)
set_col(hpet, "commandGroup", 0)
set_col(hpet, "disabled", 0)
set_col(hpet, "godLevel", 0)
set_col(hpet, "displayGroup", 0)
set_col(hpet, "addToCombatQueue", 0)
new_rows.append(hpet)

# ---------------------------------------------------------------------
# /companionattack -- cloned from `attack` (real combat-capable
# precedent: correct locomotion/state profile for issuing an attack
# order while standing/sneaking/walking/running/kneeling/etc., blocked
# while doing things incompatible with combat like sitting/climbing/
# driving). Deviations from attack:
#   - commandName -> "companionattack"
#   - scriptHook cleared (attack's Lua scriptHook "attack" is the
#     player's OWN melee/ranged attack resolution; our C++ command is
#     a completely separate companion-order dispatch, already
#     registered via commandFactory, so this must not point at the
#     player attack script)
#   - S:alert flipped 0 -> 1 and S:stunned flipped 1 -> 0, matching the
#     explicit requirement that this be usable from standing / walking
#     / running / combat / peace / alert, and NOT from stunned / dead /
#     incapacitated (attack's own S:alert=0 appears to be a vanilla
#     quirk unrelated to our command's actual reachable states -- our
#     command is issued via radial menu from the alert state today
#     (confirmed working), so leaving alert blocked here would be a
#     regression versus already-working radial dispatch)
#   - commandGroup cleared to 0 (attack's real commandGroup hash
#     0x4aa5c9e1 ties it into the base melee/ranged combat-queue
#     interrupt group shared with poison/disease/turret/droid attacks
#     etc; companionattack is a distinct, independently-queued
#     companion order and should not join that interrupt group)
#   - visible cleared to 0 (attack's visible=3 is a base-combat-ability
#     UI category value with no defined meaning for a custom command;
#     0 is the safe/neutral default used by non-core commands like
#     tellpet)
# ---------------------------------------------------------------------
companionattack = clone(attack_row)
set_col(companionattack, "commandName", "companionattack")
set_col(companionattack, "scriptHook", "")
set_col(companionattack, "failScriptHook", "")
set_col(companionattack, "cppHook", "")
set_col(companionattack, "failCppHook", "")
set_col(companionattack, "characterAbility", "companion_attack")
set_col(companionattack, "S:alert", 1)
set_col(companionattack, "S:stunned", 0)
set_col(companionattack, "targetType", 2)  # optional -- uses current combat target if none given
set_col(companionattack, "tempScript", "")
set_col(companionattack, "stringId", "")
set_col(companionattack, "visible", 2)
set_col(companionattack, "commandGroup", 0)
set_col(companionattack, "disabled", 0)
set_col(companionattack, "godLevel", 0)
set_col(companionattack, "displayGroup", 0)
new_rows.append(companionattack)

# ---------------------------------------------------------------------
# Companion System (2026-07-13, "macro list" pass) -- the five baseline
# order commands above (hpet excluded -- it stays a free-text /hpet
# <macro> chat command, not an owner-macro-list ability) are now also
# real, owner-clickable macro/hotbar abilities:
#   - visible flipped 0 -> 2 (the single most common "shown + draggable
#     to hotbar" value across all 771 real stock rows -- see NOTES.md;
#     deliberately reusing this proven-common value rather than
#     inventing/guessing a new one, same caution as the earlier
#     GRAPH_TYPE lesson)
#   - characterAbility set to "companion_<order>" -- gates the command
#     centrally via the real engine's own PlayerObject::hasAbility()
#     check (ObjectControllerImplementation.cpp) exactly like every
#     stock combat ability already does for itself. CompanionSkillTrainer
#     grants these once, permanently, to the owner's own abilityList the
#     first time they ever get a companion (first-launch flow) -- see
#     CompanionSkillTrainer.cpp and CompanionControlDeviceImplementation.
# companionattack's own characterAbility/visible are set directly above
# since it's built from a different template (attack_row); this loop
# only re-patches the four simple order commands built from tellpet_row.
# ---------------------------------------------------------------------
_BASELINE_ABILITY_NAMES = {
    "companionfollow": "companion_follow",
    "companionstay": "companion_stay",
    "companionpatrol": "companion_patrol",
    "companionstore": "companion_store",
    # Companion System (2026-07-14, "Form Up" macro pass) -- new 5th simple
    # order command, same tellpet-clone shape as the other four. See
    # CompanionFormupCommand.h / FormationManager.cpp.
    "companionformup": "companion_formup",
    # Companion System (2026-07-17, "pet command port" pass, per user
    # request) -- the remaining Creature Handler pet orders ported as
    # companion equivalents. See Companion{Guard,FollowOther,RangedAttack,
    # SpecialAttack,Group,Friend}Command.h.
    "companionguard": "companion_guard",
    "companionfollowother": "companion_followother",
    "companionrangedattack": "companion_rangedattack",
    "companionspecialone": "companion_specialone",
    "companionspecialtwo": "companion_specialtwo",
    "companiongroup": "companion_group",
    "companionfriend": "companion_friend",
    # Companion System (2026-07-20, "massive battlefield" pass, per user
    # request) -- recalls a posted companion back to its last Stay/Guard
    # position. See CompanionReturnCommand.h.
    "companionreturn": "companion_return",
    # Companion System (2026-07-20, "crafting theater" pass, per user
    # request) -- companion crafts a real item from a draft schematic path
    # argument. See CompanionCraftCommand.h.
    "companioncraft": "companion_craft",
    "companionrequestarmor": "",  # no ability gate -- server-side command already validates armorsmith/target/combat
    # Companion System (2026-07-25, "Jenkins" pass -- formerly named "Muster
    # Call" internally, renamed to "jenkins" per user request) -- summons
    # every stored companion + pet from the datapad and forms them up in
    # escort. See CompanionJenkinsCommand.h.
    #
    # Unlike companionrequestarmor (deliberately left with no ability gate
    # -- radial-only, never meant to be hotbar-draggable), Jenkins DOES get
    # a real characterAbility gate ("companion_jenkins", granted via
    # CompanionSkillTrainer::grantBaselineOwnerOrderAbilities()) because the
    # user explicitly wants it as a hotbar macro icon (2026-07-25, "i also
    # want a icon macro i can use") -- the client's ability/command browser
    # only lists commands tied to an ability the player actually has, an
    # empty-gate command is typable but never appears there to drag.
    "jenkins": "companion_jenkins",
}

# Companion System (2026-07-17, "pet command port" pass) -- unlike the
# original five self-directed order commands (targetType=0), most of the
# ported pet orders act on the OWNER'S CURRENT TARGET (guard target, escort
# target, attack target, friend target), matching companionattack's own
# targetType=2 ("optional -- uses current combat target if none given").
# companiongroup stays a pure self-directed toggle.
_TARGET_TYPE_OVERRIDES = {
    "companionguard": 2,
    "companionfollowother": 2,
    "companionrangedattack": 2,
    "companionspecialone": 2,
    "companionspecialtwo": 2,
    "companionfriend": 2,
    # Companion System (2026-07-20, "crafting theater" pass) -- resolves the
    # owner's current target (the companion to craft with), same as the
    # other target-taking baseline commands above. See CompanionCraftCommand.h.
    "companioncraft": 2,
    "companionrequestarmor": 2,  # targets the armorsmith companion, same as companioncraft
}

# ---------------------------------------------------------------------
# /companionfollow, /companionstay, /companionpatrol, /companionstore --
# all cloned from `tellpet` (simple instruction issued to a companion,
# reachable from nearly every locomotion/state) rather than from a real
# petFollow/petStay/petPatrol/petStore row, because **no such rows
# exist in this table** -- confirmed by scanning all 771 rows for
# "follow"/"stay"/"patrol"/"store" substrings: those Creature-Handler
# pet orders are click-only (dispatched straight from the radial menu
# via executeObjectControllerAction(), never through the chat-command
# parser this table gates), so they were never given command_table.iff
# rows in the stock game at all. That is itself further confirmation
# of this whole bug's root cause: pet-order-style commands are commonly
# NOT typeable in vanilla SWG either, and the only rows that exist for
# similar "simple self-directed order, no target" semantics are
# formup/setFormup (targetType=none) and tellpet (used here as the
# base for its broadly-permissive L:/S: flag profile).
#
# Deviations from tellpet, common to all four:
#   - scriptHook/cppHook cleared (see /hpet rationale above)
#   - tempScript cleared (see /hpet rationale above -- these commands
#     must not be gated behind CH taming skill either)
#   - targetType set to 0 (none) rather than tellpet's 2 (optional):
#     these are pure self-directed state toggles on the player's own
#     active companion (resolved server-side via datapad scan, see
#     NOTES.md "Active companion resolution") with no target concept
#     at all, matching the formup/setFormup precedent's targetType=0
#     more precisely than tellpet's optionally-targeted semantics.
#   - S:stunned flipped 1 -> 0 per spec
# ---------------------------------------------------------------------
def make_simple_companion_command(name):
    row = clone(tellpet_row)
    set_col(row, "commandName", name)
    set_col(row, "defaultPriority", 0)  # immediate, matches tellpet
    set_col(row, "scriptHook", "")
    set_col(row, "failScriptHook", "")
    set_col(row, "cppHook", "")
    set_col(row, "failCppHook", "")
    set_col(row, "defaultTime", 1.0)  # matches tellpet
    set_col(row, "characterAbility", _BASELINE_ABILITY_NAMES[name])
    set_col(row, "S:stunned", 0)
    set_col(row, "targetType", _TARGET_TYPE_OVERRIDES.get(name, 0))  # default: none -- self-directed companion state toggle
    set_col(row, "tempScript", "")
    set_col(row, "stringId", "")
    set_col(row, "visible", 2)
    set_col(row, "commandGroup", 0)
    set_col(row, "disabled", 0)
    set_col(row, "godLevel", 0)
    set_col(row, "displayGroup", 0)
    set_col(row, "addToCombatQueue", 0)
    return row


for name in ["companionfollow", "companionstay", "companionpatrol", "companionstore", "companionformup",
             "companionguard", "companionfollowother", "companionrangedattack",
             "companionspecialone", "companionspecialtwo", "companiongroup", "companionfriend",
             "companionreturn", "companioncraft", "companionrequestarmor", "jenkins"]:
    new_rows.append(make_simple_companion_command(name))

_BASELINE_ROW_NAMES = [
    "hpet", "companionattack", "companionfollow", "companionstay",
    "companionpatrol", "companionstore", "companionformup",
    "companionguard", "companionfollowother", "companionrangedattack",
    "companionspecialone", "companionspecialtwo", "companiongroup", "companionfriend",
    "companionreturn", "companioncraft", "companionrequestarmor", "jenkins",
]

assert len(new_rows) == 18
assert [r[0] for r in new_rows] == _BASELINE_ROW_NAMES

# ---------------------------------------------------------------------
# Companion System (2026-07-13, "macro list" pass) -- one row per
# real, invokable skill-granted companion ability (CompanionAbilityCommand.h
# in C++, registered as "companion<Ability>" in CommandConfigManager2.cpp).
# List is the output of scanning extracted/skills.iff's COMMANDS column for
# the 11 supported combat professions (see resolveProfessionToken() in
# CompanionSkillTrainer) and keeping only strings that already have a real
# registered QueueCommand elsewhere in CommandConfigManager*.cpp (certs,
# private_rifle_* progression tokens, ranged_damage_mitigation_*,
# droid_find/droid_track, place_hospital, and sneak are passive skill mods
# or utility tokens with no dispatchable action and are excluded).
#
# Each row is cloned from the REAL ability's own row (e.g. "bleedingShot")
# rather than built from scratch, preserving its already-correct locomotion/
# state profile, targetType (all 36 are targetType=2, optional), and
# addToCombatQueue -- only the fields that must differ for a companion-
# dispatched, characterAbility-gated copy are overridden:
#   - commandName -> "companion" + ability
#   - scriptHook/failScriptHook cleared (real value is the Lua script for
#     the PLAYER performing their own attack; CompanionAbilityCommand.h is a
#     self-contained C++ QueueCommand that dispatches on the companion
#     instead, same rationale as /companionattack above)
#   - characterAbility -> "companion_" + ability.lower() (the new,
#     distinct owner-side ability string CompanionSkillTrainer::
#     trainSkill()/untrainSkill() grants/revokes on the owner's own
#     PlayerObject::abilityList as the companion learns/untrains the real
#     skill that grants that ability -- see CompanionSkillTrainer.cpp)
#   - tempScript cleared (must not inherit the real skill-tree gate string
#     -- companion_<ability> is the sole gate now, same rationale as
#     /hpet/companionattack above)
#   - commandGroup cleared to 0 (companion abilities are independently
#     queued companion orders, not part of the player's own combat-queue
#     interrupt group, same rationale as /companionattack above)
#   - stringId cleared (no client localized tooltip authored yet)
#   - visible/targetType/addToCombatQueue/state+locomotion mask left as
#     cloned from the real row -- already correct and already proven (it's
#     the same real ability), no reason to override.
# ---------------------------------------------------------------------
_COMPANION_ABILITY_NAMES = [
    "applyDisease", "applyPoison", "bleedingShot", "concealShot", "confusionShot",
    "eyeShot", "fastBlast", "fireAcidCone1", "fireAcidCone2", "fireAcidSingle1",
    "fireAcidSingle2", "fireLightningCone1", "fireLightningCone2",
    "fireLightningSingle1", "fireLightningSingle2", "flameCone1", "flameCone2",
    "flameSingle1", "flameSingle2", "flurryShot1", "flurryShot2", "flushingShot1",
    "flushingShot2", "headShot3", "healMind", "knockdownFire", "mindShot2",
    "sniperShot", "sprayShot", "startleShot1", "startleShot2", "strafeShot1",
    "strafeShot2", "surpriseShot", "torsoShot", "underHandShot",
]


def get_row_ci(name):
    for r in dt.rows:
        if r[0].lower() == name.lower():
            return r
    raise KeyError(name)


def make_companion_ability_command(ability):
    row = clone(get_row_ci(ability))
    companionName = "companion" + ability
    set_col(row, "commandName", companionName)
    set_col(row, "scriptHook", "")
    set_col(row, "failScriptHook", "")
    set_col(row, "cppHook", "")
    set_col(row, "failCppHook", "")
    set_col(row, "characterAbility", "companion_" + ability.lower())
    set_col(row, "tempScript", "")
    set_col(row, "stringId", "")
    set_col(row, "commandGroup", 0)
    set_col(row, "disabled", 0)
    set_col(row, "godLevel", 0)
    # Companion System (2026-07-13, "starter profession macro list" pass) --
    # a handful of real ability rows (e.g. harvestCorpse) are visible=0 in
    # stock data (hidden from the client's own command browser for reasons
    # unrelated to us). Since the whole point of a companion<Ability> row is
    # to be owner-hotbar-draggable, force visible up to 2 (the single most
    # common "shown" value, same reasoning as the baseline order commands)
    # whenever the cloned row would otherwise be hidden -- but leave an
    # already-nonzero value (e.g. sample/survey's real visible=3) alone
    # rather than overriding a proven-working existing category.
    visibleIdx = colIndex["visible"]
    currentVisible = struct.unpack("<i", row[visibleIdx])[0]
    if currentVisible == 0:
        set_col(row, "visible", 2)
    return row


ability_rows = [make_companion_ability_command(a) for a in _COMPANION_ABILITY_NAMES]
new_rows.extend(ability_rows)

# ---------------------------------------------------------------------
# Companion System (2026-07-13, "starter profession macro list" pass) --
# closes the follow-up explicitly flagged in the prior pass's NOTES.md
# entry: extend the same real-command + characterAbility + owner-
# ability-list treatment to the 6 starter (non-badge-gated) professions'
# own real invokable commands (crafting_artisan/combat_brawler/
# combat_marksman/science_medic/outdoors_scout/social_entertainer
# novice-tier skills.iff COMMANDS entries). Same filtering discipline as
# the 36 badge-gated abilities above: every name below is confirmed to
# have both a real registered QueueCommand (commandFactory) and a real
# command_table.iff row already -- verified via the same brute-force
# scan used for the master-profession list. Unlike that list, several of
# the "+variant"/argument-taking commands from these six trees
# (flourish+1..8, startdance+basic, startmusic+starwars1, slitherhorn,
# musician/dancer/imagedesigner category-unlock markers, cert_* weapon
# certifications) are deliberately excluded for the identical reason
# documented in CompanionSkillTrainer.cpp's ABILITY_MACRO_DESCRIPTIONS
# comment -- they either take an argument /hpet's/this dispatcher's
# single-macro pipeline never forwards, or aren't independently
# invokable actions at all.
# ---------------------------------------------------------------------
_STARTER_ABILITY_NAMES = [
    "healDamage", "healWound", "tendWound", "tendDamage", "diagnose",
    "medicalForage", "harvestCorpse", "startDance", "stopDance",
    "startMusic", "stopMusic", "sample", "survey", "warcry1",
    "intimidate1", "berserk1", "taunt", "polearmLunge1", "unarmedLunge1",
    "melee1hLunge1", "melee2hLunge1", "centerOfBeing", "pointBlankArea1",
    "pointBlankSingle1", "overchargeShot1",
]

starter_ability_rows = [make_companion_ability_command(a) for a in _STARTER_ABILITY_NAMES]
new_rows.extend(starter_ability_rows)

assert len(new_rows) == 18 + len(_COMPANION_ABILITY_NAMES) + len(_STARTER_ABILITY_NAMES)
assert [r[0] for r in new_rows[:18]] == _BASELINE_ROW_NAMES
_afterBaseline = new_rows[18:]
assert [r[0] for r in _afterBaseline[:len(_COMPANION_ABILITY_NAMES)]] == ["companion" + a for a in _COMPANION_ABILITY_NAMES]
assert [r[0] for r in _afterBaseline[len(_COMPANION_ABILITY_NAMES):]] == ["companion" + a for a in _STARTER_ABILITY_NAMES]

dt.rows.extend(new_rows)
# Compare against the base this run actually read, not a baked-in 771 --
# the base differs per client content (SWGEmu 771, aftermath 779).
assert len(dt.rows) == BASE_ROWS + len(new_rows), \
    f"expected {BASE_ROWS} + {len(new_rows)} rows, got {len(dt.rows)}"
print(f"final command_table.iff: {len(dt.rows)} rows ({BASE_ROWS} base + {len(new_rows)} companion)")

rebuilt = dt.serialize()

os.makedirs("patched", exist_ok=True)
with open(OUT, "wb") as f:
    f.write(rebuilt)

print(f"wrote {OUT}: {len(dt.rows)} rows, {len(rebuilt)} bytes")

# Re-parse what we just wrote and confirm it parses back to the same
# logical content (all original 771 rows unchanged, all new rows present
# with expected commandName values, each exactly once) as an independent
# sanity check on top of the base-file round-trip already verified
# separately.
dt2, end2 = DataTable.parse(rebuilt)
assert end2 == len(rebuilt)
assert len(dt2.rows) == BASE_ROWS + len(new_rows), \
    f"re-parse mismatch: {len(dt2.rows)} vs {BASE_ROWS}+{len(new_rows)}"
assert dt2.rows[:771] == dt.rows[:771]
names2 = [r[0] for r in dt2.rows]
expected_names = list(_BASELINE_ROW_NAMES) + ["companion" + a for a in _COMPANION_ABILITY_NAMES] + ["companion" + a for a in _STARTER_ABILITY_NAMES]
for expected in expected_names:
    assert names2.count(expected) == 1, expected
print(f"post-write re-parse sanity check OK ({len(expected_names)} companion rows verified)")
