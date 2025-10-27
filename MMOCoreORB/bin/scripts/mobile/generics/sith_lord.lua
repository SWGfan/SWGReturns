sith_lord = Creature:new {
  objectName = "Sith Lord",
  socialGroup = "sith",
  faction = "sith",
  level = 70,
  chanceHit = 0.6,
  damageMin = 520,
  damageMax = 760,
  baseXp = 12000,
  baseHAM = 35000,
  baseHAMmax = 41000,
  armor = 2,
  resists = {55,55,40,60,60,55,40,40,35},
  pvpBitmask = AGGRESSIVE + ATTACKABLE + ENEMY,
  creatureBitmask = PACK + KILLER,
  optionsBitmask = AIENABLED,
  templates = {
    "object/mobile/dressed_dark_jedi_human_male_02.iff",
    "object/mobile/dressed_dark_jedi_human_female_02.iff"
  },
  lootGroups = {},
  weapons = {"dark_jedi_weapons_gen2"},
  attacks = lightsabermaster
}
CreatureTemplates:addCreatureTemplate(sith_lord, "sith_lord")
