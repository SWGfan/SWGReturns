#!/usr/bin/env python3

from pathlib import Path
import shutil

FILE = Path("src/server/zone/objects/player/sessions/EntertainingSessionImplementation.cpp")

if not FILE.exists():
    print("File not found:", FILE)
    raise SystemExit(1)

text = FILE.read_text(encoding="utf-8")
orig = text

bak = Path(str(FILE) + ".phase6b.bak")
if not bak.exists():
    shutil.copy2(FILE, bak)

# -------------------------------------------------
# Give entertainer buffs their own CRC
# -------------------------------------------------

text = text.replace(
    'STRING_HASHCODE("medical_enhance_action")',
    'STRING_HASHCODE("performance_inspiration")'
)

# -------------------------------------------------
# Music Buff
# -------------------------------------------------

music_old = """Locker locker(focusBuff);
                        creature->addBuff(focusBuff);"""

music_new = """Locker locker(focusBuff);

// ===== Returns Phase 6B =====

// Combat
focusBuff->setSkillModifier("combat_haste",10);
focusBuff->setSkillModifier("luck",20);

focusBuff->setSkillModifier("melee_defense",10);
focusBuff->setSkillModifier("ranged_defense",10);

focusBuff->setSkillModifier("dodge",8);
focusBuff->setSkillModifier("block",8);
focusBuff->setSkillModifier("counterattack",8);

// Accuracy
focusBuff->setSkillModifier("rifle_accuracy",10);
focusBuff->setSkillModifier("carbine_accuracy",10);
focusBuff->setSkillModifier("pistol_accuracy",10);

focusBuff->setSkillModifier("onehandmelee_accuracy",10);
focusBuff->setSkillModifier("twohandmelee_accuracy",10);
focusBuff->setSkillModifier("polearm_accuracy",10);

// Speed
focusBuff->setSkillModifier("rifle_speed",10);
focusBuff->setSkillModifier("carbine_speed",10);
focusBuff->setSkillModifier("pistol_speed",10);

focusBuff->setSkillModifier("onehandmelee_speed",10);
focusBuff->setSkillModifier("twohandmelee_speed",10);
focusBuff->setSkillModifier("polearm_speed",10);

// Jedi
focusBuff->setSkillModifier("jedi_force_power_max",250);
focusBuff->setSkillModifier("jedi_force_power_regen",15);
focusBuff->setSkillModifier("force_defense",15);
focusBuff->setSkillModifier("lightsaber_toughness",15);
focusBuff->setSkillModifier("jedi_toughness",15);

// Ranger
focusBuff->setSkillModifier("camouflage",20);
focusBuff->setSkillModifier("foraging",15);
focusBuff->setSkillModifier("creature_harvesting",20);

focusBuff->setSkillModifier("slope_move",20);
focusBuff->setSkillModifier("group_slope_move",20);
focusBuff->setSkillModifier("take_cover",20);

// Crafting
focusBuff->setSkillModifier("weapon_assembly",10);
focusBuff->setSkillModifier("weapon_experimentation",10);

focusBuff->setSkillModifier("armor_assembly",10);
focusBuff->setSkillModifier("armor_experimentation",10);

focusBuff->setSkillModifier("clothing_assembly",10);
focusBuff->setSkillModifier("clothing_experimentation",10);

focusBuff->setSkillModifier("general_experimentation",10);

// ===== End Returns Phase 6B =====

creature->addBuff(focusBuff);"""

if music_old in text:
    text = text.replace(music_old, music_new)

# -------------------------------------------------
# Dance Buff
# -------------------------------------------------

dance_old = """Locker locker(mindBuff);
                        creature->addBuff(mindBuff);"""

dance_new = music_new.replace("focusBuff", "mindBuff")

if dance_old in text:
    text = text.replace(dance_old, dance_new)

if text != orig:
    FILE.write_text(text, encoding="utf-8")
    print("Entertainer overhaul installed.")
    print("Backup:", bak)
else:
    print("Nothing changed (already patched?)")
