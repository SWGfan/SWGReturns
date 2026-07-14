#!/usr/bin/env python3

from pathlib import Path
import shutil
import re
import sys

FILE = Path(
    "src/server/zone/objects/player/sessions/EntertainingSessionImplementation.cpp"
)

if not FILE.exists():
    print("Couldn't locate", FILE)
    sys.exit(1)

backup = Path(str(FILE) + ".6d04.bak")

if not backup.exists():
    shutil.copy2(FILE, backup)

text = FILE.read_text(errors="ignore")
original = text

if "6D.04 Ranger Utility" in text:
    print("Already patched.")
    sys.exit(0)


def patch(buff):

    global text

    pattern = re.compile(
        rf'Locker\s+locker\(\s*{buff}\s*\)\s*;',
        re.MULTILINE
    )

    replacement = rf'''
                        Locker locker({buff});

                        // ======================================
                        // 6D.04 Ranger Utility
                        // ======================================

                        {buff}->setSkillModifier("camouflage",10);
                        {buff}->setSkillModifier("foraging",10);
                        {buff}->setSkillModifier("creature_harvesting",10);

                        {buff}->setSkillModifier("surveying",5);

                        {buff}->setSkillModifier("camp",10);
                        {buff}->setSkillModifier("rescue",5);

                        {buff}->setSkillModifier("slope_move",10);
                        {buff}->setSkillModifier("group_slope_move",10);

                        {buff}->setSkillModifier("take_cover",10);
'''

    text, count = pattern.subn(replacement, text, count=1)

    return count


music = patch("focusBuff")
dance = patch("mindBuff")

if music == 0:
    print("Music buff not patched.")

if dance == 0:
    print("Dance buff not patched.")

if text == original:
    print("Nothing changed.")
    sys.exit(1)

FILE.write_text(text)

print()
print("==============================")
print("6D.04 Ranger Utility Installed")
print("==============================")
print("Music:", music)
print("Dance:", dance)
print("Backup:", backup)
