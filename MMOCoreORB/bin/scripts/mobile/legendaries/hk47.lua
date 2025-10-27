includeFile("shared/_boss_attacks.lua")
hk47 = Creature:new {
  objectName = "HK-47",
  customName = "HK-47",
  socialGroup = "bounty",
  faction = "",
  level = 80,
  chanceHit = 0.72,
  damageMin = 620,
  damageMax = 920,
  baseXp = 19000,
  baseHAM = 54000,
  baseHAMmax = 62000,
  armor = 2,
  resists = {55,55,75,45,45,45,75,40,40},
  pvpBitmask = AGGRESSIVE + ATTACKABLE + ENEMY,
  creatureBitmask = KILLER + PACK,
  optionsBitmask = AIENABLED + INTERESTING,
  templates = {"object/mobile/protocol_droid_red.iff"},
  lootGroups = {},
  weapons = {"droid_probot_weapons"},
  attacks = marksmanmaster
}
CreatureTemplates:addCreatureTemplate(hk47, "hk47")
