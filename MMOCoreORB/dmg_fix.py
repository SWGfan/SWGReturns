#!/usr/bin/env python3

from pathlib import Path
import re
import shutil

f = Path("src/server/zone/managers/combat/CombatManager.cpp")

if not f.exists():
    print("CombatManager.cpp not found")
    raise SystemExit

text = f.read_text(encoding="utf-8")

bak = Path(str(f) + ".playerdamage.bak")
if not bak.exists():
    shutil.copy2(f, bak)

pattern = re.compile(
    r'if\s*\(\s*attacker->isPlayerCreature\(\)\s*\)\s*'
    r'damage\s*\*=\s*1\.5\s*;',
    re.MULTILINE
)

new = """// Returns combat rebalance
        if (attacker->isPlayerCreature())
                damage *= 1.10f;"""

text2, count = pattern.subn(new, text, count=1)

if count:
    f.write_text(text2, encoding="utf-8")
    print("✓ Player damage reduced from 1.5x to 1.10x")
else:
    print("Couldn't find the multiplier automatically.")
    print("Please run:")
    print("grep -n 'damage \\*= 1.5' src/server/zone/managers/combat/CombatManager.cpp")
