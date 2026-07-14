#!/usr/bin/env python3

from pathlib import Path
import shutil
import re
import sys

ROOT = Path.home() / "StarDust-2" / "MMOCoreORB"

FILE = ROOT / "bin/scripts/screenplays/static_spawns/yavin4_static_spawns.lua"

if not FILE.exists():
    print("File not found:", FILE)
    sys.exit(1)

BACKUP = FILE.with_suffix(FILE.suffix + ".phase731b.bak")
shutil.copy2(FILE, BACKUP)

text = FILE.read_text()

# Already added?
if "trainer_dark_sentinel" in text or "trainer_light_sentinel" in text:
    print("FRS trainers already exist.")
    sys.exit(0)

insert = '''
                -- Jedi Enclaves
                {"trainer_light_sentinel", 300, -5579.0, 87.7, 4908.5, -179, 0},
                {"trainer_dark_sentinel", 300, 5074.2, 78.8, 313.9, 90, 0},
'''

# Find the end of the mobiles table:
#
#        }
# }
#
pattern = re.compile(r'(\n\s*\}\n\})', re.MULTILINE)

match = pattern.search(text)

if not match:
    print("Could not find end of mobiles table.")
    sys.exit(1)

text = text[:match.start()] + "\n" + insert + text[match.start():]

FILE.write_text(text)

print("Done.")
print("Backup:", BACKUP)
