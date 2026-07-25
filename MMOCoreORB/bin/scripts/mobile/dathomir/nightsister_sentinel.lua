nightsister_sentinel = Creature:new {
	objectName = "@mob/creature_names:nightsister_sentinal",
	randomNameType = NAME_GENERIC,
	randomNameTag = true,
	mobType = MOB_NPC,
	socialGroup = "nightsister",
	faction = "nightsister",
	level = 250,
	chanceHit = 3.5,
	damageMin = 800,
	damageMax = 1200,
	baseXp = 22000,
	baseHAM = 55000,
	baseHAMmax = 65000,
	armor = 3,
	resists = {60,60,60,80,80,80,80,80,40},
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

	templates = {"object/mobile/dressed_dathomir_nightsister_sentinal.iff"},
	lootGroups = {
		{
			groups = {
				{group = "nightsister_common", chance = 3000000},
				{group = "melee_weapons", chance = 3000000},
				{group = "power_crystals", chance = 2000000},
				{group = "clothing_attachments", chance = 1000000},
				{group = "armor_attachments", chance = 1000000},
			},
			lootChance = 4000000,
		},
	},

	primaryWeapon = "force_sword",
	secondaryWeapon = "unarmed",
	conversationTemplate = "",

	primaryAttacks = merge(fencermid,swordsmanmid,pikemanmaster,brawlermaster,forcewielder),
	secondaryAttacks = forcewielder
}

CreatureTemplates:addCreatureTemplate(nightsister_sentinel, "nightsister_sentinel")
