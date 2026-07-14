#!/usr/bin/env python3

from pathlib import Path
import shutil
import datetime

ROOT = Path("bin/scripts/object/draft_schematic")
OUT = Path("bin/scripts/managers/crafting/schematics.lua")

if not ROOT.exists():
    print("ERROR: Cannot find", ROOT)
    raise SystemExit(1)

#
# Backup existing file
#
timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")

if OUT.exists():
    backup = OUT.with_suffix(f".phase713b_{timestamp}.bak")
    shutil.copy2(OUT, backup)
    print("Backup created:", backup)

schematics = []

#
# Enumerate every draft schematic Lua
#
for lua in sorted(ROOT.rglob("*.lua")):

    rel = lua.relative_to(ROOT).as_posix()

    #
    # Skip infrastructure files
    #
    if rel.endswith("objects.lua"):
        continue

    if rel.endswith("serverobjects.lua"):
        continue

    #
    # Remove .lua
    #
    rel = rel[:-4]

    #
    # Convert to iff path
    #
    iff = f"object/draft_schematic/{rel}.iff"

    schematics.append(iff)

#
# Remove duplicates while preserving order
#
seen = set()
unique = []

for s in schematics:
    if s not in seen:
        seen.add(s)
        unique.append(s)

#
# Write file
#
with open(OUT, "w") as fp:

    fp.write("-- AUTO-GENERATED\n")
    fp.write("-- Phase 7.13b\n")
    fp.write("-- DO NOT EDIT MANUALLY\n\n")

    fp.write("schematics = {\n")

    for path in unique:
        fp.write(f'    {{path="{path}"}},\n')

    fp.write("}\n")

print()
print("=" * 72)
print("Phase 7.13b Complete")
print("=" * 72)
print()

print("Schematics written :", len(unique))
print("Output file        :", OUT)
