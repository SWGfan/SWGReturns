#!/usr/bin/env python3

from pathlib import Path
import re
import shutil

ROOT = Path(".")

PATCHES = {
    "src/server/zone/managers/combat/CombatManager.cpp": [

        # --------------------------------------------------
        # Global player damage 1.5 -> 1.10
        # --------------------------------------------------
        (
            r'if\s*\(\s*attacker->isPlayerCreature\(\)\s*\)\s*damage\s*\*=\s*1\.5\s*;',
            '''// Returns Combat 2.0
        if (attacker->isPlayerCreature())
                damage *= 1.10f;'''
        ),

        # --------------------------------------------------
        # Lair multiplier
        # --------------------------------------------------
        (
            r'damage\s*\*=\s*3\.5\s*;',
            'damage *= 1.75f;'
        ),

        # --------------------------------------------------
        # Melee bonus
        # --------------------------------------------------
        (
            r'damage\s*\*=\s*1\.25\s*;',
            'damage *= 1.35f;'
        ),

        # --------------------------------------------------
        # Insert PvP burst limiter
        # --------------------------------------------------
        (
            r'(damage\s*=\s*applyDamageModifiers\s*\(\s*attacker\s*,\s*weapon\s*,\s*damage\s*,\s*data\s*\)\s*;)',
            r'''\1

        //
        // Returns Combat 2.0
        //
        if (attacker->isPlayerCreature() && defender->isPlayerCreature()) {

                float weaponCap = weapon->getMaxDamage() * 3.0f;

                if (damage > weaponCap)
                        damage = weaponCap;

                float hamCap = defender->getMaxHAM() * 0.35f;

                if (damage > hamCap)
                        damage = hamCap;
        }'''
        ),

        # --------------------------------------------------
        # Armor usefulness
        # --------------------------------------------------
        (
            r'(damage\s*\+=\s*defender->getSkillMod\("private_damage_susceptibility"\)\s*;)',
            r'''\1

        // Returns Combat 2.0
        if (defender->isPlayerCreature())
                damage *= 0.90f;'''
        ),
    ],

    "bin/scripts/commands/strafeShot1.lua": [
        (
            r'damageMultiplier\s*=\s*2\.0',
            'damageMultiplier = 1.40'
        )
    ],

    "bin/scripts/commands/strafeShot2.lua": [
        (
            r'damageMultiplier\s*=\s*2\.0',
            'damageMultiplier = 1.40'
        )
    ]
}


def patch_file(filename, rules):

    path = ROOT / filename

    if not path.exists():
        print(f"Missing: {filename}")
        return 0

    text = path.read_text(encoding="utf-8")
    original = text

    count = 0

    for pattern, replacement in rules:

        text, n = re.subn(
            pattern,
            replacement,
            text,
            count=1,
            flags=re.MULTILINE | re.DOTALL
        )

        count += n

    if text != original:

        bak = Path(str(path) + ".returns_patch1.bak")

        if not bak.exists():
            shutil.copy2(path, bak)

        path.write_text(text, encoding="utf-8")

    return count


total = 0

for file, rules in PATCHES.items():
    changed = patch_file(file, rules)

    if changed:
        print(f"Patched {file} ({changed} changes)")
        total += changed
    else:
        print(f"No changes: {file}")

print("\n==============================")
print(f"Finished. {total} total changes.")
print("==============================")
