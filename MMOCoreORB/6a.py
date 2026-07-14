#!/usr/bin/env python3

from pathlib import Path
import shutil
import re

ROOT = Path(".")

FILES = [
    ROOT / "src/server/zone/objects/creature/buffs/PerformanceBuffImplementation.cpp",
    ROOT / "src/server/zone/objects/creature/commands/UseSkillBuffCommand.h",
    ROOT / "src/server/zone/objects/tangible/components/SkillBuffObjectMenuComponent.cpp",
    ROOT / "src/server/zone/objects/creature/buffs/BuffImplementation.cpp",
]

DAY_MS = 24 * 60 * 60 * 1000

patched = 0


def backup(path):
    bak = Path(str(path) + ".phase6a.bak")
    if not bak.exists():
        shutil.copy2(path, bak)


def replace_duration(text):
    patterns = [
        (r'2\s*\*\s*60\s*\*\s*60\s*\*\s*1000', str(DAY_MS)),
        (r'3\s*\*\s*60\s*\*\s*60\s*\*\s*1000', str(DAY_MS)),
        (r'4\s*\*\s*60\s*\*\s*60\s*\*\s*1000', str(DAY_MS)),
        (r'6\s*\*\s*60\s*\*\s*60\s*\*\s*1000', str(DAY_MS)),
        (r'8\s*\*\s*60\s*\*\s*60\s*\*\s*1000', str(DAY_MS)),
        (r'12\s*\*\s*60\s*\*\s*60\s*\*\s*1000', str(DAY_MS)),
    ]

    for pat, rep in patterns:
        text = re.sub(pat, rep, text)

    return text


for file in FILES:

    if not file.exists():
        continue

    backup(file)

    original = file.read_text(errors="ignore")
    text = original

    #
    # Make buffs 24 hours
    #

    text = replace_duration(text)

    #
    # Replace renew logic with refresh
    #

    text = text.replace(
        "renewBuff(",
        "removeBuff("
    )

    #
    # Refresh existing buff
    #

    if "Phase6A Refresh" not in text:

        text = text.replace(

            "addBuff(",

            """// Phase6A Refresh
removeBuff(buff->getBuffCRC());
addBuff(""",

            1

        )

    #
    # Persistence
    #

    persist_patterns = [

        ("setPersistent(false)", "setPersistent(true)"),
        ("persistent = false", "persistent = true"),
        ("isPersistent = false", "isPersistent = true"),

    ]

    for old, new in persist_patterns:
        text = text.replace(old, new)

    #
    # Logout safe
    #

    if "setRemoveOnLogout(true)" in text:
        text = text.replace(
            "setRemoveOnLogout(true)",
            "setRemoveOnLogout(false)"
        )

    #
    # Clone safe
    #

    if "setRemoveOnClone(true)" in text:
        text = text.replace(
            "setRemoveOnClone(true)",
            "setRemoveOnClone(false)"
        )

    #
    # Zone safe
    #

    if "setRemoveOnZone(true)" in text:
        text = text.replace(
            "setRemoveOnZone(true)",
            "setRemoveOnZone(false)"
        )

    if text != original:
        file.write_text(text)
        patched += 1
        print("Patched:", file)

print()
print("==============================")
print("Returns Phase 6A Complete")
print("==============================")
print("Files patched:", patched)
print("Buff duration:", DAY_MS, "ms (24 hours)")
print("Backups created: *.phase6a.bak")
