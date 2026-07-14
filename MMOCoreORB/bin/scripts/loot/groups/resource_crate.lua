resource_crate = {
	description = "",
	minimumLevel = 0,
	maximumLevel = 0,
	lootItems = {
		{itemTemplate = "resource_deed", weight = 3500000},
		{groupTemplate = "krayt_pearls", weight = 1500000},
		{groupTemplate = "weapon_component_advanced", weight = 1500000},
		{groupTemplate = "chemistry_component_advanced", weight = 1500000},
		{groupTemplate = "g_named_crystals", weight = 1000000},
		{groupTemplate = "wearables_all", weight = 1000000},
	}
}

addLootGroupTemplate("resource_crate", resource_crate)
