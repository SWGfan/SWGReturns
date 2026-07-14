#!/usr/bin/env python3

from pathlib import Path
import shutil
import filecmp
import argparse

parser = argparse.ArgumentParser(description="Sync Flurry loot into Stardust")
parser.add_argument("flurry", help="Path to Flurry MMOCoreORB")
parser.add_argument("--overwrite", action="store_true",
                    help="Overwrite changed files (default copies only missing files)")
args = parser.parse_args()

STARDUST = Path(".").resolve()
FLURRY = Path(args.flurry).resolve()

SRC = FLURRY / "bin/scripts/loot"
DST = STARDUST / "bin/scripts/loot"

if not SRC.exists():
    print("Couldn't locate Flurry loot directory:")
    print(SRC)
    exit(1)

copied = 0
updated = 0
skipped = 0
backups = 0

print("==========================================")
print(" Phase 7 - Flurry Loot Sync")
print("==========================================")

SKIP_DIRS = {
    "custom_loot",
}

for src in SRC.rglob("*"):

    rel = src.relative_to(SRC)

    if len(rel.parts) > 0 and rel.parts[0] in SKIP_DIRS:
        continue

    dst = DST / rel

    if src.is_dir():
        dst.mkdir(parents=True, exist_ok=True)
        continue

    if not dst.exists():
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
        copied += 1
        print("[NEW ]", rel)
        continue

    if filecmp.cmp(src, dst, shallow=False):
        skipped += 1
        continue

    if not args.overwrite:
        skipped += 1
        continue

    bak = dst.with_suffix(dst.suffix + ".phase7.bak")

    if not bak.exists():
        shutil.copy2(dst, bak)
        backups += 1

    shutil.copy2(src, dst)

    updated += 1
    print("[SYNC]", rel)

print()
print("==========================================")
print("Finished")
print("==========================================")
print(f"New files : {copied}")
print(f"Updated   : {updated}")
print(f"Skipped   : {skipped}")
print(f"Backups   : {backups}")
