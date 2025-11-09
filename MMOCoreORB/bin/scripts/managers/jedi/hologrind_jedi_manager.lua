-- scripts/managers/jedi/hologrind_jedi_manager.lua
-- Force Accumulation (no profession grind), pre-Pub9 conversion
-- Holocron grants FP, shows hint, enables optional auto-10k-on-login
-- Auto-10k is now opt-in (via holocron), not automatic for everyone

local JediManager = require("managers.jedi.jedi_manager")

jediManagerName = "HologrindJediManager"

-- ========= Accumulation tuning =========
local FORCE_TO_UNLOCK            = 10000   -- total needed to convert this toon to Jedi
local STARTING_JEDI_SKILL        = "force_title_jedi_novice"
local STARTING_JEDI_STATE        = 1

-- Passive Force regen
local FORCE_REGEN_TICK_MS        = 5000
local FORCE_REGEN_AMOUNT         = 25

-- Legacy migration
local LEGACY_REQUIRED_MASTERED   = 3
local LEGACY_FORCE_GRANT         = 10000

-- Per-kill Force
local FP_KILL_BASE               = 6
local FP_KILL_LEVEL_SCALE        = 0.50
local FP_KILL_MIN                = 1
local FP_KILL_MAX                = 30

-- Mission/dungeon Force
local FP_MISSION_BASE            = 180
local FP_MISSION_TIER_BONUS      = 90
local FP_DUNGEON_BASE            = 800
local FP_DUNGEON_TIER_BONUS      = 300

-- Group bonus
local GROUP_STEP                 = 0.12
local GROUP_MAX                  = 2.0

-- Holocron behavior
local HOLOCRON_FORCE_AWARD       = 750     -- FP per holocron use
local AUTO_LOGIN_FORCE_AWARD     = 10000   -- what to give on login when option was enabled

-- Compatibility vars for the old hologrind snippet
NUMBEROFPROFESSIONSTOMASTER                  = 3
MAXIMUMNUMBEROFPROFESSIONSTOSHOWWITHHOLOCRON = 1

HologrindJediManager = JediManager:new {
  screenplayName      = jediManagerName,
  jediManagerName     = jediManagerName,
  jediProgressionType = HOLOGRINDJEDIPROGRESSION,
  startingEvent       = nil,
}

-- ===========================================================
-- Profession list: COMBAT ONLY (for legacy detection only)
-- ===========================================================
function HologrindJediManager:getGrindableProfessionList()
  return {
    { "combat_bountyhunter_master", COMBAT_BOUNTYHUNTER_MASTER },
    { "combat_brawler_master",      COMBAT_BRAWLER_MASTER      },
    { "combat_carbine_master",      COMBAT_CARBINE_MASTER      },
    { "combat_commando_master",     COMBAT_COMMANDO_MASTER     },
    { "combat_1hsword_master",      COMBAT_1HSWORD_MASTER      },
    { "combat_marksman_master",     COMBAT_MARKSMAN_MASTER     },
    { "combat_polearm_master",      COMBAT_POLEARM_MASTER      },
    { "combat_pistol_master",       COMBAT_PISTOL_MASTER       },
    { "combat_rifleman_master",     COMBAT_RIFLEMAN_MASTER     },
    { "combat_smuggler_master",     COMBAT_SMUGGLER_MASTER     },
    { "combat_2hsword_master",      COMBAT_2HSWORD_MASTER      },
    { "combat_unarmed_master",      COMBAT_UNARMED_MASTER      },
  }
end

function HologrindJediManager:getProfessionStringIdFromBadgeNumber(badgeNumber)
  local list = self:getGrindableProfessionList()
  for i = 1, #list do
    if list[i][2] == badgeNumber then
      return list[i][1]
    end
  end
  return "Unknown profession"
end

function HologrindJediManager:getNumberOfMasteredProfessions(pCreature)
  local pGhost = CreatureObject(pCreature):getPlayerObject()
  if not pGhost then return 0 end
  local profs = PlayerObject(pGhost):getHologrindProfessions()
  local n = 0
  for i = 1, #profs do
    if PlayerObject(pGhost):hasBadge(profs[i]) then
      n = n + 1
    end
  end
  return n
end

-- =========================
-- Data persistence helpers
-- =========================
local PREFIX = "hgj:"
local function oid(creo) return SceneObject(creo):getObjectID() end
local function K(creo, suffix) return PREFIX .. tostring(oid(creo)) .. ":" .. suffix end
-- Keys:
--   fp           -> current force
--   regen        -> regen enabled
--   migrated     -> legacy migration done
--   auto10k_enabled -> player opted in (via holocron) to get 10k on a later login
--   autogranted  -> 10k actually given already

function HologrindJediManager:getForcePoints(pCreature)
  local v = tonumber(readData(K(pCreature,"fp")))
  return v or 0
end

function HologrindJediManager:setForcePoints(pCreature, val)
  if not pCreature then return end
  if not val then val = 0 end
  if val < 0 then val = 0 end
  writeData(K(pCreature,"fp"), math.floor(val))
