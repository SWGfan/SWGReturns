object_draft_schematic_clothing_clothing_armor_mandalorian_imperial_gloves = object_draft_schematic_clothing_shared_clothing_armor_mandalorian_imperial_gloves:new {

   templateType = DRAFTSCHEMATIC,

   customObjectName = "Mandalorian Imperial Armor Gloves",

   craftingToolTab = 2,
   complexity = 1,
   size = 4,
   factoryCrateSize = 0,

   xpType = "crafting_clothing_armor",
   xp = 250,

   assemblySkill = "armor_assembly",
   experimentingSkill = "armor_experimentation",
   customizationSkill = "armor_customization",

   customizationOptions = {2},
   customizationStringNames = {"/private/index_color_1"},
   customizationDefaults = {0},

   ingredientTemplateNames = {"craft_clothing_ingredients_n", "craft_clothing_ingredients_n", "craft_clothing_ingredients_n", "craft_clothing_ingredients_n", "craft_clothing_ingredients_n", "craft_clothing_ingredients_n", "craft_clothing_ingredients_n", "craft_clothing_ingredients_n", "craft_clothing_ingredients_n"},
   ingredientTitleNames = {"auxilary_coverage", "body", "liner", "hardware_and_attachments", "binding_and_reinforcement", "padding", "armor", "load_bearing_harness", "reinforcement"},
   ingredientSlotType = {0, 0, 0, 0, 0, 0, 1, 1, 1},
   resourceTypes = {"ore_intrusive", "fuel_petrochem_solid_known", "fiberplast_naboo", "aluminum", "copper_beyrllius", "hide_wooly", "object/tangible/component/armor/shared_armor_segment_ris.iff", "object/tangible/component/clothing/shared_synthetic_cloth.iff", "object/tangible/loot/dungeon/death_watch_bunker/shared_emulsion_protection.iff"},
   resourceQuantities = {100, 100, 50, 60, 50, 40, 1, 1, 1},
   contribution = {100, 100, 100, 100, 100, 100, 100, 100, 100},


   targetTemplate = "object/tangible/wearables/armor/mandalorian_imperial/armor_mandalorian_imperial_gloves.iff",

   additionalTemplates = {
             }

}

ObjectTemplates:addTemplate(object_draft_schematic_clothing_clothing_armor_mandalorian_imperial_gloves, "object/draft_schematic/clothing/clothing_armor_mandalorian_imperial_gloves.iff")
