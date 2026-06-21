#!/usr/bin/env python3
"""
patch_playermanager_lua.py
Adds shipwrightExpMultiplier to scripts/managers/player_manager.lua.

Usage:
    python3 patch_playermanager_lua.py /path/to/MMOCoreORB
"""
import sys, os, shutil

if len(sys.argv) < 2:
    print(__doc__); sys.exit(1)

LUA = sys.argv[1].rstrip("/") + "/bin/scripts/managers/player_manager.lua"

if not os.path.isfile(LUA):
    sys.exit(f"ERROR: not found: {LUA}")

with open(LUA) as f:
    src = f.read()

if "shipwrightExpMultiplier" in src:
    print("Already present — nothing to do."); sys.exit(0)

shutil.copy2(LUA, LUA + ".bak")

with open(LUA, "a") as f:
    f.write("\nshipwrightExpMultiplier = 10\n")

print("Patched ✓  — shipwrightExpMultiplier = 10 appended to player_manager.lua")
