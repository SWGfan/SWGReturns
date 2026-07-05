ring_medic_use = {
	minimumLevel = 0,
	maximumLevel = -1,
	customObjectName = "The epitome of Health",
	directObjectTemplate = "object/tangible/wearables/ring/aakuan_ring.iff",
	craftingValues = {
	},
	customizationStringNames = {},
	customizationValues = {},
	skillMods = {
		{"healing_wound_treatment", 25},
		{"healing_wound_speed", 25},
    {"healing_range_speed", 25},
    {"healing_range", 25},
    {"healing_injury_treatment", 25},
    {"healing_injury_speed", 25},
    {"healing_ability", 25},
    {"combat_healing_ability", 25},
    {"combat_medic_effectiveness", 25},
	}
}

addLootItemTemplate("ring_medic_use", ring_medic_use)
