
#!/usr/bin/env python3

from pathlib import Path
import shutil
import re
import sys

CPP = Path("src/server/zone/managers/loot/LootManagerImplementation.cpp")

if not CPP.exists():
    print("Cannot find:", CPP)
    sys.exit(1)

bak = CPP.with_suffix(".cpp.phase703_remove_validation.bak")

if not bak.exists():
    shutil.copy2(CPP, bak)
    print("Backup:", bak)

text = CPP.read_text()

pattern = re.compile(
    r'''
        \s*const\s+char\*\s+crateGroups\[\]\s*=\s*\{.*?\};
        \s*
        for\s*\(
            .*?
        \)\s*\{
            .*?crateGroups\[i\].*?
        \}
    ''',
    re.DOTALL | re.VERBOSE
)

text, count = pattern.subn("\n", text)

CPP.write_text(text)

print()
print("="*72)
print("Removed", count, "crate validation block(s).")
print("="*72)
