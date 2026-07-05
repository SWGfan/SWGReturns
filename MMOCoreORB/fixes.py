#!/usr/bin/env python3
"""
apply_stardust_improvements.py
Implements the following improvements for StarDust-2:

  COMBAT:
    1. Vibro-Motors (fine_tuned, high_powered, high_frequency, Nightsister)
       boosted to ~2500 damage when fully modified
    2. Weapon powerups: 250 uses -> 1000 uses (C++ change)

  RESOURCES:
    3. High quality floor on all resources: always roll 85-100% of max stat (C++ change)

  PETS (new faction pets for both Rebel & Imperial):
    4. Death Watch: Wraith, Overlord, Battle Droid, Herald, Child of the Watch
    5. Black Sun: Guard, Henchman, Thug, Assassin
    6. Nightsister Elder
    7. Acklay
    8. Dark Jedi: Adept, Knight, Master

  CRAFTING/DROPS:
    9. Advanced weapon components added to elite Tusken Raiders + Naboo Mauler Lord

Usage:
  python3 apply_stardust_improvements.py <stardust_root>
"""

import sys, os, shutil, re, datetime

if len(sys.argv) < 2:
    print(__doc__); sys.exit(1)

root = sys.argv[1].rstrip("/")
ts   = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")

def bak(p):
    shutil.copy2(p, f"{p}.improve_{ts}.bak")

def patch(src, old, new, label):
    if old not in src:
        print(f"  WARN ({label}): anchor not found")
        return src, 0
    print(f"  PATCHED: {label}")
    return src.replace(old, new, 1), 1

# ── 1. Vibro-Motor damage boost ───────────────────────────────────────────────
print("=== 1. Vibro-Motor damage boost ===")

LOOT = f"{root}/bin/scripts/loot/items"

vibro_changes = {
    f"{LOOT}/fine_tuned_vibro_motor.lua": (
        '\tcraftingValues = {\n'
        '\t\t{\"attackspeed\",-0.2,-0.4,1},\n'
        '\t\t{\"useCount\",1,5,0},\n'
        '\t},',
        '\tcraftingValues = {\n'
        '\t\t{\"attackspeed\",-0.2,-0.4,1},\n'
        '\t\t{\"mindamage\",150,200,1},\n'
        '\t\t{\"maxdamage\",200,250,1},\n'
        '\t\t{\"useCount\",1,5,0},\n'
        '\t},',
        "fine_tuned_vibro_motor damage boost"
    ),
    f"{LOOT}/high_powered_vibro_motor.lua": (
        '\tcraftingValues = {\n'
        '\t\t{\"maxdamage\",20,20.01,1}, -- setting at 20,20.01, allows damage to be effected by legendary, exceptional, and yellow modifiers\n'
        '\t\t{\"useCount\",1,5,0},\n'
        '\t},',
        '\tcraftingValues = {\n'
        '\t\t{\"mindamage\",150,200,1},\n'
        '\t\t{\"maxdamage\",200,250,1},\n'
        '\t\t{\"useCount\",1,5,0},\n'
        '\t},',
        "high_powered_vibro_motor damage boost"
    ),
    f"{LOOT}/high_frequency_vibro_motor.lua": (
        '\tcraftingValues = {\n'
        '\t\t{\"woundchance\",5,8,10},\n'
        '\t\t{\"useCount\",1,5,0},\n'
        '\t},',
        '\tcraftingValues = {\n'
        '\t\t{\"woundchance\",5,8,10},\n'
        '\t\t{\"mindamage\",100,150,1},\n'
        '\t\t{\"maxdamage\",150,200,1},\n'
        '\t\t{\"useCount\",1,5,0},\n'
        '\t},',
        "high_frequency_vibro_motor damage boost"
    ),
}

for fpath, (old, new, label) in vibro_changes.items():
    if not os.path.isfile(fpath):
        print(f"  SKIP (not found): {fpath}")
        continue
    with open(fpath) as f: content = f.read()
    if new.strip().split('\n')[0] in content:
        print(f"  SKIP (already done): {label}")
        continue
    bak(fpath)
    content, n = patch(content, old, new, label)
    if n: 
        with open(fpath, 'w') as f: f.write(content)

