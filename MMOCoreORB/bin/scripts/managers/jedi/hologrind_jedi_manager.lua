-- scripts/managers/jedi/hologrind_jedi_manager.lua
-- Force Accumulation (no profession grind), pre-Pub9 style conversion
-- Holocrons destroyed on use
-- Persists only via readData/writeData

local JediManager = require("managers.jedi.jedi_manager")

jediManagerName = "HologrindJediManager"

-- ========= Accumulation tuning =========
local FORCE_TO_UNLOCK            = 10000
local STARTING_JEDI_SKILL        = "force_title_jedi_novice"
local STARTING_JEDI_STATE        = 1    -- kept for reference

-- Passive Force regen after conversion
local FORCE_REGEN_TICK_MS        = 5000
local FORCE_REGEN_AMOUNT         = 25

-- Legacy migration (one time grant if old hologrind was already completed)
local LEGACY_REQUIRED_MASTERED   = 3
local LEGACY_FORCE_GRANT         = 10000

-- Per kill Force
local FP_KILL_BASE               = 6
local FP_KILL_LEVEL_SCALE        = 0.50
local FP_KILL_MIN                = 1
local FP_KILL_MAX                = 30

-- Terminal missions
local FP_MISSION_BASE            = 180
local FP_MISSION_TIER_BONUS      = 90

-- Dungeons
local FP_DUNGEON_BASE            = 800
local FP_DUNGEON_TIER_BONUS      = 300

-- Group bonus
local GROUP_STEP                 = 0.12
local GROUP_MAX                  = 2.0

-- Compatibility variables kept for holocron snippet
NUMBEROFPROFESSIONSTOMASTER                  = 3
MAXIMUMNUMBEROFPROFESSIONSTOSHOWWITHHOLOCRON = 1

HologrindJediManager = JediManager:new {
  screenplayName      = jediManagerName,
  jediManagerName     = jediManagerName,
  jediProgressionType = HOLOGRINDJEDIPROGRESSION,
  startingEvent       = nil,
}

-- ===========================================================
-- Profession list: combat only, for legacy detection only
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
  if not pGhost then
    return 0
  end
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
local PREFIX = "hgj:"  -- hologrind_jedi
local function oid(creo)
  return SceneObject(creo):getObjectID()
end
local function K(creo, suffix)
  return PREFIX .. tostring(oid(creo)) .. ":" .. suffix
end
-- Keys per character:
--   K(creo,"fp")         -> integer current Force Points
--   K(creo,"regen")      -> 1 if regen enabled
--   K(creo,"migrated")   -> 1 if legacy autogrant performed
--   K(creo,"villagemig") -> 1 if village-to-hologrind/FRS choice was already presented
--   K(creo,"autofp")     -> 1 if "auto 10000 FP on login" is enabled

function HologrindJediManager:getForcePoints(pCreature)
  local v = tonumber(readData(K(pCreature, "fp")))
  return v or 0
end

function HologrindJediManager:setForcePoints(pCreature, val)
  if not pCreature then
    return
  end
  if not val then
    val = 0
  end
  if val < 0 then
    val = 0
  end
  writeData(K(pCreature, "fp"), math.floor(val))
end

function HologrindJediManager:addForcePoints(pCreature, amt)
  if not pCreature or not amt or amt == 0 then
    return
  end
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
    if n and n > 0 then
      return n
    end
  end
  if CreatureObject(creo).getGroupObject then
    local pGroup = CreatureObject(creo):getGroupObject()
    if pGroup and pGroup.getMemberCount then
      local n = pGroup:getMemberCount()
      if n and n > 0 then
        return n
      end
    elseif pGroup and pGroup.getGroupSize then
      local n = pGroup:getGroupSize()
      if n and n > 0 then
        return n
      end
    end
  end
  return 1
end

local function applyGroupBonus(creo, base)
  local size = getGroupSizeFor(creo)
  if size <= 1 then
    return base
  end
  local mult = 1.0 + (size - 1) * GROUP_STEP
  if mult > GROUP_MAX then
    mult = GROUP_MAX
  end
  return math.floor(base * mult)
end

-- =========================
-- Conversion and regen
-- =========================
function HologrindJediManager:isJedi(pCreature)
  local pGhost = CreatureObject(pCreature):getPlayerObject()
  return pGhost and PlayerObject(pGhost):isJedi() or false
