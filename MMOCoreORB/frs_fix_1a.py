#!/usr/bin/env python3

from pathlib import Path
import shutil
import re

cwd = Path.cwd()

if (cwd / "src").exists():
    ROOT = cwd
elif (cwd / "MMOCoreORB" / "src").exists():
    ROOT = cwd / "MMOCoreORB"
else:
    files = list(cwd.rglob("FrsManagerImplementation.cpp"))
    if not files:
        print("Couldn't locate FrsManagerImplementation.cpp")
        raise SystemExit(1)
    ROOT = files[0].parents[5]

CPP = ROOT / "src/server/zone/managers/frs/FrsManagerImplementation.cpp"

if not CPP.exists():
    print(CPP)
    raise SystemExit(1)

backup = CPP.with_suffix(".cpp.phase719_login.bak")

if not backup.exists():
    shutil.copy2(CPP, backup)

text = CPP.read_text()

if "PHASE719_LOGIN_ENROLL" in text:
    print("Already patched.")
    raise SystemExit()

m = re.search(
    r'void\s+FrsManagerImplementation::playerLoggedIn\s*\([^)]*\)\s*\{',
    text)

if not m:
    print("Couldn't find playerLoggedIn()")
    raise SystemExit()

brace = text.find("{", m.start()) + 1

insert = r'''

        // =====================================================
        // PHASE719_LOGIN_ENROLL
        // Auto-enroll Jedi Knights into FRS
        // =====================================================

        PlayerObject* ghost = player->getPlayerObject();

        if (ghost != nullptr) {

                FrsData* playerData = ghost->getFrsData();

                if (playerData != nullptr &&
                    playerData->getCouncilType() == 0 &&
                    player->hasSkill("force_title_jedi_rank_03")) {

                        if (player->getFaction() == Factions::FACTIONIMPERIAL) {

                                playerData->setCouncilType(COUNCIL_DARK);
                                setPlayerRank(player, 0);

                                info("Phase719: Auto-enrolled " +
                                        player->getFirstName() +
                                        " into Dark Council.", true);

                        } else if (player->getFaction() == Factions::FACTIONREBEL) {

                                playerData->setCouncilType(COUNCIL_LIGHT);
                                setPlayerRank(player, 0);

                                info("Phase719: Auto-enrolled " +
                                        player->getFirstName() +
                                        " into Light Council.", true);
                        }
                }
        }

        // =====================================================

'''

text = text[:brace] + insert + text[brace:]

CPP.write_text(text)

print()
print("==========================================")
print("Phase 7.19 Part 1 (Revised)")
print("==========================================")
print("Patched :", CPP)
print("Backup  :", backup)
print("Done.")
