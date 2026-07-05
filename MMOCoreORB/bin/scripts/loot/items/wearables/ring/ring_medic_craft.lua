ring_medic_craft = {
	minimumLevel = 0,
	maximumLevel = -1,
	customObjectName = "The subtle art of Stims",
	directObjectTemplate = "object/tangible/wearables/ring/aakuan_ring.iff",
	craftingValues = {
	},
	customizationStringNames = {},
	customizationValues = {},
	skillMods = {
		{"combat_medicine_assembly", 25},
		{"combat_medicine_experimentation", 25},
    {"medicine_assembly", 25},
    {"medicine_experimentation", 25},
	}
}

addLootItemTemplate("ring_medic_craft", ring_medic_craft)
