#!/usr/bin/env python3
"""
===========================================================
Phase 7.22A
Flurry -> Stardust Weapon Component Framework Importer

Copies ONLY the weapon component framework.

Creates .phase722A.bak backups before overwriting.

===========================================================
"""

from pathlib import Path
import shutil
from datetime import datetime

FLURRY = Path.home() / "SWGFlurry" / "MMOCoreORB"
STARDUST = Path.home() / "StarDust-2" / "MMOCoreORB"

BACKUP_SUFFIX = ".phase722A.bak"

FILES = [

# --------------------------------------------------------
# Weapon Component Framework
# --------------------------------------------------------

"bin/scripts/object/custom_content/draft_schematic/weapon/component/objects.lua",
"bin/scripts/object/custom_content/draft_schematic/weapon/component/serverobjects.lua",

# --------------------------------------------------------
# Weapon Core Framework
# --------------------------------------------------------

"bin/scripts/object/custom_content/draft_schematic/weapon/core/objects.lua",
"bin/scripts/object/custom_content/draft_schematic/weapon/core/serverobjects.lua",

# --------------------------------------------------------
# Weapon Components
# --------------------------------------------------------

"bin/scripts/object/custom_content/draft_schematic/weapon/component/weapon_power_bit.lua",
"bin/scripts/object/custom_content/draft_schematic/weapon/component/projectile_rifle_barrel_faux_bowcaster.lua",
"bin/scripts/object/custom_content/draft_schematic/weapon/component/vibro_unit_nightsister.lua",

# --------------------------------------------------------
# Weapon Cores
# --------------------------------------------------------

"bin/scripts/object/custom_content/draft_schematic/weapon/core/weapon_core_heavy_standard.lua",
"bin/scripts/object/custom_content/draft_schematic/weapon/core/weapon_core_heavy_advanced.lua",

"bin/scripts/object/custom_content/draft_schematic/weapon/core/weapon_core_melee_basic.lua",
"bin/scripts/object/custom_content/draft_schematic/weapon/core/weapon_core_melee_standard.lua",
"bin/scripts/object/custom_content/draft_schematic/weapon/core/weapon_core_melee_advanced.lua",

"bin/scripts/object/custom_content/draft_schematic/weapon/core/weapon_core_ranged_basic.lua",
"bin/scripts/object/custom_content/draft_schematic/weapon/core/weapon_core_ranged_standard.lua",
"bin/scripts/object/custom_content/draft_schematic/weapon/core/weapon_core_ranged_advanced.lua",
]

print("="*72)
print("Phase 7.22A Framework Import")
print("="*72)

copied = 0
skipped = 0
missing = 0

for rel in FILES:

    src = FLURRY / rel
    dst = STARDUST / rel

    if not src.exists():
        print("[MISSING IN FLURRY]", rel)
        missing += 1
        continue

    dst.parent.mkdir(parents=True, exist_ok=True)

    if dst.exists():

        backup = dst.with_suffix(dst.suffix + BACKUP_SUFFIX)

        shutil.copy2(dst, backup)

    shutil.copy2(src, dst)

    print("[COPIED]", rel)

    copied += 1

print()
print("="*72)
print("Updating weapon/serverobjects.lua")
print("="*72)

weapon_server = STARDUST / "bin/scripts/object/custom_content/draft_schematic/weapon/serverobjects.lua"

if weapon_server.exists():

    text = weapon_server.read_text()

    changed = False

    line = 'includeFile("custom_content/draft_schematic/weapon/component/serverobjects.lua")'

    if line not in text:

        text += "\n" + line
        changed = True

    line = 'includeFile("custom_content/draft_schematic/weapon/core/serverobjects.lua")'

    if line not in text:

        text += "\n" + line
        changed = True

    if changed:

        shutil.copy2(
            weapon_server,
            weapon_server.with_suffix(".lua" + BACKUP_SUFFIX)
        )

        weapon_server.write_text(text)

        print("[UPDATED] weapon/serverobjects.lua")

    else:

        print("[OK] already registered")

else:

    print("[WARNING] weapon/serverobjects.lua not found")

print()
print("="*72)
print("Summary")
print("="*72)

print("Copied :", copied)
print("Missing :", missing)

print()
print("Done.")
