trash_common = {
	description = "",
	minimumLevel = 0,
	maximumLevel = 0,
	lootItems = {
		{groupTemplate = "junk", weight = 5000000},      
		{itemTemplate = "collectiontierone", weight = 2000000},
		{groupTemplate = "clothing_attachments", weight = 1500000},
		{groupTemplate = "armor_attachments", weight = 1500000} 
	}
}

addLootGroupTemplate("trash_common", trash_common)