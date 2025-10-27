require("scripts.managers.planet.regions")

kaas_regions = {
  {"a_rebel_outpost",-6131,2705,{1,700},NOSPAWNAREA + NOBUILDZONEAREA},  -- kaas city

        {"borgle_bat_cave",2850,3890,{1,200},NOSPAWNAREA + NOBUILDZONEAREA}, -- smuggler op
        {"camp_and_BH",3342,2634,{1,200},NOSPAWNAREA + NOBUILDZONEAREA}, --gundark cave
        {"camp_and_skeleton",6017,-1141,{1,400},NOSPAWNAREA + NOBUILDZONEAREA}, --tomb of vitiate

        {"cobral_tent",-70,6370,{1,100},NOSPAWNAREA + NOBUILDZONEAREA}, -- ruined sith complex

  {"narmle_easy_newbie",-5205,-2290,{1,1400},SPAWNAREA,{"kaas_world"},256},
  {"narmle_medium_newbie",-5200,-2290,{3,1400,2200},SPAWNAREA,{"kaas_world"},384},


        {"rebel_outpost",3677,-6447,{1,1500},SPAWNAREA,{"kaas_world"},384},
  {"restuss",0,0,{1,0},UNDEFINEDAREA},
  {"restuss_easy_newbie",5300,5700,{1,1400},SPAWNAREA,{"kaas_world"},256},
  {"restuss_medium_newbie",5305,5700,{3,1400,2200},SPAWNAREA,{"kaas_world"},384},

  {"world_spawner",0,0,{1,-1},SPAWNAREA + WORLDSPAWNAREA,{"kaas_world","global"},2048},

}

