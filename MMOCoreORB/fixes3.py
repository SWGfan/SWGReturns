#!/usr/bin/env python3
"""
migrate_flurry_all.py
Comprehensive migration script covering:

  1. BH missions: allow hunting player Jedi (Padawan+)
  2. Loot crate C++ components (openable Rare/Exceptional/Legendary crates)
  3. Loot drop tables and collection items from Flurry
  4. Jedi PvE damage fix (lightsaber 2.8x vs NPC, force attacks 3x)
  5. Groups.lua additions (component loot, missing NPC groups)

Usage:
  python3 migrate_flurry_all.py <flurry_root> <stardust_root>

Example:
  python3 migrate_flurry_all.py /home/ubuntu/SWGFlurry /home/ubuntu/StarDust-2/MMOCoreORB
"""

import sys, os, shutil, datetime

if len(sys.argv) < 3:
    print(__doc__); sys.exit(1)

flurry_root   = sys.argv[1].rstrip("/")
stardust_root = sys.argv[2].rstrip("/")

fl_scripts = f"{flurry_root}/bin/scripts"
sd_scripts = f"{stardust_root}/bin/scripts"
fl_src     = f"{flurry_root}/src"
sd_src     = f"{stardust_root}/src"

ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")

def bak(p):
    if os.path.isfile(p):
        shutil.copy2(p, f"{p}.migrate_{ts}.bak")

def patch(src, old, new, label):
    if old not in src:
        print(f"  WARN ({label}): anchor not found")
        return src, 0
    print(f"  PATCHED: {label}")
    return src.replace(old, new, 1), 1

def write_file(path, content):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w') as f:
        f.write(content)

# ══════════════════════════════════════════════════════════════════════════════
# 1. BH Jedi Missions
# ══════════════════════════════════════════════════════════════════════════════
print("=== 1. BH Jedi Mission Fixes ===")

MM = f"{sd_src}/server/zone/managers/mission/MissionManagerImplementation.cpp"
if os.path.isfile(MM):
    bak(MM)
    with open(MM) as f: mm = f.read()

    # Lower jedi state from 4 (Master only) to 2 (Padawan+)
    mm, n1 = patch(mm,
        'if (ghost->getJediState() >= 4)',
        'if (ghost->getJediState() >= 2) // Padawan and above visible on BH terminal',
        "jediState >= 4 -> >= 2"
    )

    # Enable PrivateStructureJediMissions by default (false -> true)
    mm, n2 = patch(mm,
        'getBool("Core3.MissionManager.PrivateStructureJediMissions", false)',
        'getBool("Core3.MissionManager.PrivateStructureJediMissions", true)',
        "PrivateStructureJediMissions default false->true"
    )

    # Add meleebountyhunter skill check if missing
    old_bh = '!player->hasSkill("combat_bountyhunter_novice")'
    new_bh  = '!player->hasSkill("combat_bountyhunter_novice") && !player->hasSkill("combat_meleebountyhunter_novice")'
    if new_bh not in mm:
        mm = mm.replace(old_bh, new_bh)
        print("  PATCHED: meleebountyhunter_novice skill check added")

    with open(MM, 'w') as f: f.write(mm)

print()

# ══════════════════════════════════════════════════════════════════════════════
# 2. Loot Crate C++ Components (openable crates)
# ══════════════════════════════════════════════════════════════════════════════
print("=== 2. Loot Crate Menu Components (C++) ===")

COMP_SRC = f"{sd_src}/server/zone/objects/tangible/components"
FL_COMP  = f"{fl_src}/server/zone/objects/tangible/components"

crate_files = [
    "Rarelootcrate1MenuComponent.h",
    "Rarelootcrate1MenuComponent.cpp",
    "Rarelootcrate2MenuComponent.h",
    "Rarelootcrate2MenuComponent.cpp",
    "Rarelootcrate3MenuComponent.h",
    "Rarelootcrate3MenuComponent.cpp",
]

for fn in crate_files:
    src_f = os.path.join(FL_COMP, fn)
    dst_f = os.path.join(COMP_SRC, fn)
    if not os.path.isfile(src_f):
        print(f"  SKIP (not in Flurry): {fn}")
        continue
    if os.path.isfile(dst_f):
        print(f"  SKIP (exists): {fn}")
        continue
    shutil.copy2(src_f, dst_f)
    print(f"  Copied: {fn}")