end

function HologrindJediManager:addForcePoints(pCreature, amt)
  if not pCreature or not amt or amt == 0 then return end
  local now = self:getForcePoints(pCreature) + math.floor(amt)
  self:setForcePoints(pCreature, now)
  self:checkAndConvertIfReady(pCreature)
end

-- =========================
-- Group bonus helpers
-- =========================
local function getGroupSizeFor(creo)
  if CreatureObject(creo).getGroupSize then
    local n = CreatureObject(creo):getGroupSize()
    if n and n > 0 then return n end
  end
  if CreatureObject(creo).getGroupObject then
    local pGroup = CreatureObject(creo):getGroupObject()
    if pGroup and pGroup.getMemberCount then
      local n = pGroup:getMemberCount()
      if n and n > 0 then return n end
    elseif pGroup and pGroup.getGroupSize then
      local n = pGroup:getGroupSize()
      if n and n > 0 then return n end
    end
  end
  return 1
end

local function applyGroupBonus(creo, base)
  local size = getGroupSizeFor(creo)
  if size <= 1 then return base end
  local mult = 1.0 + (size - 1) * GROUP_STEP
  if mult > GROUP_MAX then mult = GROUP_MAX end
  return math.floor(base * mult)
end

-- =========================
-- Conversion & regen
-- =========================
function HologrindJediManager:isJedi(pCreature)
  local pGhost = CreatureObject(pCreature):getPlayerObject()
  return pGhost and PlayerObject(pGhost):isJedi() or false
end

function HologrindJediManager:convertToJedi(pCreature)
  local pGhost = CreatureObject(pCreature):getPlayerObject()
  if not pGhost or self:isJedi(pCreature) then return end

  PlayerObject(pGhost):setJediState(STARTING_JEDI_STATE)
  awardSkill(pCreature, STARTING_JEDI_SKILL)
  writeData(K(pCreature,"regen"), 1)
  self:scheduleForceRegenTick(pCreature)

  CreatureObject(pCreature):sendSystemMessage("You feel the Force awaken within you. You are now a Jedi.")
end

function HologrindJediManager:checkAndConvertIfReady(pCreature)
  if self:isJedi(pCreature) then return end
  if self:getForcePoints(pCreature) >= FORCE_TO_UNLOCK then
    self:convertToJedi(pCreature)
  end
end

function HologrindJediManager:scheduleForceRegenTick(pCreature)
  createEvent(FORCE_REGEN_TICK_MS, "HologrindJediManager", "forceRegenTick", pCreature, "")
end

function HologrindJediManager:forceRegenTick(pCreature, _)
  if tonumber(readData(K(pCreature,"regen"))) == 1 then
    local pGhost = CreatureObject(pCreature):getPlayerObject()
    if not pGhost then return end
    local po = PlayerObject(pGhost)
    if po.getForcePower and po.getMaxForcePower and po.setForcePower then
      local cur, max = po:getForcePower(), po:getMaxForcePower()
      if cur < max then
        local add = FORCE_REGEN_AMOUNT
        if cur + add > max then add = max - cur end
        if add > 0 then po:setForcePower(cur + add) end
      end
    end
    self:scheduleForceRegenTick(pCreature)
  end
end

-- =========================
-- Legacy/village migration
-- =========================
function HologrindJediManager:legacyAutograntIfEligible(pCreature)
  if tonumber(readData(K(pCreature,"migrated"))) == 1 then return end
  local pGhost = CreatureObject(pCreature):getPlayerObject()
  if not pGhost then return end
  local mastered = self:getNumberOfMasteredProfessions(pCreature)
  if mastered >= LEGACY_REQUIRED_MASTERED then
    if self:getForcePoints(pCreature) < LEGACY_FORCE_GRANT then
      self:setForcePoints(pCreature, LEGACY_FORCE_GRANT)
    end
    writeData(K(pCreature,"migrated"), 1)
    CreatureObject(pCreature):sendSystemMessage("Your past trials resonate with the Force. Your attunement surges to a new height.")
    self:checkAndConvertIfReady(pCreature)
  else
    writeData(K(pCreature,"migrated"), 1)
  end
end

-- =========================
-- Per-kill Force
-- =========================
function HologrindJediManager:onKilledCreature(pCreature, pVictim)
  if not pCreature or not pVictim then return 0 end
  if self:isJedi(pCreature) then return 0 end
  if SceneObject(pVictim):isPlayerCreature() then return 0 end

  local fp = FP_KILL_BASE
  local lvlP, lvlV = nil, nil
  if CreatureObject(pCreature).getLevel then lvlP = CreatureObject(pCreature):getLevel() end
  if CreatureObject(pVictim).getLevel then lvlV = CreatureObject(pVictim):getLevel() end
  if lvlP and lvlV then
    fp = fp + ((lvlV - lvlP) * FP_KILL_LEVEL_SCALE)
  end
  if fp < FP_KILL_MIN then fp = FP_KILL_MIN end
  if fp > FP_KILL_MAX then fp = FP_KILL_MAX end

  fp = applyGroupBonus(pCreature, fp)
  self:addForcePoints(pCreature, fp)
  return 0
