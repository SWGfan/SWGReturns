#!/usr/bin/env python3

from pathlib import Path

ROOT = Path(".").resolve()

lootdir = ROOT / "bin/scripts/loot/groups/custom"
lootdir.mkdir(parents=True, exist_ok=True)

groups = {

"common_lewt_box_01":[
("rarelootsystem",3000000),
("resource_crate_loot",2500000),
("resource_deed_loot",1500000),
("legendary_comp_group",1000000),
("armor_attachments",1000000),
("clothing_attachments",1000000),
],

"rare_lewt_box_01":[
("rarelootsystem",2500000),
("legendary_comp_group",2500000),
("boss_rare",1500000),
("resource_deed_loot",1000000),
("resource_crate_loot",1000000),
("g_rifle_t21_legendary",500000),
("g_pistol_fwg5_legendary",500000),
("g_baton_stun_legendary",500000),
("g_lance_nightsister_legendary",500000),
],

"event_lewt_box_01":[
("legendary_comp_group",2000000),
("boss_rare",2000000),
("rarelootsystem",1500000),
("resource_deed_loot",1000000),
("resource_crate_loot",1000000),
("g_rifle_t21_legendary",625000),
("g_pistol_fwg5_legendary",625000),
("g_baton_stun_legendary",625000),
("g_lance_nightsister_legendary",625000),
],

}

for name,items in groups.items():

    path=lootdir/(name+".lua")

    with open(path,"w") as f:

        f.write(name+" = {\n")
        f.write('    description = "",\n')
        f.write("    minimumLevel = 0,\n")
        f.write("    maximumLevel = -1,\n")
        f.write("    lootItems = {\n")

        for item,weight in items:

            f.write(
                f'        {{itemTemplate = "{item}", weight = {weight}}},\n'
            )

        f.write("    }\n")
        f.write("}\n\n")
        f.write(f'addLootGroupTemplate("{name}", {name})\n')

    print("Created",path)

print()
print("Done.")
