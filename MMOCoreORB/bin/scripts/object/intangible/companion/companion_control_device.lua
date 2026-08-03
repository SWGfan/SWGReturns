--Copyright (C) 2010 <SWGEmu>


--This File is part of Core3.

--This program is free software; you can redistribute
--it and/or modify it under the terms of the GNU Lesser
--General Public License as published by the Free Software
--Foundation; either version 2 of the License,
--or (at your option) any later version.

--This program is distributed in the hope that it will be useful,
--but WITHOUT ANY WARRANTY; without even the implied warranty of
--MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
--See the GNU Lesser General Public License for
--more details.

--You should have received a copy of the GNU Lesser General
--Public License along with this program; if not, write to
--the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

--Companion System -- the object template createObject() actually
-- instantiates. Inherits object_intangible_pet_shared_pet_control (the
-- real Creature Handler pet control device's shared template, defined in
-- object/intangible/pet/objects.lua) so it reuses that existing, already
-- client-known appearance/icon -- no new client assets needed. This is
-- safe to reference here because object/main.lua fully loads allobjects.lua
-- (which defines object_intangible_pet_shared_pet_control) before loading
-- serverobjects.lua (which is what pulls in this file).
--
-- gameObjectType is overridden to SceneObjectType::COMPANIONCONTROLDEVICE
-- (0x80D = 2061), which is what makes
-- ObjectManager::loadObjectFromTemplate() dispatch to our own
-- CompanionControlDeviceImplementation class instead of PetControlDevice --
-- see server/zone/managers/object/ObjectManager.cpp,
-- server/zone/objects/scene/SceneObjectType.h, and
-- docs/companion_system/NOTES.md.
-- Companion System (2026-07-15, "datapad device should show a human model"
-- -- see NOTES.md and docs/companion_system/tools/
-- build_device_template_patch.py): a CUSTOM client template shipped in
-- companion_patch.tre -- a clone of the 3PO protocol droid intangible with
-- its appearance repointed at the human male body (appearance/hum_m.sat,
-- the same body the companion actor itself uses), so the datapad preview
-- shows a human instead of the generic pet-control blob. NOTE: the preview
-- is a static template model -- it cannot mirror the companion's live
-- clothes/armor/weapon per instance (client templates are fixed per object
-- type). Existing devices carry the old persisted clientObjectCRC --
-- CompanionControlDeviceImplementation::spawnObject() re-stamps it every
-- summon (visible after the next relog).
-- REVERTED (2026-07-15, live test): the custom human-appearance intangible
-- template rendered as an INVISIBLE, unclickable datapad entry -- the
-- client evidently can't build an intangible preview from the raw player
-- body .sat (appearance/hum_m.sat needs customization data an intangible
-- never carries). Back to the proven pet-control client template;
-- CompanionControlDeviceImplementation::initializeTransientMembers()
-- self-heals the persisted clientObjectCRC of any device created while the
-- broken template was live. The crafted iff stays harmlessly in
-- companion_patch.tre (unused). A human datapad model needs a different
-- technique -- flagged in NOTES.md as attempted/reverted.
object_intangible_companion_companion_control_device = object_intangible_pet_shared_pet_control:new {
	gameObjectType = 2062 -- SceneObjectType::COMPANIONCONTROLDEVICE (0x80E)
}

ObjectTemplates:addTemplate(object_intangible_companion_companion_control_device, "object/intangible/companion/companion_control_device.iff")
