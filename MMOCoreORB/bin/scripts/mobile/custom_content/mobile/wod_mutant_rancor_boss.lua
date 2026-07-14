wod_mutant_rancor_boss = Creature:new {
	customName = "Borvo",
	socialGroup = "nightsister",
  faction = "nightsister",
	level = 400,
	chanceHit = 0.13,
	damageMin = 735,
	damageMax = 1182,
	baseXp = 62000,
	baseHAM = 200000,
	baseHAMmax = 200000,
	armor = 1,
	resists = {100,30,30,50,50,50,50,75,35},
	meatType = "",
	meatAmount = 0,
	hideType = "",
	hideAmount = 0,
	boneType = "",
	boneAmount = 0,
	milk = 0,
	tamingChance = 0,
	ferocity = 0,
	pvpBitmask = AGGRESSIVE + ATTACKABLE + ENEMY,
  creatureBitmask = PACK + KILLER,
  optionsBitmask = AIENABLED,
  diet = HERBIVORE,

	templates = {"object/mobile/wod_mutant_rancor_boss.iff"},
	lootGroups = {
		{
			groups = {
				{group = "rare_lewt_box_01", chance = 10000000}
			},
			lootChance = 5000000
		}
	},
	weapons = {},
	conversationTemplate = "",
	attacks = {
    {"creatureareableeding",""},
    {"creatureareacombo","stateAccuracyBonus=100"},
    {"creatureareaknockdown","stateAccuracyBonus=100"},
    {"strongpoison",""},
    {"dizzyattack",""}
  }
}

CreatureTemplates:addCreatureTemplate(wod_mutant_rancor_boss, "wod_mutant_rancor_boss")
