--Copyright (C) 2026 <SWGEmu>
--Pocket Boy slot machine (Companion System project, 2026-07-19 -- see
--docs/companion_system/NOTES.md). Reuses the stock slot machine's CLIENT
--template (no TRE change needed) but routes its radial to the custom
--PocketBoyMenuComponent instead of the stock GamblingManager terminal.

object_tangible_gambling_slot_pocket_boy = object_tangible_gambling_slot_shared_standard:new {
	gameObjectType = 16407,

	objectMenuComponent = "PocketBoyMenuComponent"
}

ObjectTemplates:addTemplate(object_tangible_gambling_slot_pocket_boy, "object/tangible/gambling/slot/pocket_boy.iff")
