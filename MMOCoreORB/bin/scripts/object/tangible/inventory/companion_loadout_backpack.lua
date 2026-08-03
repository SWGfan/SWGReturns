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

--Linking Engine3 statically or dynamically with other modules
--is making a combined work based on Engine3.
--Thus, the terms and conditions of the GNU Lesser General Public License
--cover the whole combination.

--In addition, as a special exception, the copyright holders of Engine3
--give you permission to combine Engine3 program with free software
--programs or libraries that are released under the GNU LGPL and with
--code included in the standard release of Core3 under the GNU LGPL
--license (or modified versions of such code, with unchanged license).
--You may copy and distribute such a system following the terms of the
--GNU LGPL for Engine3 and the licenses of the other code concerned,
--provided that you include the source code of that other code when
--and as the GNU LGPL requires distribution of source code.

--Note that people who make modified versions of Engine3 are not obligated
--to grant this special exception for their modified versions;
--it is their choice whether to do so. The GNU Lesser General Public License
--gives permission to release a modified version without this exception;
--this exception also makes it possible to release a modified version


-- Companion System (2026-07-14, "player-side loadout backpack" redesign --
-- see docs/companion_system/NOTES.md). A real, ordinary bag that lives in
-- the PLAYER'S OWN inventory (created once at companion-recruitment time,
-- SkillManager.cpp's companion_master_novice grant block) -- not a
-- companion-side object at all. Has its own dedicated container component
-- (CompanionLoadoutContainerComponent) that auto-equips any weapon/wearable
-- dropped into it onto the player's active companion, displacing whatever
-- was previously equipped in that slot into the player's main inventory,
-- and auto-feeds any food/drink dropped into it to the companion.
--
-- 2026-07-14 fix: was originally based on shared_creature_inventory.iff,
-- but that client template has NO appearance file and gameObjectType 8197
-- (the client's internal creature-inventory type), so the client rendered
-- it as an invisible, un-openable object inside the player's inventory --
-- the "items not shown in a proper container" bug. shared_backpack_s01.iff
-- is a real, already-shipped wearable backpack (icon, open-container radial,
-- normal container window) so no TRE patch is needed.
object_tangible_inventory_companion_loadout_backpack = object_tangible_wearables_backpack_shared_backpack_s01:new {
	containerComponent = "CompanionLoadoutContainerComponent",
	objectName = "@companion:loadout_backpack_name",
}

ObjectTemplates:addTemplate(object_tangible_inventory_companion_loadout_backpack, "object/tangible/inventory/companion_loadout_backpack.iff")