end

-- FRS-compatible convert
function HologrindJediManager:convertToJedi(pCreature)
  local pGhost = CreatureObject(pCreature):getPlayerObject()
  if not pGhost or self:isJedi(pCreature) then
    return
  end

  PlayerObject(pGhost):setJediState(2)
  PlayerObject(pGhost):setJediProgressionType(4) -- HOLOGRINDJEDIPROGRESSION

  awardSkill(pCreature, "force_title_jedi_novice")
  awardSkill(pCreature, "force_title_jedi_rank_02")

  local pInventory = SceneObject(pCreature):getSlottedObject("inventory")
  if pInventory ~= nil and not SceneObject(pInventory):isContainerFullRecursive() then
    giveItem(pInventory, "object/tangible/wearables/robe/robe_jedi_padawan.iff", -1)
  else
    CreatureObject(pCreature):sendSystemMessage("@jedi_spam:inventory_full_jedi_robe")
  end

  CreatureObject(pCreature):playEffect("clienteffect/trap_electric_01.cef", "")
  CreatureObject(pCreature):playMusicMessage("sound/music_become_jedi.snd")

  writeData(K(pCreature, "regen"), 1)
  self:scheduleForceRegenTick(pCreature)

  CreatureObject(pCreature):sendSystemMessage("The holocron knowledge surges through you. The Force flows. You are now a Jedi.")
end

function HologrindJediManager:checkAndConvertIfReady(pCreature)
  if self:isJedi(pCreature) then
    return
  end
  if self:getForcePoints(pCreature) >= FORCE_TO_UNLOCK then
    self:convertToJedi(pCreature)
  end
end

function HologrindJediManager:scheduleForceRegenTick(pCreature)
  createEvent(FORCE_REGEN_TICK_MS, "HologrindJediManager", "forceRegenTick", pCreature, "")
end

function HologrindJediManager:forceRegenTick(pCreature, _)
  if tonumber(readData(K(pCreature, "regen"))) == 1 then
    local pGhost = CreatureObject(pCreature):getPlayerObject()
    if not pGhost then
      return
    end
    local po = PlayerObject(pGhost)
    if po.getForcePower and po.getMaxForcePower and po.setForcePower then
      local cur = po:getForcePower()
      local max = po:getMaxForcePower()
      if cur < max then
        local add = FORCE_REGEN_AMOUNT
        if cur + add > max then
          add = max - cur
        end
        if add > 0 then
          po:setForcePower(cur + add)
        end
      end
    end
    self:scheduleForceRegenTick(pCreature)
  end
end

-- =========================
-- Legacy migration
-- =========================
function HologrindJediManager:legacyAutograntIfEligible(pCreature)
  if tonumber(readData(K(pCreature, "migrated"))) == 1 then
    return
  end
  local pGhost = CreatureObject(pCreature):getPlayerObject()
  if not pGhost then
    return
  end
  local mastered = self:getNumberOfMasteredProfessions(pCreature)
  if mastered >= LEGACY_REQUIRED_MASTERED then
    if self:getForcePoints(pCreature) < LEGACY_FORCE_GRANT then
      self:setForcePoints(pCreature, LEGACY_FORCE_GRANT)
    end
    writeData(K(pCreature, "migrated"), 1)
    CreatureObject(pCreature):sendSystemMessage("Your past trials resonate with the Force. Your attunement surges to a new height.")
    self:checkAndConvertIfReady(pCreature)
  else
    writeData(K(pCreature, "migrated"), 1)
  end
end

-- =========================
-- Village → hologrind/FRS migration (optional)
-- =========================
function HologrindJediManager:shouldOfferVillageMigration(pCreature)
  if not pCreature then
    return false
  end
  if tonumber(readData(K(pCreature, "villagemig"))) == 1 then
    return false
  end
  local pGhost = CreatureObject(pCreature):getPlayerObject()
  if not pGhost then
    return false
  end
  if not PlayerObject(pGhost):isJedi() then
    return false
  end

  local prog = 0
  if PlayerObject(pGhost).getJediProgressionType then
    prog = PlayerObject(pGhost):getJediProgressionType()
  end

  local hasVillageFlag = CreatureObject(pCreature):hasScreenPlayState(32, "VillageJediProgression")

  if prog ~= 4 or hasVillageFlag then
    return true
  end
  return false
