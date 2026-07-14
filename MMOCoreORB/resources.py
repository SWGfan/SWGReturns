#!/usr/bin/env python3

from pathlib import Path
import shutil
import re
import sys

FILE = Path(
    "src/server/zone/managers/resource/resourcespawner/ResourceSpawner.cpp"
)

if not FILE.exists():
    print("ERROR: Couldn't locate:")
    print(FILE)
    sys.exit(1)

backup = FILE.with_suffix(FILE.suffix + ".phase7_resources.bak")

if not backup.exists():
    shutil.copy2(FILE, backup)
    print("Created backup:", backup.name)

text = FILE.read_text(encoding="utf-8", errors="ignore")

if "Phase 7 Resource Overhaul" in text:
    print("Already patched.")
    sys.exit(0)

###########################################################################
# Patch randomizeValue()
###########################################################################

pattern = re.compile(
    r'int\s+ResourceSpawner::randomizeValue\s*\(\s*int\s+min\s*,\s*int\s+max\s*\)\s*\{.*?\n\}',
    re.DOTALL
)

replacement = r'''
// =====================================================
// Phase 7 Resource Overhaul
// Every generated resource attribute = 1000
// =====================================================
int ResourceSpawner::randomizeValue(int min, int max) {
        return 1000;
}
'''

text, count = pattern.subn(replacement, text, count=1)

if count == 0:
    print("Couldn't locate randomizeValue()")
    sys.exit(1)

###########################################################################
# Patch expiration
###########################################################################

expire_pattern = re.compile(
    r'long\s+ResourceSpawner::getRandomExpirationTime\s*\([^)]*\)\s*\{.*?\n\}',
    re.DOTALL
)

expire_replacement = r'''
long ResourceSpawner::getRandomExpirationTime(const ResourceTreeEntry* resourceEntry) {

        // Longer resource lifetimes

        if (resourceEntry->isOrganic())
                return getRandomUnixTimestamp(14, 30);

        return getRandomUnixTimestamp(21, 45);
}
'''

text, expire = expire_pattern.subn(expire_replacement, text, count=1)

if expire == 0:
    print("WARNING: Couldn't patch expiration timer.")

###########################################################################

FILE.write_text(text, encoding="utf-8")

print()
print("==========================================")
print(" Phase 7 Resource Overhaul Installed")
print("==========================================")
print("✔ Resource quality = 1000")
print("✔ Organic lifetime = 14-30 days")
print("✔ Inorganic lifetime = 21-45 days")
print("✔ Backup:", backup.name)
