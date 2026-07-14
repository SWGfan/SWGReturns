#!/usr/bin/env python3
"""
======================================================================
Phase 7.22D

Real Advanced Component Synchronizer

Copies ONLY missing/different files from Flurry using the VERIFIED
paths discovered during Phase 7.22C.

Creates .phase722D.bak backups.

======================================================================
"""

from pathlib import Path
import shutil
import filecmp

FLURRY = Path.home() / "SWGFlurry" / "MMOCoreORB"
STARDUST = Path.home() / "StarDust-2" / "MMOCoreORB"

BACKUP_EXT = ".phase722D.bak"

FILES = [

# ------------------------------------------------------------
# Weapon Draft Schematics
# ------------------------------------------------------------

"bin/scripts/object/draft_schematic/weapon/component/blade_vibro_unit_advanced.lua",

# ------------------------------------------------------------
# Chemistry Draft Schematics
# ------------------------------------------------------------

"bin/scripts/object/draft_schematic/chemistry/component/biologic_effect_controller_advanced.lua",
"bin/scripts/object/draft_schematic/chemistry/component/liquid_delivery_suspension_advanced.lua",
"bin/scripts/object/draft_schematic/chemistry/component/release_mechanism_duration_advanced.lua",
"bin/scripts/object/draft_schematic/chemistry/component/solid_delivery_shell_advanced.lua",

# ------------------------------------------------------------
# Chemistry Registrations
# ------------------------------------------------------------

"bin/scripts/object/draft_schematic/chemistry/component/serverobjects.lua",
"bin/scripts/object/draft_schematic/chemistry/component/objects.lua",

# ------------------------------------------------------------
# Nightsister Draft
# ------------------------------------------------------------

"bin/scripts/object/custom_content/draft_schematic/weapon/component/vibro_unit_nightsister.lua",

# ------------------------------------------------------------
# Custom Content Registrations
# ------------------------------------------------------------

"bin/scripts/object/custom_content/draft_schematic/weapon/component/serverobjects.lua",
"bin/scripts/object/custom_content/draft_schematic/weapon/component/objects.lua",

# ------------------------------------------------------------
# Loot Templates
# ------------------------------------------------------------

"bin/scripts/loot/items/component_loot/biologic_effect_controller_advanced.lua",
"bin/scripts/loot/items/component_loot/liquid_delivery_suspension_advanced.lua",
"bin/scripts/loot/items/component_loot/release_mechanism_duration_advanced.lua",
"bin/scripts/loot/items/component_loot/solid_delivery_shell_advanced.lua",
"bin/scripts/loot/items/component_loot/reinforcement_core_advanced.lua",
"bin/scripts/loot/items/component_loot/sword_core_advanced.lua",
"bin/scripts/loot/items/component_loot/vibro_unit_advanced.lua",

# ------------------------------------------------------------
# Loot Group
# ------------------------------------------------------------

"bin/scripts/loot/groups/component_loot/chemistry_component_advanced.lua"

]

copied = 0
same = 0
missing = 0

print("=" * 70)
print("Phase 7.22D")
print("=" * 70)

for rel in FILES:

    src = FLURRY / rel
    dst = STARDUST / rel

    if not src.exists():
        print("[MISSING IN FLURRY]")
        print(" ", rel)
        missing += 1
        continue

    dst.parent.mkdir(parents=True, exist_ok=True)

    if dst.exists():

        if filecmp.cmp(src, dst, shallow=False):
            print("[SAME]")
            print(" ", rel)
            same += 1
            continue

        backup = dst.with_suffix(dst.suffix + BACKUP_EXT)
        shutil.copy2(dst, backup)

    shutil.copy2(src, dst)

    copied += 1

    print("[COPIED]")
    print(" ", rel)

print()

########################################################################

PATCHES = {

"bin/scripts/loot/items.lua":[

'includeFile("items/component_loot/biologic_effect_controller_advanced.lua")',
'includeFile("items/component_loot/liquid_delivery_suspension_advanced.lua")',
'includeFile("items/component_loot/release_mechanism_duration_advanced.lua")',
'includeFile("items/component_loot/solid_delivery_shell_advanced.lua")',
'includeFile("items/component_loot/reinforcement_core_advanced.lua")',
'includeFile("items/component_loot/sword_core_advanced.lua")',
'includeFile("items/component_loot/vibro_unit_advanced.lua")'

]

}

print("=" * 70)
print("Updating registrations")
print("=" * 70)

for rel, entries in PATCHES.items():

    path = STARDUST / rel

    if not path.exists():
        print("[SKIP]", rel)
        continue

    text = path.read_text(errors="ignore")

    changed = False

    for entry in entries:

        if entry not in text:
            text += "\n" + entry
            changed = True

    if changed:

        backup = path.with_suffix(path.suffix + BACKUP_EXT)
        shutil.copy2(path, backup)

        path.write_text(text)

        print("[UPDATED]", rel)

    else:

        print("[OK]", rel)

print()
print("=" * 70)
print("SUMMARY")
print("=" * 70)
print("Copied :", copied)
print("Same   :", same)
print("Missing:", missing)
print("=" * 70)
print("Done.")
