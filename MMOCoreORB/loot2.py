#!/usr/bin/env python3

from pathlib import Path
import shutil

# -------------------------------------------------
# CHANGE THIS IF NEEDED
# -------------------------------------------------

RETURNS = Path.home() / "StarDust-2/MMOCoreORB"
FLURRY = Path.home() / "SWGFlurry/MMOCoreORB"

# -------------------------------------------------

COPY_DIRS = [

    "bin/scripts/loot/items/collection",
    "bin/scripts/loot/groups",

]

COPY_FILES = [

    "bin/scripts/loot/items.lua",
    "bin/scripts/loot/groups.lua",

]

copied = 0
updated = 0
backups = 0


def backup(path):

    global backups

    bak = Path(str(path) + ".pre_flurry")

    if not bak.exists():
        shutil.copy2(path, bak)
        backups += 1


#
# Copy entire collection directory
#

for rel in COPY_DIRS:

    src = FLURRY / rel
    dst = RETURNS / rel

    if not src.exists():
        print("Missing:", src)
        continue

    dst.mkdir(parents=True, exist_ok=True)

    for f in src.rglob("*"):

        if not f.is_file():
            continue

        relfile = f.relative_to(src)

        out = dst / relfile

        out.parent.mkdir(parents=True, exist_ok=True)

        if out.exists():

            with open(f, "rb") as a, open(out, "rb") as b:
                if a.read() == b.read():
                    continue

            backup(out)

            shutil.copy2(f, out)

            updated += 1

            print("Updated:", out)

        else:

            shutil.copy2(f, out)

            copied += 1

            print("Copied:", out)


#
# Merge items.lua/groups.lua
#

for rel in COPY_FILES:

    src = FLURRY / rel
    dst = RETURNS / rel

    if not src.exists() or not dst.exists():
        continue

    backup(dst)

    src_lines = src.read_text().splitlines()
    dst_lines = dst.read_text().splitlines()

    changed = False

    for line in src_lines:

        line = line.rstrip()

        if "includeFile(" not in line:
            continue

        if "collection" not in line.lower():
            continue

        if line not in dst_lines:

            dst_lines.append(line)

            changed = True

            print("Added include:", line)

    if changed:

        dst.write_text("\n".join(dst_lines) + "\n")

        updated += 1


print()
print("===================================")
print("Flurry Collection Import Complete")
print("===================================")
print("Files copied :", copied)
print("Files updated:", updated)
print("Backups made :", backups)
print()
print("Review the changes, then rebuild Core3.")
