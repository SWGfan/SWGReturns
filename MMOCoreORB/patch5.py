#!/usr/bin/env python3

from pathlib import Path
import re
import shutil

PATCHES = {
    "src/server/zone/managers/combat/CombatManager.cpp": [

        # Slightly reduce private damage multiplier effectiveness.
        (
            r'if\s*\(\s*damageMultiplier\s*!=\s*0\s*\)\s*'
            r'damage\s*\*=\s*damageMultiplier\s*;',
            '''if (damageMultiplier != 0) {
                float mult = damageMultiplier;

                // Returns Combat 2.0
                if (mult > 3.0f)
                        mult = 3.0f;

                damage *= mult;
        }'''
        ),

        # Clamp excessive ranged damage bonuses.
        (
            r'damage\s*\+=\s*attacker->getSkillMod\("private_ranged_damage_bonus"\)\s*;',
            '''{
                int rangedBonus = attacker->getSkillMod("private_ranged_damage_bonus");

                if (rangedBonus > 250)
                        rangedBonus = 250;

                damage += rangedBonus;
        }'''
        ),

        # Clamp excessive melee damage bonuses.
        (
            r'damage\s*\+=\s*attacker->getSkillMod\("private_melee_damage_bonus"\)\s*;',
            '''{
                int meleeBonus = attacker->getSkillMod("private_melee_damage_bonus");

                if (meleeBonus > 250)
                        meleeBonus = 250;

                damage += meleeBonus;
        }'''
        ),

        # Clamp private damage bonus.
        (
            r'damage\s*\+=\s*attacker->getSkillMod\("private_damage_bonus"\)\s*;',
            '''{
                int bonus = attacker->getSkillMod("private_damage_bonus");

                if (bonus > 300)
                        bonus = 300;

                damage += bonus;
        }'''
        ),
    ],

    "bin/scripts/commands/strafeShot1.lua": [
        (
            r'actionCostMultiplier\s*=\s*1\.5',
            'actionCostMultiplier = 1.75'
        ),
    ],

    "bin/scripts/commands/strafeShot2.lua": [
        (
            r'actionCostMultiplier\s*=\s*1\.25',
            'actionCostMultiplier = 1.75'
        ),
        (
            r'visMod\s*=\s*25',
            'visMod = 40'
        ),
    ],
}


def backup(path):
    bak = Path(str(path) + ".returns_patch5.bak")
    if not bak.exists():
        shutil.copy2(path, bak)


total = 0

for filename, rules in PATCHES.items():

    path = Path(filename)

    if not path.exists():
        print("Missing:", filename)
        continue

    text = path.read_text(encoding="utf-8")
    original = text

    for pattern, replacement in rules:

        text, count = re.subn(
            pattern,
            replacement,
            text,
            count=1,
            flags=re.MULTILINE | re.DOTALL
        )

        total += count

    if text != original:
        backup(path)
        path.write_text(text, encoding="utf-8")
        print("Patched:", filename)
    else:
        print("Already patched:", filename)

print()
print("==============================")
print("Returns Patch 5 Complete")
print(f"{total} change(s) applied.")
print("==============================")
