-- Companion System (2026-07-20, "companion kill token" pass, per user
-- request) -- Veteran Reward Vendor NPC. Redeems "Companion Killed Token"
-- items (one granted per companion kill -- see CreatureManagerImplementation
-- ::notifyDestruction()) for reward items/credits. Modeled directly on
-- trainer_companion_master.lua (same visuals, invulnerable + conversable),
-- placed 5m west of it in tatooine_mos_eisley.lua. conversationTemplate is
-- deliberately the SAME value as the Companion Master trainer's own --
-- purely as a proven-working carrier so Converse dispatches into the
-- server's trainerConvHandler branch at all; AiAgentImplementation.cpp
-- gates on this NPC's template NAME ("veteran_reward_vendor") to open our
-- own purchase menu instead of the real training dialog. See
-- VeteranRewardVendorSuiCallback.h and docs/companion_system/NOTES.md.

veteran_reward_vendor = Creature:new {
	objectName = "@mob/creature_names:veteran_reward_vendor",
	randomNameType = NAME_GENERIC,
	randomNameTag = true,
	mobType = MOB_NPC,
	faction = "",
	level = 100,
	chanceHit = 0.390000,
	damageMin = 290,
	damageMax = 300,
	baseXp = 2914,
	baseHAM = 8400,
	baseHAMmax = 10200,
	armor = 0,
	resists = {-1,-1,-1,-1,-1,-1,-1,-1,-1},
	meatType = "",
	meatAmount = 0,
	hideType = "",
	hideAmount = 0,
	boneType = "",
	boneAmount = 0,
	milk = 0,
	tamingChance = 0.000000,
	ferocity = 0,
	pvpBitmask = NONE,
	creatureBitmask = NONE,
	optionsBitmask = INVULNERABLE + CONVERSABLE,
	diet = HERBIVORE,

	templates = {
		"object/mobile/dressed_creaturehandler_trainer_human_male_01.iff",
		"object/mobile/dressed_creaturehandler_trainer_rodian_female_01.iff",
		"object/mobile/dressed_creaturehandler_trainer_zabrak_male_01.iff"
	},
	lootGroups = {},

	primaryWeapon = "unarmed",
	secondaryWeapon = "none",
	conversationTemplate = "companionMasterTrainerConvoTemplate",

	primaryAttacks = {},
	secondaryAttacks = {}
}
CreatureTemplates:addCreatureTemplate(veteran_reward_vendor,"veteran_reward_vendor")
