#!/usr/bin/env python3

import argparse
import shutil
from pathlib import Path

parser = argparse.ArgumentParser(
    description="Merge Flurry draft schematic registration files into Stardust"
)

parser.add_argument("flurry")

args = parser.parse_args()

ROOT = Path(".").resolve()
FLURRY = Path(args.flurry).resolve()

FILES = [

    # root draft schematic loaders
    "bin/scripts/object/draft_schematic/objects.lua",
    "bin/scripts/object/draft_schematic/serverobjects.lua",

    # clothing loaders
    "bin/scripts/object/draft_schematic/clothing/objects.lua",
    "bin/scripts/object/draft_schematic/clothing/serverobjects.lua",

    # armor loaders
    "bin/scripts/object/draft_schematic/armor/objects.lua",
    "bin/scripts/object/draft_schematic/armor/serverobjects.lua",

    # armor components
    "bin/scripts/object/draft_schematic/armor/component/objects.lua",
    "bin/scripts/object/draft_schematic/armor/component/serverobjects.lua",
]

merged = 0
backups = 0

print("=" * 72)
print("Phase 7.06f - Merge Flurry Registrations")
print("=" * 72)

for rel in FILES:

    src = FLURRY / rel
    dst = ROOT / rel

    if not src.exists():
        continue

    if not dst.exists():
        continue

    backup = dst.with_suffix(dst.suffix + ".phase706f.bak")

    if not backup.exists():
        shutil.copy2(dst, backup)
        backups += 1

    dstLines = dst.read_text().splitlines()
    srcLines = src.read_text().splitlines()

    existing = set(l.strip() for l in dstLines)

    added = []

    for line in srcLines:

        s = line.strip()

        if not s.startswith("includeFile"):
            continue

        if s in existing:
            continue

        added.append(line)
        existing.add(s)

    if added:

        with dst.open("a") as f:

            f.write("\n")
            f.write("-- Phase 7.06f Flurry Import\n")

            for line in added:
                f.write(line.rstrip() + "\n")

        print(f"{rel} (+{len(added)})")

        merged += len(added)

print()
print("=" * 72)
print("Finished")
print("=" * 72)
print("Merged :", merged)
print("Backups:", backups)
