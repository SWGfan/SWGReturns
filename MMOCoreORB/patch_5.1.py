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

bak = Path(str(FILE) + ".returns_patch5_1.bak")
if not bak.exists():
    shutil.copy2(FILE, bak)


# ------------------------------------------------------------------
# Clamp private_damage_bonus
# ------------------------------------------------------------------

text = re.sub(
r'damage\s*\+=\s*attacker->getSkillMod\("private_damage_bonus"\)\s*;',
'''
{
        int bonus = attacker->getSkillMod("private_damage_bonus");

        if (bonus > 200)
                bonus = 200;

        damage += bonus;
}
''',
text,
count=1,
flags=re.MULTILINE
)

# ------------------------------------------------------------------
# Clamp ranged bonus
# ------------------------------------------------------------------

text = re.sub(
r'damage\s*\+=\s*attacker->getSkillMod\("private_ranged_damage_bonus"\)\s*;',
'''
{
        int ranged = attacker->getSkillMod("private_ranged_damage_bonus");

        if (ranged > 150)
                ranged = 150;

        damage += ranged;
}
''',
text,
count=1,
flags=re.MULTILINE
)

# ------------------------------------------------------------------
# Clamp melee bonus
# ------------------------------------------------------------------

text = re.sub(
r'damage\s*\+=\s*attacker->getSkillMod\("private_melee_damage_bonus"\)\s*;',
'''
{
        int melee = attacker->getSkillMod("private_melee_damage_bonus");

        if (melee > 150)
                melee = 150;

        damage += melee;
}
''',
text,
count=1,
flags=re.MULTILINE
)

# ------------------------------------------------------------------
# Replace multiplier logic
# ------------------------------------------------------------------

text = re.sub(
r'''
int\s+damageMultiplier\s*=\s*attacker->getSkillMod\("private_damage_multiplier"\);\s*

if\s*\(\s*damageMultiplier\s*!=\s*0\s*\)\s*
\s*damage\s*\*=\s*damageMultiplier\s*;
''',
'''
int damageMultiplier = attacker->getSkillMod("private_damage_multiplier");

if (damageMultiplier > 0) {

        float mult = (float)damageMultiplier;

        if (mult > 2.50f)
                mult = 2.50f;

        damage *= mult;
}
''',
text,
count=1,
flags=re.MULTILINE | re.DOTALL
)

# ------------------------------------------------------------------
# Global sanity clamp
# ------------------------------------------------------------------

anchor = "return damage;"

if "Returns Damage Clamp" not in text:

    text = text.replace(
        anchor,
'''
        // Returns Damage Clamp

        if (damage > weapon->getMaxDamage() * 2.50f)
                damage = weapon->getMaxDamage() * 2.50f;

        return damage;
''',
1
)

if text != original:
    FILE.write_text(text, encoding="utf-8")
    print("CombatManager patched successfully.")
else:
    print("Already patched or no matching patterns found.")
