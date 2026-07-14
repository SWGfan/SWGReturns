#!/usr/bin/env python3
"""
Phase 8.27
Fix Nightsister Vibro Unit inheritance

Changes:

object_tangible_component_weapon_shared_vibro_unit_nightsister:new

to

object_tangible_component_weapon_shared_vibro_unit_advanced:new

Creates a .bak backup.
"""

from pathlib import Path
import shutil

ROOT = Path(".").resolve()

TARGET = ROOT / "bin/scripts/object/tangible/component/weapon/vibro_unit_nightsister.lua"

if not TARGET.exists():
    print("ERROR:")
    print(TARGET)
    print("not found.")
    raise SystemExit(1)

backup = TARGET.with_suffix(".lua.phase827.bak")

if not backup.exists():
    shutil.copy2(TARGET, backup)
    print("Backup created:")
    print(backup)

text = TARGET.read_text(encoding="utf8")

old = "object_tangible_component_weapon_shared_vibro_unit_nightsister:new"
new = "object_tangible_component_weapon_shared_vibro_unit_advanced:new"

if old not in text:
    print("Already patched or expected text not found.")
    raise SystemExit(0)

text = text.replace(old, new)

TARGET.write_text(text, encoding="utf8")

print()
print("=" * 70)
print("Phase 8.27 applied successfully.")
print("Inheritance changed to Advanced Vibro Unit.")
print("=" * 70)
