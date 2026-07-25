sith_shadow_thug = Creature:new {
	objectName = "@mob/creature_names:shadow_thug",
	socialGroup = "sith_shadow",
	faction = "sith_shadow",
	mobType = MOB_NPC,
	level = 250,
	chanceHit = 0.9,
	damageMin = 1200,
	damageMax = 1800,
	baseXp = 25000,
	baseHAM = 1200000,
	baseHAMmax = 1500000,
	armor = 2,
	resists = {120,120,120,120,120,120,120,120,100},
	meatType = "",
	meatAmount = 0,
	hideType = "",
	hideAmount = 0,
	boneType = "",
	boneAmount = 0,
	milk = 0,
	tamingChance = 0,
	ferocity = 0,
	pvpBitmask = ATTACKABLE + ENEMY + AGGRESSIVE,
	creatureBitmask = KILLER,
	optionsBitmask = AIENABLED,
	diet = HERBIVORE,

	templates = { "sith_shadow" },
	lootGroups = {
		{
			groups = {
				{group = "junk", chance = 4000000},
				{group = "tailor_components", chance = 1500000},
				{group = "loot_kit_parts", chance = 1500000},
				{group = "printer_parts", chance = 1000000},
				{group = "wearables_common", chance = 1000000},
				{group = "clothing_attachments", chance = 500000},
				{group = "armor_attachments", chance = 500000},
			},
		},
		{
			groups = {
				{group = "village_resources", chance =  10000000}
			},
			lootChance = 7500000
		},
	},

	primaryWeapon = "dark_jedi_weapons_gen4",
	secondaryWeapon = "unarmed",
	thrownWeapon = "thrown_weapons",

	conversationTemplate = "",

	primaryAttacks = merge(riflemanmaster,pistoleermaster,carbineermaster,marksmanmaster,brawlermaster),
	secondaryAttacks = { },
	
	specialDamageMult = 1.5
}

CreatureTemplates:addCreatureTemplate(sith_shadow_thug, "sith_shadow_thug")