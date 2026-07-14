#!/usr/bin/env python3

from pathlib import Path
import shutil
import argparse

parser = argparse.ArgumentParser(description="Sync armor crafting assets from Flurry")
parser.add_argument("flurry", help="Path to Flurry MMOCoreORB")
args = parser.parse_args()

ROOT = Path(".").resolve()
FLURRY = Path(args.flurry).resolve()

COPY_DIRS = [
    "bin/scripts/object/draft_schematic/armor",
    "bin/scripts/object/manufacture_schematic/armor",
    "bin/scripts/object/tangible/component/armor",
]

copied = 0
skipped = 0
created = 0

print("=" * 60)
print("Phase 7.06a - Flurry Armor Crafting Sync")
print("=" * 60)

for directory in COPY_DIRS:

    srcRoot = FLURRY / directory
    dstRoot = ROOT / directory

    if not srcRoot.exists():
        print(f"Skipping missing Flurry directory: {directory}")
        continue

    for src in srcRoot.rglob("*"):

        rel = src.relative_to(srcRoot)
        dst = dstRoot / rel

        if src.is_dir():
            dst.mkdir(parents=True, exist_ok=True)
            continue

        # Skip custom files
        if "custom" in src.name.lower():
            skipped += 1
            continue

        if dst.exists():
            skipped += 1
            continue

        dst.parent.mkdir(parents=True, exist_ok=True)

        shutil.copy2(src, dst)

        copied += 1

        print("Copied:", dst.relative_to(ROOT))

print()
print("=" * 60)
print("Finished")
print("=" * 60)
print(f"Copied : {copied}")
print(f"Skipped: {skipped}")