end

-- =========================
-- Terminal missions / Dungeons
-- =========================
function HologrindJediManager:notifyMissionCompleted(pCreature, missionType, tier)
  if not pCreature then return end
  local t = tonumber(tier) or 1
  if t < 1 then t = 1 end
  local base = FP_MISSION_BASE + (t - 1) * FP_MISSION_TIER_BONUS
  local fp = applyGroupBonus(pCreature, base)
  self:addForcePoints(pCreature, fp)
  CreatureObject(pCreature):sendSystemMessage(string.format("Your %s success strengthens your resolve. (+%d Force)", missionType or "mission", fp))
end

function HologrindJediManager:notifyDungeonCompleted(pCreature, dungeonName, tier)
  if not pCreature then return end
  local t = tonumber(tier) or 1
  if t < 1 then t = 1 end
  local base = FP_DUNGEON_BASE + (t - 1) * FP_DUNGEON_TIER_BONUS
  local fp = applyGroupBonus(pCreature, base)
  self:addForcePoints(pCreature, fp)
  CreatureObject(pCreature):sendSystemMessage(string.format("Victory in %s deepens your attunement. (+%d Force)", dungeonName or "the depths", fp))
end

function HologrindJediManager:addForceForMission(pCreature, amountOverride)
  if not pCreature then return end
  local fp = applyGroupBonus(pCreature, amountOverride or FP_MISSION_BASE)
  self:addForcePoints(pCreature, fp)
end

function HologrindJediManager:addForceForDungeon(pCreature, amountOverride)
  if not pCreature then return end
  local fp = applyGroupBonus(pCreature, amountOverride or FP_DUNGEON_BASE)
  self:addForcePoints(pCreature, fp)
end

-- =========================
-- Holocrons (grant FP, enable auto-10k, destroy)
-- =========================
local function isHolocronTemplate(path)
  if not path then return false end
  local t = string.lower(path)
  return t == "object/tangible/jedi/jedi_holocron_light.iff"
      or t == "object/tangible/jedi/jedi_holocron_dark.iff"
end

-- original snippet
function HologrindJediManager:sendHolocronMessage(pCreatureObject)
	if self:getNumberOfMasteredProfessions(pCreatureObject) >= MAXIMUMNUMBEROFPROFESSIONSTOSHOWWITHHOLOCRON then
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

local function whisperForceProgress(pCreature)
  local cur = HologrindJediManager:getForcePoints(pCreature)
  local need = FORCE_TO_UNLOCK
  CreatureObject(pCreature):sendSystemMessage(string.format("You sense the Force… %d / %d.", cur, need))
end

function HologrindJediManager:useItem(pSceneObject, _, pCreature)
  if not pCreature or not pSceneObject then return end
  local template = SceneObject(pSceneObject):getTemplateObjectPath()
  if not isHolocronTemplate(template) then return end

  -- 1) grant FP
  self:addForcePoints(pCreature, HOLOCRON_FORCE_AWARD)

  -- 2) show classic holocron message
  self:sendHolocronMessage(pCreature)

  -- 3) show numeric progress
  whisperForceProgress(pCreature)

  -- 4) enable optional auto-10k on future login for this toon
  writeData(K(pCreature,"auto10k_enabled"), 1)
  CreatureObject(pCreature):sendSystemMessage("The holocron imparts deeper insight. Future meditations will hasten your awakening.")

  -- 5) destroy the holocron
  SceneObject(pSceneObject):destroyObjectFromWorld()
  SceneObject(pSceneObject):destroyObjectFromDatabase()
end

-- =========================
-- Lifecycle / observers
-- =========================
function HologrindJediManager:onPlayerCreated(pCreature)
  -- nothing
end

function HologrindJediManager:onPlayerLoggedIn(pCreature)
  if not pCreature then return end

  -- legacy/village check
  self:legacyAutograntIfEligible(pCreature)

  -- if player opted in via holocron earlier, grant 10k once now
  if not self:isJedi(pCreature) then
    local enabled  = tonumber(readData(K(pCreature,"auto10k_enabled"))) or 0
    local granted  = tonumber(readData(K(pCreature,"autogranted"))) or 0
    if enabled == 1 and granted ~= 1 then
      self:setForcePoints(pCreature, AUTO_LOGIN_FORCE_AWARD)
      writeData(K(pCreature,"autogranted"), 1)
      self:checkAndConvertIfReady(pCreature)
    end
  end

  -- resume regen if enabled
  if tonumber(readData(K(pCreature,"regen"))) == 1 then
    self:scheduleForceRegenTick(pCreature)
  end

  -- per-kill FP observer
  createObserver(KILLEDCREATURE, "HologrindJediManager", "onKilledCreature", pCreature)
end

function HologrindJediManager:canLearnSkill()
  return true
end

registerScreenPlay("HologrindJediManager", true)
return HologrindJediManager
