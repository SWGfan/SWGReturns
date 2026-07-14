#!/usr/bin/env python3

import os
import re
import shutil
from pathlib import Path

# -------------------------------------------------------
# Automatically locate the armor directory
# -------------------------------------------------------

SCRIPT_DIR = Path(__file__).resolve().parent

SEARCH_PATHS = [
    SCRIPT_DIR / "bin/scripts/object/tangible/wearables/armor",
    SCRIPT_DIR / "MMOCoreORB/bin/scripts/object/tangible/wearables/armor",
    SCRIPT_DIR.parent / "MMOCoreORB/bin/scripts/object/tangible/wearables/armor",
    Path.cwd() / "bin/scripts/object/tangible/wearables/armor",
    Path.cwd() / "MMOCoreORB/bin/scripts/object/tangible/wearables/armor",
]

ROOT = None

for path in SEARCH_PATHS:
    if path.exists():
        ROOT = path
        break

if ROOT is None:
    print("ERROR: Could not locate:")
    print("bin/scripts/object/tangible/wearables/armor")
    exit(1)

print(f"Using armor directory:\n{ROOT}\n")

# -------------------------------------------------------
# Mandalorian armor folders
# -------------------------------------------------------

TARGETS = [
    "mandalorian",
    "mandalorian_rebel",
    "mandalorian_imperial",
    "mandalorian_sabine"
]

# Match:
# rating = LIGHT
# rating=LIGHT
# rating = LIGHT,
# rating = LIGHT --comment

pattern = re.compile(
    r'(\brating\s*=\s*)LIGHT(\s*,?)',
    flags=re.IGNORECASE
)

modified = 0
checked = 0

for folder in TARGETS:

    folder_path = ROOT / folder

    if not folder_path.exists():
        print(f"Skipping missing folder: {folder}")
        continue

    print(f"Scanning {folder}")

    for file in folder_path.rglob("*.lua"):

        checked += 1

        text = file.read_text(encoding="utf-8")

        newtext = pattern.sub(r"\1HEAVY\2", text)

        if newtext != text:

            backup = file.with_suffix(file.suffix + ".bak")

            if not backup.exists():
                shutil.copy2(file, backup)

            file.write_text(newtext, encoding="utf-8")

            modified += 1
            print(f"  Modified {file.name}")

print()
print("--------------------------------")
print(f"Files scanned : {checked}")
print(f"Files changed : {modified}")
print("--------------------------------")
