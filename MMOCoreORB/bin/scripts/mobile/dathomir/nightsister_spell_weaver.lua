nightsister_spell_weaver = Creature:new {
	objectName = "@mob/creature_names:nightsister_spell_weaver",
	randomNameType = NAME_GENERIC,
	randomNameTag = true,
	mobType = MOB_NPC,
	socialGroup = "nightsister",
	faction = "nightsister",
	level = 250,
	chanceHit = 4.0,
	damageMin = 900,
	damageMax = 1400,
	baseXp = 25000,
	baseHAM = 60000,
	baseHAMmax = 72000,
	armor = 4,
	resists = {50,90,50,85,85,85,85,85,45},
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

	templates = {"object/mobile/dressed_dathomir_nightsister_spellweaver.iff"},
	lootGroups = {
		{
			groups = {
				{group = "nightsister_rare", chance = 2500000},
				{group = "nightsister_common", chance = 2500000},
				{group = "power_crystals", chance = 2500000},
				{group = "melee_weapons", chance = 2500000},
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

CreatureTemplates:addCreatureTemplate(nightsister_spell_weaver, "nightsister_spell_weaver")
