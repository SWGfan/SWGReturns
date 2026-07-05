#!/usr/bin/env python3
"""
migrate_flurry_loot_tables.py
Migrates Flurry's improved loot tables, crate items, and component groups
into StarDust-2.

What it migrates:
  1. items/collection/ — all crate/tier/diamond/rareloot item definitions
     (rareloot1/2/3, collectiontierone/two/three, collectiondiamond,
      collectionheroic, contraband, resource_crate, resource_deed,
      darkfrs, lightfrs, force_bread)
  2. items/collection/ — adds includeFile lines to items.lua
  3. groups.lua — adds missing NPC loot group includes from Flurry
  4. Verifies component_loot groups are included
  5. Adds krayt_pearls_flawless and mythosaur_common group includes

Usage:
  python3 migrate_flurry_loot_tables.py <flurry_root> <stardust_root>

Example:
  python3 migrate_flurry_loot_tables.py /home/ubuntu/SWGFlurry /home/ubuntu/StarDust-2/MMOCoreORB
"""

import sys, os, shutil, datetime

if len(sys.argv) < 3:
    print(__doc__); sys.exit(1)

flurry_root   = sys.argv[1].rstrip("/")
stardust_root = sys.argv[2].rstrip("/")

fl = f"{flurry_root}/bin/scripts/loot"
sd = f"{stardust_root}/bin/scripts/loot"

for p in (fl, sd):
    if not os.path.isdir(p):
        sys.exit(f"ERROR: not found: {p}")

ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")

def patch(src, old, new, label):
    if old not in src:
        print(f"  WARN ({label}): anchor not found")
        return src, 0
    print(f"  PATCHED: {label}")
    return src.replace(old, new, 1), 1

# ── 1. Copy collection item files ─────────────────────────────────────────────
print("=== Step 1: Collection/crate item definitions ===")
fl_col = f"{fl}/items/collection"
sd_col = f"{sd}/items/collection"

os.makedirs(sd_col, exist_ok=True)
copied = skipped = 0
for fn in os.listdir(fl_col):
    if not fn.endswith(".lua"):
        continue
    src_f = os.path.join(fl_col, fn)
    dst_f = os.path.join(sd_col, fn)
    if os.path.exists(dst_f):
        skipped += 1
    else:
        shutil.copy2(src_f, dst_f)
        copied += 1
        print(f"  Copied: {fn}")

print(f"  Total: {copied} copied, {skipped} existing")

# ── 2. Add collection includes to items.lua ───────────────────────────────────
print()
print("=== Step 2: Add collection includes to items.lua ===")
sd_items = f"{sd}/items.lua"

if not os.path.isfile(sd_items):
    print(f"  WARN: {sd_items} not found")
else:
    with open(sd_items) as f:
        items_content = f.read()

    # Build list of includes to add
    to_add = []
    for fn in sorted(os.listdir(sd_col)):
        if not fn.endswith(".lua"):
            continue
        name = fn[:-4]
        include = f'includeFile("items/collection/{fn}")'
        if include not in items_content:
            to_add.append(include)

    if to_add:
        shutil.copy2(sd_items, f"{sd_items}.lootfix_{ts}.bak")
        with open(sd_items, 'a') as f:
            f.write("\n-- Flurry collection/crate items (migrated)\n")
            for line in to_add:
                f.write(line + "\n")
        print(f"  Added {len(to_add)} includeFile() lines to items.lua")
    else:
        print("  SKIP: all collection files already included")

# ── 3. Fix groups.lua ─────────────────────────────────────────────────────────
print()
print("=== Step 3: Fix groups.lua ===")
sd_groups = f"{sd}/groups.lua"

if not os.path.isfile(sd_groups):
    print(f"  WARN: {sd_groups} not found")
