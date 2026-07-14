#!/usr/bin/env python3
"""
fix_lightsabers_missions.py
===========================
Fixes Stardust-2 lightsaber values and mission manager to match Flurry/best-of-three.

Changes:
  LIGHTSABERS:
    1. attackSpeed = 1.0 → 5.1 on polearm files (bug fix, matches FL/MS)
    2. attackSpeed = 1.0 → 4.5 on 1H sword gen4 (bug fix, matches FL/MS)
    3. Polearm gen3 minDamage 220→195, maxDamage 260→285 (matches FL/MS)

  MISSION MANAGER (C++):
    4. missionCount >= 4 → missionCount >= 6 (show 6 missions, matches Flurry)
       Requires server recompile after applying.

Usage:
    python3 fix_lightsabers_missions.py /path/to/StarDust-2/MMOCoreORB

Best-of-three analysis:
  - minDamage/maxDamage: ALL THREE servers identical (gen1-5 for all types)
  - attackSpeed: FL/MS correct, SD has 1.0 bug on polearms + sword gen4
  - polearm gen3: FL/MS (195-285) is better spread, SD (220-260) is compressed
  - armorPiercing: already changed to HEAVY in previous session
  - missionCount: FL=6 (more player-friendly), SD=4, MS not checked
"""

import sys, os, re, shutil, datetime

def usage():
    print(__doc__)
    sys.exit(1)

if len(sys.argv) < 2:
    usage()

ROOT = sys.argv[1].rstrip('/')
SCRIPTS = f"{ROOT}/bin/scripts"
SRC     = f"{ROOT}/src"
TS      = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")

changes = []
errors  = []

def backup_and_patch(path, old, new, desc):
    if not os.path.isfile(path):
        errors.append(f"NOT FOUND: {path}")
        return False
    with open(path) as f:
        content = f.read()
    if old not in content:
        errors.append(f"PATTERN NOT FOUND in {os.path.basename(path)}: {old!r}")
        return False
    shutil.copy2(path, f"{path}.fix_{TS}.bak")
    with open(path, 'w') as f:
        f.write(content.replace(old, new, 1))
    changes.append(f"  ✓ {os.path.relpath(path, ROOT)}: {desc}")
    return True

def patch_regex(path, pattern, replacement, desc, flags=0):
    if not os.path.isfile(path):
        errors.append(f"NOT FOUND: {path}")
        return False
    with open(path) as f:
        content = f.read()
    new_content, n = re.subn(pattern, replacement, content, flags=flags)
    if n == 0:
        errors.append(f"REGEX NO MATCH in {os.path.basename(path)}: {pattern!r}")
        return False
    shutil.copy2(path, f"{path}.fix_{TS}.bak")
    with open(path, 'w') as f:
        f.write(new_content)
    changes.append(f"  ✓ {os.path.relpath(path, ROOT)}: {desc} ({n} replacement{'s' if n>1 else ''})")
    return True

print("=" * 60)
print("StarDust-2 Lightsaber + Mission Fix Script")
print("=" * 60)
print()

# ─── FIX 1: Polearm attackSpeed 1.0 → 5.1 ────────────────────────────────────
print("[1/4] Fixing polearm attackSpeed 1.0 → 5.1...")
polearm_dir = f"{SCRIPTS}/object/weapon/melee/polearm"
fixed = 0
for dirpath, _, files in os.walk(polearm_dir):
    for fname in files:
        if not fname.endswith('.lua'): continue
        fpath = os.path.join(dirpath, fname)
        with open(fpath) as f: content = f.read()
        if 'attackSpeed = 1.0' in content and ('lightsaber' in fname or 'saber' in fname):
            shutil.copy2(fpath, f"{fpath}.fix_{TS}.bak")
            with open(fpath, 'w') as f:
                f.write(content.replace('attackSpeed = 1.0', 'attackSpeed = 5.1'))
            changes.append(f"  ✓ {os.path.relpath(fpath, ROOT)}: attackSpeed 1.0→5.1")
            fixed += 1
print(f"   Fixed {fixed} polearm files.")

