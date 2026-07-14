#!/usr/bin/env python3

from pathlib import Path
import shutil
import re

# -------------------------------------------------------------------
# Locate MMOCoreORB
# -------------------------------------------------------------------

cwd = Path.cwd()

if (cwd / "src").exists():
    ROOT = cwd
elif (cwd / "MMOCoreORB" / "src").exists():
    ROOT = cwd / "MMOCoreORB"
else:
    found = list(cwd.rglob("SkillManager.cpp"))
    if not found:
        print("Couldn't locate SkillManager.cpp")
        raise SystemExit(1)

    FILE = found[0]
    ROOT = FILE.parents[4]

FILE = ROOT / "src/server/zone/managers/skill/SkillManager.cpp"

if not FILE.exists():
    print("Missing:")
    print(FILE)
    raise SystemExit(1)

BACKUP = FILE.with_suffix(".cpp.phase719_skill.bak")

if not BACKUP.exists():
    shutil.copy2(FILE, BACKUP)

text = FILE.read_text()

if "PHASE719_FRS_SKILL_UPDATE" in text:
    print("Already patched.")
    raise SystemExit()

# -------------------------------------------------------------------
# Find the verifySkillBoxSkillMods call inside awardSkill()
# -------------------------------------------------------------------

needle = "SkillModManager::instance()->verifySkillBoxSkillMods(creature);"

idx = text.find(needle)

if idx == -1:
    print("Couldn't locate SkillModManager verification.")
    raise SystemExit(1)

insert = r'''

        // =====================================================
        // PHASE719_FRS_SKILL_UPDATE
        // Synchronize FRS whenever a skill is awarded.
        // =====================================================

        {
                ManagedReference<FrsManager*> frsManager =
                        creature->getZoneServer()->getFrsManager();

                if (frsManager != nullptr)
                        frsManager->updatePlayerSkills(creature);
        }

        // =====================================================

'''

text = text[:idx] + insert + text[idx:]

FILE.write_text(text)

print()
print("==========================================")
print("Phase 7.19 Part 2")
print("==========================================")
print("Patched:")
print(FILE)
print()
print("Backup:")
print(BACKUP)
print()
print("Done.")
