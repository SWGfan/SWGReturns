#!/usr/bin/env python3

from pathlib import Path
import shutil
import re

patched = 0


def backup(path):
    bak = Path(str(path) + ".returns_patch4.bak")
    if not bak.exists():
        shutil.copy2(path, bak)


def patch(pathname, func):
    global patched

    p = Path(pathname)

    if not p.exists():
        print("Missing:", pathname)
        return

    txt = p.read_text()

    orig = txt

    txt = func(txt)

    if txt != orig:
        backup(p)
        p.write_text(txt)
        patched += 1
        print("Patched", pathname)
    else:
        print("Already patched:", pathname)


##########################################################
# CombatManager.cpp
##########################################################

def combat(txt):

    #
    # Better Force Armor
    #

    if "Returns Force Armor" not in txt:

        anchor = "int saberToughness = defender->getSkillMod(\"lightsaber_toughness\");"

        if anchor in txt:

            txt = txt.replace(
                anchor,
                anchor +
"""

        // Returns Force Armor
        if (forceArmor > 0 && !isWearingArmor(defender)) {

                float reduction = 0.12f + (forceArmor * 0.0035f);

                if (reduction > 0.45f)
                        reduction = 0.45f;

                damage *= (1.0f - reduction);
        }
"""
            )

    #
    # Passive saber deflection
    #

    if "Returns Passive Deflection" not in txt:

        anchor = "damage = getDefenderToughnessModifier(defender, weapon->getAttackType(), weapon->getDamageType(), damage);"

        if anchor in txt:

            txt = txt.replace(
                anchor,
                anchor +
"""

        // Returns Passive Deflection

        if (defender->getWeapon() != nullptr &&
            defender->getWeapon()->isJediWeapon() &&
            weapon->getAttackType() == SharedWeaponObjectTemplate::RANGEDATTACK) {

                damage *= 0.85f;
        }

        if (damage < 50.f)
                damage = 50.f;
"""
            )

    #
    # Jedi Defense Bonus
    #

    if "Returns Jedi Defense Bonus" not in txt:

        txt = re.sub(
            r'(targetDefense\s*\+=\s*defender->getSkillMod\("dodge_attack"\)\s*;)',
            r'''\1

        // Returns Jedi Defense Bonus

        if (defender->getPlayerObject() != nullptr &&
            defender->getPlayerObject()->isJedi())

                targetDefense += 10;
''',
            txt,
            count=1,
            flags=re.MULTILINE
        )

    return txt


##########################################################
# PlayerObjectImplementation.cpp
##########################################################

def player(txt):

    #
    # Meditation
    #

    txt = re.sub(
        r'modifier\s*=\s*10\s*;',
        'modifier = 15;',
        txt,
        count=1
    )

    #
    # 10% cheaper Force
    #

    if "Returns Force Cost" not in txt:

        txt = re.sub(

            r'playerObject->setForcePower\(\s*playerObject->getForcePower\(\)\s*-\s*force\s*\)\s*;',

            '''// Returns Force Cost

                int reducedForce = int(force * 0.90f);

                playerObject->setForcePower(
                        playerObject->getForcePower() - reducedForce);''',

            txt,

            count=1,

            flags=re.DOTALL
        )

    return txt


##########################################################

patch(
"src/server/zone/managers/combat/CombatManager.cpp",
combat
)

patch(
"src/server/zone/objects/player/PlayerObjectImplementation.cpp",
player
)

print()
print("============================")
print("Patch 4 Complete")
print("Files patched:", patched)
print("============================")