end

function HologrindJediManager:showVillageMigrationChoice(pCreature)
  local sui = LuaSuiManager()
  local text = "This character appears to be an older Village-unlocked Jedi.\n\nDo you want to migrate this Jedi to the hologrind/FRS-compatible progression so it stays consistent with your custom holocron unlock?"
  sui:sendMessageBox(pCreature, pCreature,
    "Jedi Migration",
    text,
    "@yes",
    "@no",
    "HologrindJediManager",
    "onVillageMigrationChoice")
end

function HologrindJediManager:onVillageMigrationChoice(pCreature, pSui, eventIndex, ...)
  if not pCreature then
    return
  end
  if eventIndex == 0 then
    local pGhost = CreatureObject(pCreature):getPlayerObject()
    if pGhost then
      PlayerObject(pGhost):setJediProgressionType(4)
      writeData(K(pCreature, "regen"), 1)
      self:scheduleForceRegenTick(pCreature)
      CreatureObject(pCreature):sendSystemMessage("Your Jedi progression has been migrated to the hologrind/FRS path.")
    end
    writeData(K(pCreature, "villagemig"), 1)
  else
    writeData(K(pCreature, "villagemig"), 1)
    CreatureObject(pCreature):sendSystemMessage("You chose to keep your existing Village Jedi configuration.")
  end
end

-- =========================
-- Per kill Force
-- =========================
function HologrindJediManager:onKilledCreature(pCreature, pVictim)
  if not pCreature or not pVictim then
    return 0
  end
  if self:isJedi(pCreature) then
    return 0
  end
  if SceneObject(pVictim):isPlayerCreature() then
    return 0
  end

  local fp = FP_KILL_BASE
  local lvlP, lvlV = nil, nil
  if CreatureObject(pCreature).getLevel then
    lvlP = CreatureObject(pCreature):getLevel()
  end
  if CreatureObject(pVictim).getLevel then
    lvlV = CreatureObject(pVictim):getLevel()
  end
  if lvlP and lvlV then
    fp = fp + ((lvlV - lvlP) * FP_KILL_LEVEL_SCALE)
  end
  if fp < FP_KILL_MIN then
    fp = FP_KILL_MIN
  end
  if fp > FP_KILL_MAX then
    fp = FP_KILL_MAX
  end

  fp = applyGroupBonus(pCreature, fp)
  self:addForcePoints(pCreature, fp)
  return 0
end

-- =========================
-- Mission and dungeon hooks
-- =========================
function HologrindJediManager:notifyMissionCompleted(pCreature, missionType, tier)
  if not pCreature then
    return
  end
  local t = tonumber(tier) or 1
  if t < 1 then
    t = 1
  end
  local base = FP_MISSION_BASE + (t - 1) * FP_MISSION_TIER_BONUS
  local fp = applyGroupBonus(pCreature, base)
  self:addForcePoints(pCreature, fp)
  CreatureObject(pCreature):sendSystemMessage(string.format("Your %s success strengthens your resolve. (+%d Force)", missionType or "mission", fp))
end

function HologrindJediManager:notifyDungeonCompleted(pCreature, dungeonName, tier)
  if not pCreature then
    return
  end
  local t = tonumber(tier) or 1
  if t < 1 then
    t = 1
  end
  local base = FP_DUNGEON_BASE + (t - 1) * FP_DUNGEON_TIER_BONUS
  local fp = applyGroupBonus(pCreature, base)
  self:addForcePoints(pCreature, fp)
  CreatureObject(pCreature):sendSystemMessage(string.format("Victory in %s deepens your attunement. (+%d Force)", dungeonName or "the depths", fp))
end

function HologrindJediManager:addForceForMission(pCreature, amountOverride)
  if not pCreature then
    return
  end
  local fp = applyGroupBonus(pCreature, amountOverride or FP_MISSION_BASE)
  self:addForcePoints(pCreature, fp)
end

function HologrindJediManager:addForceForDungeon(pCreature, amountOverride)
  if not pCreature then
    return
  end
  local fp = applyGroupBonus(pCreature, amountOverride or FP_DUNGEON_BASE)
  self:addForcePoints(pCreature, fp)
end

