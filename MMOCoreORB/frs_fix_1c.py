#!/usr/bin/env python3

from pathlib import Path
import shutil

# Locate MMOCoreORB
cwd = Path.cwd()

if (cwd / "src").exists():
    root = cwd
elif (cwd / "MMOCoreORB" / "src").exists():
    root = cwd / "MMOCoreORB"
else:
    found = list(cwd.rglob("PlayerManagerImplementation.cpp"))
    if not found:
        print("PlayerManagerImplementation.cpp not found.")
        raise SystemExit(1)
    root = found[0].parents[4]

cpp = root / "src/server/zone/managers/player/PlayerManagerImplementation.cpp"

if not cpp.exists():
    print(cpp)
    raise SystemExit(1)

backup = cpp.with_suffix(".cpp.phase719_jedifrsxp.bak")

if not backup.exists():
    shutil.copy2(cpp, backup)

text = cpp.read_text()

if "PHASE719_JEDI_FRS_XP" in text:
    print("Already patched.")
    raise SystemExit()

needle = "player->notifyObservers(ObserverEventType::XPAWARDED, player, xp);"

idx = text.find(needle)

if idx == -1:
    print("Couldn't locate XP hook.")
    raise SystemExit()

idx += len(needle)

insert = r'''

        // =====================================================
        // PHASE719_JEDI_FRS_XP
        // Award FRS XP from Jedi XP gains only.
        // =====================================================

        if (xp > 0 && xpType.beginsWith("jedi")) {

                FrsManager* frsManager = server->getFrsManager();

                if (frsManager != nullptr) {

                        int frsXp = xp / 100;

                        if (frsXp < 1)
                                frsXp = 1;

                        frsManager->adjustFrsExperience(player, frsXp, false);
                }
        }

        // =====================================================

'''

text = text[:idx] + insert + text[idx:]

cpp.write_text(text)

print()
print("======================================")
print("Phase 7.19 Part 3")
print("======================================")
print("Patched:", cpp)
print("Backup :", backup)
print("Done.")
