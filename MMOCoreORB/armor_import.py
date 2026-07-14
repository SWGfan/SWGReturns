#!/usr/bin/env python3

from pathlib import Path
import shutil
import argparse
import filecmp

parser = argparse.ArgumentParser(description="Undo old Flurry import and import only armor schematics")
parser.add_argument("flurry", help="Path to Flurry MMOCoreORB")
parser.add_argument("--overwrite", action="store_true")
args = parser.parse_args()

ROOT = Path(".").resolve()
FLURRY = Path(args.flurry).resolve()

#
# Directories the OLD importer touched
#
OLD_IMPORTS = [
    "bin/scripts/object/draft_schematic",
    "bin/scripts/object/manufacture_schematic",
    "bin/scripts/object/tangible/component",
    "bin/scripts/object/tangible/wearables",
    "bin/scripts/object/static",
    "bin/scripts/object/building",
    "bin/scripts/object/intangible",
]

#
# Directories we actually want
#
ARMOR_IMPORTS = [
    "bin/scripts/object/draft_schematic/armor",
    "bin/scripts/object/manufacture_schematic/armor",
]

removed = 0
restored = 0
copied = 0
updated = 0
skipped = 0
backups = 0

print("=" * 70)
print("Undoing previous Phase 7.06c import...")
print("=" * 70)

#
# Restore backups
#
for bak in ROOT.rglob("*.phase706c.bak"):

    original = Path(str(bak).replace(".phase706c.bak", ""))

    shutil.copy2(bak, original)
    bak.unlink()

    restored += 1
    print("[RESTORE]", original.relative_to(ROOT))

#
# Remove files that the old importer created
#
for directory in OLD_IMPORTS:

    src_root = FLURRY / directory
    dst_root = ROOT / directory

    if not src_root.exists():
        continue

    for src in src_root.rglob("*"):

        if src.is_dir():
            continue

        rel = src.relative_to(src_root)
        dst = dst_root / rel

        #
        # Skip armor schematics—we're going to import them again.
        #
        if "draft_schematic/armor" in str(dst):
            continue

        if "manufacture_schematic/armor" in str(dst):
            continue

        if dst.exists():

            # If it doesn't have a backup, assume the old importer created it.
            bak = dst.with_suffix(dst.suffix + ".phase706c.bak")

            if not bak.exists():
                dst.unlink()
                removed += 1
                print("[REMOVE ]", dst.relative_to(ROOT))

print()
print("=" * 70)
print("Importing Armor Schematics")
print("=" * 70)

for directory in ARMOR_IMPORTS:

    src_root = FLURRY / directory
    dst_root = ROOT / directory

    if not src_root.exists():
        continue

    for src in src_root.rglob("*.lua"):

        rel = src.relative_to(src_root)
        dst = dst_root / rel

        dst.parent.mkdir(parents=True, exist_ok=True)

        if not dst.exists():
            shutil.copy2(src, dst)
            copied += 1
            print("[NEW ]", rel)
            continue

        if filecmp.cmp(src, dst, shallow=False):
            skipped += 1
            continue

        if not args.overwrite:
            updated += 1
            print("[DIFF]", rel)
            continue

        bak = dst.with_suffix(dst.suffix + ".phase706c_armor.bak")

        if not bak.exists():
            shutil.copy2(dst, bak)
            backups += 1

        shutil.copy2(src, dst)
        updated += 1
        print("[SYNC]", rel)

print()
print("=" * 70)
print("Finished")
print("=" * 70)
print(f"Restored : {restored}")
print(f"Removed  : {removed}")
print(f"Copied   : {copied}")
print(f"Updated  : {updated}")
print(f"Skipped  : {skipped}")
print(f"Backups  : {backups}")
