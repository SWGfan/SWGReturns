#!/usr/bin/env python3

from pathlib import Path
import shutil
import re

ROOT = Path.home() / "StarDust-2" / "MMOCoreORB"

CPP = ROOT / "src/server/zone/objects/player/sessions/EntertainingSessionImplementation.cpp"

BACKUP = CPP.with_suffix(".cpp.phase724.bak")

if not CPP.exists():
    print("Cannot find:")
    print(CPP)
    raise SystemExit(1)

print("Backing up...")
shutil.copy2(CPP, BACKUP)

text = CPP.read_text()

changes = 0

# ----------------------------------------------------------
# Restore buffStrength calculation
# ----------------------------------------------------------

old = r'float\s+buffStrength\s*=\s*86400\.0\s*;'

new = (
    'float buffStrength = '
    'getEntertainerBuffStrength(creature, performanceType) / 100.0f;'
)

text, n = re.subn(old, new, text)

changes += n

# ----------------------------------------------------------
# Force every PerformanceBuff duration to 10800 seconds
# ----------------------------------------------------------

text, n = re.subn(
    r'new\s+PerformanceBuff\s*\(\s*([^;]*?),\s*buffDuration\s*\*\s*60\s*,',
    r'new PerformanceBuff(\1, 10800,',
    text,
    flags=re.DOTALL
)
changes += n

text, n = re.subn(
    r'new\s+PerformanceBuff\s*\(\s*([^;]*?),\s*buffDuration\s*\*\s*105\s*,',
    r'new PerformanceBuff(\1, 10800,',
    text,
    flags=re.DOTALL
)
changes += n

text, n = re.subn(
    r'new\s+PerformanceBuff\s*\(\s*([^;]*?),\s*86400\s*,',
    r'new PerformanceBuff(\1, 10800,',
    text,
    flags=re.DOTALL
)
changes += n

CPP.write_text(text)

print()
print("=" * 60)
print("Phase 7.24 Complete")
print("=" * 60)
print("Backup :", BACKUP.name)
print("Changes:", changes)
print("=" * 60)

print()
print("Now rebuild:")
print()
print("cd ~/StarDust-2/MMOCoreORB")
print("make -j$(nproc)")
