includeFile("shared/_boss_attacks.lua")
sith_boss = Creature:new {
  objectName = "Sith Champion",
  socialGroup = "sith",
  faction = "sith",
  level = 85,
  chanceHit = 0.72,
  damageMin = 680,
  damageMax = 980,
  baseXp = 18000,
  baseHAM = 52000,
  baseHAMmax = 62000,
  armor = 2,
  resists = {65,65,55,70,70,65,50,50,45},
  pvpBitmask = AGGRESSIVE + ATTACKABLE + ENEMY,
  creatureBitmask = KILLER,
  optionsBitmask = AIENABLED + INTERESTING,
  templates = {
    "object/mobile/dressed_dark_jedi_zabrak_male_01.iff",
    "object/mobile/dressed_dark_jedi_zabrak_female_01.iff"
  },
  lootGroups = {},
  weapons = {"dark_jedi_weapons_gen4"},
  attacks = lightsabermaster
}
CreatureTemplates:addCreatureTemplate(sith_boss, "sith_boss")
