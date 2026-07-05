#!/usr/bin/env python3
"""
migrate_flurry_missions.py
Migrates mission content from Flurry into StarDust-2.

What it migrates:
  1. Destroy mission improvements to shared planets:
       - dathomir: adds ancient rancor bull entry (level 85-90)
       - factional_imperial: adds rebel jedi knight camp (level 120-300)
       - factional_rebel: adds imperial dark jedi knight camp (level 120-300)
       - dantooine: uncomments graul mauler boss lair
  2. New planet destroy mission files from Flurry:
       - jakku   -> jakku_destroy_missions.lua  (skip if already exists)
       - taanab  -> taanab_destroy_missions.lua (skip if already exists)
       - korriban -> moraband_destroy_missions.lua (replaces sparse placeholder,
                     table/group names rewritten to moraband_*)
  3. mission_manager.lua: playerBountyKillBuffer = 0, trailing comma fix
  4. mission_level_choice screenplay + screenplays.lua include

Usage:
  python3 migrate_flurry_missions.py <flurry_root> <stardust_root> [backup_dir]

Example:
  python3 migrate_flurry_missions.py /home/ubuntu/SWGFlurry /home/ubuntu/StarDust-2/MMOCoreORB
"""

import sys, os, re, shutil, datetime

if len(sys.argv) < 3:
    print(__doc__); sys.exit(1)

flurry_root   = sys.argv[1].rstrip("/")
stardust_root = sys.argv[2].rstrip("/")
backup_base   = sys.argv[3].rstrip("/") if len(sys.argv) >= 4 else os.path.dirname(stardust_root)

flurry_scripts   = f"{flurry_root}/bin/scripts"
stardust_scripts = f"{stardust_root}/bin/scripts"

for p in (flurry_scripts, stardust_scripts):
    if not os.path.isdir(p):
        sys.exit(f"ERROR: not found: {p}")

# ── 0. Backup ──────────────────────────────────────────────────────────────────
timestamp     = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
stardust_name = os.path.basename(stardust_root)
backup_path   = f"{backup_base}/{stardust_name}_mission_backup_{timestamp}"

print("=== Step 0: Backing up StarDust-2 ===")
print(f"Backup -> {backup_path}  (this may take a moment...)", flush=True)
try:
    shutil.copytree(stardust_root, backup_path)
    print("Backup complete ✓")
except Exception as e:
    sys.exit(f"ERROR: Backup failed: {e}")
print()

# ── 1. Destroy mission improvements on shared planets ─────────────────────────
print("=== Step 1: Destroy mission improvements ===")

flurry_dm   = f"{flurry_scripts}/mobile/spawn/destroy_mission"
stardust_dm = f"{stardust_scripts}/mobile/spawn/destroy_mission"

def patch_file(path, old, new, description):
    with open(path) as f:
        content = f.read()
    if new.strip() in content:
        print(f"  SKIP ({description}): already present")
        return False
    if old not in content:
        print(f"  SKIP ({description}): anchor not found")
        return False
    with open(path, 'w') as f:
        f.write(content.replace(old, new, 1))
    print(f"  PATCHED: {description}")
    return True

# dathomir: add ancient rancor bull
dathomir = f"{stardust_dm}/dathomir_destroy_missions.lua"
if os.path.isfile(dathomir):
    patch_file(
        dathomir,
        '\t\t}\n\t}\n}\n\naddDestroyMissionGroup("dathomir_destroy_missions"',
        '\t\t},\n\t\t{\n\t\t\tlairTemplateName = "dathomir_rancor_ancient_bull_lair_neutral_large",\n\t\t\tminDifficulty = 85,\n\t\t\tmaxDifficulty = 90,\n\t\t\tsize = 35,\n\t\t},\n\t}\n}\n\naddDestroyMissionGroup("dathomir_destroy_missions"',
        "dathomir: ancient rancor bull"
    )

# factional_imperial: rebel jedi knight camp
fi = f"{stardust_dm}/factional_imperial_destroy_missions.lua"
if os.path.isfile(fi):
    patch_file(
        fi,
        '\t}\n}\n\naddDestroyMissionGroup("factional_imperial_destroy_missions"',
        '\t\t{\n\t\t\tlairTemplateName = "global_rebel_light_jedi_knight_camp_rebel_large_theater",\n\t\t\tminDifficulty = 120,\n\t\t\tmaxDifficulty = 300,\n\t\t\tsize = 25,\n\t\t},\n\t}\n}\n\naddDestroyMissionGroup("factional_imperial_destroy_missions"',
        "factional_imperial: rebel jedi camp"
    )

# factional_rebel: imperial dark jedi camp
fr = f"{stardust_dm}/factional_rebel_destroy_missions.lua"
if os.path.isfile(fr):
    patch_file(
        fr,
        '\t}\n}\n\naddDestroyMissionGroup("factional_rebel_destroy_missions"',
        '\t\t{\n\t\t\tlairTemplateName = "global_imperial_dark_jedi_knight_camp_imperial_large_theater",\n\t\t\tminDifficulty = 120,\n\t\t\tmaxDifficulty = 300,\n\t\t\tsize = 25,\n\t\t},\n\t}\n}\n\naddDestroyMissionGroup("factional_rebel_destroy_missions"',
        "factional_rebel: imperial dark jedi camp"
    )

