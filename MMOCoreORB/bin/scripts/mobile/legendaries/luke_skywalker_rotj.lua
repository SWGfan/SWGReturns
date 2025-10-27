includeFile("shared/_boss_attacks.lua")
luke_skywalker_rotj = Creature:new {
  objectName = "Luke Skywalker",
  customName = "Luke Skywalker",
  socialGroup = "rebel",
  faction = "rebel",
  level = 85,
  chanceHit = 0.75,
  damageMin = 700,
  damageMax = 1000,
  baseXp = 22000,
  baseHAM = 60000,
  baseHAMmax = 70000,
  armor = 2,
  resists = {65,65,55,70,70,65,50,50,45},
  pvpBitmask = AGGRESSIVE + ATTACKABLE + ENEMY,
  creatureBitmask = KILLER,
  optionsBitmask = AIENABLED + INTERESTING,
  templates = {"object/mobile/dressed_jedi_trainer_human_male_01.iff"},
  lootGroups = {},
  weapons = {"light_jedi_weapons"},
  attacks = lightsabermaster
}
CreatureTemplates:addCreatureTemplate(luke_skywalker_rotj, "luke_skywalker_rotj")
