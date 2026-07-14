krayt_dragon_tissue_enhanced = {
	minimumLevel = 0,
	maximumLevel = -1,
	customObjectName = "Enhanced Krayt Dragon Tissues",
	directObjectTemplate = "object/tangible/component/weapon/blaster_power_handler_enhancement_krayt.iff",
	craftingValues = {
			{"maxdamage",225,350,0},
    		{"mindamage",175,225,0},		
		{"attackspeed",0,-2.2,1},
		{"useCount",25,75,0},
	},
	customizationStringNames = {},
	customizationValues = {}
}

addLootItemTemplate("krayt_dragon_tissue_enhanced", krayt_dragon_tissue_enhanced)
