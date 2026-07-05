#!/usr/bin/env python3
"""
migrate_flurry_missionmanager.py
Patches MissionManagerImplementation.cpp in StarDust-2 to integrate
Flurry's mission_level_choice screenplay into the destroy mission
difficulty and lair-selection logic.

Two targeted patches:
  1. randomizeDestroyMission(): reads levelChoice from screenplay data
     and uses it as the difficulty offset (falls back to group/player
     level if no choice is set).
  2. getLairSpawn(): reads levelChoice and uses it as the player level
     for lair pool selection (same fallback).

MissionObjectImplementation.cpp is NOT touched — StarDust is already
ahead of Flurry there (it has getTypeAsString() that Flurry lacks).

Usage:
  python3 migrate_flurry_missionmanager.py <stardust_root> [backup_dir]

Example:
  python3 migrate_flurry_missionmanager.py /home/ubuntu/StarDust-2/MMOCoreORB
"""

import sys, os, shutil, datetime

if len(sys.argv) < 2:
    print(__doc__); sys.exit(1)

stardust_root = sys.argv[1].rstrip("/")
backup_base   = sys.argv[2].rstrip("/") if len(sys.argv) >= 3 else os.path.dirname(stardust_root)

CPP = (f"{stardust_root}/src/server/zone/managers/mission/"
       f"MissionManagerImplementation.cpp")

if not os.path.isfile(CPP):
    sys.exit(f"ERROR: not found: {CPP}")

# ── 0. Backup ──────────────────────────────────────────────────────────────────
timestamp   = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
backup_path = f"{backup_base}/MissionManagerImplementation.cpp.{timestamp}.bak"
shutil.copy2(CPP, backup_path)
print(f"Backup -> {backup_path}")
print()

with open(CPP) as f:
    src = f.read()

if 'mission_level_choice' in src:
    print("Already patched — mission_level_choice already present in this file.")
    print("Nothing to do.")
    sys.exit(0)

changes = 0

# ── Patch 1: randomizeDestroyMission difficulty display calc ──────────────────
# Stardust currently:
#   int diffDisplay = difficultyLevel < 5 ? 4 : difficultyLevel;
#   PlayerObject* targetGhost = player->getPlayerObject();
#
#   if (player->isGrouped()) {
#       bool includeFactionPets = ...;
#       Reference<GroupObject*> group = player->getGroup();
#       if (group != nullptr) {
#           Locker locker(group);
#           diffDisplay += group->getGroupLevel(includeFactionPets);
#       }
#   } else {
#       diffDisplay += playerLevel;
#   }
#
# After patch: check mission_level_choice first, fall back to group/player.

OLD1 = (
    "\tint diffDisplay = difficultyLevel < 5 ? 4 : difficultyLevel;\n"
    "\tPlayerObject* targetGhost = player->getPlayerObject();\n"
    "\n"
    "\tif (player->isGrouped()) {\n"
    "\t\tbool includeFactionPets = faction != Factions::FACTIONNEUTRAL || ConfigManager::instance()->includeFactionPetsForMissionDifficulty();\n"
    "\t\tReference<GroupObject*> group = player->getGroup();\n"
    "\n"
    "\t\tif (group != nullptr) {\n"
    "\t\t\tLocker locker(group);\n"
    "\t\t\tdiffDisplay += group->getGroupLevel(includeFactionPets);\n"
    "\t\t}\n"
    "\t} else {\n"
    "\t\tdiffDisplay += playerLevel;\n"
    "\t}\n"
)