else:
    shutil.copy2(sd_groups, f"{sd_groups}.lootfix_{ts}.bak")
    with open(sd_groups) as f:
        groups_content = f.read()

    changes = 0

    # 3a. Add component_loot groups if missing
    component_groups = [
        ('groups/component_loot/chemistry_component_advanced.lua', 'chemistry_component_advanced'),
        ('groups/component_loot/weapon_component_advanced.lua',    'weapon_component_advanced'),
        ('groups/component_loot/chemistry_component.lua',          'chemistry_component'),
        ('groups/component_loot/weapon_component.lua',             'weapon_component'),
        ('groups/component_loot/jedi_comp_group.lua',              'jedi_comp_group'),
        ('groups/component_loot/legendary_comp_group.lua',         'legendary_comp_group'),
        ('groups/component_loot/jedi_pack_group.lua',              'jedi_pack_group'),
    ]

    missing_comps = []
    for path, name in component_groups:
        include = f'includeFile("{path}")'
        full_path = f"{sd}/{path}"
        if include not in groups_content and os.path.isfile(full_path):
            missing_comps.append(include)

    if missing_comps:
        groups_content += "\n-- component loot groups (migrated)\n"
        for line in missing_comps:
            groups_content += line + "\n"
            print(f"  Added: {line}")
        changes += len(missing_comps)

    # 3b. Add Flurry NPC groups missing from Stardust
    fl_npc_groups = [
        'groups/npc/dantari_common.lua',
        'groups/npc/kunga_common.lua',
        'groups/npc/mokk_common.lua',
        'groups/npc/nightsister_rare.lua',
    ]
    missing_npc = []
    for path in fl_npc_groups:
        include = f'includeFile("{path}")'
        fl_full = f"{fl}/{path}"
        sd_full = f"{sd}/{path}"
        if include not in groups_content and os.path.isfile(fl_full):
            # Copy the file first
            os.makedirs(os.path.dirname(sd_full), exist_ok=True)
            if not os.path.exists(sd_full):
                shutil.copy2(fl_full, sd_full)
                print(f"  Copied group file: {path}")
            missing_npc.append(include)

    if missing_npc:
        groups_content += "\n-- Flurry NPC loot groups (migrated)\n"
        for line in missing_npc:
            groups_content += line + "\n"
            print(f"  Added: {line}")
        changes += len(missing_npc)

    # 3c. Add krayt_pearls_flawless and mythosaur_common if missing
    extra_groups = [
        ('groups/creature/krayt_pearls_flawless.lua', 'krayt_pearls_flawless'),
        ('groups/creature/mythosaur_common.lua',       'mythosaur_common'),
    ]
    for path, name in extra_groups:
        include = f'includeFile("{path}")'
        fl_full = f"{fl}/{path}"
        sd_full = f"{sd}/{path}"
        if include not in groups_content and os.path.isfile(fl_full):
            os.makedirs(os.path.dirname(sd_full), exist_ok=True)
            if not os.path.exists(sd_full):
                shutil.copy2(fl_full, sd_full)
                print(f"  Copied: {path}")
            groups_content += include + "\n"
            print(f"  Added: {include}")
            changes += 1

    if changes:
        with open(sd_groups, 'w') as f:
            f.write(groups_content)
        print(f"  Total groups.lua changes: {changes}")
    else:
        print("  No changes needed to groups.lua")

# ── 4. Copy rarelootsystem and lootcollectiontierdiamonds groups ──────────────
print()
print("=== Step 4: Rare loot system and diamond tier groups ===")

special_groups = [
    'groups/rarelootsystem.lua',
    'groups/lootcollectiontierdiamonds.lua',
    'groups/lootcollectiontierthree.lua',
]
for path in special_groups:
    fl_f = f"{fl}/{path}"
    sd_f = f"{sd}/{path}"
    if os.path.isfile(fl_f) and not os.path.exists(sd_f):
        os.makedirs(os.path.dirname(sd_f), exist_ok=True)
        shutil.copy2(fl_f, sd_f)
        print(f"  Copied: {path}")
        # Add to groups.lua
        include = f'includeFile("{path}")'
        with open(sd_groups) as f:
            gc = f.read()
        if include not in gc:
            with open(sd_groups, 'a') as f:
                f.write(include + "\n")
            print(f"  Added to groups.lua: {include}")
    elif os.path.isfile(fl_f):
        print(f"  SKIP (exists): {path}")
    else:
        print(f"  SKIP (not in Flurry): {path}")

print()
print("=== Done. Restart server to apply loot table changes. ===")
