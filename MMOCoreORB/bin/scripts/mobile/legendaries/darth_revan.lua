includeFile("shared/_boss_attacks.lua")
darth_revan = Creature:new {
  objectName = "Darth Revan",
  customName = "Darth Revan",
  socialGroup = "sith",
  faction = "sith",
  level = 90,
  chanceHit = 0.78,
  damageMin = 720,
  damageMax = 1050,
  baseXp = 22000,
  baseHAM = 62000,
  baseHAMmax = 72000,
  armor = 2,
  resists = {70,70,60,70,70,65,55,55,50},
  pvpBitmask = AGGRESSIVE + ATTACKABLE + ENEMY,
  creatureBitmask = KILLER + PACK,
  optionsBitmask = AIENABLED + INTERESTING,
  templates = {"object/mobile/dressed_dark_jedi_human_male_02.iff"},
  lootGroups = {},
  weapons = {"dark_jedi_weapons_gen4"},
  attacks = lightsabermaster
}
CreatureTemplates:addCreatureTemplate(darth_revan, "darth_revan")
