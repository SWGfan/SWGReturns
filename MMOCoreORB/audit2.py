#!/usr/bin/env python3
"""
============================================================
Phase 7.22B
Advanced Component Crafting Audit

READ ONLY

Audits every advanced component to verify:

    - Tangible object
    - Draft schematic
    - Loot template
    - Loot registration
    - Schematics.lua registration
    - Tangible serverobjects registration
    - Draft serverobjects registration

Outputs:

    phase722B_crafting_report.txt
============================================================
"""

from pathlib import Path

ROOT = Path.home() / "StarDust-2" / "MMOCoreORB"

COMPONENTS = {

"blaster_pistol_barrel_advanced": {
    "type":"weapon"
},

"blaster_rifle_barrel_advanced":{
    "type":"weapon"
},

"blaster_power_handler_advanced":{
    "type":"weapon"
},

"projectile_pistol_barrel_advanced":{
    "type":"weapon"
},

"projectile_rifle_barrel_advanced":{
    "type":"weapon"
},

"projectile_feed_mechanism_advanced":{
    "type":"weapon"
},

"scope_weapon_advanced":{
    "type":"weapon"
},

"stock_advanced":{
    "type":"weapon"
},

"reinforcement_core_advanced":{
    "type":"weapon"
},

"sword_core_advanced":{
    "type":"weapon"
},

"vibro_unit_advanced":{
    "type":"weapon"
},

"biologic_effect_controller_advanced":{
    "type":"chem"
},

"release_mechanism_duration_advanced":{
    "type":"chem"
},

"liquid_delivery_suspension_advanced":{
    "type":"chem"
},

"solid_delivery_shell_advanced":{
    "type":"chem"
},

"vibro_unit_nightsister":{
    "type":"weapon"
}

}

FILES = [

ROOT/"bin/scripts/loot/items.lua",

ROOT/"bin/scripts/managers/crafting/schematics.lua",

ROOT/"bin/scripts/object/tangible/component/weapon/serverobjects.lua",

ROOT/"bin/scripts/object/tangible/component/chemistry/serverobjects.lua",

ROOT/"bin/scripts/object/draft_schematic/weapon/component/serverobjects.lua"

]

texts={}

for f in FILES:

    if f.exists():
        texts[f.name]=f.read_text(errors="ignore")
    else:
        texts[f.name]=""

report=[]

report.append("="*70)
report.append("Phase 7.22B Crafting Audit")
report.append("="*70)
report.append("")

for comp,data in COMPONENTS.items():

    report.append(comp)
    report.append("-"*len(comp))

    if data["type"]=="weapon":

        tangible=ROOT/f"bin/scripts/object/tangible/component/weapon/{comp}.lua"

    else:

        tangible=ROOT/f"bin/scripts/object/tangible/component/chemistry/{comp}.lua"

    draft=ROOT/f"bin/scripts/object/draft_schematic/weapon/component/{comp}.lua"

    loot=ROOT/f"bin/scripts/loot/items/component_loot/{comp}.lua"

    report.append(
        f"Tangible........ {'OK' if tangible.exists() else 'MISSING'}"
    )

    report.append(
        f"Draft........... {'OK' if draft.exists() else 'MISSING'}"
    )

    report.append(
        f"Loot Template... {'OK' if loot.exists() else 'MISSING'}"
    )

    report.append(
        f"Loot Items...... {'OK' if comp in texts['items.lua'] else 'MISSING'}"
    )

    report.append(
        f"Schematics...... {'OK' if comp in texts['schematics.lua'] else 'MISSING'}"
    )

    if data["type"]=="weapon":

        reg=comp in texts["serverobjects.lua"]

    else:

        reg=comp in texts["serverobjects.lua"]

    report.append(
        f"ServerObjects... {'OK' if reg else 'MISSING'}"
    )

    report.append("")

Path("phase722B_crafting_report.txt").write_text(
    "\n".join(report)
)

print("="*70)
print("Audit complete.")
print("Output:")
print("  phase722B_crafting_report.txt")
print("="*70)
