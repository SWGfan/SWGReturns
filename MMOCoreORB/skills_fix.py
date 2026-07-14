#!/usr/bin/env python3

from pathlib import Path
import shutil
import argparse
import filecmp

parser = argparse.ArgumentParser(
    description="Restore missing Lua skills tree from Stardust"
)

parser.add_argument(
    "--source",
    required=True,
    help="Path to the Stardust MMOCoreORB"
)

parser.add_argument(
    "--dest",
    default=".",
    help="Path to your current MMOCoreORB"
)

parser.add_argument(
    "--overwrite",
    action="store_true",
    help="Overwrite different files (default only restores missing files)"
)

args = parser.parse_args()

SRC = Path(args.source).resolve()
DST = Path(args.dest).resolve()

SRC_SKILLS = SRC / "bin/scripts/skills"
DST_SKILLS = DST / "bin/scripts/skills"

if not SRC_SKILLS.exists():
    print("ERROR: Source skills directory not found:")
    print(SRC_SKILLS)
    exit(1)

if not DST_SKILLS.exists():
    print("ERROR: Destination skills directory not found:")
    print(DST_SKILLS)
    exit(1)

restored = 0
updated = 0
skipped = 0

print("=" * 72)
print("Phase 7.12 - Restore Skills Tree")
print("=" * 72)
print()

for src in SRC_SKILLS.rglob("*"):

    rel = src.relative_to(SRC_SKILLS)
    dst = DST_SKILLS / rel

    if src.is_dir():
        dst.mkdir(parents=True, exist_ok=True)
        continue

    if not dst.exists():

        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)

        restored += 1
        print("RESTORED :", rel)

    else:

        same = filecmp.cmp(src, dst, shallow=False)

        if same:
            skipped += 1
            continue

        if args.overwrite:

            backup = dst.with_suffix(dst.suffix + ".phase712.bak")

            if not backup.exists():
                shutil.copy2(dst, backup)

            shutil.copy2(src, dst)

            updated += 1
            print("UPDATED  :", rel)

        else:
            skipped += 1

print()
print("=" * 72)
print("Restore Complete")
print("=" * 72)
print("Missing files restored :", restored)
print("Files updated          :", updated)
print("Files skipped          :", skipped)
print("=" * 72)