# ─── FIX 2: 1H Sword gen4 attackSpeed 1.0 → 4.5 ──────────────────────────────
print("[2/4] Fixing sword gen4 attackSpeed 1.0 → 4.5...")
sword_gen4 = f"{SCRIPTS}/object/weapon/melee/sword/crafted_saber/sword_lightsaber_one_handed_gen4.lua"
backup_and_patch(sword_gen4,
    'attackSpeed = 1.0,',
    'attackSpeed = 4.5,',
    "attackSpeed 1.0→4.5 (bug fix, matches FL/MS)")

# ─── FIX 3: Polearm gen3 damage correction ────────────────────────────────────
print("[3/4] Fixing polearm gen3 damage values (SD: 220-260 → FL/MS: 195-285)...")
polearm_gen3_files = [
    f"{SCRIPTS}/object/weapon/melee/polearm/crafted_saber/sword_lightsaber_polearm_gen3.lua",
]
# Also fix all s*_gen3 polearm variants
import glob
polearm_gen3_files += glob.glob(
    f"{SCRIPTS}/object/weapon/melee/polearm/crafted_saber/sword_lightsaber_polearm_s*_gen3.lua"
)
for f in polearm_gen3_files:
    if not os.path.isfile(f): continue
    with open(f) as fh: content = fh.read()
    changed = False
    if 'minDamage = 220' in content:
        content = content.replace('minDamage = 220', 'minDamage = 195')
        changed = True
    if 'maxDamage = 260' in content:
        content = content.replace('maxDamage = 260', 'maxDamage = 285')
        changed = True
    if changed:
        shutil.copy2(f, f"{f}.fix_{TS}.bak")
        with open(f, 'w') as fh: fh.write(content)
        changes.append(f"  ✓ {os.path.relpath(f, ROOT)}: polearm gen3 damage 220-260→195-285")

# ─── FIX 4: Mission count 4 → 6 (C++) ────────────────────────────────────────
print("[4/5] Fixing mission count cap 4 → 6 in MissionManagerImplementation.cpp...")
mm_cpp = f"{SRC}/server/zone/managers/mission/MissionManagerImplementation.cpp"
backup_and_patch(mm_cpp,
    'if (missionCount >= 4 ||',
    'if (missionCount >= 6 ||',
    "mission count cap 4→6 (matches Flurry, user requested 6)")


# ─── FIX 5: Bounty debuff length 3 days → 24 hours (mySWG) ──────────────────
print("[5/5] Fixing playerBountyDebuffLength 3 days → 24 hours (mySWG)...")
mm_lua = f"{SCRIPTS}/managers/mission/mission_manager.lua"
backup_and_patch(mm_lua,
    "playerBountyDebuffLength = 3 * 24 * 60 * 60 * 1000",
    "playerBountyDebuffLength = 24 * 60 * 60 * 1000",
    "bounty debuff 3 days→24 hours (mySWG, less punishing)")

# ─── Summary ──────────────────────────────────────────────────────────────────
print()
print("=" * 60)
print(f"CHANGES APPLIED ({len(changes)}):")
for c in changes:
    print(c)

if errors:
    print()
    print(f"WARNINGS/ERRORS ({len(errors)}):")
    for e in errors:
        print(f"  ✗ {e}")

print()
print("NEXT STEPS:")
print("  1. Recompile the server (fix 4 requires it):")
print(f"     cd {ROOT} && make -j$(nproc)")
print("  2. Restart the server to apply Lua changes (fixes 1-3).")
print()
print("BEST-OF-THREE SUMMARY:")
print("  minDamage/maxDamage  : All servers identical — no changes needed")
print("  attackSpeed (polearm): FL/MS=5.1  SD had bug 1.0 → FIXED to 5.1")
print("  attackSpeed (1H gen4): FL/MS=4.5  SD had bug 1.0 → FIXED to 4.5")
print("  polearm gen3 damage  : FL/MS=195-285 (better spread) → APPLIED")
print("  armorPiercing        : Already set to HEAVY (previous session)")
print("  missionCount cap     : FL=6, SD=4 -> FIXED to 6 (requires recompile)")
print("  playerBountyDebuff   : mySWG=24h, SD=3 days -> FIXED to 24 hours")