# Also write a Nightsister vibro motor item if it doesn't exist
ns_motor_path = f"{LOOT}/nightsister_vibro_motor.lua"
if not os.path.isfile(ns_motor_path):
    with open(ns_motor_path, 'w') as f:
        f.write('''nightsister_vibro_motor = {
\tminimumLevel = 0,
\tmaximumLevel = -1,
\tcustomObjectName = "Nightsister Vibro Motor",
\tdirectObjectTemplate = "object/tangible/component/weapon/vibro_unit_enhancement_max_damage.iff",
\tcraftingValues = {
\t\t{\"mindamage\",175,225,1},
\t\t{\"maxdamage\",225,275,1},
\t\t{\"woundchance\",5,10,10},
\t\t{\"useCount\",1,5,0},
\t},
\tcustomizationStringNames = {},
\tcustomizationValues = {}
}

addLootItemTemplate("nightsister_vibro_motor", nightsister_vibro_motor)
''')
    print("  Created: nightsister_vibro_motor.lua (225-275 max damage + woundchance)")

    # Add to items.lua if not there
    items_lua = f"{root}/bin/scripts/loot/items.lua"
    if os.path.isfile(items_lua):
        with open(items_lua) as f: il = f.read()
        inc = 'includeFile("items/nightsister_vibro_motor.lua")'
        if inc not in il:
            with open(items_lua, 'a') as f:
                f.write(f"\n{inc}\n")
            print("  Added nightsister_vibro_motor to items.lua")

print()

# ── 2. Weapon powerups: 250 -> 1000 uses ─────────────────────────────────────
print("=== 2. Weapon powerups: 250 -> 1000 uses (C++) ===")

POWERUP_CPP = f"{root}/src/server/zone/objects/tangible/powerup/PowerupObjectImplementation.cpp"
if os.path.isfile(POWERUP_CPP):
    with open(POWERUP_CPP) as f: src = f.read()
    src, n = patch(src,
        '\t\tuses = 250; // Powerups are always 100 uses',
        '\t\tuses = 1000; // Boosted to 1000 uses',
        "powerup uses 250->1000"
    )
    if n:
        bak(POWERUP_CPP)
        with open(POWERUP_CPP, 'w') as f: f.write(src)
else:
    print(f"  WARN: {POWERUP_CPP} not found")

print()

# ── 3. Resource quality floor ─────────────────────────────────────────────────
print("=== 3. Resource quality floor (C++) ===")

RES_CPP = f"{root}/src/server/zone/managers/resource/resourcespawner/ResourceSpawner.cpp"
if os.path.isfile(RES_CPP):
    with open(RES_CPP) as f: src = f.read()
    src, n = patch(src,
        'int ResourceSpawner::randomizeValue(int min, int max) {\n'
        '\tint randomStat = System::random(max - min) + min;\n'
        '\n'
        '\tbool aboveBreakpoint = System::random(9) == 7;',
        'int ResourceSpawner::randomizeValue(int min, int max) {\n'
        '\t// High quality floor: always roll in top 15% of range\n'
        '\tint qualityFloor = min + (int)((max - min) * 0.85f);\n'
        '\tif (qualityFloor >= max) return max;\n'
        '\tint randomStat = System::random(max - qualityFloor) + qualityFloor;\n'
        '\n'
        '\tbool aboveBreakpoint = System::random(9) == 7;',
        "resource quality floor 85-100%"
    )
    if n:
        bak(RES_CPP)
        with open(RES_CPP, 'w') as f: f.write(src)
else:
    print(f"  WARN: {RES_CPP} not found")

print()

# ── 4-8. New faction pets ─────────────────────────────────────────────────────
print("=== 4-8. New faction pets ===")

PET_DIR = f"{root}/bin/scripts/object/custom_content/intangible/pet"
MOB_DIR = f"{root}/bin/scripts/mobile"
os.makedirs(PET_DIR, exist_ok=True)

