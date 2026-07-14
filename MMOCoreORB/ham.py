#!/usr/bin/env python3

from pathlib import Path
import re
import shutil

p = Path("src/server/zone/managers/combat/CombatManager.cpp")

text = p.read_text()

bak = Path(str(p) + ".hamcap.bak")
if not bak.exists():
    shutil.copy2(p, bak)

pattern = re.compile(
    r'''
if\s*\(\s*attacker->isPlayerCreature\(\)\s*&&\s*defender->isPlayerCreature\(\)\s*\)\s*
\{
.*?
float\s+hamCap\s*=\s*defender->getMaxHAM\(\)\s*\*\s*0\.35f\s*;
.*?
\}
''',
    re.DOTALL | re.VERBOSE,
)

replacement = r'''
if (attacker->isPlayerCreature() && defender->isPlayerCreature()) {

        float weaponCap = weapon->getMaxDamage() * 2.5f;

        if (damage > weaponCap)
                damage = weaponCap;
}
'''

new, n = pattern.subn(replacement, text, count=1)

if n:
    p.write_text(new)
    print("Removed broken HAM cap.")
else:
    print("HAM cap block not found.")
