#!/usr/bin/env python3

import shutil
from pathlib import Path

ROOT = Path.home() / "StarDust-2" / "MMOCoreORB"
FILE = ROOT / "bin/scripts/screenplays/jedi/components/ForceShrineMenuComponent.lua"

if not FILE.exists():
    print(f"ERROR: {FILE} not found")
    exit(1)

backup = FILE.with_suffix(".lua.phase720_knight_fix.bak")
if not backup.exists():
    shutil.copy2(FILE, backup)
    print(f"[BACKUP] {backup.name}")

text = FILE.read_text(encoding="utf-8")

# Prevent double-patching
if "PHASE720 KNIGHT FIX" in text:
    print("[INFO] Patch already applied.")
    exit(0)

old = """function ForceShrineMenuComponent:doMeditate(pObject, pPlayer)
"""

new = """function ForceShrineMenuComponent:doMeditate(pObject, pPlayer)

        -- ==========================================================
        -- PHASE720 KNIGHT FIX
        -- Never restart Knight Trials for completed Jedi Knights.
        -- ==========================================================
        if (CreatureObject(pPlayer):hasSkill("force_title_jedi_rank_03")) then
                CreatureObject(pPlayer):sendSystemMessage("@jedi_trials:force_shrine_wisdom_" .. getRandomNumber(1, 15))
                return
        end

"""

if old not in text:
    print("[ERROR] Could not locate doMeditate()")
    exit(1)

text = text.replace(old, new, 1)

FILE.write_text(text, encoding="utf-8")

print()
print("==========================================")
print(" Phase 7.20 Knight Shrine Fix Installed ")
print("==========================================")
print()
print("Modified:")
print("  ForceShrineMenuComponent.lua")
print()
print("Behavior:")
print(" • Jedi Knights (force_title_jedi_rank_03) no longer restart Knight Trials.")
print(" • Force Shrines simply give wisdom messages.")
print(" • Padawan Trials remain unchanged.")
print()
print("Restart the server after applying.")
