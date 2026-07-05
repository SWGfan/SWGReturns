#!/usr/bin/env python3
"""
migrate_flurry_loot.py
Migrates Flurry's custom_loot directory (items + groups) into a StarDust-2 install.

What it does:
  0. Creates a full timestamped backup of the entire StarDust-2 folder first
  1. Copies Flurry's bin/scripts/loot/custom_loot/ into StarDust's bin/scripts/loot/
  2. Appends the required includeFile() lines to StarDust's loot/serverobjects.lua
     (only if they aren't already there)
  3. Produces a migration report

Usage:
  python3 migrate_flurry_loot.py <flurry_root> <stardust_root> [backup_dir]

  backup_dir defaults to the parent of stardust_root if not specified.

Example (live server):
  python3 migrate_flurry_loot.py \\
    /home/ubuntu/SWGFlurry/MMOCoreORB \\
    /home/ubuntu/StarDust-2/MMOCoreORB

  python3 migrate_flurry_loot.py \\
    /home/ubuntu/SWGFlurry/MMOCoreORB \\
    /home/ubuntu/StarDust-2/MMOCoreORB \\
    /mnt/backups
"""

import sys, os, shutil, datetime

# ── Args ───────────────────────────────────────────────────────────────────────
if len(sys.argv) < 3:
    print(__doc__); sys.exit(1)

flurry_root   = sys.argv[1].rstrip("/")
stardust_root = sys.argv[2].rstrip("/")
backup_base   = sys.argv[3].rstrip("/") if len(sys.argv) >= 4 else os.path.dirname(stardust_root)

flurry_loot   = f"{flurry_root}/bin/scripts/loot"
stardust_loot = f"{stardust_root}/bin/scripts/loot"

for p in (flurry_loot, stardust_loot):
    if not os.path.isdir(p):
        sys.exit(f"ERROR: directory not found: {p}")

# ── 0. Full backup of StarDust-2 root ─────────────────────────────────────────
timestamp   = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
stardust_name = os.path.basename(stardust_root)
backup_path = f"{backup_base}/{stardust_name}_backup_{timestamp}"

print(f"=== Step 0: Backing up StarDust-2 ===")
print(f"Source : {stardust_root}")
print(f"Backup : {backup_path}")
print("This may take a minute for large installs...", flush=True)

try:
    shutil.copytree(stardust_root, backup_path)
    # Count backed-up files for confirmation
    backed_up = sum(len(files) for _, _, files in os.walk(backup_path))
    print(f"Backup complete: {backed_up:,} files -> {backup_path}")
except Exception as e:
    sys.exit(f"ERROR: Backup failed: {e}\nAborting migration — nothing was changed.")

print()

# ── 1. Copy custom_loot tree ───────────────────────────────────────────────────
print("=== Step 1: Copying Flurry custom_loot ===")
src_custom = f"{flurry_loot}/custom_loot"
dst_custom  = f"{stardust_loot}/custom_loot"

if not os.path.isdir(src_custom):
    sys.exit(f"ERROR: Flurry custom_loot not found at: {src_custom}")

if os.path.exists(dst_custom):
    print(f"WARNING: {dst_custom} already exists — merging (existing files kept, new files added)")
    copied = skipped = 0
    for dirpath, dirnames, filenames in os.walk(src_custom):
        rel     = os.path.relpath(dirpath, src_custom)
        dst_dir = os.path.join(dst_custom, rel)
        os.makedirs(dst_dir, exist_ok=True)
        for fn in filenames:
            src_file = os.path.join(dirpath, fn)
            dst_file = os.path.join(dst_dir, fn)
            if os.path.exists(dst_file):
                skipped += 1
            else:
                shutil.copy2(src_file, dst_file)
                copied += 1
    print(f"Copied {copied:,} new files, skipped {skipped:,} existing files.")
else:
    shutil.copytree(src_custom, dst_custom)
    total = sum(len(f) for _, _, f in os.walk(dst_custom))
    print(f"Copied {total:,} files -> {dst_custom}")

print()

# ── 2. Patch serverobjects.lua ─────────────────────────────────────────────────
print("=== Step 2: Patching serverobjects.lua ===")
serverobjects = f"{stardust_loot}/serverobjects.lua"

if not os.path.isfile(serverobjects):
    sys.exit(f"ERROR: serverobjects.lua not found at: {serverobjects}")

with open(serverobjects) as f:
    content = f.read()

lines_to_add = [
    ('custom_loot/groups.lua',          'includeFile("custom_loot/groups.lua")'),
    ('custom_loot/common_groups.lua',   'includeFile("custom_loot/common_groups.lua")'),
    ('custom_loot/uncommon_groups.lua', 'includeFile("custom_loot/uncommon_groups.lua")'),
    ('custom_loot/rare_groups.lua',     'includeFile("custom_loot/rare_groups.lua")'),
    ('custom_loot/items.lua',           'includeFile("custom_loot/items.lua")'),
]

to_append       = []
already_present = []
for key, line in lines_to_add:
    if key in content:
        already_present.append(line)
    else:
        to_append.append(line)

if already_present:
    print(f"Already present in serverobjects.lua (skipped):")
    for l in already_present:
        print(f"  {l}")

if to_append:
    # serverobjects.lua backup already covered by the full tree backup above,
    # but keep a local one too for easy single-file restore
    shutil.copy2(serverobjects, serverobjects + ".pre_flurry_migration.bak")
    with open(serverobjects, "a") as f:
        f.write("\n-- Flurry custom loot (migrated)\n")
        for line in to_append:
            f.write(line + "\n")
    print(f"Appended {len(to_append)} includeFile() line(s) to serverobjects.lua:")
    for l in to_append:
        print(f"  {l}")
else:
    print("serverobjects.lua already fully up to date — no changes needed.")

print()

# ── 3. Summary ─────────────────────────────────────────────────────────────────
print("=== Migration Summary ===")
items_root = f"{dst_custom}/items"
if os.path.isdir(items_root):
    cats = sorted(d for d in os.listdir(items_root) if os.path.isdir(f"{items_root}/{d}"))
    print(f"Custom item categories: {len(cats)}")
    for c in cats:
        n = len(os.listdir(f"{items_root}/{c}"))
        print(f"  {c}: {n} files")
else:
    print("(no items subdirectory found)")

groups_root = f"{dst_custom}/groups"
if os.path.isdir(groups_root):
    group_files = sum(len(f) for _, _, f in os.walk(groups_root))
    print(f"\nCustom group files: {group_files}")

print(f"""
Backup location : {backup_path}
To roll back    : rm -rf {stardust_root} && cp -r {backup_path} {stardust_root}

Done. Restart the server to load the new loot content.
""")
