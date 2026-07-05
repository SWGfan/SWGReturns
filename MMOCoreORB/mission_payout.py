#!/usr/bin/env python3
"""
increase_mission_payout.py
Increases all mission type payouts by 5x in StarDust-2.

Mission types patched:
  destroy   -> mission_manager.lua (Lua config, no recompile needed)
  bounty    -> MissionManagerImplementation.cpp
  deliver   -> MissionManagerImplementation.cpp
  survey    -> MissionManagerImplementation.cpp
  hunting   -> MissionManagerImplementation.cpp
  recon     -> MissionManagerImplementation.cpp
  entertainer -> MissionManagerImplementation.cpp

Usage:
  python3 increase_mission_payout.py <stardust_root> [backup_dir]

Example:
  python3 increase_mission_payout.py /home/ubuntu/StarDust-2/MMOCoreORB
"""

import sys, os, shutil, datetime

if len(sys.argv) < 2:
    print(__doc__); sys.exit(1)

stardust_root = sys.argv[1].rstrip("/")
backup_base   = sys.argv[2].rstrip("/") if len(sys.argv) >= 3 else stardust_root

CPP = (f"{stardust_root}/src/server/zone/managers/mission/"
       f"MissionManagerImplementation.cpp")
LUA = (f"{stardust_root}/bin/scripts/managers/mission/mission_manager.lua")

for p in (CPP, LUA):
    if not os.path.isfile(p):
        sys.exit(f"ERROR: not found: {p}")

# ── Backup ─────────────────────────────────────────────────────────────────────
ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
cpp_bak = f"{CPP}.payout5x_{ts}.bak"
lua_bak = f"{LUA}.payout5x_{ts}.bak"
shutil.copy2(CPP, cpp_bak)
shutil.copy2(LUA, lua_bak)
print(f"Backed up:")
print(f"  {cpp_bak}")
print(f"  {lua_bak}")
print()

with open(CPP) as f:
    cpp = f.read()
with open(LUA) as f:
    lua = f.read()

cpp_changes = 0
lua_changes = 0

def apply(src, old, new, label):
    if old not in src:
        print(f"  WARN ({label}): anchor not found — skipping")
        return src, 0
    result = src.replace(old, new, 1)
    print(f"  PATCHED: {label}")
    return result, 1

# ── 1. Destroy missions (Lua config) ──────────────────────────────────────────
print("=== Destroy missions (Lua) ===")

lua, n = apply(lua,
    'destroyMissionDifficultyRewardFactor = 375',
    'destroyMissionDifficultyRewardFactor = 1875',
    "destroyMissionDifficultyRewardFactor 375 -> 1875"
)
lua_changes += n

lua, n = apply(lua,
    'destroyMissionDifficultyRandomReward = 15',
    'destroyMissionDifficultyRandomReward = 75',
    "destroyMissionDifficultyRandomReward 15 -> 75"
)
lua_changes += n

# ── 2. Bounty missions ────────────────────────────────────────────────────────
print()
print("=== Bounty missions (C++) ===")

cpp, n = apply(cpp,
    '\t\tif (level == 1) {\n'
    '\t\t\treward = creoLevel * (200 + System::random(200));\n'
    '\t\t} else if (level == 2) {\n'
    '\t\t\treward = creoLevel * (250 + System::random(250));\n'
    '\t\t} else if (level == 3) {\n'
    '\t\t\treward = creoLevel * (300 + System::random(300));\n'
    '\t\t}',
    '\t\tif (level == 1) {\n'
    '\t\t\treward = creoLevel * (1000 + System::random(1000));\n'
    '\t\t} else if (level == 2) {\n'
    '\t\t\treward = creoLevel * (1250 + System::random(1250));\n'
    '\t\t} else if (level == 3) {\n'
    '\t\t\treward = creoLevel * (1500 + System::random(1500));\n'
    '\t\t}',
    "bounty rewards 200/250/300 -> 1000/1250/1500 per level"
)
cpp_changes += n

# ── 3. Deliver missions ───────────────────────────────────────────────────────
print()
print("=== Deliver missions (C++) ===")

cpp, n = apply(cpp,
    '\tint baseCredits = 40;\n'
    '\tint deliverDistanceCredits = (playerPosition.distanceTo(*(startNpc->getPosition())) + startNpc->getPosition()->distanceTo(*(endNpc->getPosition()))) / 10;',
    '\tint baseCredits = 200;\n'
    '\tint deliverDistanceCredits = (playerPosition.distanceTo(*(startNpc->getPosition())) + startNpc->getPosition()->distanceTo(*(endNpc->getPosition()))) / 2;',
    "deliver base 40->200, distance /10 -> /2"
)
cpp_changes += n

# ── 4. Survey missions ────────────────────────────────────────────────────────
print()
print("=== Survey missions (C++) ===")

cpp, n = apply(cpp,
    '\tmission->setRewardCredits(400 + (randLevel - minLevel) * 20 + System::random(100));',
    '\tmission->setRewardCredits(2000 + (randLevel - minLevel) * 100 + System::random(500));',
    "survey 400+level*20+rand(100) -> 2000+level*100+rand(500)"
)
cpp_changes += n

# ── 5. Entertainer missions ───────────────────────────────────────────────────
print()
print("=== Entertainer missions (C++) ===")

cpp, n = apply(cpp,
    '\tint distanceReward = player->getWorldPosition().distanceTo(target->getPosition()) / 10;\n'
    '\n'
    '\tmission->setRewardCredits(100 + distanceReward + System::random(100));',
    '\tint distanceReward = player->getWorldPosition().distanceTo(target->getPosition()) / 2;\n'
    '\n'
    '\tmission->setRewardCredits(500 + distanceReward + System::random(500));',
    "entertainer 100+dist/10+rand(100) -> 500+dist/2+rand(500)"
)
cpp_changes += n

# ── 6. Hunting missions ───────────────────────────────────────────────────────
print()
print("=== Hunting missions (C++) ===")

cpp, n = apply(cpp,
    '\tint baseReward = 500 + (difficulty * 100 * randomLairSpawn->getMinDifficulty());\n'
    '\tmission->setRewardCredits(baseReward + System::random(100));',
    '\tint baseReward = 2500 + (difficulty * 500 * randomLairSpawn->getMinDifficulty());\n'
    '\tmission->setRewardCredits(baseReward + System::random(500));',
    "hunting 500+diff*100*min -> 2500+diff*500*min"
)
cpp_changes += n

# ── 7. Recon missions ─────────────────────────────────────────────────────────
print()
print("=== Recon missions (C++) ===")

cpp, n = apply(cpp,
    '\tint reward = position.distanceTo(player->getWorldPosition()) / 5;\n'
    '\n'
    '\tmission->setRewardCredits(50 + reward);',
    '\tint reward = position.distanceTo(player->getWorldPosition());\n'
    '\n'
    '\tmission->setRewardCredits(250 + reward);',
    "recon 50+dist/5 -> 250+dist"
)
cpp_changes += n

# ── Write ──────────────────────────────────────────────────────────────────────
with open(CPP, 'w') as f:
    f.write(cpp)
with open(LUA, 'w') as f:
    f.write(lua)

print()
print("=== Summary ===")
print(f"C++ changes: {cpp_changes}/6")
print(f"Lua changes: {lua_changes}/2")
print()
if cpp_changes > 0:
    print("Recompile required for C++ changes to take effect.")
if lua_changes > 0:
    print("Destroy mission Lua changes take effect on next server restart (no recompile).")
print(f"\nRollback C++: cp {cpp_bak} {CPP}")
print(f"Rollback Lua: cp {lua_bak} {LUA}")
