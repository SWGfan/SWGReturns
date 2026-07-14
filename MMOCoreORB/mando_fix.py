#!/usr/bin/env python3

import os
import re
import shutil

ROOT = os.getcwd()

BACKUP = True

changed = 0

SKILL_REPLACEMENTS = [
    (
        r"requiredSkill\s*=\s*\"[^\"]+\"",
        'requiredSkill = "master_armorsmith"',
    ),
    (
        r"requiredSkill\s*=\s*'[^']+'",
        "requiredSkill = 'master_armorsmith'",
    ),
    (
        r"requiredSkill\s*=\s*[A-Za-z0-9_]+",
        "requiredSkill = master_armorsmith",
    ),
]

REMOVE_PATTERNS = [
    r".*death.?watch.*\n?",
    r".*Death.?Watch.*\n?",
    r".*craftingRoom.*\n?",
    r".*requiredRoom.*\n?",
    r".*requiredCell.*\n?",
    r".*cellRestriction.*\n?",
    r".*bunker.*\n?",
]

def backup(path):
    if BACKUP:
        shutil.copy2(path, path + ".bak")

def process(path):
    global changed

    with open(path, "r", encoding="utf8", errors="ignore") as f:
        text = f.read()

    original = text

    for pattern, repl in SKILL_REPLACEMENTS:
        text = re.sub(pattern, repl, text)

    for pattern in REMOVE_PATTERNS:
        text = re.sub(pattern, "", text, flags=re.IGNORECASE)

    if text != original:
        backup(path)

        with open(path, "w", encoding="utf8") as f:
            f.write(text)

        changed += 1
        print("Patched:", path)

for root, dirs, files in os.walk(ROOT):

    for file in files:

        if not file.endswith(".lua"):
            continue

        lower = file.lower()

        if "mandalorian" not in lower:
            continue

        process(os.path.join(root, file))

print()
print("------------------------------------")
print("Finished")
print("Files Modified:", changed)
print("------------------------------------")
