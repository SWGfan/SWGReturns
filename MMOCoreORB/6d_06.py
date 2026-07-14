#!/usr/bin/env python3

from pathlib import Path
import re

FILES = [
    Path("src/server/zone/managers/player/PlayerManagerImplementation.cpp"),
    Path("src/server/zone/objects/player/sessions/EntertainingSessionImplementation.cpp"),
]

VALID_MODS = {
    "healing_ability",
    "combat_healing_ability",
    "combat_medic_effectiveness",
    "healing_wound_speed",
    "healing_injury_speed",
    "healing_range",
    "healing_range_speed",
    "bleeding_defense",
    "disease_defense",
    "poison_defense",
    "resistance_bleeding",
    "resistance_disease",
    "resistance_poison",
    "melee_defense",
    "ranged_defense",
    "dodge",
    "block",
    "counterattack",
    "luck",
    "combat_haste",
    "camouflage",
    "foraging",
    "creature_harvesting",
    "surveying",
    "camp",
    "rescue",
    "take_cover",
    "cover",
    "slope_move",
    "group_slope_move",
    "general_assembly",
    "general_experimentation",
    "weapon_assembly",
    "weapon_experimentation",
    "armor_assembly",
    "armor_experimentation",
    "clothing_assembly",
    "clothing_experimentation",
    "food_experimentation",
    "medicine_experimentation",
    "droid_experimentation",
    "weapon_repair",
    "armor_repair",
    "clothing_repair",
}

pattern = re.compile(
    r'setSkillModifier\s*\(\s*"([^"]+)"\s*,\s*(-?\d+)'
)

print("=" * 60)
print("Support Buff Validator")
print("=" * 60)

for file in FILES:

    if not file.exists():
        print(f"\nMissing: {file}")
        continue

    print(f"\nChecking {file}")

    mods = {}

    for line, text in enumerate(file.read_text(errors="ignore").splitlines(), 1):

        m = pattern.search(text)

        if not m:
            continue

        mod = m.group(1)
        value = int(m.group(2))

        if mod not in VALID_MODS:
            print(f"  [WARNING] Unknown skill mod '{mod}' at line {line}")

        if mod in mods:
            print(
                f"  [DUPLICATE] {mod} "
                f"(line {mods[mod][0]} and {line})"
            )
        else:
            mods[mod] = (line, value)

    print(f"  {len(mods)} unique skill modifiers")

print("\nChecking backups...")

suffixes = [
    ".phase6a.bak",
    ".phase6b.bak",
    ".phase6c.bak",
    ".6d01.bak",
    ".6d02.bak",
    ".6d03.bak",
    ".6d04.bak",
    ".6d05.bak",
]

for file in FILES:
    for suffix in suffixes:
        bak = Path(str(file) + suffix)
        if bak.exists():
            print("  OK:", bak.name)

print("\nValidation complete.")
