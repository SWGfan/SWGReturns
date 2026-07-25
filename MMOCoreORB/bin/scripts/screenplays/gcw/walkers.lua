-- AT-AT Walker Mobile Template
-- NOTE: Requires object/mobile/atat_walker.iff and object/mobile/at_st_walker.iff
-- If those IFF models don't exist, walkers cannot be spawned visually.
-- Using existing imperial walker references as fallback.

atatWalkerTemplate = Creature:new {
	template = "object/mobile/atat_walker.iff",
	objectName = "AT-AT Walker (Siege)",
	socialGroup = "imperial",
	faction = 1,
	level = 120,
	chanceHit = 1.0,
	damageMin = 1500,
	damageMax = 2500,
	range = 100,
	baseXp = 0,
	baseHAM = 500000,
	baseHAMmax = 500000,
	armor = 200,
	resists = {100,100,100,150,150,150,150,150,150},
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
	creatureBitmask = KILLER + STALKER + PACK,
	diet = HERBIVORE,
	scale = 3.5,
	templates = {"object/mobile/atat_walker.iff"},
	primaryWeapon = "heavy_blaster",
	secondaryWeapon = "none",
	primaryAttacks = merge({ "creature_spit_flame", "creature_spit_acid" }),
	secondaryAttacks = {},
	lootGroups = {},
	lootChance = 0,
	conversationTemplate = "",
	optionsBitmask = AIENABLED
}

atstWalkerTemplate = Creature:new {
	template = "object/mobile/at_st_walker.iff",
	objectName = "AT-ST Walker",
	socialGroup = "imperial",
	faction = 1,
	level = 95,
	chanceHit = 0.8,
	damageMin = 600,
	damageMax = 1000,
	range = 100,
	baseXp = 5000,
	baseHAM = 150000,
	baseHAMmax = 150000,
	armor = 100,
	resists = {80,80,80,100,100,100,100,100,100},
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
	diet = HERBIVORE,
	scale = 2.0,
	templates = {"object/mobile/at_st_walker.iff"},
	primaryWeapon = "rifle_flak",
	secondaryWeapon = "none",
	primaryAttacks = merge({ "creature_spit_flame" }),
	secondaryAttacks = {},
	lootGroups = {},
	lootChance = 0,
	conversationTemplate = "",
	optionsBitmask = AIENABLED
}

CreatureTemplates:addCreatureTemplate(atatWalkerTemplate, "atat_walker")
CreatureTemplates:addCreatureTemplate(atstWalkerTemplate, "at_st_walker")

-- AT-AT troop deployment helper
function deployATATTroops(walker)
	if walker == nil then return end
	local wx = SceneObject(walker):getPositionX()
	local wz = SceneObject(walker):getPositionZ()
	local wy = SceneObject(walker):getPositionY()

	local troops = {
		{ template = "stormtrooper", count = 8 },
		{ template = "stormtrooper_sniper", count = 2 },
		{ template = "stormtrooper_medic", count = 1 },
		{ template = "imperial_officer", count = 1 },
		{ template = "dark_trooper", count = 2 }
	}

	for _, group in ipairs(troops) do
		for i = 1, group.count do
			local x = wx + math.random(-5, 5)
			local z = wz + math.random(-5, 5)
			spawnMobile("imperial", group.template, 1, x, z, wy, 0, 0)
		end
	end
end

function deployATSTTroops(walker)
	if walker == nil then return end
	local wx = SceneObject(walker):getPositionX()
	local wz = SceneObject(walker):getPositionZ()
	local wy = SceneObject(walker):getPositionY()

	local troops = {
		{ template = "stormtrooper", count = 4 },
		{ template = "scout_trooper", count = 2 }
	}

	for _, group in ipairs(troops) do
		for i = 1, group.count do
			local x = wx + math.random(-3, 3)
			local z = wz + math.random(-3, 3)
			spawnMobile("imperial", group.template, 1, x, z, wy, 0, 0)
		end
	end
end
