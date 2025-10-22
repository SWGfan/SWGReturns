-- scripts/managers/jedi/hologrind_jedi_manager.lua
-- Hologrind → Force Accumulation (pre-Pub9), holocrons destroyed on use, immediate conversion

local JediManager   = require("managers.jedi.jedi_manager")
local ObjectManager = require("managers.object.object_manager")

jediManagerName = "HologrindJediManager"

-- ========= Accumulation tuning =========
local FORCE_TO_UNLOCK           = 10000        -- total needed to unlock Jedi
local FORCE_PER_KILL_BASE       = 8            -- per PvE kill (will be group-scaled)
local FORCE_PER_MISSION_BASE    = 250          -- per terminal mission completion
local FORCE_PER_DUNGEON_BASE    = 1500         -- per “major” dungeon/themepark completion
local GROUP_BONUS_STEP          = 0.15         -- +15% per extra groupmate (capped)
local GROUP_BONUS_MAX           = 0.75         -- up to +75% total bonus

-- Autoconversion for legacy hologrind players
local AUTO_GRANT_ON_OLD_GRIND   = true
local LEGACY_FORCE_GRANT        = 10000        -- grant once if old hologrind complete

-- ========= Pre-Pub9 path =========
local STARTING_JEDI_SKILL       = "force_title_jedi_novice"
local STARTING_JEDI_STATE       = 1            -- 1 = padawan/jedi novice (pre-Pub9)

-- ========= ObjVar keys (persist on the player creature) =========
local O_FORCE_POINTS            = "holo.force_points"
local O_AUTO_GRANT_DONE         = "holo.legacy_autogrant_done"

-- ========= “Compatibility” vars retained for your snippet =========
NUMBEROFPROFESSIONSTOMASTER                 = 3   -- only used for legacy conversion check
MAXIMUMNUMBEROFPROFESSIONSTOSHOWWITHHOLOCRON = 1  -- your requested snippet uses this

HologrindJediManager = JediManager:new {
  screenplayName      = jediManagerName,
  jediManagerName     = jediManagerName,
  jediProgressionType = HOLOGRINDJEDIPROGRESSION,
  startingEvent       = nil,
}

-- ===========================================================
-- Profession list: COMBAT ONLY (commented out non-combat)
-- (needed only for legacy “old hologrind complete” detection)
-- ===========================================================
function HologrindJediManager:getGrindableProfessionList()
  local t = {
    { "combat_bountyhunter_master", COMBAT_BOUNTYHUNTER_MASTER  },
    { "combat_brawler_master",      COMBAT_BRAWLER_MASTER       },
    { "combat_carbine_master",      COMBAT_CARBINE_MASTER       },
    { "combat_commando_master",     COMBAT_COMMANDO_MASTER      },
    { "combat_1hsword_master",      COMBAT_1HSWORD_MASTER       },
    { "combat_marksman_master",     COMBAT_MARKSMAN_MASTER      },
    { "combat_polearm_master",      COMBAT_POLEARM_MASTER       },
    { "combat_pistol_master",       COMBAT_PISTOL_MASTER        },
    { "combat_rifleman_master",     COMBAT_RIFLEMAN_MASTER      },
    { "combat_smuggler_master",     COMBAT_SMUGGLER_MASTER      },
    { "combat_2hsword_master",      COMBAT_2HSWORD_MASTER       },
    { "combat_unarmed_master",      COMBAT_UNARMED_MASTER       },
    -- non-combat intentionally commented:
    -- { "science_combatmedic_master",  SCIENCE_COMBATMEDIC_MASTER },
    -- { "outdoors_ranger_master",      OUTDOORS_RANGER_MASTER    },
    -- { "outdoors_scout_master",       OUTDOORS_SCOUT_MASTER     },
    -- { "outdoors_squadleader_master", OUTDOORS_SQUADLEADER_MASTER },
  }
  return t
end

-- ===================================
-- Helpers: persistent Force read/set
-- ===================================
local function getForce(pCreature)
  if (pCreature == nil) then return 0 end
  local v = SceneObject(pCreature):getObjVar(O_FORCE_POINTS)
  if v == nil then return 0 end
  return tonumber(v) or 0
end

