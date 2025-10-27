includeFile("shared/_boss_attacks.lua")
emperor_palpatine = Creature:new {
  objectName = "Emperor Palpatine",
  customName = "Darth Sidious",
  socialGroup = "imperial",
  faction = "imperial",
  level = 90,
  chanceHit = 0.8,
  damageMin = 740,
  damageMax = 1080,
  baseXp = 24000,
  baseHAM = 64000,
  baseHAMmax = 76000,
  armor = 2,
  resists = {70,70,60,75,75,70,55,55,50},
  pvpBitmask = AGGRESSIVE + ATTACKABLE + ENEMY,
  creatureBitmask = KILLER,
  optionsBitmask = AIENABLED + INTERESTING,
  templates = {"object/mobile/dressed_dark_jedi_human_male_02.iff"},
  lootGroups = {},
  weapons = {"dark_jedi_weapons_gen4"},
  attacks = forcepowermaster
}
CreatureTemplates:addCreatureTemplate(emperor_palpatine, "emperor_palpatine")
