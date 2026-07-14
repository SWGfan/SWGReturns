#!/usr/bin/env python3
"""
boost_vibro_blacksun.py
Sets all vibro motor and Black Sun razor damage to 1000.

Usage:
  python3 boost_vibro_blacksun.py <stardust_root>
"""

import sys, os, shutil, datetime

if len(sys.argv) < 2:
    print(__doc__); sys.exit(1)

root = sys.argv[1].rstrip("/")
ts   = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
LOOT = f"{root}/bin/scripts/loot"

def overwrite(path, content):
    if not os.path.isfile(path):
        print(f"  SKIP (not found): {path}")
        return
    shutil.copy2(path, f"{path}.boost_{ts}.bak")
    with open(path, 'w') as f: f.write(content)
    print(f"  Done: {os.path.basename(path)}")

overwrite(f"{LOOT}/items/fine_tuned_vibro_motor.lua", '''fine_tuned_vibro_motor = {
\tminimumLevel = 0,
\tmaximumLevel = -1,
\tcustomObjectName = "",
\tdirectObjectTemplate = "object/tangible/component/weapon/vibro_unit_enhancement_min_damage.iff",
\tcraftingValues = {
\t\t{"mindamage",1000,1000,1},
\t\t{"maxdamage",1000,1000,1},
\t\t{"attackspeed",-0.3,-0.5,1},
\t\t{"useCount",1,5,0},
\t},
\tcustomizationStringNames = {},
\tcustomizationValues = {}
}
addLootItemTemplate("fine_tuned_vibro_motor", fine_tuned_vibro_motor)
''')

overwrite(f"{LOOT}/items/high_powered_vibro_motor.lua", '''high_powered_vibro_motor = {
\tminimumLevel = 0,
\tmaximumLevel = -1,
\tcustomObjectName = "",
\tdirectObjectTemplate = "object/tangible/component/weapon/vibro_unit_enhancement_max_damage.iff",
\tcraftingValues = {
\t\t{"mindamage",1000,1000,1},
\t\t{"maxdamage",1000,1000,1},
\t\t{"useCount",1,5,0},
\t},
\tcustomizationStringNames = {},
\tcustomizationValues = {}
}
addLootItemTemplate("high_powered_vibro_motor", high_powered_vibro_motor)
''')

overwrite(f"{LOOT}/items/high_frequency_vibro_motor.lua", '''high_frequency_vibro_motor = {
\tminimumLevel = 0,
\tmaximumLevel = -1,
\tcustomObjectName = "",
\tdirectObjectTemplate = "object/tangible/component/weapon/vibro_unit_enhancement_wounding.iff",
\tcraftingValues = {
\t\t{"mindamage",1000,1000,1},
\t\t{"maxdamage",1000,1000,1},
\t\t{"woundchance",25,35,10},
\t\t{"useCount",1,5,0},
\t},
\tcustomizationStringNames = {},
\tcustomizationValues = {}
}
addLootItemTemplate("high_frequency_vibro_motor", high_frequency_vibro_motor)
''')

overwrite(f"{LOOT}/items/npc/nightsister_vibro_unit.lua", '''nightsister_vibro_unit = {
\tminimumLevel = 0,
\tmaximumLevel = -1,
\tcustomObjectName = "",
\tdirectObjectTemplate = "object/tangible/component/weapon/vibro_unit_nightsister.iff",
\tcraftingValues = {
\t\t{"mindamage",1000,1000,1},
\t\t{"maxdamage",1000,1000,1},
\t\t{"attackspeed",-0.2,-0.4,1},
\t\t{"woundchance",25,35,10},
\t\t{"hitpoints",500,1000,0},
\t\t{"zerorangemod",10,25,0},
\t\t{"maxrangemod",10,25,0},
\t\t{"midrangemod",10,25,0},
\t\t{"attackhealthcost",0,0,0},
\t\t{"attackactioncost",30,0,0},
\t\t{"attackmindcost",0,0,0},
\t\t{"useCount",1,5,0},
\t},
\tcustomizationStringNames = {},
\tcustomizationValues = {}
}
addLootItemTemplate("nightsister_vibro_unit", nightsister_vibro_unit)
''')

for fname, tpl in [
    ("blacksun_razor",         "object/weapon/melee/special/blacksun_razor.iff"),
    ("blacksun_razor_generic", "object/weapon/melee/special/blacksun_razor_generic.iff"),
]:
    overwrite(f"{LOOT}/custom_loot/items/weapons/{fname}.lua", f'''{fname} = {{
\tminimumLevel = 0,
\tmaximumLevel = -1,
\tcustomObjectName = "Blacksun Razor",
\tdirectObjectTemplate = "{tpl}",
\tcraftingValues = {{
\t\t{{"mindamage",1000,1000,0}},
\t\t{{"maxdamage",1000,1000,0}},
\t\t{{"attackspeed",2.5,1.8,1}},
\t\t{{"woundchance",20,35,0}},
\t\t{{"hitpoints",1500,3000,0}},
\t\t{{"zerorangemod",10,20,0}},
\t\t{{"maxrangemod",10,20,0}},
\t\t{{"midrange",3,3,0}},
\t\t{{"midrangemod",10,20,0}},
\t\t{{"maxrange",7,7,0}},
\t\t{{"attackhealthcost",20,10,0}},
\t\t{{"attackactioncost",30,15,0}},
\t\t{{"attackmindcost",20,10,0}},
\t}},
\tcustomizationStringNames = {{}},
\tcustomizationValues = {{}},
\trandomDotChance = 300,
\tjunkDealerTypeNeeded = JUNKWEAPONS,
\tjunkMinValue = 100,
\tjunkMaxValue = 200
}}
addLootItemTemplate("{fname}", {fname})
''')

print("\nAll set to 1000 damage. Restart server to apply.")
