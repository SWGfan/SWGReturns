vitiate = Creature:new {
	objectName = "@theme_park_name:emperor_palpatine",
	customName = "Vitiate",
	socialGroup = "dark_jedi",
	mobType = MOB_NPC,
	level = 450,
	chanceHit = 1.0,
	damageMin = 2500,
	damageMax = 3200,
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
	ferocity = 0,
	pvpBitmask = AGGRESSIVE + ATTACKABLE + ENEMY,
	creatureBitmask = KILLER + STALKER,
	optionsBitmask = AIENABLED,
	diet = HERBIVORE,
	scale = 1.2,

	-- Use dark jedi male model (mara_jade was female, caused black HAM bars)
	templates = {"object/mobile/dressed_dark_jedi_human_male_01.iff"},
	
	lootGroups = {
		{
			groups = {
				{group = "holocron_dark", chance = 1000000},        
				{group = "named_crystals", chance = 1000000},
				{group = "massassi_sith_weapons", chance = 2000000},
				{group = "armor_attachments", chance = 3000000},
				{group = "clothing_attachments", chance = 3000000},
				{group = "kaas_weapons", chance = 2000000},
				{group = "dark_jedi_common", chance = 1000000},
				{group = "rare_lewt_box_01", chance = 1000000},
				{group = "heroic_pistols", chance = 500000},
				{group = "heroic_melee", chance = 500000}
			},
			lootChance = 10000000,
		}
	},
	 -- Primary and secondary weapon should be different types (rifle/carbine, carbine/pistol, rifle/unarmed, etc)
   -- Unarmed should be put on secondary unless the mobile doesn't use weapons, in which case "unarmed" should be put primary and "none" as secondary
   primaryWeapon = "dark_jedi_weapons_gen4",
   secondaryWeapon = "unarmed",
   conversationTemplate = "",

   -- primaryAttacks and secondaryAttacks should be separate skill groups specific to the weapon type listed in primaryWeapon and secondaryWeapon
   -- Use merge() to merge groups in creatureskills.lua together. If a weapon is set to "none", set the attacks variable to empty brackets
   primaryAttacks = lightsabermaster,
   secondaryAttacks = forcepowermaster,
   
   -- Damage multiplier for proper boss scaling
   specialDamageMult = 2.0
}

CreatureTemplates:addCreatureTemplate(vitiate, "vitiate")