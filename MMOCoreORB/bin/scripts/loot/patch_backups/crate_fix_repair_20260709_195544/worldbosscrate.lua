-- Direct rewards instead of a non-opening crate shell.

worldbosscrate = {
	description = ,
	minimumLevel = 0,
	maximumLevel = 0,
	lootItems = {
		{groupTemplate = lootcollectiontierdiamond, weight = 2500000},
		{groupTemplate = lootcollectiontierthree, weight = 2000000},
		{groupTemplate = weapon_component_advanced, weight = 1500000},
		{groupTemplate = chemistry_component_advanced, weight = 1300000},
		{groupTemplate = g_named_crystals, weight = 1200000},
		{groupTemplate = krayt_pearls, weight = 1000000},
		{groupTemplate = wearables_all, weight = 500000},
	}
}

addLootGroupTemplate(worldbosscrate, worldbosscrate)
