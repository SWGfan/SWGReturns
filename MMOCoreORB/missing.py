#!/usr/bin/env python3
"""
=============================================================
Phase 7.22C

Synchronize Missing Advanced Components
Flurry -> Stardust

Repairs only the components proven missing by Phase 7.22B.

Creates .phase722C.bak backups.

=============================================================
"""

from pathlib import Path
import shutil

FLURRY = Path.home() / "SWGFlurry" / "MMOCoreORB"
STARDUST = Path.home() / "StarDust-2" / "MMOCoreORB"

BACKUP = ".phase722C.bak"

FILES = [

# -------------------------------------------------------
# Weapon Drafts
# -------------------------------------------------------

"bin/scripts/object/draft_schematic/weapon/component/vibro_unit_advanced.lua",

# -------------------------------------------------------
# Chemistry Drafts
# -------------------------------------------------------

"bin/scripts/object/draft_schematic/chemistry/biologic_effect_controller_advanced.lua",
"bin/scripts/object/draft_schematic/chemistry/release_mechanism_duration_advanced.lua",
"bin/scripts/object/draft_schematic/chemistry/liquid_delivery_suspension_advanced.lua",
"bin/scripts/object/draft_schematic/chemistry/solid_delivery_shell_advanced.lua",

# -------------------------------------------------------
# Nightsister
# -------------------------------------------------------

"bin/scripts/object/draft_schematic/weapon/component/vibro_unit_nightsister.lua",

# -------------------------------------------------------
# Loot Templates
# -------------------------------------------------------

"bin/scripts/loot/items/component_loot/reinforcement_core_advanced.lua",
"bin/scripts/loot/items/component_loot/sword_core_advanced.lua",
"bin/scripts/loot/items/component_loot/vibro_unit_advanced.lua",
"bin/scripts/loot/items/component_loot/vibro_unit_nightsister.lua",
]

print("=" * 70)
print("Phase 7.22C")
print("=" * 70)

copied = 0
missing = 0

for rel in FILES:

    src = FLURRY / rel
    dst = STARDUST / rel

    if not src.exists():
        print("[NOT FOUND IN FLURRY]")
        print(" ", rel)
        missing += 1
        continue

    dst.parent.mkdir(parents=True, exist_ok=True)

    if dst.exists():

        backup = dst.with_suffix(dst.suffix + BACKUP)

        shutil.copy2(dst, backup)

    shutil.copy2(src, dst)

    copied += 1

    print("[COPIED]")
    print(" ", rel)

print()

########################################################################

PATCHES = [

(
"bin/scripts/loot/items.lua",
[
'includeFile("items/component_loot/reinforcement_core_advanced.lua")',
'includeFile("items/component_loot/sword_core_advanced.lua")',
'includeFile("items/component_loot/vibro_unit_advanced.lua")',
'includeFile("items/component_loot/vibro_unit_nightsister.lua")'
]
),

(
"bin/scripts/object/draft_schematic/weapon/component/serverobjects.lua",
[
'includeFile("draft_schematic/weapon/component/vibro_unit_advanced.lua")',
'includeFile("draft_schematic/weapon/component/vibro_unit_nightsister.lua")'
]
),

(
"bin/scripts/object/draft_schematic/chemistry/serverobjects.lua",
[
'includeFile("draft_schematic/chemistry/biologic_effect_controller_advanced.lua")',
'includeFile("draft_schematic/chemistry/release_mechanism_duration_advanced.lua")',
'includeFile("draft_schematic/chemistry/liquid_delivery_suspension_advanced.lua")',
'includeFile("draft_schematic/chemistry/solid_delivery_shell_advanced.lua")'
]
)

]

########################################################################

print("=" * 70)
print("Patching registrations")
print("=" * 70)

for file, lines in PATCHES:

    path = STARDUST / file

    if not path.exists():
        print("[SKIP]")
        print(file)
        continue

    text = path.read_text(errors="ignore")

    changed = False

    for line in lines:

        if line not in text:

            text += "\n" + line
            changed = True

    if changed:

        backup = path.with_suffix(path.suffix + BACKUP)

        shutil.copy2(path, backup)

        path.write_text(text)

        print("[UPDATED]")
        print(file)

    else:

        print("[OK]")
        print(file)

print()
print("=" * 70)
print("SUMMARY")
print("=" * 70)

print("Copied :", copied)
print("Missing from Flurry :", missing)

print()
print("Done.")
