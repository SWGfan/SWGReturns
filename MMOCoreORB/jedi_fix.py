#!/usr/bin/env python3

from pathlib import Path
import re
import shutil

PATCHES = {

"src/server/zone/managers/combat/CombatManager.cpp":[

# Jedi Toughness
(
r'jediToughness\s*/\s*500\.f',
'jediToughness / 325.f'
),

# Saber Toughness
(
r'saberToughness\s*/\s*500\.f',
'saberToughness / 325.f'
),

# JT curve
(
r'int\s+maxReduction\s*=\s*81\s*;',
'int maxReduction = 90;'
),

# LS curve
(
r'int\s+maxReduction\s*=\s*80\s*;',
'int maxReduction = 90;'
),

],

"src/server/zone/objects/player/PlayerObjectImplementation.cpp":[

# Force Regen
(
r'const\s+static\s+uint32\s+tick\s*=\s*5\s*;',
'const static uint32 tick = 8;'
),

# Force Pool
(
r'maxForce\s*\+=\s*\(forcePowerMod\s*\+\s*forceControlMod\)\s*\*\s*10\s*;',
'maxForce += (forcePowerMod + forceControlMod) * 15;'
),

],

"src/server/zone/managers/skill/SkillManager.cpp":[

(
r'if\s*\(\s*skill->getSkillName\(\)\s*==\s*"force_title_jedi_rank_02"\s*\)',
'if (ghost->isJedi())'
)

],

"src/server/zone/objects/player/PlayerObjectImplementation.cpp.extra":[

(
r'playerCreature->hasSkill\("force_title_jedi_rank_02"\)',
'isJedi()'
)

],

"src/server/zone/managers/combat/CombatManager.cpp.force":[

(
r'if\s*\(\s*playerObject\s*!=\s*nullptr\s*&&\s*playerObject->isJedi\(\)\s*\)\s*VisibilityManager::instance',
'''if (playerObject != nullptr && playerObject->isJedi())
                                VisibilityManager::instance'''
)

]

}


def backup(path):
    bak = Path(str(path)+".returns_patch2.bak")
    if not bak.exists():
        shutil.copy2(path,bak)


total=0

for filename,repls in PATCHES.items():

    real=filename.replace(".extra","").replace(".force","")

    p=Path(real)

    if not p.exists():
        print("Missing:",real)
        continue

    txt=p.read_text()

    orig=txt

    for pat,new in repls:

        txt,n=re.subn(
            pat,
            new,
            txt,
            count=1,
            flags=re.MULTILINE|re.DOTALL
        )

        if n:
            total+=n
            print("Patched",real)

    if txt!=orig:
        backup(p)
        p.write_text(txt)

print()
print("===================================")
print("Patch 2 Complete")
print(total,"changes applied")
print("===================================")
