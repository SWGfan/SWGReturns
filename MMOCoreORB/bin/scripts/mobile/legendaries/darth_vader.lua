includeFile("shared/_boss_attacks.lua")
darth_vader = Creature:new {
  objectName = "Darth Vader",
  customName = "Darth Vader",
  socialGroup = "imperial",
  faction = "imperial",
  level = 90,
  chanceHit = 0.8,
  damageMin = 760,
  damageMax = 1100,
  baseXp = 24000,
  baseHAM = 66000,
  baseHAMmax = 78000,
  armor = 2,
  resists = {70,70,60,75,75,70,55,55,50},
  pvpBitmask = AGGRESSIVE + ATTACKABLE + ENEMY,
  creatureBitmask = KILLER,
  optionsBitmask = AIENABLED + INTERESTING,
  templates = { "object/mobile/dressed_dark_jedi_human_male_01.iff" },
  lootGroups = {},
  weapons = {"dark_jedi_weapons_gen4"},
  attacks = lightsabermaster
}
CreatureTemplates:addCreatureTemplate(darth_vader_custom, "darth_vader_custom")
