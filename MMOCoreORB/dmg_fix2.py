#!/usr/bin/env python3

from pathlib import Path
import re
import shutil

f = Path("bin/scripts/commands/strafeShot2.lua")

text = f.read_text()

bak = Path(str(f) + ".returns_patch1.bak")
if not bak.exists():
    shutil.copy2(f, bak)

text2, n = re.subn(
    r'damageMultiplier\s*=\s*3\b',
    'damageMultiplier = 1.75',
    text,
    count=1
)

if n:
    f.write_text(text2)
    print("Advanced Strafe reduced from 3.0x to 1.75x")
else:
    print("Pattern not found.")
