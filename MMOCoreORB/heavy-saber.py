#!/usr/bin/env python3

from pathlib import Path
import shutil
import re

ROOT = Path("/home/ubuntu/StarDust-2/MMOCoreORB/bin/scripts/object/weapon/melee")

FOLDERS = [
    ROOT / "sword",
    ROOT / "2h_sword",
    ROOT / "polearm",
]

changed = []

# Change LIGHT armor piercing to HEAVY
pattern = re.compile(
    r'(\barmorPiercing\s*=\s*)LIGHT(\s*,)',
    re.MULTILINE
)

for folder in FOLDERS:

    if not folder.exists():
        continue

    for lua in folder.rglob("*.lua"):

        # Only touch crafted sabers
        if "crafted_saber" not in str(lua):
            continue

        text = lua.read_text(encoding="utf8")

        new = pattern.sub(r"\1HEAVY\2", text)

        if new != text:

            backup = lua.with_suffix(".lua.bak")

            if not backup.exists():
                shutil.copy2(lua, backup)

            lua.write_text(new, encoding="utf8")

            changed.append(lua)

print("=" * 70)
print("StormSWG Lightsaber Heavy Armor Piercing")
print("=" * 70)
print()

print("Modified:", len(changed))
print()

for f in changed:
    print(f)
