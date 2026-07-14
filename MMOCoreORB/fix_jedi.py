#!/usr/bin/env python3

from pathlib import Path
import shutil
import re

PATCHES = []

###########################################################
# PlayerObjectImplementation.cpp
###########################################################

PATCHES.append((
"src/server/zone/objects/player/PlayerObjectImplementation.cpp",

[
(
r"const static uint32 tick = 5;",
"""// OP Jedi Patch
uint32 forceTick = getForcePowerRegen();

if (forceTick < 12)
    forceTick = 12;
"""
),

(
r"uint32 forceTick = tick \* modifier;",
"""if (creature->isMeditating())
    forceTick *= 10;
"""
),

(
r"maxForce \+= \(forcePowerMod \+ forceControlMod\) \* 10;",
"""maxForce += (forcePowerMod + forceControlMod) * 15;"""
)
]
))

###########################################################
# JediQueueCommand.h
###########################################################

PATCHES.append((
"src/server/zone/objects/creature/commands/JediQueueCommand.h",

[
(
r"return forceCost \+ \(int\)\(\(\(manipulationMod \* frsModifier\) \+ \.5\)\);",

"""int reduction =
(int)((manipulationMod * frsModifier)+0.5f);

return std::max(1, forceCost - reduction);"""
)
]
))

###########################################################
# ForcePowersQueueCommand.h
###########################################################

PATCHES.append((
"src/server/zone/objects/creature/commands/ForcePowersQueueCommand.h",

[
(
r"return forceCost \+ \(int\)\(\(\(manipulationMod \* frsModifier\) \+ \.5\)\);",

"""int reduction =
(int)((manipulationMod * frsModifier)+0.5f);

return std::max(1, forceCost - reduction);"""
)
]
))

###########################################################
# CombatManager
###########################################################

PATCHES.append((
"src/server/zone/managers/combat/CombatManager.cpp",

[
(
r"damage = applyDamage\(",

"""if (weapon != nullptr && weapon->isJediWeapon())
    damage = (int)(damage * 1.20f);

damage = applyDamage("""
)
]
))

###########################################################

patched = 0

for filename, replacements in PATCHES:

    path = Path(filename)

    if not path.exists():
        print("Missing:", filename)
        continue

    text = path.read_text()

    backup = Path(str(path)+".opjedi.bak")

    if not backup.exists():
        shutil.copy2(path, backup)

    changed = False

    for pattern, repl in replacements:

        newtext = re.sub(pattern, repl, text)

        if newtext != text:
            text = newtext
            changed = True

    if changed:
        path.write_text(text)
        print("Patched:", filename)
        patched += 1
    else:
        print("No changes:", filename)

print()
print("Finished.")
print("Files modified:", patched)
