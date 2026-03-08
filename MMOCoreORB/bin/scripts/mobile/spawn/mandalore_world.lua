-- Mandalore Spawn Groups

mandalore_world = {
	mobile = {
		-- Named NPCs (Bosses)
		{"MandaloreTheResurrector", 300, -1, -1, -1, 100, 5},
		{"DarthCaedus", 300, -1, -1, -1, 100, 5},
		{"TorVizsla", 300, -1, -1, -1, 100, 8},
		{"ToborroTheHutt", 300, -1, -1, -1, 100, 8},
		
		-- Clan members
		{"TaungWarrior", 300, -1, -1, -1, 100, 15},
		{"DextonClanHunter", 300, -1, -1, -1, 100, 18},
		{"BralorClanMercenary", 300, -1, -1, -1, 100, 18},
		{"VizslaLoyalist", 300, -1, -1, -1, 100, 18},
		{"PykeSyndicateCriminal", 300, -1, -1, -1, 100, 18},
		
		-- Combat units
		{"deathwatch_sbd", 300, -1, -1, -1, 100, 15},
		{"DarthCaedusFollower", 300, -1, -1, -1, 100, 15},
		
		-- Creatures
		{"narglatch_bruiser", 300, -1, -1, -1, 100, 20},
		{"baz_nitch_terror", 300, -1, -1, -1, 100, 20},
		{"mytho_king", 300, -1, -1, -1, 100, 15},
		{"rancor_guard", 300, -1, -1, -1, 100, 15},
		
		-- Force beings (added for variety)
		{"dark_jedi_knight", 300, -1, -1, -1, 100, 12},
		{"dark_jedi_sentinel", 300, -1, -1, -1, 100, 12},
		{"force_trained_archaist", 300, -1, -1, -1, 100, 15},
	},
}

addSpawnGroup("mandalore_world", mandalore_world);
