#!/usr/bin/env python3

import argparse
import shutil
from pathlib import Path

parser = argparse.ArgumentParser(
    description="Merge Flurry armor schematic registrations into Stardust"
)

parser.add_argument(
    "flurry",
    help="Path to Flurry MMOCoreORB"
)

args = parser.parse_args()

ROOT = Path(".").resolve()
FLURRY = Path(args.flurry).resolve()

FILES = [
    "bin/scripts/object/draft_schematic/clothing/objects.lua",
    "bin/scripts/object/draft_schematic/clothing/serverobjects.lua",
]

added = 0

for rel in FILES:

    flurry = FLURRY / rel
    stardust = ROOT / rel

    if not flurry.exists():
        print(f"Missing Flurry file: {rel}")
        continue

    if not stardust.exists():
        print(f"Missing Stardust file: {rel}")
        continue

    backup = stardust.with_suffix(stardust.suffix + ".phase706e.bak")

    if not backup.exists():
        shutil.copy2(stardust, backup)

    dst_lines = stardust.read_text().splitlines()
    src_lines = flurry.read_text().splitlines()

    existing = set(dst_lines)

    new_lines = []

    for line in src_lines:

        s = line.strip()

        #
        # only clothing armor schematics
        #

        if "clothing_armor_" not in s:
            continue

        if s in existing:
            continue

        new_lines.append(line)
        existing.add(s)
        added += 1

    if new_lines:

        with stardust.open("a") as f:

            f.write("\n")
            f.write("-- Phase 7.06e Imported From Flurry\n")

            for line in new_lines:
                f.write(line.rstrip() + "\n")

        print(f"Updated {rel} (+{len(new_lines)})")

print()

print("=" * 60)
print("Finished")
print("=" * 60)
print("Registration lines added:", added)
