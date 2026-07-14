#!/usr/bin/env python3
"""
Phase 7.21.1
Advanced Weaponsmith Audit

Compares SWGFlurry against Stardust and reports every file,
registration, and reference for the advanced weaponsmith
components.

READ ONLY.
"""

from pathlib import Path
import os

FLURRY = Path.home() / "SWGFlurry" / "MMOCoreORB"
STARDUST = Path.home() / "StarDust-2" / "MMOCoreORB"

if not FLURRY.exists():
    print("ERROR: Flurry source not found:")
    print(FLURRY)
    exit(1)

if not STARDUST.exists():
    print("ERROR: Stardust source not found:")
    print(STARDUST)
    exit(1)

COMPONENTS = [

    "blaster_pistol_barrel_advanced",
    "blaster_rifle_barrel_advanced",
    "blaster_power_handler_advanced",

    "projectile_pistol_barrel_advanced",
    "projectile_rifle_barrel_advanced",
    "projectile_feed_mechanism_advanced",

    "scope_weapon_advanced",
    "stock_advanced",
]


def find_component(root, component):

    results = []

    for path, dirs, files in os.walk(root):

        for f in files:

            if component.lower() in f.lower():
                results.append(Path(path) / f)

    return sorted(results)


def search_text(root, component):

    matches = []

    for path, dirs, files in os.walk(root):

        for f in files:

            if not f.endswith((".lua", ".cpp", ".h", ".iff", ".tre")):
                continue

            full = Path(path) / f

            try:

                text = full.read_text(errors="ignore")

            except:

                continue

            if component in text:
                matches.append(full)

    return sorted(matches)


print()
print("=" * 70)
print(" PHASE 7.21.1")
print(" Advanced Weaponsmith Audit")
print("=" * 70)

total_missing = 0

for component in COMPONENTS:

    print()
    print(component)
    print("-" * len(component))

    flurry_files = find_component(FLURRY, component)
    stardust_files = find_component(STARDUST, component)

    print()

    print("Files")

    if flurry_files:

        for f in flurry_files:

            rel = f.relative_to(FLURRY)

            exists = (STARDUST / rel).exists()

            if exists:
                print("  OK ", rel)
            else:
                print("  MISSING ", rel)
                total_missing += 1

    else:
        print("  Not found in Flurry.")

    print()

    print("Registrations / References")

    flurry_refs = search_text(FLURRY, component)
    stardust_refs = search_text(STARDUST, component)

    flurry_rel = {
        str(x.relative_to(FLURRY))
        for x in flurry_refs
    }

    stardust_rel = {
        str(x.relative_to(STARDUST))
        for x in stardust_refs
    }

    missing_refs = sorted(flurry_rel - stardust_rel)

    if missing_refs:

        for r in missing_refs:

            print("  MISSING REF:", r)

    else:

        print("  No missing registrations detected.")

print()
print("=" * 70)
print("Audit Complete")
print("=" * 70)

print()

print("Missing files:", total_missing)

print()
print("Next:")
print("Run Phase 7.21.2 importer after reviewing this report.")
