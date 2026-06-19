-- Coded by BoosterSteel 19-03-2026
-- Revan Grand Master
-- Phase 3 Final Trial for Light Jedi players (75% stronger than Phase 1)
-- Double tier loot drops

revan_grand_master = Creature:new {
	customName = "Revan - Grand Jedi Master",
	socialGroup = "dark_jedi",
	pvpFaction = "",
	faction = "",
	level = 525,
	chanceHit = 30.00,
	damageMin = 3150,
	damageMax = 5793,
	baseXp = 487358,
	baseHAM = 736750,
	baseHAMmax = 1036000,
	armor = 3,
	resists = {55,55,55,55,55,55,55,55,55},
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
	creatureBitmask = KILLER + STALKER,
	optionsBitmask = AIENABLED,
	diet = HERBIVORE,
	templates = {"object/mobile/som/blackguard_wilder.iff"},
	lootGroups = {
		{
			groups = {
				{group = "junk",        chance = 5000000},
				{group = "armor_all",   chance = 4500000},
				{group = "weapons_all", chance = 500000}
			},
			lootChance = 10000000
		},
		{
			groups = {
				{group = "wearables_all",     chance = 5000000},
				{group = "loot_kit_parts",    chance = 2500000},
				{group = "tailor_components", chance = 2500000}
			},
			lootChance = 10000000
		},
		-- Double tier drops for the final trial
		{
			groups = {
				{group = "tierone",     chance = 3500000},
				{group = "tiertwo",     chance = 3500000},
				{group = "tierthree",   chance = 1500000},
				{group = "tierdiamond", chance = 1500000},
			},
			lootChance = 10000000
		},
		{
			groups = {
				{group = "tierone",     chance = 3500000},
				{group = "tiertwo",     chance = 3500000},
				{group = "tierthree",   chance = 1500000},
				{group = "tierdiamond", chance = 1500000},
			},
			lootChance = 10000000
		},
	},
	weapons = {"dark_jedi_weapons_gen4"},
	reactionStf = "@npc_reaction/slang",
	attacks = merge(lightsabermaster)
}

CreatureTemplates:addCreatureTemplate(revan_grand_master, "revan_grand_master")