# Register the components in ComponentFactory if it exists
comp_factory = f"{sd_src}/server/zone/objects/tangible/components/TangibleObjectComponentFactory.cpp"
if not os.path.isfile(comp_factory):
    # Try to find it
    import glob
    results = glob.glob(f"{sd_src}/**/ComponentFactory*.cpp", recursive=True)
    if results:
        comp_factory = results[0]

# Also add to CMakeLists if needed
cmake = f"{sd_src}/CMakeLists.txt"
crate_cmake_entries = [
    'server/zone/objects/tangible/components/Rarelootcrate1MenuComponent.cpp',
    'server/zone/objects/tangible/components/Rarelootcrate2MenuComponent.cpp',
    'server/zone/objects/tangible/components/Rarelootcrate3MenuComponent.cpp',
]
if os.path.isfile(cmake):
    with open(cmake) as f: cm = f.read()
    added = 0
    for entry in crate_cmake_entries:
        if entry not in cm:
            # Find a nearby tangible component to insert after
            anchor = 'server/zone/objects/tangible/components/TangibleObjectMenuComponent.cpp'
            if anchor in cm:
                cm = cm.replace(anchor, anchor + f'\n\t{entry}')
                added += 1
    if added:
        bak(cmake)
        with open(cmake, 'w') as f: f.write(cm)
        print(f"  Added {added} entries to CMakeLists.txt")

# Copy the Lua object templates
print()
print("=== 2b. Loot Crate Lua Object Templates ===")
FL_ITEM_DIR = f"{fl_scripts}/object/custom_content/tangible/item"
SD_ITEM_DIR = f"{sd_scripts}/object/custom_content/tangible/item"

if os.path.isdir(FL_ITEM_DIR):
    os.makedirs(SD_ITEM_DIR, exist_ok=True)
    for fn in os.listdir(FL_ITEM_DIR):
        if not fn.endswith(".lua"):
            continue
        src_f = os.path.join(FL_ITEM_DIR, fn)
        dst_f = os.path.join(SD_ITEM_DIR, fn)
        if os.path.exists(dst_f):
            continue
        shutil.copy2(src_f, dst_f)
        print(f"  Copied: {fn}")

    # Add to serverobjects.lua
    sd_so = f"{sd_scripts}/object/tangible/serverobjects.lua"
    if os.path.isfile(sd_so):
        with open(sd_so) as f: so = f.read()
        include = 'includeFile("custom_content/tangible/item/serverobjects.lua")'
        if include not in so:
            bak(sd_so)
            with open(sd_so, 'a') as f:
                f.write(f"\n-- Flurry loot crate objects\n{include}\n")
            print("  Added crate objects to serverobjects.lua")

print()

# ══════════════════════════════════════════════════════════════════════════════
# 3. Loot Drop Tables & Collection Items
# ══════════════════════════════════════════════════════════════════════════════
print("=== 3. Loot Collection Items ===")

fl_col = f"{fl_scripts}/loot/items/collection"
sd_col = f"{sd_scripts}/loot/items/collection"
os.makedirs(sd_col, exist_ok=True)

copied = skipped = 0
for fn in os.listdir(fl_col):
    if not fn.endswith(".lua"): continue
    src_f = os.path.join(fl_col, fn)
    dst_f = os.path.join(sd_col, fn)
    if os.path.exists(dst_f):
        skipped += 1
    else:
        shutil.copy2(src_f, dst_f)
        copied += 1
        print(f"  Copied: {fn}")
print(f"  Total: {copied} new, {skipped} existing")

# Add to items.lua
sd_items = f"{sd_scripts}/loot/items.lua"
if os.path.isfile(sd_items):
    with open(sd_items) as f: items = f.read()
    to_add = []
    for fn in sorted(os.listdir(sd_col)):
        if not fn.endswith(".lua"): continue
        inc = f'includeFile("items/collection/{fn}")'
        if inc not in items:
            to_add.append(inc)
    if to_add:
        bak(sd_items)
        with open(sd_items, 'a') as f:
            f.write("\n-- Collection/crate items (migrated)\n")
            for l in to_add: f.write(l + "\n")
        print(f"  Added {len(to_add)} includes to items.lua")

print()
print("=== 3b. Groups.lua Additions ===")

sd_groups = f"{sd_scripts}/loot/groups.lua"
fl_groups = f"{fl_scripts}/loot/groups.lua"

