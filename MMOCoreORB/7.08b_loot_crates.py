#!/usr/bin/env python3

from pathlib import Path
import shutil
import re

ROOT = Path(".").resolve()

FILES = {
    "bin/scripts/custom_scripts/tools/CommonLewtBoxMenuComponent.lua": [
        'createLoot(inventory, "rarelootsystem", 250, true)',
        'createLoot(inventory, "rarelootsystem", 275, true)',
        'createLoot(inventory, "rarelootsystem", 300, true)',
        'createLoot(inventory, "resource_crate_loot", 300, true)',
        'createLoot(inventory, "resource_deed_loot", 350, true)',
    ],

    "bin/scripts/custom_scripts/tools/RareLewtBoxMenuComponent.lua": [
        'createLoot(inventory, "rarelootsystem", 350, true)',
        'createLoot(inventory, "legendary_comp_group", 400, true)',
        'createLoot(inventory, "boss_rare", 425, true)',
        'createLoot(inventory, "resource_deed_loot", 450, true)',
        'createLoot(inventory, "resource_crate_loot", 450, true)',
    ],

    "bin/scripts/custom_scripts/tools/EventLewtBoxMenuComponent.lua": [
        'createLoot(inventory, "rarelootsystem", 400, true)',
        'createLoot(inventory, "legendary_comp_group", 500, true)',
        'createLoot(inventory, "boss_rare", 500, true)',
        'createLoot(inventory, "g_rifle_t21_legendary", 550, true)',
        'createLoot(inventory, "g_pistol_fwg5_legendary", 550, true)',
        'createLoot(inventory, "g_baton_stun_legendary", 550, true)',
        'createLoot(inventory, "g_lance_nightsister_legendary", 550, true)',
        'createLoot(inventory, "resource_deed_loot", 500, true)',
        'createLoot(inventory, "resource_crate_loot", 500, true)',
    ]
}

pattern = re.compile(
    r'if\s*\(inventory\s*~=\s*nil\)\s*then.*?end',
    re.DOTALL
)

patched = 0
backups = 0

print("=" * 70)
print("Phase 7.08b - Patch Lewt Box Loot")
print("=" * 70)

for relpath, lootlines in FILES.items():

    path = ROOT / relpath

    if not path.exists():
        print("Missing:", relpath)
        continue

    backup = path.with_suffix(path.suffix + ".phase708b.bak")

    if not backup.exists():
        shutil.copy2(path, backup)
        backups += 1

    text = path.read_text(encoding="utf-8")

    replacement = "if (inventory ~= nil) then\n"

    for line in lootlines:
        replacement += "            " + line + "\n"

    replacement += "        end"

    newtext, count = pattern.subn(replacement, text, count=1)

    if count == 0:
        print("Couldn't patch:", relpath)
        continue

    path.write_text(newtext, encoding="utf-8")

    patched += 1

    print("Patched:", relpath)

print()
print("=" * 70)
print("Finished")
print("=" * 70)
print("Patched :", patched)
print("Backups :", backups)
