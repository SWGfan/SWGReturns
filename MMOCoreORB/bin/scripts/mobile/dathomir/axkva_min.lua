axkva_min = Creature:new {
	objectName = "@mob/creature_names:axkva_min",
	socialGroup = "nightsister",
	faction = "nightsister",
	mobType = MOB_NPC,
	level = 400,
	chanceHit = 25.0,
	damageMin = 6000,
	damageMax = 12000,
	specialDamageMult = 25.0,
	baseXp = 50000,
	baseHAM = 15000000,
	baseHAMmax = 20000000,
	armor = 3,
	resists = {150,150,150,150,150,150,150,150,150},
	meatType = "",
	meatAmount = 0,
	hideType = "",
	hideAmount = 0,
	boneType = "",
	boneAmount = 0,
	milk = 0,
	tamingChance = 0,
	ferocity = 15,
	pvpBitmask = AGGRESSIVE + ATTACKABLE + ENEMY,
	creatureBitmask = PACK + KILLER + HEALER,
	optionsBitmask = AIENABLED,
	diet = HERBIVORE,

	templates = {"object/mobile/dressed_dathomir_nightsister_axkva.iff"},
	lootGroups = {
		{
			groups = {
				{group = "power_crystals", chance = 1800000},
				{group = "nightsister_common", chance = 5200000},
				{group = "pistols", chance = 2000000},
				{group = "rifles", chance = 2000000},
				{group = "carbines", chance = 2000000},
				{group = "melee_weapons", chance = 4000000},
				{group = "armor_attachments", chance = 1000000},
				{group = "clothing_attachments", chance = 1000000},
				{group = "wearables_rare", chance = 1000000}
			}
		},
		{
			groups = {
				{group = "axkva_min", chance = 10000000},
			},
			lootChance = 10000000
		},
		{
			groups = {
				{group = "rare_lewt_box_01", chance = 10000000},
			},
			lootChance = 7000000,
		}
	},

	-- Primary and secondary weapon should be different types (rifle/carbine, carbine/pistol, rifle/unarmed, etc)
	-- Unarmed should be put on secondary unless the mobile doesn't use weapons, in which case "unarmed" should be put primary and "none" as secondary
	primaryWeapon = "force_sword",
	secondaryWeapon = "unarmed",
	conversationTemplate = "",

	-- primaryAttacks and secondaryAttacks should be separate skill groups specific to the weapon type listed in primaryWeapon and secondaryWeapon
	-- Use merge() to merge groups in creatureskills.lua together. If a weapon is set to "none", set the attacks variable to empty brackets
	primaryAttacks = merge(fencermaster,swordsmanmaster,pikemanmaster,brawlermaster,forcepowermaster),
	secondaryAttacks = forcepowermaster
}

CreatureTemplates:addCreatureTemplate(axkva_min, "axkva_min")
