#!/usr/bin/env python3
#
# phase8_factory_crate_rebalance.py
#
# Changes ONLY:
#     factoryCrateSize = 1000
# into:
#     factoryCrateSize = 250
#
# Makes a .bak backup the first time it edits a file.
#

from pathlib import Path
import re

ROOT = Path("/home/ubuntu/StarDust-2/MMOCoreORB")

SEARCH = ROOT / "bin/scripts/object/draft_schematic"

OLD = 1000
NEW = 250

regex = re.compile(r"(\bfactoryCrateSize\s*=\s*)1000(\b)")

modified = 0

print("=" * 70)
print("Factory Crate Rebalance")
print("=" * 70)

for lua in SEARCH.rglob("*.lua"):
    try:
        text = lua.read_text(encoding="utf-8")
    except Exception:
        continue

    newtext, count = regex.subn(r"\g<1>250\2", text)

    if count:
        backup = lua.with_suffix(lua.suffix + ".phase825.bak")

        if not backup.exists():
            backup.write_text(text, encoding="utf-8")

        lua.write_text(newtext, encoding="utf-8")

        modified += 1
        print(f"[UPDATED] {lua.relative_to(ROOT)}")

print()
print("=" * 70)
print(f"Files modified: {modified}")
print("=" * 70)
