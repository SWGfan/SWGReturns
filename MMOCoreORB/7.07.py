#!/usr/bin/env python3

import re
import shutil
from pathlib import Path

ROOT = Path(".")
BUILDINGS = ROOT / "bin/scripts/object/building"

if not BUILDINGS.exists():
    print("ERROR: Cannot find", BUILDINGS)
    raise SystemExit(1)

pattern = re.compile(
    r'(allowedZones\s*=\s*\{)(.*?)(\})',
    re.DOTALL
)

modified = 0
already = 0
backups = 0
scanned = 0

print("=" * 72)
print("Phase 7.07 - Enable Yavin4 Building")
print("=" * 72)

for lua in BUILDINGS.rglob("*.lua"):

    scanned += 1

    text = lua.read_text(encoding="utf-8", errors="ignore")

    m = pattern.search(text)

    if not m:
        continue

    zone_block = m.group(2)

    if '"yavin4"' in zone_block or "'yavin4'" in zone_block:
        already += 1
        continue

    backup = lua.with_suffix(lua.suffix + ".phase707.bak")

    if not backup.exists():
        shutil.copy2(lua, backup)
        backups += 1

    zone_block = zone_block.rstrip()

    if zone_block.endswith(","):
        new_block = zone_block + '\n    "yavin4",'
    else:
        new_block = zone_block + ',\n    "yavin4"'

    text = text[:m.start(2)] + new_block + text[m.end(2):]

    lua.write_text(text, encoding="utf-8")

    modified += 1

    print("Patched:", lua.relative_to(ROOT))

print()
print("=" * 72)
print("Finished")
print("=" * 72)
print("Scanned :", scanned)
print("Modified:", modified)
print("Already :", already)
print("Backups :", backups)
