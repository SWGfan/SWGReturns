includeFile("shared/_boss_attacks.lua")
bastila_shan = Creature:new {
  objectName = "Bastila Shan",
  customName = "Bastila Shan",
  socialGroup = "rebel",
  faction = "rebel",
  level = 80,
  chanceHit = 0.7,
  damageMin = 600,
  damageMax = 900,
  baseXp = 18000,
  baseHAM = 52000,
  baseHAMmax = 60000,
  armor = 2,
  resists = {60,60,55,65,65,60,45,45,40},
  pvpBitmask = AGGRESSIVE + ATTACKABLE + ENEMY,
  creatureBitmask = KILLER,
  optionsBitmask = AIENABLED + INTERESTING,
  templates = {"object/mobile/dressed_jedi_trainer_human_female_01.iff"},
  lootGroups = {},
  weapons = {"light_jedi_weapons"},
  attacks = bossAttacks.saber_duelist
}
CreatureTemplates:addCreatureTemplate(bastila_shan, "bastila_shan")
