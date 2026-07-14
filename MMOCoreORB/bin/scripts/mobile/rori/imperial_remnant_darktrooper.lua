imperial_remnant_darktrooper = Creature:new {
	customName = "an Imperial Remnant Darktrooper",
	socialGroup = "mercenary",
	faction = "",
	level = 300,
	chanceHit = 14.0,
	damageMin = 486,
	damageMax = 696,
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
	scale = 1.3,

	templates = {"object/mobile/dressed_stormtrooper_black_black.iff"},
	lootGroups = {
	},
	
	primaryWeapon = "stormtrooper_rifle",
	secondaryWeapon = "stormtrooper_polearm",
	conversationTemplate = "",
	
	primaryAttacks = merge(marksmanmaster,riflemanmaster),
	secondaryAttacks = merge(brawlermaster,pikemanmaster)
}

CreatureTemplates:addCreatureTemplate(imperial_remnant_darktrooper, "imperial_remnant_darktrooper")
