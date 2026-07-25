dark_jedi_voice = Creature:new {
	objectName = "@mob/creature_names:dark_jedi_knight",
	customName = "Varyn Kai - Voice of the Sith Emperor",
	socialGroup = "dark_jedi",
	mobType = MOB_NPC,
	faction = "",
	level = 380,
	chanceHit = 1.0,
	damageMin = 2200,
	damageMax = 3000,
	baseXp = 45000,
	baseHAM = 1000000,
	baseHAMmax = 1200000,
	armor = 3,
	resists = {130,130,130,130,130,130,130,130,100},
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

	templates = {"object/mobile/mara_jade.iff"},
	
	lootGroups = {
		{
			groups = {
				{group = "holocron_dark", chance = 600000},
				{group = "holocron_light", chance = 600000},
				{group = "power_crystals", chance = 600000},
				{group = "rifles", chance = 1300000},
				{group = "pistols", chance = 1300000},
				{group = "melee_weapons", chance = 1300000},
				{group = "armor_attachments", chance = 1100000},
				{group = "clothing_attachments", chance = 1100000},
				{group = "carbines", chance = 1300000},
				{group = "dark_jedi_common", chance = 800000}
			}
		}
	},
	
	outfit = "prophet_outfit",
	reactionStf = "@npc_reaction/sith_shadow",
	primaryWeapon = "dark_jedi_weapons_gen4",
	secondaryWeapon = "unarmed",
	conversationTemplate = "",

	primaryAttacks = lightsabermaster,
	secondaryAttacks = forcepowermaster,
	
	specialDamageMult = 2.0
}

CreatureTemplates:addCreatureTemplate(dark_jedi_voice, "dark_jedi_voice")