#!/usr/bin/env python3

from pathlib import Path
import shutil
import re
import sys

FILE = Path("src/server/zone/managers/player/PlayerManagerImplementation.cpp")

if not FILE.exists():
    print("ERROR: Cannot find", FILE)
    sys.exit(1)

backup = FILE.with_suffix(FILE.suffix + ".phase6c.bak")
if not backup.exists():
    shutil.copy2(FILE, backup)
    print("Backup:", backup)

text = FILE.read_text(encoding="utf-8", errors="ignore")
original = text

#
# Locate healEnhance()
#

m = re.search(
    r'int\s+PlayerManagerImplementation::healEnhance\s*\([^)]*\)\s*\{',
    text
)

if not m:
    print("Couldn't find healEnhance()")
    sys.exit(1)

start = m.end()

#
# Find end of function
#

depth = 1
i = start

while i < len(text) and depth > 0:
    c = text[i]

    if c == "{":
        depth += 1
    elif c == "}":
        depth -= 1

    i += 1

func = text[m.start():i]

#
# Already patched?
#

if "Returns Phase 6C" in func:
    print("Already patched.")
    sys.exit(0)

#
# Insert before addBuff
#

insert = r'''
        // =====================================================
        // Returns Phase 6C
        // =====================================================

        buff->setSkillModifier("healing_ability",15);
        buff->setSkillModifier("combat_healing_ability",15);
        buff->setSkillModifier("combat_medic_effectiveness",20);

        buff->setSkillModifier("bleeding_defense",35);
        buff->setSkillModifier("disease_defense",35);
        buff->setSkillModifier("poison_defense",35);

        buff->setSkillModifier("blind_defense",25);
        buff->setSkillModifier("dizzy_defense",25);
        buff->setSkillModifier("knockdown_defense",25);
        buff->setSkillModifier("stun_defense",25);

        buff->setSkillModifier("melee_defense",5);
        buff->setSkillModifier("ranged_defense",5);

        buff->setSkillModifier("healing_wound_treatment",25);
        buff->setSkillModifier("healing_injury_treatment",25);

        buff->setSkillModifier("healing_wound_speed",20);
        buff->setSkillModifier("healing_injury_speed",20);

        buff->setSkillModifier("healing_range",15);
        buff->setSkillModifier("healing_range_speed",15);

'''

newfunc, count = re.subn(
    r'(\s*patient->addBuff\s*\(\s*buff\s*\)\s*;)',
    insert + r'\1',
    func,
    count=1
)

if count == 0:
    print("Couldn't find patient->addBuff(buff);")
    sys.exit(1)

#
# Extend doctor buffs to 24 hours
#

newfunc = re.sub(
    r'new\s+Buff\s*\(\s*patient\s*,\s*buffcrc\s*,\s*duration\s*,\s*BuffType::MEDICAL\s*\)',
    'new Buff(patient, buffcrc, 86400, BuffType::MEDICAL)',
    newfunc
)

#
# Replace function
#

text = text[:m.start()] + newfunc + text[i:]

FILE.write_text(text, encoding="utf-8")

print()
print("==============================")
print("Phase 6C installed")
print("==============================")
print("Doctor buffs enhanced")
print("24 hour duration")
print("Backup:", backup)
