

object_draft_schematic_weapon_loot_soulstinger = object_draft_schematic_weapon_shared_loot_soulstinger:new {

   templateType = DRAFTSCHEMATIC,

   customObjectName = "Sayormi Soulstinger",

   craftingToolTab = 1, -- (See DraftSchematicObjectTemplate.h)
   complexity = 1, 
   size = 2, 

   xpType = "crafting_weapons_general", 
   xp = 65, 

   assemblySkill = "weapon_assembly", 
   experimentingSkill = "weapon_experimentation", 
   customizationSkill = "weapon_customization", 

   customizationOptions = {},
   customizationStringNames = {},
   customizationDefaults = {},

   ingredientTemplateNames = {"craft_weapon_ingredients_n", "craft_weapon_ingredients_n", "craft_weapon_ingredients_n", "craft_weapon_ingredients_n", "craft_weapon_ingredients_n"},
   ingredientTitleNames = {"grip_unit", "strike_face", "vibro_unit_and_power_cell_brackets", "power_cell_socket", "vibration_generator"},
   ingredientSlotType = {0, 0, 0, 0, 1},
   resourceTypes = {"metal_ferrous", "steel", "metal", "copper", "object/tangible/component/weapon/shared_vibro_unit.iff"},
   resourceQuantities = {12, 8, 8, 4, 1},
   contribution = {100, 100, 100, 100, 100},


   targetTemplate = "object/weapon/melee/special/ep3_loot_soulstinger.iff",

   additionalTemplates = {
             }

}
ObjectTemplates:addTemplate(object_draft_schematic_weapon_loot_soulstinger, "object/draft_schematic/weapon/loot_soulstinger.iff")
