#!/usr/bin/env python3

from pathlib import Path
import shutil
from datetime import datetime

# ---------------------------------------------------------------------
# CONFIG
# ---------------------------------------------------------------------

STOCK = Path("/home/ubuntu/Desktop/Stardust-2/MMOCoreORB")
LIVE  = Path("/home/ubuntu/StarDust-2/MMOCoreORB")

STOCK_DS = STOCK / "bin/scripts/object/draft_schematic"
LIVE_DS  = LIVE  / "bin/scripts/object/draft_schematic"

STOCK_SCHEMATICS = STOCK / "bin/scripts/managers/crafting/schematics.lua"
LIVE_SCHEMATICS  = LIVE  / "bin/scripts/managers/crafting/schematics.lua"

BACKUP = LIVE / ("phase715_backup_" +
                 datetime.now().strftime("%Y%m%d_%H%M%S"))

# ---------------------------------------------------------------------

print("="*72)
print("Phase 7.15 - Restore Draft Schematics")
print("="*72)
print()

BACKUP.mkdir(parents=True)

print("Creating backup...")

#
# Backup live draft_schematic tree
#
shutil.copytree(
    LIVE_DS,
    BACKUP / "draft_schematic",
    dirs_exist_ok=True
)

#
# Backup schematics.lua
#
if LIVE_SCHEMATICS.exists():
    shutil.copy2(
        LIVE_SCHEMATICS,
        BACKUP / "schematics.lua"
    )

print("Backup written to")
print(BACKUP)
print()

# ---------------------------------------------------------------------
# Discover custom files
# ---------------------------------------------------------------------

print("Scanning custom schematics...")

custom = []

for live in LIVE_DS.rglob("*.lua"):

    rel = live.relative_to(LIVE_DS)

    stock = STOCK_DS / rel

    if not stock.exists():
        custom.append(rel)

print("Custom files :", len(custom))
print()

# ---------------------------------------------------------------------
# Restore stock tree
# ---------------------------------------------------------------------

print("Restoring stock draft_schematic tree...")

if LIVE_DS.exists():
    shutil.rmtree(LIVE_DS)

shutil.copytree(STOCK_DS, LIVE_DS)

print("Done.")
print()

# ---------------------------------------------------------------------
# Restore stock schematics.lua
# ---------------------------------------------------------------------

print("Restoring stock schematics.lua")

shutil.copy2(
    STOCK_SCHEMATICS,
    LIVE_SCHEMATICS
)

print()

# ---------------------------------------------------------------------
# Restore custom files
# ---------------------------------------------------------------------

print("Restoring custom files...")

restored = 0

for rel in custom:

    src = BACKUP / "draft_schematic" / rel
    dst = LIVE_DS / rel

    dst.parent.mkdir(parents=True, exist_ok=True)

    shutil.copy2(src, dst)

    restored += 1

print("Custom files restored :", restored)
print()

# ---------------------------------------------------------------------
# Report
# ---------------------------------------------------------------------

print("="*72)
print("FINISHED")
print("="*72)
print()

print("Next step:")
print("Restore custom includeFile() entries")
print("Restore custom schematics.lua entries")
print("Compile")
