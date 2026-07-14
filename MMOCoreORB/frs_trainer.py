#!/usr/bin/env python3

"""
=========================================================
Phase 7.30
FRS Trainer Importer

Imports the mySWG FRS trainers into Stardust.

Copies:
    trainer_dark_sentinel.lua
    trainer_light_sentinel.lua

Updates:
    mobile/trainer/serverobjects.lua
    mobile/conversations/trainer/trainer_conv.lua

Creates .phase730.bak backups.

Safe to run multiple times.
=========================================================
"""

from pathlib import Path
import shutil
import re
import sys

FLURRY = Path.home() / "mySWG" / "MMOCoreORB"
STARDUST = Path.home() / "StarDust-2" / "MMOCoreORB"

BACKUP_EXT = ".phase730.bak"

FILES = [

"bin/scripts/mobile/trainer/trainer_dark_sentinel.lua",
"bin/scripts/mobile/trainer/trainer_light_sentinel.lua",

]

SERVEROBJECTS = STARDUST / "bin/scripts/mobile/trainer/serverobjects.lua"

TRAINER_CONV = STARDUST / "bin/scripts/mobile/conversations/trainer/trainer_conv.lua"

SERVER_LINES = [

'includeFile("trainer/trainer_dark_sentinel.lua")',
'includeFile("trainer/trainer_light_sentinel.lua")'

]

CONV_LINES = [

'createTrainerConversationTemplate("darkfrsTrainerConvoTemplate", "trainer_frs_dark")',
'createTrainerConversationTemplate("lightfrsTrainerConvoTemplate", "trainer_frs_light")'

]

print("=" * 70)
print("Phase 7.30 - FRS Trainer Import")
print("=" * 70)

copied = 0
updated = 0

#
# Copy trainer mobiles
#

for rel in FILES:

    src = FLURRY / rel
    dst = STARDUST / rel

    if not src.exists():
        print("[MISSING]", rel)
        continue

    dst.parent.mkdir(parents=True, exist_ok=True)

    if dst.exists():
        shutil.copy2(dst, str(dst) + BACKUP_EXT)

    shutil.copy2(src, dst)

    copied += 1

    print("[COPIED]", rel)

#
# Patch serverobjects.lua
#

if SERVEROBJECTS.exists():

    text = SERVEROBJECTS.read_text()

    changed = False

    for line in SERVER_LINES:

        if line not in text:
            text += "\n" + line
            changed = True

    if changed:

        shutil.copy2(SERVEROBJECTS, str(SERVEROBJECTS) + BACKUP_EXT)

        SERVEROBJECTS.write_text(text)

        updated += 1

        print("[UPDATED] mobile/trainer/serverobjects.lua")

    else:

        print("[OK] mobile/trainer/serverobjects.lua")

else:

    print("[MISSING] mobile/trainer/serverobjects.lua")

#
# Patch trainer_conv.lua
#

if TRAINER_CONV.exists():

    text = TRAINER_CONV.read_text()

    changed = False

    for line in CONV_LINES:

        if line not in text:
            text += "\n" + line
            changed = True

    if changed:

        shutil.copy2(TRAINER_CONV, str(TRAINER_CONV) + BACKUP_EXT)

        TRAINER_CONV.write_text(text)

        updated += 1

        print("[UPDATED] trainer_conv.lua")

    else:

        print("[OK] trainer_conv.lua")

else:

    print("[MISSING] trainer_conv.lua")

print()
print("=" * 70)
print("SUMMARY")
print("=" * 70)
print("Mobiles copied :", copied)
print("Files updated  :", updated)
print("=" * 70)

print()
print("Done.")
print()
print("Next rebuild:")
print()
print("make -j$(nproc)")
