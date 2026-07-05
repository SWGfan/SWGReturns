tusken_king_guard = Creature:new {
	customName = "Tusken Kingsguard",
	socialGroup = "tusken_raider",
	faction = "tusken_raider",
	level = 300,
	chanceHit = 25.0,
	damageMin = 925,
	damageMax = 1325,
	baseXp = 25000,
	baseHAM = 95000,
	baseHAMmax = 105000,
	armor = 1,
	resists = {55,55,55,55,55,55,55,55,25},
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
	creatureBitmask = PACK + KILLER + STALKER,
	optionsBitmask = AIENABLED,
	diet = HERBIVORE,

	templates = {"object/mobile/tusken_raider.iff"},
	lootGroups = {
		{
			groups = {
				{group = "tusken_clothing_schematics", chance = 2500000},
				{group = "tusken_common", chance = 7500000},
			},
			lootChance = 1000000,
		},
		{
			groups = {
				{group = "token_stardust", chance = 10000000}
			},
				lootChance = 100000,
		},
	},
	
	primaryWeapon = "tusken_ranged",
	secondaryWeapon = "tusken_melee",
	conversationTemplate = "",
	
	primaryAttacks = merge(marksmanmaster,riflemanmaster),
	secondaryAttacks = merge(brawlermaster,fencermaster)
}

CreatureTemplates:addCreatureTemplate(tusken_king_guard, "tusken_king_guard")