# Pet PCD definitions: name, mobile_template, faction (rebel/imperial/both/neutral)
pets = [
    ("death_watch_wraith_pcd",        "death_watch_defender",      "both"),
    ("death_watch_overlord_pcd",      "death_watch_overlord",      "both"),
    ("death_watch_battle_droid_pcd",  "death_watch_battle_droid",  "both"),
    ("death_watch_herald_pcd",        "death_watch_warrior",       "both"),
    ("child_of_the_watch_pcd",        "death_watch_soldier",       "both"),
    ("black_sun_guard_pcd",           "black_sun_guard",           "both"),
    ("black_sun_henchman_pcd",        "black_sun_henchman",        "both"),
    ("black_sun_thug_pcd",            "black_sun_thug",            "both"),
    ("black_sun_assassin_pcd",        "black_sun_assassin",        "both"),
    ("nightsister_elder_pcd",         "nightsister_elder",         "neutral"),
    ("acklay_pcd",                    "acklay",                    "neutral"),
    ("dark_jedi_adept_pcd",           "dark_jedi_adept",           "both"),
    ("dark_jedi_knight_pcd",          "dark_jedi_knight",          "both"),
    ("dark_jedi_master_pcd",          "dark_jedi_master",          "both"),
]

pet_includes = []
created_pets = 0

for pcd_name, mobile_template, faction in pets:
    pcd_path = f"{PET_DIR}/{pcd_name}.lua"
    if os.path.isfile(pcd_path):
        continue

    # Write PCD file
    with open(pcd_path, 'w') as f:
        f.write(f'''-- {pcd_name}: custom faction pet
-- Available to: {faction}

{pcd_name} = {{
\tcustomName = "",
\tpetTemplate = "object/mobile/{mobile_template}.iff",
\tcontrolDeviceTemplate = "object/intangible/pet/{pcd_name}.iff",
\tgenerationLimit = 1,
\tpetLevel = 75,
\tpetSpecialAttack1 = "",
\tpetSpecialAttack2 = "",
}}

-- Register the PCD
if (PetManager ~= nil) then
\tPetManager:addPetTemplate("{pcd_name}", {pcd_name})
end
''')
    pet_includes.append(f'includeFile("custom_content/intangible/pet/{pcd_name}.lua")')
    created_pets += 1
    print(f"  Created pet: {pcd_name}")

print(f"  Total: {created_pets} new pet PCDs created")

# Add includes to pet serverobjects.lua
pet_so = f"{root}/bin/scripts/object/intangible/pet/serverobjects.lua"
if pet_includes and os.path.isfile(pet_so):
    with open(pet_so) as f: so = f.read()
    to_add = [l for l in pet_includes if l not in so]
    if to_add:
        bak(pet_so)
        with open(pet_so, 'a') as f:
            f.write("\n-- New faction pets (custom)\n")
            for l in to_add:
                f.write(l + "\n")
        print(f"  Added {len(to_add)} pet includes to serverobjects.lua")

print()

# ── 9. Advanced weapon components in elite Tusken + Naboo Mauler Lord ─────────
print("=== 9. Advanced weapon component drops ===")

# Mobile files to patch with weapon_component_advanced loot group
mob_patches = [
    (f"{MOB_DIR}/tatooine/tusken_elite_guard.lua",
     '{group = "tusken_common", chance = 3500000},',
     '{group = "tusken_common", chance = 3000000},\n\t\t\t\t{group = "weapon_component_advanced", chance = 500000},',
     "tusken_elite_guard: weapon_component_advanced"),

    (f"{MOB_DIR}/tatooine/tusken_torture_lord.lua",
     '{group = "tusken_common", chance =',
     '{group = "weapon_component_advanced", chance = 750000},\n\t\t\t\t{group = "tusken_common", chance =',
     "tusken_torture_lord: weapon_component_advanced"),

    (f"{MOB_DIR}/naboo/mauler_lord.lua",
     '{group = "mauler_common", chance = 2000000}',
     '{group = "mauler_common", chance = 2000000},\n\t\t\t\t{group = "weapon_component_advanced", chance = 1000000}',
     "naboo_mauler_lord: weapon_component_advanced"),
]

for fpath, old, new, label in mob_patches:
    if not os.path.isfile(fpath):
        print(f"  SKIP (not found): {fpath}")
        continue
    with open(fpath) as f: content = f.read()
    if "weapon_component_advanced" in content:
        print(f"  SKIP (already done): {label}")
        continue
    bak(fpath)
    content, n = patch(content, old, new, label)
    if n:
        with open(fpath, 'w') as f: f.write(content)

print()
print("=== Summary ===")
print("Lua changes (restart only): vibro motors, pets, weapon component drops")
print("C++ changes (recompile needed): powerup uses, resource quality floor")
print(f"\nAll backed up with suffix .improve_{ts}.bak")