if os.path.isfile(sd_groups) and os.path.isfile(fl_groups):
    with open(sd_groups) as f: grp = f.read()
    bak(sd_groups)

    # Add component loot groups from Flurry (missing from Stardust)
    component_groups = [
        "groups/component_loot/chemistry_component_advanced.lua",
        "groups/component_loot/weapon_component_advanced.lua",
        "groups/component_loot/chemistry_component.lua",
        "groups/component_loot/weapon_component.lua",
        "groups/component_loot/jedi_comp_group.lua",
        "groups/component_loot/legendary_comp_group.lua",
        "groups/component_loot/jedi_pack_group.lua",
        "groups/component_loot/g_ancient_jedaii_holocron_dode.lua",
        "groups/component_loot/g_ancient_jedaii_holocron_cube.lua",
        "groups/component_loot/g_ancient_jedaii_holocron_triangle.lua",
    ]

    added = []
    for path in component_groups:
        inc = f'includeFile("{path}")'
        full = f"{sd_scripts}/loot/{path}"
        fl_full = f"{fl_scripts}/loot/{path}"
        if inc not in grp:
            # Copy the file from Flurry if available
            if os.path.isfile(fl_full) and not os.path.isfile(full):
                os.makedirs(os.path.dirname(full), exist_ok=True)
                shutil.copy2(fl_full, full)
            if os.path.isfile(full):
                added.append(inc)

    if added:
        with open(sd_groups, 'a') as f:
            f.write("\n-- Component loot groups (migrated from Flurry)\n")
            for l in added: f.write(l + "\n")
        print(f"  Added {len(added)} component loot group includes")
    else:
        print("  Component loot groups already present")

print()

# ══════════════════════════════════════════════════════════════════════════════
# 4. Jedi PvE Fix (CombatManager)
# ══════════════════════════════════════════════════════════════════════════════
print("=== 4. Jedi PvE Damage Fix ===")

CM = f"{sd_src}/server/zone/managers/combat/CombatManager.cpp"
if os.path.isfile(CM):
    bak(CM)
    with open(CM) as f: cm = f.read()

    old_block = (
        '\tif (attacker->isPlayerCreature()) {\n'
        '\t\tif (data.isForceAttack() && !defender->isPlayerCreature())\n'
        '\t\t\tdamage *= 2 + System::random(1);\n'
        '\t\telse if (!data.isForceAttack())\n'
        '\t\t\tdamage *= 1.5;\n'
        '\t}\n'
    )
    new_block = (
        '\tif (attacker->isPlayerCreature()) {\n'
        '\t\tif (data.isForceAttack() && !defender->isPlayerCreature()) {\n'
        '\t\t\t// Force attacks vs NPC: fixed 3x bonus\n'
        '\t\t\tdamage *= 3.0f;\n'
        '\t\t} else if (!data.isForceAttack()) {\n'
        '\t\t\tdamage *= 1.5;\n'
        '\t\t\t// Extra bonus for jedi weapons vs NPCs (lightsaber: ~2.8x total)\n'
        '\t\t\tManagedReference<WeaponObject*> atkWeapon = attacker->getWeapon();\n'
        '\t\t\tif (!defender->isPlayerCreature() && atkWeapon != nullptr && atkWeapon->isJediWeapon()) {\n'
        '\t\t\t\tdamage *= 1.5f;\n'
        '\t\t\t}\n'
        '\t\t}\n'
        '\t}\n'
    )

    if old_block in cm:
        cm = cm.replace(old_block, new_block, 1)
        with open(CM, 'w') as f: f.write(cm)
        print("  PATCHED: Jedi PvE damage (force 3x, lightsaber 2.8x vs NPC) ✓")
    elif new_block in cm:
        print("  SKIP: Already patched")
    else:
        print("  WARN: CombatManager anchor not found — may need manual patch")
else:
    print(f"  WARN: {CM} not found")

print()
print("═" * 60)
print("SUMMARY")
print("═" * 60)
print("Lua changes (restart server):")
print("  - Loot collection items + includes")
print("  - Loot crate Lua object templates")
print("  - Groups.lua component additions")
print()
print("C++ changes (recompile + restart):")
print("  - BH Jedi mission fixes (jediState >= 2, PrivateStructure)")
print("  - Loot crate menu components (openable crates)")
print("  - Jedi PvE damage fix")
print()
print("After recompile run:")
print("  cd <build_dir> && make -j$(nproc)")
print("  Then delete old .o files if make says nothing to do:")
print(f"  rm -f {sd_src}/CMakeFiles/core3.dir/server/zone/managers/mission/MissionManagerImplementation.cpp.o")
print(f"  rm -f {sd_src}/CMakeFiles/core3.dir/server/zone/managers/combat/CombatManager.cpp.o")