local function setForce(pCreature, value)
  if (pCreature == nil) then return end
  SceneObject(pCreature):setObjVar(O_FORCE_POINTS, math.max(0, math.floor(value)))
end

local function addForce(pCreature, delta)
  if (pCreature == nil or delta == nil) then return 0 end
  local cur = getForce(pCreature)
  local now = cur + math.floor(delta)
  setForce(pCreature, now)
  return now
end

-- Group bonus: +15% per extra member, capped (does not penalize solo)
local function applyGroupBonus(pCreature, base)
  if (pCreature == nil) then return base end
  local pGroup = CreatureObject(pCreature):getGroupObject()
  if pGroup == nil then return base end
  local size = GroupObject(pGroup):getGroupSize()
  if not size or size < 2 then return base end
  local bonus = math.min(GROUP_BONUS_MAX, (size - 1) * GROUP_BONUS_STEP)
  return math.floor(base + (base * bonus))
end

-- ============================
-- Legacy hologrind converters
-- ============================
function HologrindJediManager:getNumberOfMasteredProfessions(pCreature)
  local pGhost = CreatureObject(pCreature):getPlayerObject()
  if (pGhost == nil) then return 0 end
  local profs = PlayerObject(pGhost):getHologrindProfessions()
  local n = 0
  for i = 1, #profs do
    if PlayerObject(pGhost):hasBadge(profs[i]) then n = n + 1 end
  end
  return n
end

function HologrindJediManager:getProfessionStringIdFromBadgeNumber(badgeNumber)
  local list = self:getGrindableProfessionList()
  for i = 1, #list do
    if list[i][2] == badgeNumber then return list[i][1] end
  end
  return "Unknown profession"
end

-- ========= Jedi state helpers =========
function HologrindJediManager:isJedi(pCreature)
  local pGhost = CreatureObject(pCreature):getPlayerObject()
  return pGhost and PlayerObject(pGhost):isJedi() or false
end

local function convertToJediNow(pCreature)
  if (pCreature == nil) then return end
  if CreatureObject(pCreature):hasSkill(STARTING_JEDI_SKILL) then return end
  PlayerObject(CreatureObject(pCreature):getPlayerObject()):setJediState(STARTING_JEDI_STATE)
  awardSkill(pCreature, STARTING_JEDI_SKILL)
  CreatureObject(pCreature):sendSystemMessage("You feel the Living Force surge through you. You are now a Jedi Novice.")
end

-- =========================================
-- Character creation & login instrumentation
-- =========================================
function HologrindJediManager:onPlayerCreated(pCreature)
  -- nothing special here for accumulation
end

function HologrindJediManager:onPlayerLoggedIn(pCreature)
  if (pCreature == nil) then return end

  -- One-time auto-grant for players who had already finished the old hologrind
  if AUTO_GRANT_ON_OLD_GRIND then
    local already = SceneObject(pCreature):getObjVar(O_AUTO_GRANT_DONE)
    if already ~= 1 then
      local mastered = self:getNumberOfMasteredProfessions(pCreature)
      if mastered >= NUMBEROFPROFESSIONSTOMASTER then
        setForce(pCreature, math.max(getForce(pCreature), LEGACY_FORCE_GRANT))
        SceneObject(pCreature):setObjVar(O_AUTO_GRANT_DONE, 1)
        CreatureObject(pCreature):sendSystemMessage("Ancient efforts echo… your connection to the Force strengthens (+10,000).")
      end
    end
  end

  -- Register kill observer (per-kill Force)
  createObserver(KILLEDCREATURE, "HologrindJediManager", "onKilledCreature", pCreature)

  -- If player already at/above threshold, convert instantly on login
  if not self:isJedi(pCreature) and getForce(pCreature) >= FORCE_TO_UNLOCK then
    convertToJediNow(pCreature)
  end
end

-- Per-kill Force drip
function HologrindJediManager:onKilledCreature(pCreature, pVictim)
  if (pCreature == nil or pVictim == nil) then return 0 end
  if self:isJedi(pCreature) then return 0 end
  -- Only PvE
  if SceneObject(pVictim):isPlayerCreature() then return 0 end

  local add = applyGroupBonus(pCreature, FORCE_PER_KILL_BASE)
  local now = addForce(pCreature, add)

  -- Convert immediately when crossing the line
  if now >= FORCE_TO_UNLOCK then
    convertToJediNow(pCreature)
  end
  return 0
