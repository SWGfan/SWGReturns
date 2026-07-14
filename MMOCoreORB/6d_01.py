#!/usr/bin/env python3

from pathlib import Path
import shutil
import re
import sys

FILES = [
    Path("src/server/zone/managers/player/PlayerManagerImplementation.cpp"),
    Path("src/server/zone/objects/player/sessions/EntertainingSessionImplementation.cpp"),
]

patched = 0

def backup(f):
    bak = Path(str(f) + ".6d01.bak")
    if not bak.exists():
        shutil.copy2(f, bak)

for file in FILES:

    if not file.exists():
        print("Missing:", file)
        continue

    backup(file)

    text = file.read_text(errors="ignore")
    original = text

    #
    # ----------------------------------------
    # Doctor healEnhance()
    # ----------------------------------------
    #

    doctor_pattern = re.compile(
        r'if\s*\(\s*value\s*>\s*buffvalue\s*\)\s*'
        r'return\s+0\s*;\s*'
        r'\s*buffdiff\s*-=\s*value\s*;',
        re.MULTILINE
    )

    doctor_replace = r'''
                        if (value > buffvalue)
                                return 0;

                        if (value == buffvalue) {
                                buff->setTimeRemaining(duration);
                                return buffvalue;
                        }

                        patient->removeBuff(buffcrc);

                        buffdiff -= value;
'''

    text, count = doctor_pattern.subn(doctor_replace, text)

    if count:
        print(file.name, ": Doctor refresh patched")

    #
    # ----------------------------------------
    # Entertainer
    # ----------------------------------------
    #

    ent_pattern = re.compile(
        r'if\s*\(\s*oldBuff\s*!=\s*nullptr\s*&&\s*oldBuff->getBuffStrength\(\)\s*>\s*buffStrength\s*\)\s*'
        r'return\s*;',
        re.MULTILINE
    )

    ent_replace = r'''
                        if (oldBuff != nullptr) {

                                if (oldBuff->getBuffStrength() > buffStrength)
                                        return;

                                if (oldBuff->getBuffStrength() == buffStrength) {
                                        oldBuff->setTimeRemaining(buffDuration * 105);
                                        return;
                                }

                                creature->removeBuff(oldBuff->getBuffCRC());
                        }
'''

    text, count2 = ent_pattern.subn(ent_replace, text)

    if count2:
        print(file.name, ": Entertainer refresh patched")

    #
    # Write file
    #

    if text != original:
        file.write_text(text)
        patched += 1

print()
print("==============================")
print("6D.01 Smart Rebuff Complete")
print("==============================")
print("Files modified:", patched)

if patched == 0:
    print()
    print("Nothing changed.")
    print("Likely your source differs from stock.")
