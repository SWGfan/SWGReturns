--Should all created players start with God Mode? 1 = yes, 0 = no
freeGodMode = 0;
--How many cash credits new characters start with after creating a character (changed during test phase, normal value is 100)
startingCash = 200000
--startingCash = 100000
--How many bank credits new characters start with after creating a character (changed during test phase, normal value is 1000)
startingBank = 200000
--startingBank = 100000
--How many skill points a new characters start with
skillPoints = 500

professions = {
	"combat_brawler",
	"combat_marksman",
	"crafting_artisan",
	"outdoors_scout",
	"science_medic",
	"social_entertainer",
	--"jedi_padawan"
}

marksmanPistol = "object/weapon/ranged/pistol/pistol_cdef.iff"
	
marksmanRifle = "object/weapon/ranged/rifle/rifle_cdef.iff"

marksmanCarbine = "object/weapon/ranged/carbine/carbine_cdef.iff"

brawlerOneHander = "object/weapon/melee/knife/knife_survival.iff"

brawlerTwoHander = "object/weapon/melee/axe/axe_heavy_duty.iff"

brawlerPolearm = "object/weapon/melee/polearm/lance_staff_wood_s1.iff"

genericTool = "object/tangible/crafting/station/generic_tool.iff"

foodTool = "object/tangible/crafting/station/food_tool.iff"

mineralTool = "object/tangible/survey_tool/survey_tool_mineral.iff"

chemicalTool = "object/tangible/survey_tool/survey_tool_liquid.iff"

slitherhorn = "object/tangible/instrument/slitherhorn.iff"

Swoop_Bike = "object/tangible/deed/vehicle_deed/speederbike_swoop_deed.iff"

robe = "object/tangible/wearables/robe/robe_jedi_padawan.iff"

jediTool = "object/tangible/crafting/station/jedi_tool.iff"

jediCrystal = "object/tangible/component/weapon/lightsaber/lightsaber_module_force_crystal.iff"

gasTool = "object/tangible/survey_tool/survey_tool_gas.iff"

professionSpecificItems = {
	combat_brawler = { brawlerOneHander, brawlerTwoHander, brawlerPolearm },
	combat_marksman = { marksmanPistol, marksmanCarbine, marksmanRifle },
	crafting_artisan = { genericTool, mineralTool, chemicalTool },
	--jedi_padawan = { robe, jediTool, jediCrystal, genericTool, mineralTool, chemicalTool, gasTool },
	outdoors_scout = { genericTool },
	science_medic = { foodTool },
	social_entertainer = { slitherhorn }
}

commonStartingItems = { Swoop_Bike }
