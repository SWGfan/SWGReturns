object_draft_schematic_clothing_custom_robe_tusken_raider_s02_crafted = object_draft_schematic_clothing_custom_shared_robe_tusken_raider_s02_crafted:new {

   templateType = DRAFTSCHEMATIC,

   customObjectName = "Dark Sash",

   craftingToolTab = 8, -- (See DraftSchematicObjectTemplate.h)
   complexity = 1, 
   size = 2, 
   factoryCrateSize = 0,
   xpType = "crafting_general", 
   xp = 30, 

   assemblySkill = "general_assembly", 
   experimentingSkill = "general_experimentation", 
   customizationSkill = "clothing_customization", 

   customizationOptions = {2, 1},
   customizationStringNames = {"/private/index_color_1", "/private/index_color_2"},
   customizationDefaults = {0, 0},

   ingredientTemplateNames = {"craft_clothing_ingredients_n", "craft_clothing_ingredients_n", "craft_clothing_ingredients_n","craft_clothing_ingredients_n"},
   ingredientTitleNames = {"hardware", "load_bearing_harness", "reinforcement", "hardware_and_attachments"},
   ingredientSlotType = {1, 0, 1, 1},
   resourceTypes = {"object/tangible/component/clothing/shared_metal_fasteners.iff", "hide_wooly_tatooine", "object/tangible/component/clothing/shared_reinforced_fiber_panels.iff", "object/tangible/component/clothing/shared_reinforced_fiber_panels.iff"},
   resourceQuantities = {3, 275, 2, 2},
   contribution = {100, 100, 100, 100},


   targetTemplate = "object/tangible/wearables/robe/robe_tusken_raider_s02_crafted.iff",

   additionalTemplates = {
             }

}
ObjectTemplates:addTemplate(object_draft_schematic_clothing_custom_robe_tusken_raider_s02_crafted, "object/draft_schematic/clothing/custom/robe_tusken_raider_s02_crafted.iff")
