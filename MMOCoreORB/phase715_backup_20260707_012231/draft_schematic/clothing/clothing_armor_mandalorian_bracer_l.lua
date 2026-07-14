object_draft_schematic_clothing_clothing_armor_mandalorian_bracer_l = object_draft_schematic_clothing_shared_clothing_armor_mandalorian_bracer_l:new {
   templateType = DRAFTSCHEMATIC,

   customObjectName = "Mandalorian Armor Left Bracer",

   craftingToolTab = 2, -- (See DraftSchemticImplementation.h)
   complexity = 45, 
   size = 1, 

   xpType = "crafting_clothing_armor", 
   xp = 836, 

   assemblySkill = "armor_assembly", 
   experimentingSkill = "armor_experimentation", 
   customizationSkill = "armor_customization", 

   customizationOptions = {},
   customizationStringNames = {},
   customizationDefaults = {},

   ingredientTemplateNames = {"craft_clothing_ingredients_n", "craft_clothing_ingredients_n", "craft_clothing_ingredients_n", "craft_clothing_ingredients_n", "craft_clothing_ingredients_n", "craft_clothing_ingredients_n", "craft_clothing_ingredients_n", "craft_clothing_ingredients_n", "craft_clothing_ingredients_n"},
   ingredientTitleNames = {"auxilary_coverage", "body", "liner", "hardware_and_attachments", "binding_and_reinforcement", "padding", "armor", "load_bearing_harness", "reinforcement"},
   ingredientSlotType = {0, 0, 0, 0, 0, 0, 1, 1, 1},
   resourceQuantities = {150, 75, 65, 50, 40, 30, 1, 1, 1},
   contribution = {100, 100, 100, 100, 100, 100, 100, 100, 100},


   targetTemplate = "object/tangible/wearables/armor/mandalorian/armor_mandalorian_bracer_l.iff",

   additionalTemplates = {
             }
}

ObjectTemplates:addTemplate(object_draft_schematic_clothing_clothing_armor_mandalorian_bracer_l, "object/draft_schematic/clothing/clothing_armor_mandalorian_bracer_l.iff")