#!/usr/bin/env python3

from pathlib import Path

ROOT = Path(".")

WEARABLES = ROOT / "bin/scripts/object/tangible/wearables/armor"
SCHEMATICS = ROOT / "bin/scripts/object/draft_schematic/armor"

if not WEARABLES.exists():
    print("Couldn't locate wearable armor directory.")
    raise SystemExit

if not SCHEMATICS.exists():
    print("Couldn't locate draft schematic directory.")
    raise SystemExit

print("=" * 70)
print(" Phase 7.06b - Armor Crafting Scanner")
print("=" * 70)

schematic_names = set()

#
# Build list of every schematic filename
#

for f in SCHEMATICS.rglob("*.lua"):
    schematic_names.add(f.stem.lower())

missing = []

#
# Compare wearables against schematics
#

for wearable in sorted(WEARABLES.rglob("*.lua")):

    name = wearable.stem.lower()

    found = False

    #
    # Loose comparison
    #

    for schematic in schematic_names:

        if name in schematic:
            found = True
            break

        if schematic in name:
            found = True
            break

    if not found:
        missing.append(wearable)

print()

print("Missing Craftable Armor")
print("-----------------------")

for f in missing:
    print(f.relative_to(ROOT))

print()

print("=" * 70)
print("Wearables :", len(list(WEARABLES.rglob("*.lua"))))
print("Schematics:", len(schematic_names))
print("Missing   :", len(missing))
print("=" * 70)

#
# Save report
#

report = ROOT / "missing_craftable_armor.txt"

with report.open("w") as fp:

    for f in missing:
        fp.write(str(f.relative_to(ROOT)) + "\n")

print()
print("Report written to:")
print(report)
