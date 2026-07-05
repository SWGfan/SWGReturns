event_animal = Creature:new {
	customName = "A Huurton Lunatik (Event)",
	mobType = MOB_CARNIVORE,
	faction = "kun",
	level = 350,
	chanceHit = 27.25,
	damageMin = 1120,
	damageMax = 1350,
	baseXp = 26654,
	baseHAM = 221000,
	baseHAMmax = 292000,
	armor = 1,
	resists = {45,45,45,45,45,45,45,45,60},
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
	creatureBitmask = PACK + KILLER + HEALER,
	optionsBitmask = AIENABLED,
	diet = HERBIVORE,

	templates = {"object/mobile/huurton_hue.iff"},
	lootGroups = {
		{
			groups = {
				{group = "power_crystals", chance = 800000},
				{group = "nightsister_rare", chance = 2200000},
				{group = "junk", chance = 1700000},
				{group = "armor_attachments", chance = 1600000},
				{group = "clothing_attachments", chance = 1600000},
				{group = "ranged_weapons", chance = 800000},
				{group = "melee_weapons", chance = 800000},
				{group = "wearables_scarce", chance = 500000}
			},
				lootChance = 10000000,
		},
		{
			groups = {
				{group = "junk", chance = 2500000},
				{group = "armor_attachments", chance = 2000000},
				{group = "clothing_attachments", chance = 2000000},
				{group = "pistols", chance = 750000},
				{group = "rifles", chance = 750000},
				{group = "carbines", chance = 750000},
				{group = "bounty_hunter_armor", chance = 200000},
				{group = "jetpack_base", chance = 50000},
				{group = "wearables_common", chance = 500000},
				{group = "wearables_uncommon", chance = 500000}
			},
				lootChance = 10000000,
		},
	},

	primaryWeapon = "unarmed",
	secondaryWeapon = "none",
	conversationTemplate = "",
	
	primaryAttacks = { {"creatureareaattack","stateAccuracyBonus=50"}, {"blindattack","stateAccuracyBonus=100"}, {"posturedownattack","stateAccuracyBonus=50"}, {"creatureareacombo","stateAccuracyBonus=75"} },
	secondaryAttacks = { }
}

CreatureTemplates:addCreatureTemplate(event_animal, "event_animal")
