light_jedi_master = Creature:new {
	customName = "a Jedi Master",
	randomNameType = NAME_GENERIC,
  randomNameTag = true,
	socialGroup = "rebel",
	faction = "rebel",
	level = 350,
	chanceHit = 15.26,
	damageMin = 945,
	damageMax = 1737,
	baseXp = 27849,
	baseHAM = 321000,
	baseHAMmax = 392000,
	armor = 2,
	-- {kinetic, energy, electric, stun, blast, heat, cold, acid, ls}
	resists = {90,90,90,90,90,90,90,90,40},
	meatType = "",
	meatAmount = 0,
	hideType = "",
	hideAmount = 0,
	boneType = "",
	boneAmount = 0,
	milk = 2 * 0,
	tamingChance = 0,
	ferocity = 0,
	pvpBitmask = ATTACKABLE,
	creatureBitmask = KILLER + STALKER,
	optionsBitmask = AIENABLED,
	diet = HERBIVORE,
	lightsaberColor = 1,

	templates = {
		"object/mobile/dressed_jedi_trainer_old_human_male_01.iff",
		"object/mobile/dressed_jedi_trainer_chiss_male_01.iff",
		"object/mobile/dressed_jedi_trainer_nikto_male_01.iff",
		"object/mobile/dressed_jedi_trainer_twilek_female_01.iff",
    	},

	lootGroups = {
		{
			groups = {
				{group = "holocron_light", chance = 300000},
				{group = "holocron_dark", chance = 300000},
				{group = "power_crystals", chance = 1200000},
				{group = "ranged_weapons", chance = 1300000},
				{group = "melee_weapons", chance = 1300000},
				{group = "armor_attachments", chance = 2400000},
				{group = "clothing_attachments", chance = 2400000},
				{group = "lightjedifrs", chance = 800000}
			},
				lootChance = 10000000,
		},
		{
			groups = {
				{group = "jedi_jewelry", chance = 5000000},
				{group = "clothing_attachments", chance = 5000000}
			},
				lootChance = 2500000,
		},
	},

	-- Primary and secondary weapon should be different types (rifle/carbine, carbine/pistol, rifle/unarmed, etc)
  -- Unarmed should be put on secondary unless the mobile doesn't use weapons, in which case "unarmed" should be put primary and "none" as secondary
  primaryWeapon = "light_jedi_weapons_gen4",
  secondaryWeapon = "unarmed",
  conversationTemplate = "",

  -- primaryAttacks and secondaryAttacks should be separate skill groups specific to the weapon type listed in primaryWeapon and secondaryWeapon
  -- Use merge() to merge groups in creatureskills.lua together. If a weapon is set to "none", set the attacks variable to empty brackets
  primaryAttacks = lightsabermaster,
  secondaryAttacks = forcepowermaster
}

CreatureTemplates:addCreatureTemplate(light_jedi_master, "light_jedi_master")
