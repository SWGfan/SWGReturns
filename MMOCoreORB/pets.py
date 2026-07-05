#!/usr/bin/env python3
"""
migrate_flurry_pets.py
Copies Flurry's custom pet content into StarDust-2.

What it migrates:
  1. object/custom_content/intangible/pet/  (87 files — BM pets, droids, mounts,
     tcg familiars, som pets, holiday pets etc.)
  2. object/custom_content/tangible/deed/pet_deed/ (4 deed files)
  3. Adds includeFile() for the custom_content pet serverobjects into
     StarDust's object/intangible/pet/serverobjects.lua
  4. mobile/pet/ — adds any Flurry mobile files not already in StarDust

Usage:
  python3 migrate_flurry_pets.py <flurry_root> <stardust_root> [backup_dir]

Example:
  python3 migrate_flurry_pets.py /home/ubuntu/SWGFlurry /home/ubuntu/StarDust-2/MMOCoreORB
"""

import sys, os, shutil, datetime

if len(sys.argv) < 3:
    print(__doc__); sys.exit(1)

flurry_root   = sys.argv[1].rstrip("/")
stardust_root = sys.argv[2].rstrip("/")
backup_base   = sys.argv[3].rstrip("/") if len(sys.argv) >= 4 else stardust_root

fl = f"{flurry_root}/bin/scripts"
sd = f"{stardust_root}/bin/scripts"

for p in (fl, sd):
    if not os.path.isdir(p):
        sys.exit(f"ERROR: not found: {p}")

ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")

# ── 1. Copy custom_content/intangible/pet ─────────────────────────────────────
print("=== Step 1: Custom pet PCD files ===")
src_pet = f"{fl}/object/custom_content/intangible/pet"
dst_pet = f"{sd}/object/custom_content/intangible/pet"

if not os.path.isdir(src_pet):
    print(f"  SKIP: {src_pet} not found in Flurry")
else:
    copied = skipped = 0
    for dirpath, dirnames, filenames in os.walk(src_pet):
        rel     = os.path.relpath(dirpath, src_pet)
        dst_dir = os.path.join(dst_pet, rel)
        os.makedirs(dst_dir, exist_ok=True)
        for fn in filenames:
            src_f = os.path.join(dirpath, fn)
            dst_f = os.path.join(dst_dir, fn)
            if os.path.exists(dst_f):
                skipped += 1
            else:
                shutil.copy2(src_f, dst_f)
                copied += 1
    print(f"  Copied {copied} files, skipped {skipped} existing")

# ── 2. Copy custom_content/tangible/deed/pet_deed ─────────────────────────────
print()
print("=== Step 2: Custom pet deed files ===")
src_deed = f"{fl}/object/custom_content/tangible/deed/pet_deed"
dst_deed = f"{sd}/object/custom_content/tangible/deed/pet_deed"

if not os.path.isdir(src_deed):
    print(f"  SKIP: {src_deed} not found in Flurry")
else:
    os.makedirs(dst_deed, exist_ok=True)
    copied = skipped = 0
    for fn in os.listdir(src_deed):
        src_f = os.path.join(src_deed, fn)
        dst_f = os.path.join(dst_deed, fn)
        if os.path.exists(dst_f):
            skipped += 1
        else:
            shutil.copy2(src_f, dst_f)
            copied += 1
            print(f"  Copied: {fn}")
    print(f"  Total: {copied} copied, {skipped} skipped")

# ── 3. Patch StarDust's intangible/pet/serverobjects.lua ──────────────────────
print()
print("=== Step 3: Patch serverobjects.lua ===")
sd_so = f"{sd}/object/intangible/pet/serverobjects.lua"

if not os.path.isfile(sd_so):
    print(f"  WARN: {sd_so} not found")
else:
    with open(sd_so) as f:
        content = f.read()

    include_line = 'includeFile("custom_content/intangible/pet/serverobjects.lua")'

    if include_line in content:
        print("  SKIP: already included in serverobjects.lua")
    else:
        shutil.copy2(sd_so, f"{sd_so}.petmig_{ts}.bak")
        # Append at the end
        with open(sd_so, 'a') as f:
            f.write(f"\n-- Flurry custom pets (migrated)\n{include_line}\n")
        print(f"  PATCHED: added {include_line}")

# ── 4. Mobile pet files unique to Flurry ──────────────────────────────────────
print()
print("=== Step 4: Mobile pet files ===")
fl_mob = f"{fl}/mobile/pet"
sd_mob = f"{sd}/mobile/pet"

if not os.path.isdir(fl_mob):
    print(f"  SKIP: {fl_mob} not found")
else:
    copied = skipped = 0
    for fn in os.listdir(fl_mob):
        if not fn.endswith(".lua"):
            continue
        src_f = os.path.join(fl_mob, fn)
        dst_f = os.path.join(sd_mob, fn)
        if os.path.exists(dst_f):
            skipped += 1
        else:
            shutil.copy2(src_f, dst_f)
            copied += 1
            print(f"  Copied mobile: {fn}")
    print(f"  Total: {copied} copied, {skipped} skipped")

# ── 5. Also copy Flurry's pet deed serverobjects into stardust deed path ──────
print()
print("=== Step 5: Pet deed serverobjects ===")
fl_deed_so = f"{fl}/object/custom_content/tangible/deed/pet_deed/serverobjects.lua"
sd_deed_dir = f"{sd}/object/tangible/deed/pet_deed"
sd_deed_so  = f"{sd_deed_dir}/serverobjects.lua"

if os.path.isfile(fl_deed_so) and os.path.isfile(sd_deed_so):
    with open(fl_deed_so) as f:
        fl_includes = set(line.strip() for line in f if 'includeFile' in line)
    with open(sd_deed_so) as f:
        sd_content = f.read()

    to_add = [l for l in fl_includes if l not in sd_content]
    if to_add:
        shutil.copy2(sd_deed_so, f"{sd_deed_so}.petmig_{ts}.bak")
        with open(sd_deed_so, 'a') as f:
            f.write("\n-- Flurry custom pet deeds (migrated)\n")
            for l in sorted(to_add):
                f.write(l + "\n")
        print(f"  Added {len(to_add)} deed includeFile() line(s)")
    else:
        print("  SKIP: deed serverobjects already up to date")

print()
print("=== Done. Restart server to load new pets. ===")
