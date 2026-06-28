force_crystal_hunter = Creature:new {
	objectName = "@mob/creature_names:dark_force_crystal_hunter",
	randomNameType = NAME_GENERIC,
	randomNameTag = true,
	socialGroup = "kun",
	faction = "",
	level = 115,
	chanceHit = 1,
	damageMin = 820,
	damageMax = 1350,
	baseXp = 10921,
	baseHAM = 24000,
	baseHAMmax = 30000,
	armor = 2,
	resists = {80,80,80,80,80,80,80,80,-1},
	meatType = "",
	meatAmount = 0,
	hideType = "",
	hideAmount = 0,
	boneType = "",
	boneAmount = 0,
	milk = 0,
	tamingChance = 0.0,
	ferocity = 0,
	pvpBitmask = AGGRESSIVE + ATTACKABLE + ENEMY,
	creatureBitmask = PACK + KILLER,
	optionsBitmask = AIENABLED,
	diet = HERBIVORE,

	templates = {"object/mobile/dressed_dark_force_crystal_hunter.iff"},
	lootGroups = {
		{
			groups = {
				{group = "cl55_backpack_loot", chance = 10000000},
			},
			lootChance = 250000
		},
		{
			groups = {
				{group = "junk", chance = 1000000},
				{group = "power_crystals", chance = 1100000},
				{group = "color_crystals", chance = 900000},
				{group = "holocron_dark", chance = 700000},
				{group = "holocron_light", chance = 700000},
				{group = "melee_weapons", chance = 1000000},
				{group = "armor_attachments", chance = 1200000},
				{group = "clothing_attachments", chance = 1200000},
				{group = "wearables_all", chance = 1200000},
				{group = "jedi_dark_robes", chance = 300000},
				{group = "jedi_gray_robes", chance = 200000},
				{group = "jedi_iconic_robes", chance = 200000},
				{group = "exceptional_chest_rewards", chance = 200000},
				{group = "legendary_chest_rewards", chance = 100000}
			}
		}
	},
	weapons = {"mixed_force_weapons"},
	conversationTemplate = "",
	attacks = merge(pikemanmaster,brawlermaster,fencermaster,swordsmanmaster,forcewielder)
}

CreatureTemplates:addCreatureTemplate(force_crystal_hunter, "force_crystal_hunter")
