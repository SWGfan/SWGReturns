nightsister_vibroblade_initiate = Creature:new {
	objectName = "@mob/creature_names:nightsister_initiate",
	randomNameType = NAME_GENERIC,
	randomNameTag = true,
	mobType = MOB_NPC,
	socialGroup = "nightsister",
	faction = "nightsister",
	level = 65,
	chanceHit = 0.55,
	damageMin = 460,
	damageMax = 620,
	baseXp = 6100,
	baseHAM = 12000,
	baseHAMmax = 15000,
	armor = 1,
	resists = {20,20,20,80,80,80,80,80,-1},
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
	creatureBitmask = PACK + KILLER + STALKER,
	optionsBitmask = AIENABLED,
	diet = HERBIVORE,
	templates = {"object/mobile/dressed_dathomir_nightsister_initiate.iff"},
	lootGroups = {
		{
			groups = {
				{group = "power_crystals",      chance = 500000},
				{group = "color_crystals",       chance = 500000},
				{group = "nightsister_common",   chance = 2000000},
				{group = "armor_attachments",    chance = 250000},
				{group = "clothing_attachments", chance = 250000},
				{group = "melee_weapons",        chance = 3500000},
				{group = "wearables_common",     chance = 500000},
				{group = "tailor_components",    chance = 500000}
			}
		}
	},
	primaryWeapon = "general_sword",
	secondaryWeapon = "unarmed",
	conversationTemplate = "",
	primaryAttacks = merge(fencernovice,swordsmanmid,brawlermaster),
	secondaryAttacks = { }
}
CreatureTemplates:addCreatureTemplate(nightsister_vibroblade_initiate, "nightsister_vibroblade_initiate")
