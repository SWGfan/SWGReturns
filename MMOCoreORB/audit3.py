#!/usr/bin/env python3
#
# phase826_weapon_audit.py
#
# Audits every craftable weapon for suspicious damage values.
# Does NOT modify anything.
#

import re
from pathlib import Path

ROOT = Path("/home/ubuntu/mySWG/MMOCoreORB")
WEAPON_DIR = ROOT / "bin/scripts/object/weapon"

REPORT = ROOT / "phase826_weapon_damage_report.txt"

MIN_RE = re.compile(r"\bminDamage\s*=\s*([0-9]+)")
MAX_RE = re.compile(r"\bmaxDamage\s*=\s*([0-9]+)")
EXPMIN_RE = re.compile(r"experimentalMin\s*=\s*\{([^}]*)\}", re.S)
EXPMAX_RE = re.compile(r"experimentalMax\s*=\s*\{([^}]*)\}", re.S)

results = []

for lua in WEAPON_DIR.rglob("*.lua"):

    try:
        text = lua.read_text(encoding="utf-8", errors="ignore")
    except Exception:
        continue

    min_match = MIN_RE.search(text)
    max_match = MAX_RE.search(text)

    if not min_match or not max_match:
        continue

    base_min = int(min_match.group(1))
    base_max = int(max_match.group(1))

    exp_min = None
    exp_max = None

    m = EXPMIN_RE.search(text)
    if m:
        nums = [x.strip() for x in m.group(1).split(",")]
        if len(nums) >= 4:
            try:
                exp_min = int(float(nums[2]))
                exp_max = int(float(nums[3]))
            except:
                pass

    m = EXPMAX_RE.search(text)
    if m:
        nums = [x.strip() for x in m.group(1).split(",")]
        if len(nums) >= 4:
            try:
                exp_min2 = int(float(nums[2]))
                exp_max2 = int(float(nums[3]))
            except:
                exp_min2 = exp_max2 = None
    else:
        exp_min2 = exp_max2 = None

    theoretical = exp_max2 if exp_max2 else base_max

    if theoretical >= 2500:
        status = "!!! OVERPOWERED !!!"
    elif theoretical >= 1200:
        status = "HIGH"
    elif theoretical >= 600:
        status = "ELEVATED"
    else:
        status = "OK"

    results.append((
        status,
        theoretical,
        lua.relative_to(ROOT),
        base_min,
        base_max,
        exp_min,
        exp_max,
        exp_min2,
        exp_max2
    ))

results.sort(key=lambda x: x[1], reverse=True)

with open(REPORT, "w", encoding="utf-8") as f:

    f.write("="*78 + "\n")
    f.write("Phase 8.26 Weapon Damage Audit\n")
    f.write("="*78 + "\n\n")

    for r in results:
        status, theo, path, bmin, bmax, emin, emax, emin2, emax2 = r

        f.write(f"{status}\n")
        f.write(f"{path}\n")
        f.write(f" Base Damage      : {bmin} - {bmax}\n")
        f.write(f" Experimental Min : {emin} - {emax}\n")
        f.write(f" Experimental Max : {emin2} - {emax2}\n")
        f.write(f" Estimated Max    : {theo}\n")
        f.write("\n")

print("="*78)
print("Weapon audit complete.")
print(f"Report written to:")
print(REPORT)
print("="*78)
