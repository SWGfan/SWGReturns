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

--Companion System -- the object template a CompanionControlDevice's
-- spawnObject() (CompanionControlDeviceImplementation.cpp) uses to
-- createObject() the live companion actor. Deliberately, permanently a
-- human male: inherits
-- object_mobile_shared_dressed_creaturehandler_trainer_human_male_01 (the
-- same shared appearance already used by the working
-- trainer_companion_master NPC -- see object/mobile/objects.lua and
-- object/mobile/dressed_creaturehandler_trainer_human_male_01.lua) so no
-- new client assets are required and the appearance is already known-good
-- in this codebase. This is the confirmed final choice (not incidental
-- reuse) -- every companion is a human male, matching the "real
-- character-like" human_male HAM baseline in
-- CompanionObjectImplementation::migrateBaselineStats().
--
-- objectMenuComponent / containerComponent ARE set here (2026-07-12 fix --
-- see docs/companion_system/NOTES.md, "Radial menu shows only Examine on a
-- summoned companion -- root cause and fix"). They were originally left
-- unset on the theory that CompanionControlDeviceImplementation::
-- spawnObject()'s runtime companion->setObjectMenuComponent(...)/
-- setContainerComponent(...) calls would always cover it -- true only for a
-- companion that is freshly summoned via the control device. Any other load
-- path (most importantly ObjectManager::loadPersistentObject() reloading an
-- already-summoned companion from the database on server restart, via
-- instantiateSceneObject(..., createComponents=true) ->
-- SceneObjectImplementation::createComponents() -> createObjectMenuComponent()/
-- createContainerComponent(), which both re-derive the component purely
-- from templateObject->getObjectMenuComponent()/getContainerComponent())
-- never calls spawnObject() again, so a template with no default here left
-- both components null until the next manual store+re-summon -- exactly
-- matching the real working precedent every other component-bearing mobile
-- template in this codebase follows (e.g.
-- object/mobile/vendor/zabrak_male.lua sets both objectMenuComponent =
-- "VendorMenuComponent" and containerComponent = "VendorContainerComponent"
-- directly on its template for the same reason: a vendor NPC is also
-- reloaded from the database across restarts without any per-instance C++
-- spawn call).
--
-- gameObjectType is overridden to SceneObjectType::COMPANIONCREATURE
-- (0x405 = 1029), which is what makes
-- ObjectManager::loadObjectFromTemplate() dispatch to our own
-- CompanionObjectImplementation class instead of the stock Creature class
-- used by every wild/NPC mobile -- see
-- server/zone/managers/object/ObjectManager.cpp,
-- server/zone/objects/scene/SceneObjectType.h, and
-- docs/companion_system/NOTES.md.
-- Companion System (2026-07-15, "equipped gear never renders on the
-- companion" fix -- see NOTES.md): rebased from
-- object_mobile_shared_dressed_creaturehandler_trainer_human_male_01 onto
-- the real PLAYER human male client template. The trainer "dressed_*"
-- client template's real TRE appearance is a canned dressed/underwear NPC
-- mesh that never composes per-slot wearable appearance -- equipped
-- weapons rendered (hand hardpoints work on any skeleton) but clothing/
-- armor never did, no matter how correctly the server slotted the items.
-- The player template is rendering-proven by definition (every player
-- renders their gear through it), and "NPC with a player-type client
-- template" is already a stock client pattern (e.g. kaja_orzee ships with
-- clientGameObjectType 1025/PLAYERCREATURE). The server-side
-- gameObjectType override below (1029, COMPANIONCREATURE) is unaffected --
-- server class dispatch reads OUR value, the client reads its own
-- template's. Existing companions carry the old clientObjectCRC persisted
-- per object -- CompanionControlDeviceImplementation::spawnObject()
-- self-heals it every summon.
object_mobile_companion_actor = object_creature_player_shared_human_male:new {
	gameObjectType = 1029, -- SceneObjectType::COMPANIONCREATURE (0x405)
	objectMenuComponent = "CompanionMenuComponent",
	containerComponent = "CompanionContainerComponent",

	-- Companion System bug fix (2026-07-13, "companion does not follow" --
	-- see docs/companion_system/NOTES.md): object_mobile_shared_dressed_
	-- creaturehandler_trainer_human_male_01 (the base template above) is a
	-- stationary trainer-NPC appearance template -- its own "Data below here
	-- is deprecated, loaded from the tres" block is entirely commented out,
	-- so it contributes NO live optionsBitmask field, and this template
	-- never set one of its own either. SharedTangibleObjectTemplate defaults
	-- optionsBitmask to plain 0 when nothing sets it (see
	-- SharedTangibleObjectTemplate.cpp). AiAgentImplementation::
	-- runBehaviorTree() (the per-tick entry point that actually drives
	-- movement/pathing for FOLLOWING, including CompanionFollowCommand's
	-- setFollowObject() call) hard-gates on
	-- "!(getOptionsBitmask() & OptionBitmask::AIENABLED)" and returns
	-- immediately, every tick, if that bit isn't set -- so the companion's
	-- behavior tree, and therefore all AI-driven movement including follow,
	-- silently never ran at all, regardless of companionState/followObject
	-- being set correctly. AIENABLED is the same flag every ordinary wild
	-- mobile template in this codebase sets (e.g.
	-- object/mobile/corellia/canoid.lua). CONVERSABLE is added alongside it
	-- to match the closest working precedent combo used throughout this
	-- codebase for non-hostile NPCs (e.g. object/mobile/dungeon/warren/
	-- mirla.lua) -- companions are not INVULNERABLE (they can take damage
	-- and die; see CompanionControlDeviceImplementation::
	-- handleCompanionDeath()), so that flag is deliberately not carried over
	-- from the unrelated trainer_companion_master.lua CreatureTemplate.
	optionsBitmask = AIENABLED + CONVERSABLE
}

ObjectTemplates:addTemplate(object_mobile_companion_actor, "object/mobile/companion_actor.iff")
