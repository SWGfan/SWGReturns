ring_jedi_regen = {
	minimumLevel = 0,
	maximumLevel = -1,
	customObjectName = "Dooku's Duplicity",
	directObjectTemplate = "object/tangible/wearables/ring/aakuan_ring.iff",
	craftingValues = {
	},
	customizationStringNames = {},
	customizationValues = {},
	skillMods = {
		{"jedi_force_power_regen", 25},
		{"jedi_toughness", 10},
		{"lightsaber_toughness", 10},
	}
}

addLootItemTemplate("ring_jedi_regen", ring_jedi_regen)
