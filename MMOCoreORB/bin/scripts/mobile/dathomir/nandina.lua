nandina = Creature:new {
	objectName = "@mob/creature_names:nightsister_rancor_tamer",
	customName = "Nandina",
	socialGroup = "nightsister",
	faction = "nightsister",
	level = 400,
	chanceHit = 20.0,
	damageMin = 3000,
	damageMax = 6000,
	specialDamageMult = 15.0,
	baseXp = 7299,
	baseHAM = 3000000,
	baseHAMmax = 3000000,
	armor = 300,
	-- {kinetic,energy,blast,heat,cold,electricity,acid,stun,ls}
	resists = {150,150,150,150,150,150,150,150,150},
	meatType = "",
	meatAmount = 0,
	hideType = "",
	hideAmount = 0,
	boneType = "",
	boneAmount = 0,
	milk = 2 * 0,
	tamingChance = 0,
	ferocity = 0,
	pvpBitmask = AGGRESSIVE + ATTACKABLE + ENEMY,
	creatureBitmask = PACK + KILLER,
	optionsBitmask = AIENABLED,
	diet = HERBIVORE,

	templates = {"object/mobile/dressed_dathomir_nightsister_rancor_tamer.iff"},
	lootGroups = {},
	weapons = {"mixed_force_weapons"},
	conversationTemplate = "",
	attacks = merge(swordsmanmid,fencermid,tkamid,pikemanmid,brawlermaster,forcewielder)
}

CreatureTemplates:addCreatureTemplate(nandina, "nandina")
