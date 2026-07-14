#!/usr/bin/env python3

from pathlib import Path
import argparse

parser = argparse.ArgumentParser(description="Discover Flurry advanced weapon components")
parser.add_argument(
    "flurry",
    help="Path to SWGFlurry/MMOCoreORB"
)

args = parser.parse_args()

FLURRY = Path(args.flurry).resolve()

if not FLURRY.exists():
    print("ERROR: Flurry path does not exist:")
    print(FLURRY)
    exit(1)

manifest = []

print("=" * 72)
print("Phase 7.08a - Discover Advanced Weapon Components")
print("=" * 72)

for lua in FLURRY.rglob("*.lua"):

    rel = lua.relative_to(FLURRY)
    path = str(rel).lower()

    #
    # Only scripts
    #
    if "bin/scripts/" not in path:
        continue

    #
    # Ignore ships
    #
    if "/ship/" in path:
        continue

    #
    # Ignore space
    #
    if "/space/" in path:
        continue

    #
    # Ignore armor
    #
    if "/armor/" in path:
        continue

    #
    # Ignore droids
    #
    if "/droid/" in path:
        continue

    #
    # Weapon component
    #
    if "/weapon/component/" in path:

        if "advanced" in path:
            manifest.append(rel)
            continue

        if "new_weapon_comp_" in path:
            manifest.append(rel)
            continue

    #
    # Loot definitions
    #
    if "/loot/" in path and "advanced" in path:
        manifest.append(rel)

manifest = sorted(set(manifest))

print()

for f in manifest:
    print(f)

print()
print("=" * 72)
print("Found", len(manifest), "files")
print("=" * 72)

outfile = Path("phase708a_manifest.txt")

with outfile.open("w") as fp:
    for f in manifest:
        fp.write(str(f) + "\n")

print()
print("Manifest written to:")
print(outfile.resolve())