-- =========================
-- Holocrons (destroy on use) + toggle for auto FP on login
-- =========================
local function isHolocronTemplate(path)
  if not path then
    return false
  end
  local t = string.lower(path)
  return t == "object/tangible/jedi/jedi_holocron_light.iff"
      or t == "object/tangible/jedi/jedi_holocron_dark.iff"
end

function HologrindJediManager:sendHolocronMessage(pCreatureObject)
  if self:getNumberOfMasteredProfessions(pCreatureObject) >= MAXIMUMNUMBEROFPROFESSIONSTOSHOWWITHHOLOCRON then
    CreatureObject(pCreatureObject):sendSystemMessage("@jedi_spam:holocron_quiet")
    return true
  else
    local pGhost = CreatureObject(pCreatureObject):getPlayerObject()

    if pGhost == nil then
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
  CreatureObject(pCreature):sendSystemMessage(string.format("You sense the Force... %d / %d.", cur, need))
end

-- show toggle SUI after holocron use
function HologrindJediManager:showAutoFPChoice(pCreature)
  local current = tonumber(readData(K(pCreature, "autofp"))) or 0
  local isOn = (current == 1)
  local desc
  if isOn then
    desc = "Auto 10000 Force Points on login is currently ENABLED.\n\nDo you want to DISABLE it for this character?"
  else
    desc = "Auto 10000 Force Points on login is currently DISABLED.\n\nDo you want to ENABLE it for this character?"
  end

  local sui = LuaSuiManager()
  sui:sendMessageBox(
    pCreature,
    pCreature,
    "Holocron: Auto Force Option",
    desc,
    "@yes",
    "@no",
    "HologrindJediManager",
    "onAutoFPChoice"
  )
end

function HologrindJediManager:onAutoFPChoice(pCreature, pSui, eventIndex, ...)
  if not pCreature then
    return
  end
  if eventIndex == 0 then
    -- toggle
    local current = tonumber(readData(K(pCreature, "autofp"))) or 0
    if current == 1 then
      writeData(K(pCreature, "autofp"), 0)
      CreatureObject(pCreature):sendSystemMessage("Auto 10000 Force Points on login has been DISABLED for this character.")
    else
      writeData(K(pCreature, "autofp"), 1)
      CreatureObject(pCreature):sendSystemMessage("Auto 10000 Force Points on login has been ENABLED for this character.")
    end
  else
    CreatureObject(pCreature):sendSystemMessage("Auto Force option unchanged.")
  end
end

function HologrindJediManager:useItem(pSceneObject, _, pCreature)
  if not pCreature or not pSceneObject then
    return
  end
  local template = SceneObject(pSceneObject):getTemplateObjectPath()
  if not isHolocronTemplate(template) then
    return
  end

  self:sendHolocronMessage(pCreature)
  whisperForceProgress(pCreature)
  self:checkAndConvertIfReady(pCreature)

  -- show toggle
  self:showAutoFPChoice(pCreature)

  -- destroy holocron
  SceneObject(pSceneObject):destroyObjectFromWorld()
  SceneObject(pSceneObject):destroyObjectFromDatabase()
end

-- =========================
-- Lifecycle and observers
-- =========================
function HologrindJediManager:onPlayerCreated(pCreature)
  -- nothing heavy here
end

function HologrindJediManager:onPlayerLoggedIn(pCreature)
  if not pCreature then
    return
  end

  -- auto FP on login if enabled
  if tonumber(readData(K(pCreature, "autofp"))) == 1 then
    self:setForcePoints(pCreature, FORCE_TO_UNLOCK)
  end

  self:legacyAutograntIfEligible(pCreature)

  if tonumber(readData(K(pCreature, "regen"))) == 1 then
    self:scheduleForceRegenTick(pCreature)
  end

  self:checkAndConvertIfReady(pCreature)
  createObserver(KILLEDCREATURE, "HologrindJediManager", "onKilledCreature", pCreature)

  local pGhost = CreatureObject(pCreature):getPlayerObject()
  if pGhost and PlayerObject(pGhost):isJedi() then
    if PlayerObject(pGhost).setJediProgressionType then
      PlayerObject(pGhost):setJediProgressionType(4)
    end
  end

  if self:shouldOfferVillageMigration(pCreature) then
    self:showVillageMigrationChoice(pCreature)
  end
end

function HologrindJediManager:canLearnSkill()
  return true
end

registerScreenPlay("HologrindJediManager", true)
return HologrindJediManager
