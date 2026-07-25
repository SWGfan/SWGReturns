dark_adept = Creature:new {
	objectName = "@mob/creature_names:dark_adept",
	randomNameType = NAME_GENERIC,
	randomNameTag = true,
	mobType = MOB_NPC,
	socialGroup = "sith_shadow",
	faction = "sith_shadow",
	level = 360,
	chanceHit = 1.0,
	damageMin = 1800,
	damageMax = 2500,
	baseXp = 40000,
	baseHAM = 800000,
	baseHAMmax = 1000000,
	armor = 2,
	resists = {120,120,120,120,120,120,120,120,90},
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
				{group = "holocron_dark", chance = 400000},
				{group = "holocron_light", chance = 400000},
				{group = "power_crystals", chance = 400000},
				{group = "color_crystals", chance = 1000000},
				{group = "rifles", chance = 1300000},
				{group = "pistols", chance = 1300000},
				{group = "melee_weapons", chance = 1300000},
				{group = "armor_attachments", chance = 900000},
				{group = "clothing_attachments", chance = 900000},
				{group = "carbines", chance = 1300000},
				{group = "wearables_rare", chance = 800000}
			}
		}
	},

	primaryWeapon = "dark_jedi_weapons_gen4",
	secondaryWeapon = "unarmed",
	conversationTemplate = "",

	primaryAttacks = lightsabermaster,
	secondaryAttacks = forcepowermaster,
	
	specialDamageMult = 1.5
}

CreatureTemplates:addCreatureTemplate(dark_adept, "dark_adept")