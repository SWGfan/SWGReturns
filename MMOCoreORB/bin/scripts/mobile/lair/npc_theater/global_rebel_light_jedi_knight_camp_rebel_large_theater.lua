global_rebel_light_jedi_knight_camp_rebel_large_theater = Lair:new {
	mobiles = {
		{"rebel_droideka",1},
		{"fbase_rebel_elite_heavy_trooper_hard",2},
		{"specforce_marine",3},
		{"specforce_heavy_weapons_specialist",1},
		{"senior_specforce_heavy_weapons_specialist",1},
		{"senior_specforce_marine",1},
		{"light_jedi_youngling",3},
		{"light_jedi_padawan_b",2},
		{"light_jedi_padawan_g",2},
		{"light_jedi_knight",2},
		{"light_jedi_consular",1},
		{"light_jedi_master",1},
		{"light_jedi_sentinel",1},
	},

	spawnLimit = 20,
	buildingsVeryEasy = {"object/building/poi/anywhere_rebel_base_large_1.iff"},
	buildingsEasy = {"object/building/poi/anywhere_rebel_base_large_1.iff"},
	buildingsMedium = {"object/building/poi/anywhere_rebel_base_large_1.iff"},
	buildingsHard = {"object/building/poi/anywhere_rebel_base_large_1.iff"},
	buildingsVeryHard = {"object/building/poi/anywhere_rebel_base_large_1.iff"},
	missionBuilding = "object/tangible/lair/base/objective_banner_rebel.iff",
	mobType = "npc",
	buildingType = "theater",
	faction = "rebel"
}

addLairTemplate("global_rebel_light_jedi_knight_camp_rebel_large_theater", global_rebel_light_jedi_knight_camp_rebel_large_theater)
