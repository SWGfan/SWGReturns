lelli_hi = Creature:new {
	objectName = "@mob/creature_names:nightsister_elder",
	customName = "Lelli Hi",
	socialGroup = "nightsister",
	faction = "nightsister",
	level = 400,
	chanceHit = 22.0,
	damageMin = 3000,
	damageMax = 6000,
	specialDamageMult = 15.0,
	baseXp = 26654,
	baseHAM = 3000000,
	baseHAMmax = 3000000,
	armor = 300,
	-- {kinetic,energy,blast,heat,cold,electricity,acid,stun,ls}
	resists = {90,90,90,100,80,100,100,100,75},
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

	templates = {"object/mobile/dressed_dathomir_nightsister_elder.iff"},
	lootGroups = {},
	weapons = {"mixed_force_weapons"},
	conversationTemplate = "",
	attacks = merge(tkamaster,swordsmanmaster,fencermaster,pikemanmaster,brawlermaster,forcepowermaster)
}

CreatureTemplates:addCreatureTemplate(lelli_hi, "lelli_hi")
