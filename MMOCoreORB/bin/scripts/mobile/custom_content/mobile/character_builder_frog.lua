character_builder_frog = Creature:new {
	objectName = "",
	randomNameType = NAME_NONE,
	randomNameTag = false,
	customName = "Starter Gear Droid",
	socialGroup = "townsperson",
	faction = "",
	mobType = MOB_NPC,
	level = 1,
	chanceHit = 0,
	damageMin = 0,
	damageMax = 0,
	baseXp = 0,
	baseHAM = 9999,
	baseHAMmax = 9999,
	armor = 0,
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
	pvpBitmask = NONE,
	creatureBitmask = NONE,
	optionsBitmask = CONVERSABLE,
	diet = HERBIVORE,

	templates = {"object/mobile/3po_protocol_droid.iff"},
	lootGroups = {},

	-- Primary and secondary weapon should be different types (rifle/carbine, carbine/pistol, rifle/unarmed, etc)
	-- Unarmed should be put on secondary unless the mobile doesn't use weapons, in which case "unarmed" should be put primary and "none" as secondary
	primaryWeapon = "unarmed",
	secondaryWeapon = "none",
	conversationTemplate = "characterBuilderFrogConvoTemplate",

	-- primaryAttacks and secondaryAttacks should be separate skill groups specific to the weapon type listed in primaryWeapon and secondaryWeapon
	-- Use merge() to merge groups in creatureskills.lua together. If a weapon is set to "none", set the attacks variable to empty brackets
	primaryAttacks = {},
	secondaryAttacks = { }
}

CreatureTemplates:addCreatureTemplate(character_builder_frog, "character_builder_frog")
