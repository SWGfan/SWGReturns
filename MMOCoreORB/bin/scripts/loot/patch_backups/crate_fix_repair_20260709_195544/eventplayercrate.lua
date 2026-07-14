-- Direct rewards instead of a non-opening crate shell.

eventplayercrate = {
	description = ,
	minimumLevel = 0,
	maximumLevel = 0,
	lootItems = {
		{groupTemplate = lootcollectiontierthree, weight = 2500000},
		{groupTemplate = lootcollectiontierdiamond, weight = 1800000},
		{groupTemplate = weapon_component_advanced, weight = 1500000},
		{groupTemplate = chemistry_component_advanced, weight = 1300000},
		{groupTemplate = g_named_crystals, weight = 1200000},
		{groupTemplate = krayt_pearls, weight = 1000000},
		{groupTemplate = wearables_all, weight = 700000},
	}
}

addLootGroupTemplate(eventplayercrate, eventplayercrate)
