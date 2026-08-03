object_draft_schematic_item_item_master_survey_tool = object_draft_schematic_item_shared_item_survey_tool_mineral:new {

   templateType = DRAFTSCHEMATIC,

   customObjectName = "Master Survey Tool",

   craftingToolTab = 524288, -- (See DraftSchematicObjectTemplate.h)
   complexity = 7,
   size = 1,
   factoryCrateType = "object/factory/factory_crate_electronics.iff",

   xpType = "crafting_general",
   xp = 55,

   assemblySkill = "general_assembly",
   experimentingSkill = "general_experimentation",
   customizationSkill = "clothing_customization",

   customizationOptions = {},
   customizationStringNames = {},
   customizationDefaults = {},

   -- Master Survey Tool (2026-07-29, Companion, per Nick's explicit
   -- request "only requires 1 steel to craft"): exactly one ingredient
   -- slot, "steel" resource class, quantity 1. "steel" confirmed as a
   -- real resource class string already used identically by
   -- item_ballot_box_terminal.lua/item_recycler_metal.lua/
   -- item_recycler_ore.lua/item_recycler_creature.lua in this repo.
   ingredientTemplateNames = {"craft_item_ingredients_n"},
   ingredientTitleNames = {"steel_casing"},
   ingredientSlotType = {0},
   resourceTypes = {"steel"},
   resourceQuantities = {1},
   contribution = {100},

   targetTemplate = "object/tangible/survey_tool/master_survey_tool.iff",

   additionalTemplates = {
             }

}
ObjectTemplates:addTemplate(object_draft_schematic_item_item_master_survey_tool, "object/draft_schematic/item/item_master_survey_tool.iff")
