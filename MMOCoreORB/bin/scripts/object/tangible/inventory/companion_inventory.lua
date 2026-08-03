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


-- Companion System (2026-07-14, "bag item-loss root cause" fix -- see
-- NOTES.md). This is a dedicated template for the companion's own personal
-- inventory bag, reusing the same real client-side appearance/model
-- (object_tangible_inventory_shared_creature_inventory, a real, already-
-- shipped client asset -- no new TRE content needed for this fix) that
-- creature_inventory.iff also uses, but with a different SERVER-side
-- container component.
--
-- Root cause this fixes: creature_inventory.iff (the template previously
-- reused here) hardcodes containerComponent = "LootContainerComponent",
-- which is built for looting a dead creature's corpse, not for a live,
-- friendly companion's ongoing personal storage:
--   * LootContainerComponent::canAddObject() unconditionally returns
--     TransferErrorCode::INVALIDTYPE ("You cannot place items into a
--     corpse.") for every single insert attempt, full stop -- so any item
--     relocated into this bag (CompanionContainerComponent.cpp's
--     relocateLooseItemToInventoryBag()) silently failed to actually land
--     there.
--   * LootContainerComponent::checkContainerPermission()'s MOVEOUT branch
--     only succeeds if the container's own ContainerPermissions ownerID
--     field equals the requesting creature's object ID (or group ID) --
--     but nothing anywhere in this project's bag-creation code
--     (CompanionControlDeviceImplementation.cpp::spawnObject()) ever calls
--     setOwner() on it, so that field is always its default/unset value and
--     MOVEOUT permission is permanently denied to everyone -- the real
--     cause of the client's canned "You can not loot that." message.
-- Both behaviors are almost certainly intentional and correct for a REAL
-- creature/droid pet's corpse-loot bag (matching live SWG's "you can only
-- access a tamed pet's held items after it dies") -- the bug was reusing
-- that same corpse-only template for a live companion's everyday storage,
-- which needs the opposite semantics entirely.
--
-- Fix, take 1 (superseded, see below): assigned CompanionContainerComponent.
-- That was itself a bug -- CompanionContainerComponent extends
-- PlayerContainerComponent, and its canAddObject() override only
-- special-cases sceneObject->isCompanionObject() (the companion itself);
-- for any other sceneObject (this bag included) it falls through to the
-- inherited PlayerContainerComponent::canAddObject(), which unconditionally
-- requires dynamic_cast<CreatureObject*>(sceneObject) to succeed on the
-- DESTINATION container. A bag is a TangibleObject, never a CreatureObject,
-- so that check always failed and every insert into this bag was silently
-- rejected with PLAYERUSEMASKERROR (confirmed via live server log,
-- 2026-07-14 -- see NOTES.md, same bug class as the loadout backpack's
-- PLAYERUSEMASKERROR fix, just one level deeper and easy to miss first
-- pass). Confirmed via character_inventory.lua that a real player's own
-- inventory bag has NO containerComponent override at all -- it just uses
-- plain, generic ContainerComponent.
--
-- Fix, take 2: dedicated CompanionBagContainerComponent (server/zone/
-- objects/companion/components/CompanionBagContainerComponent.h/.cpp) --
-- plain ContainerComponent for canAddObject()/notifyObjectInserted/Removed
-- (ordinary VOLUME-container behavior, no CreatureObject requirement), with
-- checkContainerPermission() still overridden for real ownership gating
-- (CompanionObject::isAuthorizedActor(creature), copied from
-- CompanionContainerComponent's own resolveCompanion()-based check, which
-- already worked correctly on a nested child object).
object_tangible_inventory_companion_inventory = object_tangible_inventory_shared_creature_inventory:new {
	containerComponent = "CompanionBagContainerComponent",
}

ObjectTemplates:addTemplate(object_tangible_inventory_companion_inventory, "object/tangible/inventory/companion_inventory.iff")
