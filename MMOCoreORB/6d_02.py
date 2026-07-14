#!/usr/bin/env python3

from pathlib import Path
import shutil
import re
import sys

FILE = Path(
    "src/server/zone/managers/player/PlayerManagerImplementation.cpp"
)

if not FILE.exists():
    print("File not found:", FILE)
    sys.exit(1)

backup = Path(str(FILE) + ".6d02.bak")

if not backup.exists():
    shutil.copy2(FILE, backup)

text = FILE.read_text(errors="ignore")
original = text

#
# Don't patch twice
#

if "Doctor QoL Improvements" in text:
    print("Already patched.")
    sys.exit(0)

pattern = re.compile(
    r'patient->addBuff\s*\(\s*buff\s*\)\s*;',
    re.MULTILINE
)

replacement = r'''
        // ==========================================
        // Doctor QoL Improvements
        // ==========================================

        buff->setSkillModifier("healing_ability",10);
        buff->setSkillModifier("combat_healing_ability",10);
        buff->setSkillModifier("combat_medic_effectiveness",10);

        buff->setSkillModifier("healing_wound_treatment",15);
        buff->setSkillModifier("healing_injury_treatment",15);

        buff->setSkillModifier("healing_wound_speed",10);
        buff->setSkillModifier("healing_injury_speed",10);

        buff->setSkillModifier("healing_range",10);
        buff->setSkillModifier("healing_range_speed",10);

        buff->setSkillModifier("bleeding_defense",15);
        buff->setSkillModifier("disease_defense",15);
        buff->setSkillModifier("poison_defense",15);

        buff->setSkillModifier("melee_defense",5);
        buff->setSkillModifier("ranged_defense",5);

        patient->addBuff(buff);
'''

text, count = pattern.subn(replacement, text, count=1)

if count == 0:
    print("Couldn't locate patient->addBuff(buff);")
    sys.exit(1)

FILE.write_text(text)

print()
print("==============================")
print("6D.02 Doctor Improvements")
print("==============================")
print("Installed.")
print("Backup:", backup)
