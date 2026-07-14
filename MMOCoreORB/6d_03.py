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

backup = Path(str(FILE) + ".6d03.bak")

if not backup.exists():
    shutil.copy2(FILE, backup)

text = FILE.read_text(errors="ignore")
original = text

if "Entertainer QoL Improvements" in text:
    print("Already patched.")
    sys.exit(0)


def patch(buffname):

    global text

    pattern = re.compile(
        rf'Locker\s+locker\(\s*{buffname}\s*\)\s*;',
        re.MULTILINE
    )

    replacement = rf'''
                        Locker locker({buffname});

                        // ======================================
                        // Entertainer QoL Improvements
                        // ======================================

                        {buffname}->setSkillModifier("luck",10);
                        {buffname}->setSkillModifier("combat_haste",5);

                        {buffname}->setSkillModifier("melee_defense",5);
                        {buffname}->setSkillModifier("ranged_defense",5);

                        {buffname}->setSkillModifier("healing_wound_speed",10);
                        {buffname}->setSkillModifier("healing_injury_speed",10);
                        {buffname}->setSkillModifier("healing_range_speed",10);

                        {buffname}->setSkillModifier("general_experimentation",5);
                        {buffname}->setSkillModifier("weapon_experimentation",5);
                        {buffname}->setSkillModifier("armor_experimentation",5);
                        {buffname}->setSkillModifier("clothing_experimentation",5);

                        {buffname}->setSkillModifier("droid_experimentation",5);
                        {buffname}->setSkillModifier("food_experimentation",5);
                        {buffname}->setSkillModifier("medicine_experimentation",5);
'''

    text, count = pattern.subn(replacement, text, count=1)

    return count


music = patch("focusBuff")
dance = patch("mindBuff")

if music == 0:
    print("Couldn't patch music buff.")

if dance == 0:
    print("Couldn't patch dance buff.")

if text == original:
    print("Nothing changed.")
    sys.exit(1)

FILE.write_text(text)

print()
print("==============================")
print("6D.03 Installed")
print("==============================")
print("Music Buff :", music)
print("Dance Buff :", dance)
print("Backup:", backup)
