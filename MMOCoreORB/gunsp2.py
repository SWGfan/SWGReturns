#!/usr/bin/env python3

from pathlib import Path
import argparse
import shutil

parser = argparse.ArgumentParser(
    description="Import Flurry Advanced Weapon Components"
)

parser.add_argument(
    "flurry",
    help="Path to SWGFlurry/MMOCoreORB"
)

parser.add_argument(
    "--manifest",
    default="phase708a_manifest.txt",
    help="Manifest file"
)

parser.add_argument(
    "--overwrite",
    action="store_true",
    help="Overwrite existing files"
)

args = parser.parse_args()

ROOT = Path(".").resolve()
FLURRY = Path(args.flurry).resolve()
MANIFEST = ROOT / args.manifest

if not FLURRY.exists():
    print("ERROR: Flurry path not found")
    exit(1)

if not MANIFEST.exists():
    print("ERROR: Manifest not found")
    exit(1)

copied = 0
updated = 0
skipped = 0
backups = 0
missing = 0

print("=" * 72)
print("Phase 7.08a Import")
print("=" * 72)

for line in MANIFEST.read_text().splitlines():

    line = line.strip()

    if not line:
        continue

    #
    # Manifest contains MMOCoreORB/bin/scripts/...
    #
    if line.startswith("MMOCoreORB/"):
        rel = Path(line[len("MMOCoreORB/"):])
    else:
        rel = Path(line)

    src = FLURRY / rel
    dst = ROOT / rel

    if not src.exists():
        print("[MISS]", rel)
        missing += 1
        continue

    dst.parent.mkdir(parents=True, exist_ok=True)

    if not dst.exists():

        shutil.copy2(src, dst)

        copied += 1

        print("[NEW ]", rel)

        continue

    #
    # identical?
    #

    if src.read_bytes() == dst.read_bytes():

        skipped += 1

        continue

    if not args.overwrite:

        print("[DIFF]", rel)

        updated += 1

        continue

    backup = dst.with_suffix(dst.suffix + ".phase708a.bak")

    if not backup.exists():

        shutil.copy2(dst, backup)

        backups += 1

    shutil.copy2(src, dst)

    updated += 1

    print("[SYNC]", rel)

print()

print("=" * 72)
print("Finished")
print("=" * 72)

print("Copied  :", copied)
print("Updated :", updated)
print("Skipped :", skipped)
print("Missing :", missing)
print("Backups :", backups)