end

-- ==================================================
-- Holocron handling (destroy on use) + your snippet
-- ==================================================
-- Find out and send the response from the holocron to the player
-- @param pCreatureObject pointer to the creature object of the player who used the holocron.
function HologrindJediManager:sendHolocronMessage(pCreatureObject)
	if self:getNumberOfMasteredProfessions(pCreatureObject) >= MAXIMUMNUMBEROFPROFESSIONSTOSHOWWITHHOLOCRON then
		-- The Holocron is quiet. The ancients' knowledge of the Force will no longer assist you on your journey. You must continue seeking on your own.
		CreatureObject(pCreatureObject):sendSystemMessage("@jedi_spam:holocron_quiet")
		return true
	else
		local pGhost = CreatureObject(pCreatureObject):getPlayerObject()

		if (pGhost == nil) then
			return false
		end

		local professions = PlayerObject(pGhost):getHologrindProfessions()
		for i = 1, #professions, 1 do
			if not PlayerObject(pGhost):hasBadge(professions[i]) then
				local professionText = self:getProfessionStringIdFromBadgeNumber(professions[i])
				CreatureObject(pCreatureObject):sendSystemMessageWithTO("@jedi_spam:holocron_light_information", "@skl_n:" .. professionText)
			end
		end

		return false
	end
end

-- Extra: overlay Force progress hint to the player (non-intrusive)
local function whisperForceProgress(pCreature)
  if (pCreature == nil) then return end
  local cur = getForce(pCreature)
  local remain = math.max(0, FORCE_TO_UNLOCK - cur)
  CreatureObject(pCreature):sendSystemMessage(string.format("You sense the Force around you… (%d / %d). %d more needed.", cur, FORCE_TO_UNLOCK, remain))
end

-- Treat any holocron template as valid here
local function isHolocronTemplate(path)
  if not path then return false end
  local t = string.lower(path)
  return t == "object/tangible/jedi/jedi_holocron_light.iff"
      or t == "object/tangible/jedi/jedi_holocron_dark.iff"
end

function HologrindJediManager:useItem(pSceneObject, itemType, pCreature)
  if (pCreature == nil or pSceneObject == nil) then return end

  local template = SceneObject(pSceneObject):getTemplateObjectPath()
  if not isHolocronTemplate(template) then return end

  -- Your requested vanilla holocron “hint” behavior
  self:sendHolocronMessage(pCreature)

  -- Also show the Force accumulation progress in plain numbers
  whisperForceProgress(pCreature)

  -- Convert immediately if already enough
  if not self:isJedi(pCreature) and getForce(pCreature) >= FORCE_TO_UNLOCK then
    convertToJediNow(pCreature)
  end

  -- Destroy holocron after use (classic behavior)
  SceneObject(pSceneObject):destroyObjectFromWorld()
  SceneObject(pSceneObject):destroyObjectFromDatabase()
end

-- ========= Public helpers to award Force =========
-- Call from other scripts:
--   HologrindJediManager:addForceForMission(pPlayer)
--   HologrindJediManager:addForceForDungeon(pPlayer)
function HologrindJediManager:addForceForMission(pPlayer, amountOverride)
  if (pPlayer == nil or self:isJedi(pPlayer)) then return end
  local add = applyGroupBonus(pPlayer, amountOverride or FORCE_PER_MISSION_BASE)
  local now = addForce(pPlayer, add)
  if now >= FORCE_TO_UNLOCK then convertToJediNow(pPlayer) end
end

function HologrindJediManager:addForceForDungeon(pPlayer, amountOverride)
  if (pPlayer == nil or self:isJedi(pPlayer)) then return end
  local add = applyGroupBonus(pPlayer, amountOverride or FORCE_PER_DUNGEON_BASE)
  local now = addForce(pPlayer, add)
  if now >= FORCE_TO_UNLOCK then convertToJediNow(pPlayer) end
end

-- ========= Required by the engine (always allow learning) =========
function HologrindJediManager:canLearnSkill(pPlayer, skillName)
  return true
end

registerScreenPlay("HologrindJediManager", true)
return HologrindJediManager