# dantooine: uncomment graul mauler boss
dantooine = f"{stardust_dm}/dantooine_destroy_missions.lua"
if os.path.isfile(dantooine):
    with open(dantooine) as f:
        content = f.read()
    old = '\t\t-- {\n\t\t-- \tlairTemplateName = "dantooine_graul_mauler_lair_neutral_large_boss_01",\n\t\t-- \tminDifficulty = 33,\n\t\t-- \tmaxDifficulty = 37,\n\t\t-- \tsize = 35,\n\t\t-- },'
    new = '\t\t{\n\t\t\tlairTemplateName = "dantooine_graul_mauler_lair_neutral_large_boss_01",\n\t\t\tminDifficulty = 33,\n\t\t\tmaxDifficulty = 37,\n\t\t\tsize = 35,\n\t\t},'
    if old in content:
        with open(dantooine, 'w') as f:
            f.write(content.replace(old, new, 1))
        print("  PATCHED: dantooine: uncommented graul mauler boss lair")
    else:
        print("  SKIP (dantooine graul mauler): already active or not present")

print()

# ── 2. New planet files: jakku, taanab, korriban->moraband ────────────────────
print("=== Step 2: New planet destroy mission files ===")

for planet in ("jakku", "taanab"):
    src = f"{flurry_dm}/{planet}_destroy_missions.lua"
    dst = f"{stardust_dm}/{planet}_destroy_missions.lua"
    if not os.path.isfile(src):
        print(f"  SKIP {planet}: not found in Flurry")
        continue
    if os.path.isfile(dst):
        print(f"  SKIP {planet}: already exists in StarDust")
        continue
    shutil.copy2(src, dst)
    print(f"  COPIED: {planet}_destroy_missions.lua")

# korriban -> moraband
src = f"{flurry_dm}/korriban_destroy_missions.lua"
dst = f"{stardust_dm}/moraband_destroy_missions.lua"

if not os.path.isfile(src):
    print("  SKIP korriban->moraband: korriban not found in Flurry")
else:
    with open(src) as f:
        content = f.read()
    moraband_lines = 0
    if os.path.isfile(dst):
        with open(dst) as f:
            moraband_lines = sum(1 for _ in f)
    if moraband_lines > 30:
        print(f"  SKIP korriban->moraband: moraband already has {moraband_lines} lines of content")
    else:
        content = content.replace("korriban_destroy_missions", "moraband_destroy_missions")
        with open(dst, 'w') as f:
            f.write(content)
        print(f"  PORTED: korriban -> moraband_destroy_missions.lua "
              f"({moraband_lines} placeholder lines replaced with "
              f"{len(content.splitlines())} lines of Flurry content)")

print()

# ── 3. mission_manager.lua patches ───────────────────────────────────────────
print("=== Step 3: mission_manager.lua patches ===")

mm_path = f"{stardust_scripts}/managers/mission/mission_manager.lua"
if os.path.isfile(mm_path):
    with open(mm_path) as f:
        mm = f.read()
    changed = False

    if 'playerBountyKillBuffer = 30 * 60 * 1000' in mm:
        mm = mm.replace(
            'playerBountyKillBuffer = 30 * 60 * 1000 -- Buffer before player bounty can be put back on terminal after target is killed, set 0 to disable',
            'playerBountyKillBuffer = 0 -- Buffer before player bounty can be put back on terminal after target is killed, set 0 to disable'
        )
        print("  PATCHED: playerBountyKillBuffer = 0")
        changed = True
    else:
        print("  SKIP: playerBountyKillBuffer already correct")

    if '"bh_brigand_leader" --level 20' in mm and '"bh_brigand_leader", --level 20' not in mm:
        mm = mm.replace('"bh_brigand_leader" --level 20', '"bh_brigand_leader", --level 20')
        print("  PATCHED: fixed missing comma after bh_brigand_leader")
        changed = True
    else:
        print("  SKIP: bh_brigand_leader comma already correct")

    if changed:
        with open(mm_path, 'w') as f:
            f.write(mm)
else:
    print(f"  ERROR: {mm_path} not found")

print()

# ── 4. mission_level_choice screenplay ───────────────────────────────────────
print("=== Step 4: mission_level_choice screenplay ===")

src_lua = f"{flurry_scripts}/screenplays/tools/mission_level_choice.lua"
dst_dir = f"{stardust_scripts}/screenplays/tools"
dst_lua = f"{dst_dir}/mission_level_choice.lua"
sp_lua  = f"{stardust_scripts}/screenplays/screenplays.lua"

if not os.path.isfile(src_lua):
    print(f"  SKIP: mission_level_choice.lua not found in Flurry at {src_lua}")
else:
    os.makedirs(dst_dir, exist_ok=True)
    if os.path.isfile(dst_lua):
        print("  SKIP: mission_level_choice.lua already exists in StarDust")
    else:
        shutil.copy2(src_lua, dst_lua)
        print(f"  COPIED: mission_level_choice.lua -> {dst_dir}")

    if os.path.isfile(sp_lua):
        with open(sp_lua) as f:
            sp = f.read()
        include_line = 'includeFile("tools/mission_level_choice.lua")'
        if include_line in sp:
            print("  SKIP: includeFile already in screenplays.lua")
        else:
            anchors = [
                'includeFile("tools/mission_direction_choice.lua")',
                'includeFile("tools/tools.lua")',
                'includeFile("tools/shuttle_dropoff.lua")',
            ]
            inserted = False
            for anchor in anchors:
                if anchor in sp:
                    sp = sp.replace(anchor, anchor + "\n" + include_line, 1)
                    inserted = True
                    break
            if not inserted:
                sp = include_line + "\n" + sp
            with open(sp_lua, 'w') as f:
                f.write(sp)
            print(f"  PATCHED: added {include_line} to screenplays.lua")
    else:
        print(f"  WARNING: screenplays.lua not found at {sp_lua}")

print()

# ── Summary ───────────────────────────────────────────────────────────────────
print("=== Migration Summary ===")
print(f"Backup : {backup_path}")
print(f"Rollback: rm -rf {stardust_root} && cp -r {backup_path} {stardust_root}")
print("\nDone. Restart the server to apply mission changes.")
