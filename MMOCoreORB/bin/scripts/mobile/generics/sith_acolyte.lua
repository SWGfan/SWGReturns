sith_acolyte = Creature:new {
  objectName = "Sith Acolyte",
  socialGroup = "sith",
  faction = "sith",
  level = 55,
  chanceHit = 0.48,
  damageMin = 380,
  damageMax = 560,
  baseXp = 8200,
  baseHAM = 24000,
  baseHAMmax = 29000,
  armor = 1,
  resists = {35,35,20,45,45,35,25,25,20},
  pvpBitmask = AGGRESSIVE + ATTACKABLE + ENEMY,
  creatureBitmask = PACK + KILLER,
  optionsBitmask = AIENABLED,
  diet = HERBIVORE,
  templates = {
    "object/mobile/dressed_dark_jedi_human_male_01.iff",
    "object/mobile/dressed_dark_jedi_human_female_01.iff"
  },
  lootGroups = {},
  weapons = {"dark_jedi_weapons_gen2"},
  conversationTemplate = "",
  attacks = lightsabernovice
}
CreatureTemplates:addCreatureTemplate(sith_acolyte, "sith_acolyte")
