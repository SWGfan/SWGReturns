"""
Companion System -- single shared source of truth for the 203 companion
mirror ability names.

Split out 2026-08-09 (batch 40) from build_command_table_rows.py, where
these lists originally lived, so build_companion_content.py can generate
the new per-ability hidden gating skills (see NOTES.md batch 39/40 -- the
visible=2 "held ability" check turned out not to filter anything; box-
membership alone is the real Command Browser gate, confirmed live) WITHOUT
hand-copying the list a second time. This is the exact bug class flagged in
NOTES.md batch 32 ("4th occurrence") -- two independently-maintained copies
of the same name list drifting apart -- so this module exists specifically
to prevent a 5th occurrence. build_command_table_rows.py now imports these
same lists instead of defining them inline; nothing about its own row
generation changed.

Pure data + no side effects: safe for any tool script to import without
triggering file I/O (unlike build_command_table_rows.py itself, which reads
and writes real files at module-import time).
"""

_COMPANION_ABILITY_NAMES_ORIGINAL_36 = [
    "applyDisease", "applyPoison", "bleedingShot", "concealShot", "confusionShot",
    "eyeShot", "fastBlast", "fireAcidCone1", "fireAcidCone2", "fireAcidSingle1",
    "fireAcidSingle2", "fireLightningCone1", "fireLightningCone2",
    "fireLightningSingle1", "fireLightningSingle2", "flameCone1", "flameCone2",
    "flameSingle1", "flameSingle2", "flurryShot1", "flurryShot2", "flushingShot1",
    "flushingShot2", "headShot3", "healMind", "knockdownFire", "mindShot2",
    "sniperShot", "sprayShot", "startleShot1", "startleShot2", "strafeShot1",
    "strafeShot2", "surpriseShot", "torsoShot", "underHandShot",
]

_NEW_COMPANION_ABILITY_NAMES_2026_08_07 = [
    "actionShot1", "actionShot2", "aim", "berserk2", "bodyShot1",
    "bodyShot2", "bodyShot3", "boostmorale", "burstShot1", "burstShot2",
    "chargeShot1", "chargeShot2", "cripplingShot", "cureDisease", "curePoison",
    "dazzle", "disarmingShot1", "disarmingShot2", "diveShot", "doubleTap",
    "dragIncapacitatedPlayer", "extinguishFire", "fanShot", "feignDeath", "firstAid",
    "forage", "forceOfWill", "fullAutoArea1", "fullAutoArea2", "fullAutoSingle1",
    "fullAutoSingle2", "headShot1", "headShot2", "healEnhance", "healState",
    "healthShot1", "healthShot2", "intimidate2", "kipUpShot", "lastDitch",
    "legShot1", "legShot2", "legShot3", "lowBlow", "maskscent",
    "meditate", "melee1hBlindHit1", "melee1hBlindHit2", "melee1hBodyHit1", "melee1hBodyHit2",
    "melee1hBodyHit3", "melee1hDizzyHit1", "melee1hDizzyHit2", "melee1hHealthHit1", "melee1hHealthHit2",
    "melee1hHit1", "melee1hHit2", "melee1hHit3", "melee1hLunge2", "melee1hScatterHit1",
    "melee1hScatterHit2", "melee1hSpinAttack1", "melee1hSpinAttack2", "melee2hArea1", "melee2hArea2",
    "melee2hArea3", "melee2hHeadHit1", "melee2hHeadHit2", "melee2hHeadHit3", "melee2hHit1",
    "melee2hHit2", "melee2hHit3", "melee2hLunge2", "melee2hMindHit1", "melee2hMindHit2",
    "melee2hSpinAttack1", "melee2hSpinAttack2", "melee2hSweep1", "melee2hSweep2", "mindShot1",
    "multiTargetPistolShot", "overChargeShot2", "panicShot", "pistolMeleeDefense1", "pistolMeleeDefense2",
    "pointBlankArea2", "pointBlankSingle2", "polearmActionHit1", "polearmActionHit2", "polearmArea1",
    "polearmArea2", "polearmHit1", "polearmHit2", "polearmHit3", "polearmLegHit1",
    "polearmLegHit2", "polearmLegHit3", "polearmLunge2", "polearmSpinAttack1", "polearmSpinAttack2",
    "polearmStun1", "polearmStun2", "polearmSweep1", "polearmSweep2", "powerBoost",
    "quickHeal", "rally", "retreat", "revivePlayer", "rollShot",
    "scatterShot1", "scatterShot2", "steadyaim", "stoppingShot", "suppressionFire1",
    "suppressionFire2", "takeCover", "threatenShot", "tumbleToKneeling", "tumbleToProne",
    "tumbleToStanding", "unarmedBlind1", "unarmedBodyHit1", "unarmedCombo1", "unarmedCombo2",
    "unarmedDizzy1", "unarmedHeadHit1", "unarmedHit1", "unarmedHit2", "unarmedHit3",
    "unarmedKnockdown1", "unarmedKnockdown2", "unarmedLegHit1", "unarmedLunge2", "unarmedSpinAttack1",
    "unarmedSpinAttack2", "unarmedStun1", "volleyFire", "warcry2", "warningShot",
    "wildShot1", "wildShot2",
]

_COMPANION_ABILITY_NAMES = _COMPANION_ABILITY_NAMES_ORIGINAL_36 + _NEW_COMPANION_ABILITY_NAMES_2026_08_07

_STARTER_ABILITY_NAMES = [
    "healDamage", "healWound", "tendWound", "tendDamage", "diagnose",
    "medicalForage", "harvestCorpse", "startDance", "stopDance",
    "startMusic", "stopMusic", "sample", "survey", "warcry1",
    "intimidate1", "berserk1", "taunt", "polearmLunge1", "unarmedLunge1",
    "melee1hLunge1", "melee2hLunge1", "centerOfBeing", "pointBlankArea1",
    "pointBlankSingle1", "overchargeShot1",
]

# The full 203-name set every one of the 3 lists above combines into.
# Order matches build_command_table_rows.py's own emission order
# (_COMPANION_ABILITY_NAMES's 178 first, then _STARTER_ABILITY_NAMES's 25).
ALL_MIRROR_ABILITY_NAMES = _COMPANION_ABILITY_NAMES + _STARTER_ABILITY_NAMES

if __name__ == "__main__":
    assert len(set(n.lower() for n in ALL_MIRROR_ABILITY_NAMES)) == len(ALL_MIRROR_ABILITY_NAMES), \
        "duplicate ability name found (case-insensitive)"
    print(f"OK: {len(ALL_MIRROR_ABILITY_NAMES)} unique mirror ability names "
          f"({len(_COMPANION_ABILITY_NAMES_ORIGINAL_36)} original + "
          f"{len(_NEW_COMPANION_ABILITY_NAMES_2026_08_07)} 2026-08-07 + "
          f"{len(_STARTER_ABILITY_NAMES)} starter)")
