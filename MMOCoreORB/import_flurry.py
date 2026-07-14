#!/usr/bin/env python3

import argparse
import filecmp
import shutil
from pathlib import Path

parser = argparse.ArgumentParser(
    description="Import Flurry armor draft schematics into Stardust"
)

parser.add_argument(
    "flurry",
    help="Path to Flurry MMOCoreORB"
)

parser.add_argument(
    "--overwrite",
    action="store_true",
    help="Replace existing Stardust files"
)

args = parser.parse_args()

ROOT = Path(".").resolve()
FLURRY = Path(args.flurry).resolve()

SRC = FLURRY / "bin/scripts/object/draft_schematic/clothing"
DST = ROOT / "bin/scripts/object/draft_schematic/clothing"

if not SRC.exists():
    print("ERROR: Cannot find Flurry clothing schematic directory:")
    print(SRC)
    raise SystemExit(1)

if not DST.exists():
    print("ERROR: Cannot find Stardust clothing schematic directory:")
    print(DST)
    raise SystemExit(1)

copied = 0
updated = 0
skipped = 0
backups = 0

families = {}

print("=" * 72)
print("Importing Flurry Armor Schematics")
print("=" * 72)

for src in sorted(SRC.glob("clothing_armor_*.lua")):

    #
    # Determine armor family
    #
    name = src.stem.replace("clothing_armor_", "")

    parts = name.split("_")

    if len(parts) >= 2:
        family = "_".join(parts[:-1])
    else:
        family = "misc"

    families.setdefault(family, []).append(src.name)

    dst = DST / src.name

    #
    # New file
    #
    if not dst.exists():
        shutil.copy2(src, dst)
        copied += 1
        print(f"[NEW ] {src.name}")
        continue

    #
    # Same file
    #
    if filecmp.cmp(src, dst, shallow=False):
        skipped += 1
        continue

    #
    # Different
    #
    if not args.overwrite:
        print(f"[DIFF] {src.name}")
        updated += 1
        continue

    backup = dst.with_suffix(dst.suffix + ".phase706_import.bak")

    if not backup.exists():
        shutil.copy2(dst, backup)
        backups += 1

    shutil.copy2(src, dst)

    updated += 1

    print(f"[SYNC] {src.name}")

print()
print("=" * 72)
print("Armor Families Found")
print("=" * 72)

for family in sorted(families):

    print(f"{family:30} {len(families[family]):2d} files")

print()
print("=" * 72)
print("Summary")
print("=" * 72)

print(f"Families : {len(families)}")
print(f"Copied   : {copied}")
print(f"Updated  : {updated}")
print(f"Skipped  : {skipped}")
print(f"Backups  : {backups}")
