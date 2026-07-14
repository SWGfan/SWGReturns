#!/usr/bin/env python3

from pathlib import Path
import argparse
import shutil
import filecmp

parser = argparse.ArgumentParser()
parser.add_argument("flurry")
parser.add_argument("--overwrite", action="store_true")
args = parser.parse_args()

ROOT = Path(".").resolve()
FLURRY = Path(args.flurry).resolve()

FILES = [

(
"bin/scripts/object/draft_schematic/weapon/component",
[
"blaster_rifle_barrel_advanced.lua",
"blaster_pistol_barrel_advanced.lua",
"projectile_rifle_barrel_advanced.lua",
"projectile_pistol_barrel_advanced.lua",
"stock_advanced.lua",
],
),

(
"bin/scripts/object/tangible/component/weapon",
[
"blaster_rifle_barrel_advanced.lua",
"blaster_pistol_barrel_advanced.lua",
"projectile_rifle_barrel_advanced.lua",
"projectile_pistol_barrel_advanced.lua",
"stock_advanced.lua",
],
),

]

REGISTRATION_FILES = [

"bin/scripts/object/draft_schematic/weapon/component/objects.lua",
"bin/scripts/object/draft_schematic/weapon/component/serverobjects.lua",

"bin/scripts/object/tangible/component/weapon/objects.lua",
"bin/scripts/object/tangible/component/weapon/serverobjects.lua",

]

copied = 0
updated = 0
skipped = 0
merged = 0
backups = 0

print("="*72)
print("Phase 7.08a - Import Advanced Weapon Components")
print("="*72)

#
# copy lua files
#

for directory, files in FILES:

    srcdir = FLURRY / directory
    dstdir = ROOT / directory

    for filename in files:

        src = srcdir / filename
        dst = dstdir / filename

        if not src.exists():
            print("Missing in Flurry:", filename)
            continue

        if not dst.exists():

            shutil.copy2(src,dst)

            copied += 1
            print("[NEW ]",filename)
            continue

        if filecmp.cmp(src,dst,shallow=False):
            skipped += 1
            continue

        if not args.overwrite:
            print("[DIFF]",filename)
            updated += 1
            continue

        bak = dst.with_suffix(dst.suffix+".phase708a.bak")

        if not bak.exists():
            shutil.copy2(dst,bak)
            backups += 1

        shutil.copy2(src,dst)

        updated += 1
        print("[SYNC]",filename)

#
# merge registrations
#

for rel in REGISTRATION_FILES:

    src = FLURRY / rel
    dst = ROOT / rel

    if not src.exists() or not dst.exists():
        continue

    bak = dst.with_suffix(dst.suffix+".phase708a.bak")

    if not bak.exists():
        shutil.copy2(dst,bak)

    dstlines = dst.read_text().splitlines()
    srclines = src.read_text().splitlines()

    existing = set(l.strip() for l in dstlines)

    add = []

    for line in srclines:

        s = line.strip()

        if not s.startswith("includeFile"):
            continue

        if s in existing:
            continue

        add.append(line)

    if add:

        with dst.open("a") as f:

            f.write("\n")
            f.write("-- Phase 7.08a\n")

            for line in add:
                f.write(line.rstrip()+"\n")

        merged += len(add)

print()
print("="*72)
print("Finished")
print("="*72)
print("Copied :",copied)
print("Updated:",updated)
print("Merged :",merged)
print("Skipped:",skipped)
print("Backups:",backups)
