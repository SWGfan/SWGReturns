event_death_watch_ghost = Creature:new {
	customName = "A Death Watch Ghost (Event)",
	mobType = MOB_NPC,
	socialGroup = "kun",
	faction = "",
	level = 350,
	chanceHit = 8.5,
	damageMin = 895,
	damageMax = 1500,
	baseXp = 14314,
	baseHAM = 310000,
	baseHAMmax = 350000,
	armor = 1,
	resists = {65,65,70,60,35,35,100,50,-1},
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
	creatureBitmask = KILLER,
	optionsBitmask = AIENABLED,
	diet = HERBIVORE,
	scale = 1.15,

	templates = {"object/mobile/dressed_death_watch_grey.iff"},
	lootGroups = {
		{
			groups = {
		{group = "death_watch_bunker_commoners",   chance = 6300000},
        {group = "death_watch_bunker_lieutenants", chance = 3400000},
        {group = "death_watch_bunker_ingredient_protective",  chance = 100000},
        {group = "dwb_weapons",  chance = 100000},
        {group = "death_watch_bunker_ingredient_binary",  chance = 100000}
			},
			lootChance = 10000000
		},
		{
			groups = {
		{group = "death_watch_bunker_commoners",   chance = 6300000},
        {group = "death_watch_bunker_lieutenants", chance = 3400000},
        {group = "death_watch_bunker_ingredient_protective",  chance = 100000},
        {group = "dwb_weapons",  chance = 100000},
        {group = "death_watch_bunker_ingredient_binary",  chance = 100000}
			},
			lootChance = 10000000
		},
		{
			groups = {
		{group = "death_watch_bunker_commoners",   chance = 6300000},
        {group = "death_watch_bunker_lieutenants", chance = 3400000},
        {group = "death_watch_bunker_ingredient_protective",  chance = 100000},
        {group = "dwb_weapons",  chance = 100000},
        {group = "death_watch_bunker_ingredient_binary",  chance = 100000}
			},
			lootChance = 10000000
		},
	},

	-- Primary and secondary weapon should be different types (rifle/carbine, carbine/pistol, rifle/unarmed, etc)
	-- Unarmed should be put on secondary unless the mobile doesn't use weapons, in which case "unarmed" should be put primary and "none" as secondary
	primaryWeapon = "pirate_weapons_heavy",
	secondaryWeapon = "unarmed",
	conversationTemplate = "",
	
	-- primaryAttacks and secondaryAttacks should be separate skill groups specific to the weapon type listed in primaryWeapon and secondaryWeapon
	-- Use merge() to merge groups in creatureskills.lua together. If a weapon is set to "none", set the attacks variable to empty brackets
	primaryAttacks = merge(bountyhuntermaster,marksmanmaster,brawlermaster,pikemanmaster,fencermaster,swordsmanmaster),
	secondaryAttacks = { }
}

CreatureTemplates:addCreatureTemplate(event_death_watch_ghost, "event_death_watch_ghost")
