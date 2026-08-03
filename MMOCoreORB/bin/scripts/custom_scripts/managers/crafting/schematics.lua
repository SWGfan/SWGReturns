-- Master Survey Tool (2026-07-29, Companion -- see NOTES.md "Master
-- Survey Tool"). This file is a NEW addition to an extension point that
-- already existed and was already loaded unconditionally by
-- SchematicMap::initialize() (see SchematicMap.cpp) -- no file had ever
-- been placed here before this patch. Registers every custom-scripts
-- schematic that needs a persistent DraftSchematic object auto-created
-- at server boot (required for SchematicMap::instance()->get(...) to
-- resolve a real object -- see patch_master_survey_tool_skillgrant.py).

schematics = {
	{path="object/draft_schematic/item/item_master_survey_tool.iff"},
}
