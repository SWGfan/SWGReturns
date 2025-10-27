includeFile("shared/_boss_attacks.lua")
darth_malak = Creature:new {
  objectName = "Darth Malak",
  customName = "Darth Malak",
  socialGroup = "sith",
  faction = "sith",
  level = 88,
  chanceHit = 0.76,
  damageMin = 700,
  damageMax = 1020,
  baseXp = 21000,
  baseHAM = 60000,
  baseHAMmax = 70000,
  armor = 2,
  resists = {68,68,58,70,70,65,55,55,50},
  pvpBitmask = AGGRESSIVE + ATTACKABLE + ENEMY,
  creatureBitmask = KILLER,
  optionsBitmask = AIENABLED + INTERESTING,
  templates = {"object/mobile/dressed_dark_jedi_human_male_01.iff"},
  lootGroups = {},
  weapons = {"dark_jedi_weapons_gen4"},
  attacks = lightsabermaster
}
CreatureTemplates:addCreatureTemplate(darth_malak, "darth_malak")
