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

backup = Path(str(FILE) + ".6d05.bak")

if not backup.exists():
    shutil.copy2(FILE, backup)

text = FILE.read_text(errors="ignore")
original = text

if "6D.05 Adventurer" in text:
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
                        // 6D.05 Adventurer Inspiration
                        // ======================================

                        {buff}->setSkillModifier("healing_ability",5);
                        {buff}->setSkillModifier("combat_healing_ability",5);

                        {buff}->setSkillModifier("healing_wound_speed",10);
                        {buff}->setSkillModifier("healing_injury_speed",10);

                        {buff}->setSkillModifier("healing_range_speed",5);

                        {buff}->setSkillModifier("camouflage",10);
                        {buff}->setSkillModifier("foraging",10);
                        {buff}->setSkillModifier("creature_harvesting",10);
                        {buff}->setSkillModifier("surveying",10);

                        {buff}->setSkillModifier("slope_move",15);
                        {buff}->setSkillModifier("group_slope_move",15);

                        {buff}->setSkillModifier("take_cover",10);
                        {buff}->setSkillModifier("cover",10);

                        {buff}->setSkillModifier("resistance_bleeding",10);
                        {buff}->setSkillModifier("resistance_disease",10);
                        {buff}->setSkillModifier("resistance_poison",10);

                        {buff}->setSkillModifier("general_assembly",5);
                        {buff}->setSkillModifier("general_experimentation",5);

                        {buff}->setSkillModifier("weapon_repair",10);
                        {buff}->setSkillModifier("armor_repair",10);
                        {buff}->setSkillModifier("clothing_repair",10);
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
print("6D.05 Adventurer Installed")
print("==============================")
print("Music:", music)
print("Dance:", dance)
print("Backup:", backup)
