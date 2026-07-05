luke_skywalker_elysium = Creature:new {
	objectName = "@mob/creature_names:luke_skywalker",
	socialGroup = "rebel",
	faction = "rebel",
	mobType = MOB_NPC,
	level = 100,
	chanceHit = 1,
	damageMin = 1,
	damageMax = 1,
	baseXp = 9429,
	baseHAM = 385000,
  baseHAMmax = 471000,
	armor = 1,
	resists = {0,0,0,0,0,0,0,0,-1},
	meatType = "",
	meatAmount = 0,
	hideType = "",
	hideAmount = 0,
	boneType = "",
	boneAmount = 0,
	milk = 0,
	tamingChance = 0,
	ferocity = 0,
	lightsaberColor = 2,
	pvpBitmask = ATTACKABLE + AGGRESSIVE,
  creatureBitmask = PACK + KILLER,
  diet = HERBIVORE,
  optionsBitmask = AIENABLED,
  

	templates = {"object/mobile/dressed_luke_skywalker.iff"},
	lootGroups = {},

	-- Primary and secondary weapon should be different types (rifle/carbine, carbine/pistol, rifle/unarmed, etc)
	-- Unarmed should be put on secondary unless the mobile doesn't use weapons, in which case "unarmed" should be put primary and "none" as secondary
	primaryWeapon = "luke_skywalker_weapons",
	secondaryWeapon = "unarmed",
	conversationTemplate = "theme_park_rebel_mission_giver_convotemplate",
	outfit = "luke_outfit",

	-- primaryAttacks and secondaryAttacks should be separate skill groups specific to the weapon type listed in primaryWeapon and secondaryWeapon
	-- Use merge() to merge groups in creatureskills.lua together. If a weapon is set to "none", set the attacks variable to empty brackets
	primaryAttacks = merge(lightsabermaster),
	secondaryAttacks = { }
}

CreatureTemplates:addCreatureTemplate(luke_skywalker_elysium, "luke_skywalker_elysium")
