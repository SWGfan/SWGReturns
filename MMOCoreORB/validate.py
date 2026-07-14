#!/usr/bin/env python3
"""
======================================================================
Phase 7.23

Advanced Component Validator

Validates every advanced crafting component from loot all the way to
the crafting manager.

Read-only.

Output:
    phase723_component_validation.txt
======================================================================
"""

from pathlib import Path
import re

ROOT = Path.home() / "StarDust-2" / "MMOCoreORB"

COMPONENTS = [

# Weaponsmith
("blaster_pistol_barrel_advanced","weapon"),
("blaster_rifle_barrel_advanced","weapon"),
("blaster_power_handler_advanced","weapon"),
("projectile_pistol_barrel_advanced","weapon"),
("projectile_rifle_barrel_advanced","weapon"),
("projectile_feed_mechanism_advanced","weapon"),
("scope_weapon_advanced","weapon"),
("stock_advanced","weapon"),
("reinforcement_core_advanced","weapon"),
("blade_vibro_unit_advanced","weapon"),
("sword_core_advanced","weapon"),

# Bio Engineer
("biologic_effect_controller_advanced","chem"),
("liquid_delivery_suspension_advanced","chem"),
("release_mechanism_duration_advanced","chem"),
("solid_delivery_shell_advanced","chem"),

# Rare
("vibro_unit_nightsister","weapon"),
]

FILES = {

"loot_items":
ROOT/"bin/scripts/loot/items.lua",

"schematics":
ROOT/"bin/scripts/managers/crafting/schematics.lua",

"weapon_server":
ROOT/"bin/scripts/object/tangible/component/weapon/serverobjects.lua",

"chem_server":
ROOT/"bin/scripts/object/tangible/component/chemistry/serverobjects.lua",

"weapon_draft_server":
ROOT/"bin/scripts/object/draft_schematic/weapon/component/serverobjects.lua",

"chem_draft_server":
ROOT/"bin/scripts/object/draft_schematic/chemistry/component/serverobjects.lua",

"custom_weapon_server":
ROOT/"bin/scripts/object/custom_content/draft_schematic/weapon/component/serverobjects.lua",

"crafting_contractor":
ROOT/"bin/scripts/screenplays/tasks/misc/crafting_contractor.lua",

}

TEXT = {}

for k,v in FILES.items():
    if v.exists():
        TEXT[k]=v.read_text(errors="ignore")
    else:
        TEXT[k]=""

report=[]

report.append("="*70)
report.append("Phase 7.23 Component Validation")
report.append("="*70)
report.append("")

total_ok=0
total_warn=0

for comp,ctype in COMPONENTS:

    report.append(comp)
    report.append("-"*len(comp))

    if ctype=="weapon":

        tangible = ROOT / f"bin/scripts/object/tangible/component/weapon/{comp}.lua"

        draft = ROOT / f"bin/scripts/object/draft_schematic/weapon/component/{comp}.lua"

    else:

        tangible = ROOT / f"bin/scripts/object/tangible/component/chemistry/{comp}.lua"

        draft = ROOT / f"bin/scripts/object/draft_schematic/chemistry/component/{comp}.lua"

    loot = ROOT / f"bin/scripts/loot/items/component_loot/{comp}.lua"

    checks = {}

    checks["Tangible"] = tangible.exists()
    checks["Draft"] = draft.exists()
    checks["LootTemplate"] = loot.exists()

    checks["LootItems"] = comp in TEXT["loot_items"]

    checks["Schematics"] = (
        f"{comp}.iff" in TEXT["schematics"]
    )

    checks["CraftingContractor"] = (
        comp in TEXT["crafting_contractor"]
    )

    checks["TangibleServer"] = (
        comp in TEXT["weapon_server"]
        or
        comp in TEXT["chem_server"]
    )

    checks["DraftServer"] = (
        comp in TEXT["weapon_draft_server"]
        or
        comp in TEXT["chem_draft_server"]
        or
        comp in TEXT["custom_weapon_server"]
    )

    ok=True

    for name,val in checks.items():

        if val:
            report.append(f"  [OK]      {name}")
        else:
            report.append(f"  [MISSING] {name}")
            ok=False

    if ok:
        total_ok+=1
    else:
        total_warn+=1

    report.append("")

report.append("="*70)
report.append("SUMMARY")
report.append("="*70)
report.append(f"Complete Components : {total_ok}")
report.append(f"Need Attention      : {total_warn}")

Path("phase723_component_validation.txt").write_text(
    "\n".join(report)
)

print("="*70)
print("Validation complete.")
print("Report:")
print("  phase723_component_validation.txt")
print("="*70)
