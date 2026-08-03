object_tangible_survey_tool_master_survey_tool = object_tangible_survey_tool_shared_survey_tool_all:new {

	templateType = SURVEYTOOL,

	-- 2026-08-01 hotfix: this template never set its own objectName, so the
	-- crafted item silently inherited
	-- object_tangible_survey_tool_shared_survey_tool_all's own display
	-- name ("Complete Resource Survey Tool", the stock all-purpose
	-- survey device name) -- confirmed live via Nick's own screenshot
	-- showing the crafted item's tooltip titled "COMPLETE RESOURCE
	-- SURVEY TOOL" instead of "Master Survey Tool".
	-- 2026-08-01: MUST be an STF reference, not a raw string. The earlier
	-- raw-string version ("Master Survey Tool") never resolved, so the
	-- client displayed the template path instead. See companion.stf's
	-- master_survey_tool_name entry in build_companion_content.py, and the
	-- working precedent in companion_loadout_backpack.lua.
	objectName = "@companion:master_survey_tool_name",
	detailedDescription = "@companion:master_survey_tool_desc",

	playerRaces = {
		"object/creature/player/bothan_male.iff",
		"object/creature/player/bothan_female.iff",
		"object/creature/player/human_male.iff",
		"object/creature/player/human_female.iff",
		"object/creature/player/ithorian_male.iff",
		"object/creature/player/ithorian_female.iff",
		"object/creature/player/moncal_male.iff",
		"object/creature/player/moncal_female.iff",
		"object/creature/player/rodian_male.iff",
		"object/creature/player/rodian_female.iff",
		"object/creature/player/sullustan_male.iff",
		"object/creature/player/sullustan_female.iff",
		"object/creature/player/trandoshan_male.iff",
		"object/creature/player/trandoshan_female.iff",
		"object/creature/player/twilek_male.iff",
		"object/creature/player/twilek_female.iff",
		"object/creature/player/wookiee_male.iff",
		"object/creature/player/wookiee_female.iff",
		"object/creature/player/zabrak_male.iff",
		"object/creature/player/zabrak_female.iff"
	},

	customizationOptions = {},
	customizationDefaults = {},

	-- Master Survey Tool (2026-07-29, Companion): toolType is a harmless
	-- placeholder -- the new master-survey radial path (see
	-- SurveyToolImplementation.cpp) keys entirely on surveyType below, not
	-- on this field, and never calls the stock type-filtered
	-- sendResourceListForSurvey()/sendSurvey() path this field normally
	-- gates.
	toolType = 10,
	toolAnimation = "clienteffect/survey_tool_mineral.cef",
	sampleAnimation = "clienteffect/survey_sample_mineral.cef",
	surveyType = "master_survey",

	numberExperimentalProperties = {1, 1, 1, 1},
	experimentalProperties = {"XX", "XX", "XX", "CD"},
	experimentalWeights = {1, 1, 1, 1},
	experimentalGroupTitles = {"null", "null", "null", "exp_effectiveness"},
	experimentalSubGroupTitles = {"null", "null", "hitpoints", "usemodifier"},
	experimentalMin = {0, 0, 1000, -15},
	experimentalMax = {0, 0, 1000, 15},
	experimentalCombineType = {0, 0, 4, 1},
	experimentalPrecision = {0, 0, 0, 0},
}
ObjectTemplates:addTemplate(object_tangible_survey_tool_master_survey_tool, "object/tangible/survey_tool/master_survey_tool.iff")
