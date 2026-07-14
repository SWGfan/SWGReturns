#!/usr/bin/env python3

from pathlib import Path
import shutil
import re
import sys

CPP = Path("src/server/zone/objects/player/sessions/EntertainingSessionImplementation.cpp")

if not CPP.exists():
    print("ERROR:")
    print(CPP)
    sys.exit(1)

BACKUP = CPP.with_suffix(".cpp.phase6d06d_fix.bak")

if not BACKUP.exists():
    shutil.copy2(CPP, BACKUP)
    print("Backup created:", BACKUP)

text = CPP.read_text(encoding="utf-8")

#
# Repair broken previous patch:
# getHealShockWound( * 2.5f )
#

text = re.sub(
    r'performance->getHealShockWound\s*\(\s*\*\s*2\.5f\s*\)',
    'performance->getHealShockWound() * 2.5f',
    text
)

#
# Patch the original line if it hasn't already been patched
#

pattern = re.compile(
    r'performance->getHealShockWound\(\)(?!\s*\*\s*2\.5f)'
)

text, count = pattern.subn(
    'performance->getHealShockWound() * 2.5f',
    text
)

CPP.write_text(text, encoding="utf-8")

print()
print("=" * 72)
print("Phase 6D.06d Repair")
print("=" * 72)
print("Repaired malformed calls and patched:", count)
print()
print("Now rebuild:")
print("cd build")
print("make -j$(nproc)")
