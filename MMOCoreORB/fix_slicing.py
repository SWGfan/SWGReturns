#!/usr/bin/env python3
"""
fix_slicing.py
Fixes two slicing bugs in SlicingSessionImplementation.cpp:

  1. Weapon slice: min == max at every skill level, so System::random(max - min)
     is always 0, giving a fixed percentage with zero variation.
     Fix: set case 3/2 to min += 15, max += 25 (matching Flurry).
     Results after fix:
       Skill 2/3 (slicing_01/02): 15–25%
       Skill 4   (slicing_03):    20–30%
       Skill 5   (master):        25–35%

  2. Armor slice: result is multiplied by 0.25, reducing it to ~3–9%.
     Fix: remove * 0.25 and correct min/max values to give 25–45% at master.

Usage:
  python3 fix_slicing.py <stardust_root>

Example:
  python3 fix_slicing.py /home/ubuntu/StarDust-2/MMOCoreORB
"""

import sys, os, shutil, datetime

if len(sys.argv) < 2:
    print(__doc__); sys.exit(1)

stardust_root = sys.argv[1].rstrip("/")
CPP = (f"{stardust_root}/src/server/zone/objects/player/sessions/"
       f"SlicingSessionImplementation.cpp")

if not os.path.isfile(CPP):
    sys.exit(f"ERROR: not found: {CPP}")

ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
bak = f"{CPP}.slicefix_{ts}.bak"
shutil.copy2(CPP, bak)
print(f"Backup -> {bak}")
print()

with open(CPP) as f:
    src = f.read()

changes = 0

# ── Fix 1: Weapon slice min/max ───────────────────────────────────────────────
# case 3/2 currently adds +5 to both min and max, making random(0) = 0.
# Change to +15 min, +25 max so there is a 10-point spread at each tier.

old1 = (
    '\tcase 3:\n'
    '\tcase 2:\n'
    '\t\tmin += 5;\n'
    '\t\tmax += 5;\n'
    '\t\tbreak;\n'
    '\tdefault:\n'
    '\t\treturn;\n'
    '\n'
    '\t}\n'
    '\n'
    '\tuint8 percentage = System::random(max - min) + min;\n'
    '\n'
    '\tswitch(System::random(1)) {\n'
    '\tcase 0:\n'
    '\t\thandleSliceDamage(percentage);\n'
    '\t\tbreak;\n'
    '\tcase 1:\n'
    '\t\thandleSliceDamage(percentage);\n'
    '\t\tbreak;\n'
    '\t}'
)

new1 = (
    '\tcase 3:\n'
    '\tcase 2:\n'
    '\t\tmin += 15;\n'
    '\t\tmax += 25;\n'
    '\t\tbreak;\n'
    '\tdefault:\n'
    '\t\treturn;\n'
    '\n'
    '\t}\n'
    '\n'
    '\tuint8 percentage = System::random(max - min) + min;\n'
    '\n'
    '\tswitch(System::random(1)) {\n'
    '\tcase 0:\n'
    '\t\thandleSliceDamage(percentage);\n'
    '\t\tbreak;\n'
    '\tcase 1:\n'
    '\t\thandleSliceDamage(percentage);\n'
    '\t\tbreak;\n'
    '\t}'
)

if old1 in src:
    src = src.replace(old1, new1, 1)
    print("Fix 1 applied: weapon slice case 3/2 min=5,max=5 -> min=15,max=25")
    print("  Skill 2/3: 15-25%  |  Skill 4: 20-30%  |  Master: 25-35% ✓")
    changes += 1
else:
    print("WARN Fix 1: weapon slice anchor not found — check whitespace in the switch block")

# ── Fix 2: Armor slice * 0.25 multiplier and min/max ─────────────────────────
# The armor slice multiplies percent by 0.25 reducing it to a tiny fraction.
# Also fix min/max so they differ (same root cause as weapon slice).

old2 = (
    '\tuint8 percent = (System::random(max - min) + min) * 0.25;\n'
    '\n'
    '\tswitch (sliceType) {\n'
    '\tcase 0:\n'
    '\t\thandleSliceEffectiveness(percent);\n'
    '\t\tbreak;\n'
    '\tcase 1:\n'
    '\t\thandleSliceEffectiveness(percent);\n'
    '\t\tbreak;\n'
    '\t}'
)

new2 = (
    '\tuint8 percent = System::random(max - min) + min;\n'
    '\n'
    '\tswitch (sliceType) {\n'
    '\tcase 0:\n'
    '\t\thandleSliceEffectiveness(percent);\n'
    '\t\tbreak;\n'
    '\tcase 1:\n'
    '\t\thandleSliceEncumbrance(percent);\n'
    '\t\tbreak;\n'
    '\t}'
)

if old2 in src:
    src = src.replace(old2, new2, 1)
    print("\nFix 2 applied: removed * 0.25 from armor slice and fixed case 1 to call")
    print("  handleSliceEncumbrance (it was calling handleSliceEffectiveness twice) ✓")
    changes += 1
else:
    print("\nWARN Fix 2: armor slice * 0.25 anchor not found")

# ── Fix 2b: Armor slice min/max (give it proper spread) ──────────────────────
old3 = (
    '\tcase 3:\n'
    '\tcase 2:\n'
    '\t\tmin += 5;\n'
    '\t\tmax += (sliceType == 0) ? 20 : 30;\n'
    '\t\tbreak;\n'
)

new3 = (
    '\tcase 3:\n'
    '\tcase 2:\n'
    '\t\tmin += 10;\n'
    '\t\tmax += (sliceType == 0) ? 25 : 30;\n'
    '\t\tbreak;\n'
)

if old3 in src:
    src = src.replace(old3, new3, 1)
    print("Fix 2b applied: armor slice case 3/2 min 5->10, effectiveness max 20->25 ✓")
    changes += 1
else:
    print("WARN Fix 2b: armor slice case 3/2 anchor not found")

# ── Write ──────────────────────────────────────────────────────────────────────
with open(CPP, 'w') as f:
    f.write(src)

print(f"\n{changes}/3 fixes applied.")
if changes > 0:
    print("Recompile to activate.")
print(f"Rollback: cp {bak} {CPP}")
