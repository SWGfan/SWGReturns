#!/usr/bin/env python3

from pathlib import Path
import re
import shutil

PATCHES = {
    "src/server/zone/managers/combat/CombatManager.cpp": [
        # Player damage multiplier
        (
            r'if\s*\(\s*attacker->isPlayerCreature\(\)\s*\)\s*damage\s*\*=\s*1\.5\s*;',
            '''// Returns Combat Rebalance
        if (attacker->isPlayerCreature())
                damage *= 1.15f;'''
        ),

        # Lair damage
        (
            r'damage\s*\*=\s*3\.5\s*;',
            'damage *= 1.50f;'
        ),

        # Jedi Toughness divisor
        (
            r'jediToughness\s*/\s*500\.f',
            'jediToughness / 325.f'
        ),

        # Saber Toughness divisor
        (
            r'saberToughness\s*/\s*500\.f',
            'saberToughness / 325.f'
        ),

        # Jedi curve
        (
            r'int\s+maxReduction\s*=\s*81\s*;',
            'int maxReduction = 90;'
        ),

        # Saber curve
        (
            r'int\s+maxReduction\s*=\s*80\s*;',
            'int maxReduction = 90;'
        ),
    ],

    "src/server/zone/objects/player/PlayerObjectImplementation.cpp": [

        # Force regen
        (
            r'const\s+static\s+uint32\s+tick\s*=\s*5\s*;',
            'const static uint32 tick = 8;'
        ),

        # Force pool
        (
            r'maxForce\s*\+=\s*\(forcePowerMod\s*\+\s*forceControlMod\)\s*\*\s*10\s*;',
            'maxForce += (forcePowerMod + forceControlMod) * 15;'
        ),
    ],

    "src/server/zone/managers/skill/SkillManager.cpp": [

        (
            r'if\s*\(\s*skill->getSkillName\(\)\s*==\s*"force_title_jedi_rank_02"\s*\)\s*\{',
            '''if (ghost->isJedi()) {'''
        )
    ],

    "src/server/zone/managers/combat/CombatManager.cpp.extra": [

        (
            r'if\s*\(\s*playerObject\s*!=\s*nullptr\s*&&\s*playerObject->isJedi\(\)\s*\)\s*VisibilityManager::instance\(\)->increaseVisibility',
            '''if (playerObject != nullptr && playerObject->isJedi())
                                VisibilityManager::instance()->increaseVisibility'''
        )
    ]
}


def backup(path):
    bak = Path(str(path) + ".returns_rebalance.bak")
    if not bak.exists():
        shutil.copy2(path, bak)


total = 0

for filename, rules in PATCHES.items():

    realfile = filename.replace(".extra", "")
    path = Path(realfile)

    if not path.exists():
        print("Missing:", realfile)
        continue

    text = path.read_text(encoding="utf-8")
    original = text

    for pattern, repl in rules:

        text, count = re.subn(
            pattern,
            repl,
            text,
            count=1,
            flags=re.MULTILINE | re.DOTALL
        )

        if count:
            total += count
            print(f"Patched {realfile}")

    if text != original:
        backup(path)
        path.write_text(text, encoding="utf-8")

print()
print("====================================")
print(f"Finished. {total} change(s) applied.")
print("====================================")
