wampa_alpha = Creature:new {
  objectName = "Wampa",
  socialGroup = "wampa",
  faction = "",
  level = 65,
  chanceHit = 0.58,
  damageMin = 560,
  damageMax = 820,
  baseXp = 11000,
  baseHAM = 38000,
  baseHAMmax = 44000,
  armor = 1,
  resists = {40,40,55,35,35,30,55,15,15},
  pvpBitmask = AGGRESSIVE + ATTACKABLE + ENEMY,
  creatureBitmask = PACK + STALKER,
  optionsBitmask = AIENABLED,
  templates = { "object/mobile/wampa.iff" },
  lootGroups = {},
  weapons = {},
  attacks = { {"knockdownattack",""} , {"stunattack",""} , {"posturedownattack",""} }
}
CreatureTemplates:addCreatureTemplate(wampa_alpha, "wampa_alpha")
