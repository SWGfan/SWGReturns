dark_jedi_knight = Creature:new {
	objectName = "@mob/creature_names:dark_jedi_knight",
	randomNameType = NAME_GENERIC,
	randomNameTag = true,
	mobType = MOB_NPC,
	socialGroup = "sith_shadow",
	faction = "sith_shadow",
	level = 400,
	chanceHit = 1.0,
	damageMin = 2000,
	damageMax = 2800,
	baseXp = 50000,
	baseHAM = 1200000,
	baseHAMmax = 1500000,
	armor = 3,
	resists = {130,130,130,130,130,130,130,130,100},
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
	creatureBitmask = KILLER + STALKER,
	optionsBitmask = AIENABLED,
	diet = HERBIVORE,

	templates = { "dark_jedi" },
	lootGroups = {
		{
			groups = {
				{group = "holocron_dark", chance = 300000},
				{group = "holocron_light", chance = 300000},
				{group = "power_crystals", chance = 1200000},
				{group = "ranged_weapons", chance = 1300000},
				{group = "melee_weapons", chance = 1300000},
				{group = "armor_attachments", chance = 2400000},
				{group = "clothing_attachments", chance = 2400000},
				{group = "dark_jedi_common", chance = 800000}
			},
				lootChance = 10000000,
		},
		{
			groups = {
				{group = "holocron_dark", chance = 300000},
				{group = "holocron_light", chance = 300000},
				{group = "power_crystals", chance = 1200000},
				{group = "ranged_weapons", chance = 1300000},
				{group = "melee_weapons", chance = 1300000},
				{group = "armor_attachments", chance = 2400000},
				{group = "clothing_attachments", chance = 2400000},
				{group = "dark_jedi_common", chance = 800000}
			},
				lootChance = 10000000,
		},
		{
			groups = {
				{group = "holocron_dark", chance = 300000},
				{group = "holocron_light", chance = 300000},
				{group = "power_crystals", chance = 1200000},
				{group = "ranged_weapons", chance = 1300000},
				{group = "melee_weapons", chance = 1300000},
				{group = "armor_attachments", chance = 2400000},
				{group = "clothing_attachments", chance = 2400000},
				{group = "dark_jedi_common", chance = 800000}
			},
				lootChance = 5500000,
		},
	},

	primaryWeapon = "dark_jedi_weapons_gen4",
	secondaryWeapon = "unarmed",
	conversationTemplate = "",

	primaryAttacks = lightsabermaster,
	secondaryAttacks = forcepowermaster,
	
	specialDamageMult = 2.0
}

CreatureTemplates:addCreatureTemplate(dark_jedi_knight, "dark_jedi_knight")