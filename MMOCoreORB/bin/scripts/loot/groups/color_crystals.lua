-- color_crystals loot group
-- 99% chance: plain crystal
--  1% chance: rolls the dot_chance sub-group
--    dot_chance sub-group: 75% plain, 25% DOT crystal
-- Effective DOT crystal chance: ~0.25%

color_crystals_dot_chance = {
	description = "",
	minimumLevel = 0,
	maximumLevel = 0,
	lootItems = {
		{itemTemplate = "force_color_crystal",     weight = 7500000},  -- 75% plain
		{itemTemplate = "force_color_crystal_dot", weight = 2500000},  -- 25% DOT
	}
}

addLootGroupTemplate("color_crystals_dot_chance", color_crystals_dot_chance)

color_crystals = {
	description = "",
	minimumLevel = 0,
	maximumLevel = 0,
	lootItems = {
		{itemTemplate = "force_color_crystal",          weight = 9900000},  -- 99% plain
		{groupTemplate = "color_crystals_dot_chance",   weight = 100000},   --  1% -> sub-group
	}
}

addLootGroupTemplate("color_crystals", color_crystals)