NEW1 = (
    "\tint diffDisplay = difficultyLevel < 5 ? 4 : difficultyLevel;\n"
    "\tPlayerObject* targetGhost = player->getPlayerObject();\n"
    "\n"
    "\t// Check if the player has chosen a mission difficulty level via the\n"
    "\t// mission_level_choice screenplay; if so, use that instead of combat level.\n"
    "\tString missionLevelStr = targetGhost->getScreenPlayData(\"mission_level_choice\", \"levelChoice\");\n"
    "\tint levelChoice = Integer::valueOf(missionLevelStr);\n"
    "\n"
    "\tif (levelChoice > 0) {\n"
    "\t\tdiffDisplay += levelChoice;\n"
    "\t} else if (player->isGrouped()) {\n"
    "\t\tbool includeFactionPets = faction != Factions::FACTIONNEUTRAL || ConfigManager::instance()->includeFactionPetsForMissionDifficulty();\n"
    "\t\tReference<GroupObject*> group = player->getGroup();\n"
    "\n"
    "\t\tif (group != nullptr) {\n"
    "\t\t\tLocker locker(group);\n"
    "\t\t\tdiffDisplay += group->getGroupLevel(includeFactionPets);\n"
    "\t\t}\n"
    "\t} else {\n"
    "\t\tdiffDisplay += playerLevel;\n"
    "\t}\n"
)

if OLD1 in src:
    src = src.replace(OLD1, NEW1, 1)
    print("Patch 1 applied: mission_level_choice in randomizeDestroyMission() ✓")
    changes += 1
else:
    print("WARN: Patch 1 anchor not found — randomizeDestroyMission() block may differ.")
    print("      Check whitespace around the diffDisplay/isGrouped block manually.")

# ── Patch 2: getLairSpawn player-level selection ──────────────────────────────
# Stardust currently uses group/player level to choose the lair pool.
# We add mission_level_choice priority before falling back to group level.

OLD2 = (
    "\tint playerLevel = server->getPlayerManager()->calculatePlayerLevel(player);\n"
    "\n"
    "\tif (player->isGrouped()) {\n"
    "\t\tbool includeFactionPets = faction != Factions::FACTIONNEUTRAL || ConfigManager::instance()->includeFactionPetsForMissionDifficulty();\n"
    "\t\tReference<GroupObject*> group = player->getGroup();\n"
    "\n"
    "\t\tif (group != nullptr) {\n"
    "\t\t\tLocker locker(group);\n"
    "\t\t\tplayerLevel = group->getGroupLevel(includeFactionPets);\n"
    "\t\t}\n"
    "\t}\n"
    "\n"
    "\tLairSpawn* lairSpawn = nullptr;\n"
)

NEW2 = (
    "\tint playerLevel = server->getPlayerManager()->calculatePlayerLevel(player);\n"
    "\tPlayerObject* missionGhost = player->getPlayerObject();\n"
    "\n"
    "\t// Check mission_level_choice screenplay data first so players can\n"
    "\t// manually choose their desired difficulty bracket.\n"
    "\tString missionLevelStr2 = missionGhost->getScreenPlayData(\"mission_level_choice\", \"levelChoice\");\n"
    "\tint levelChoice2 = Integer::valueOf(missionLevelStr2);\n"
    "\n"
    "\tif (levelChoice2 > 0) {\n"
    "\t\tplayerLevel = levelChoice2;\n"
    "\t} else if (player->isGrouped()) {\n"
    "\t\tbool includeFactionPets = faction != Factions::FACTIONNEUTRAL || ConfigManager::instance()->includeFactionPetsForMissionDifficulty();\n"
    "\t\tReference<GroupObject*> group = player->getGroup();\n"
    "\n"
    "\t\tif (group != nullptr) {\n"
    "\t\t\tLocker locker(group);\n"
    "\t\t\tplayerLevel = group->getGroupLevel(includeFactionPets);\n"
    "\t\t}\n"
    "\t}\n"
    "\n"
    "\tLairSpawn* lairSpawn = nullptr;\n"
)

if OLD2 in src:
    src = src.replace(OLD2, NEW2, 1)
    print("Patch 2 applied: mission_level_choice in getLairSpawn() ✓")
    changes += 1
else:
    print("WARN: Patch 2 anchor not found — getLairSpawn() block may differ.")
    print("      Check whitespace around the playerLevel/isGrouped block manually.")

# ── Write ──────────────────────────────────────────────────────────────────────
if changes > 0:
    with open(CPP, 'w') as f:
        f.write(src)
    print(f"\n{changes}/2 patches applied to {CPP}")
    print("Recompile to activate.")
else:
    print("\nNo changes written — check warnings above.")

print(f"\nTo roll back: cp {backup_path} {CPP}")
