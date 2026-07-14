#!/usr/bin/env python3

from pathlib import Path
import shutil
import re

FILE = Path("src/server/zone/managers/combat/CombatManager.cpp")

if not FILE.exists():
    print("CombatManager.cpp not found.")
    raise SystemExit(1)

text = FILE.read_text(encoding="utf-8")
original = text

bak = Path(str(FILE) + ".damage_multiplier_fix.bak")
if not bak.exists():
    shutil.copy2(FILE, bak)

pattern = re.compile(
    r'''
int\s+damageMultiplier\s*=\s*attacker->getSkillMod\("private_damage_multiplier"\)\s*;

\s*if\s*\(\s*damageMultiplier\s*!=\s*0\s*\)\s*\{

.*?

damage\s*\*=\s*mult\s*;

\s*\}
''',
    re.MULTILINE | re.DOTALL | re.VERBOSE
)

replacement = r'''
int damageMultiplier = attacker->getSkillMod("private_damage_multiplier");

if (damageMultiplier > 0) {

        //
        // Returns Combat 2.1
        // Convert skill modifier into a percentage bonus instead
        // of a literal multiplier.
        //

        float mult = 1.0f + (damageMultiplier * 0.10f);

        if (mult > 1.75f)
                mult = 1.75f;

        damage *= mult;
}
'''

text, n = pattern.subn(replacement, text, count=1)

if n == 0:
    print("Pattern not found. File may already be patched or modified.")
    raise SystemExit(1)

FILE.write_text(text, encoding="utf-8")

print()
print("===================================")
print("CombatManager patched successfully.")
print("Old multiplier: x1 x2 x3 x4 x5")
print("New multiplier: x1.10 x1.20 x1.30 x1.40 x1.50")
print("Backup:", bak)
print("===================================")
