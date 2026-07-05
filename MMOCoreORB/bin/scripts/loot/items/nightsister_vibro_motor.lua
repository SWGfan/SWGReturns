nightsister_vibro_motor = {
	minimumLevel = 0,
	maximumLevel = -1,
	customObjectName = "Nightsister Vibro Motor",
	directObjectTemplate = "object/tangible/component/weapon/vibro_unit_enhancement_max_damage.iff",
	craftingValues = {
		{"mindamage",175,225,1},
		{"maxdamage",225,275,1},
		{"woundchance",5,10,10},
		{"useCount",1,5,0},
	},
	customizationStringNames = {},
	customizationValues = {}
}

addLootItemTemplate("nightsister_vibro_motor", nightsister_vibro_motor)
