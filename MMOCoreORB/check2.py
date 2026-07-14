#!/usr/bin/env python3

import re
import shutil
from pathlib import Path

ROOT = Path(".")

TARGETS = {

    # Weapon Components
    "blaster_pistol_barrel_advanced":250,
    "blaster_rifle_barrel_advanced":250,
    "projectile_pistol_barrel_advanced":250,
    "projectile_rifle_barrel_advanced":250,
    "projectile_feed_mechanism_advanced":250,
    "blaster_power_handler_advanced":250,

    "scope_weapon_advanced":200,
    "stock_advanced":200,

    "reinforcement_core_advanced":150,
    "sword_core_advanced":150,
    "vibro_unit_advanced":150,

    "vibro_unit_nightsister":50,

    "acklay_bone_reinforcement_core":25,
    "geonosian_reinforcement_core":25,
    "wampa_bone_reinforcement_core":25,

    "weapon_power_bit":50,

    # Chemistry
    "biologic_effect_controller_advanced":200,
    "liquid_delivery_suspension_advanced":200,
    "release_mechanism_duration_advanced":200,
    "solid_delivery_shell_advanced":200,
}

SEARCH_DIRS = [

    ROOT/"bin/scripts/object/tangible/component/weapon",
    ROOT/"bin/scripts/object/tangible/component/chemistry"

]

PATTERNS = [

    r"(maxStackSize\s*=\s*)(\d+)",
    r"(maxStack\s*=\s*)(\d+)",
    r"(stackSize\s*=\s*)(\d+)",

]

changed=[]

for folder in SEARCH_DIRS:

    if not folder.exists():
        continue

    for lua in folder.rglob("*.lua"):

        stem = lua.stem

        if stem not in TARGETS:
            continue

        stack = TARGETS[stem]

        text = lua.read_text(
            encoding="utf8",
            errors="ignore"
        )

        original=text

        for pattern in PATTERNS:

            text = re.sub(
                pattern,
                rf"\g<1>{stack}",
                text
            )

        if text != original:

            backup = lua.with_suffix(".lua.phase724.bak")

            if not backup.exists():
                shutil.copy2(lua,backup)

            lua.write_text(
                text,
                encoding="utf8"
            )

            changed.append((lua,stack))

print()

print("="*70)
print("Component Stack Rebalance")
print("="*70)

for file,stack in changed:

    print(f"{file} -> {stack}")

report = Path("phase724_stack_report.txt")

with report.open("w") as f:

    f.write("Phase 7.24 Stack Rebalance\n")
    f.write("="*60+"\n\n")

    for file,stack in changed:

        f.write(f"{file} -> {stack}\n")

print()

print(f"Modified {len(changed)} files.")
print(f"Report: {report}")
