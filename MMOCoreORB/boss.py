#!/usr/bin/env python3

from pathlib import Path
import re
import shutil

cpp = Path("src/server/zone/objects/creature/CreatureObjectImplementation.cpp")

if not cpp.exists():
    print("Cannot find", cpp)
    exit(1)

bak = cpp.with_suffix(".cpp.phase710a.repair.bak")

if not bak.exists():
    shutil.copy2(cpp, bak)

text = cpp.read_text()

pattern = re.compile(
    r"""
        //\s*=+\s*
        //\s*Phase\s+7\.10a\s+World\s+Boss\s+Override.*?
        if\s*\(\s*object->getLevel\(\)\s*>=\s*120\s*\)
        \s*return\s+true;
        \s*
        if\s*\(\s*!object->isRebel\(\)\s*&&\s*!object->isImperial\(\)\s*\)
        \s*return\s+true;
    """,
    re.DOTALL | re.VERBOSE
)

replacement = r"""
        // ==========================================================
        // Phase 7.10a World Boss Override
        // ==========================================================

        if (object->getLevel() >= 120) {
                return true;
        }

        if (!object->isRebel() && !object->isImperial()) {
                return true;
        }
"""

text, n = pattern.subn(replacement, text)

if n == 0:
    print("Couldn't repair automatically.")
    print("Restore the backup or paste lines 3290-3320.")
    exit(1)

cpp.write_text(text)

print("Repaired", n, "block(s)")
