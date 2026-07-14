#!/usr/bin/env python3

from pathlib import Path
import shutil
import re
import sys

CPP = Path("src/server/zone/managers/loot/LootManagerImplementation.cpp")

if not CPP.exists():
    print("Couldn't locate LootManagerImplementation.cpp")
    sys.exit(1)

backup = CPP.with_suffix(".cpp.phase703_fix.bak")

if not backup.exists():
    shutil.copy2(CPP, backup)

text = CPP.read_text(errors="ignore")

#
# Remove the old Phase 7.03 block if present
#

start = text.find("// Phase 7.03 Boss Reward Engine")

if start != -1:
    end = text.find("int creatureLevel = Math::min(300, creature->getLevel());")

    if end != -1:
        text = text[:start] + text[end:]

#
# Find the ORIGINAL creatureLevel declaration
#

pattern = re.compile(
    r'int\s+creatureLevel\s*=\s*Math::min\s*\(\s*300\s*,\s*creature->getLevel\(\)\s*\)\s*;'
)

match = pattern.search(text)

if not match:
    print("Couldn't locate original creatureLevel declaration.")
    sys.exit(1)

insert = r'''

        // ==========================================================
        // Phase 7.03 Boss Reward Engine
        // ==========================================================

        if (creatureLevel >= 250) {

                createLoot(trx, container, "resource_crate", creatureLevel);

                if (System::random(99) < 35)
                        createLoot(trx, container, "lootcollectiontierthree", creatureLevel);

                if (System::random(99) < 15)
                        createLoot(trx, container, "lootcollectiontierdiamonds", creatureLevel);

                if (System::random(99) < 20)
                        createLoot(trx, container, "rarelootsystem", creatureLevel);

                if (System::random(99) < 15)
                        createLoot(trx, container, "legendary_comp_group", creatureLevel);

        } else if (creatureLevel >= 150) {

                if (System::random(99) < 35)
                        createLoot(trx, container, "resource_crate", creatureLevel);

                if (System::random(99) < 20)
                        createLoot(trx, container, "lootcollectiontierthree", creatureLevel);

                if (System::random(99) < 5)
                        createLoot(trx, container, "lootcollectiontierdiamonds", creatureLevel);

                if (System::random(99) < 10)
                        createLoot(trx, container, "rarelootsystem", creatureLevel);

        } else if (creatureLevel >= 80) {

                if (System::random(99) < 20)
                        createLoot(trx, container, "resource_crate", creatureLevel);

                if (System::random(99) < 10)
                        createLoot(trx, container, "lootcollectiontiertwo", creatureLevel);

                if (System::random(99) < 5)
                        createLoot(trx, container, "rarelootsystem", creatureLevel);

        } else {

                if (System::random(99) < 10)
                        createLoot(trx, container, "resource_crate", creatureLevel);

                if (System::random(99) < 5)
                        createLoot(trx, container, "lootcollectiontierone", creatureLevel);

        }

'''

text = (
    text[:match.end()]
    + insert
    + text[match.end():]
)

CPP.write_text(text)

print()
print("==========================================")
print("Phase 7.03 Fixed")
print("==========================================")
print("✓ Uses Stardust's creatureLevel variable")
print("✓ Removes duplicate declaration")
print("✓ Backup:", backup)
