-- scripts/managers/jedi/hologrind_jedi_manager.lua
-- Auto-unlock on login if not yet Jedi, FRS-compatible when the API exists
-- Uses readData/writeData only

local JediManager = require("managers.jedi.jedi_manager")

jediManagerName = "HologrindJediManager"

-- Core thresholds
local FORCE_TO_UNLOCK            = 10000
local STARTING_JEDI_SKILL        = "force_title_jedi_novice"
local STARTING_JEDI_STATE        = 1

-- Passive Force regen
local FORCE_REGEN_TICK_MS        = 5000
local FORCE_REGEN_AMOUNT         = 25

-- Legacy autogrant (old hologrind)
local LEGACY_REQUIRED_MASTERED   = 3
local LEGACY_FORCE_GRANT         = 10000

-- Per-kill accumulation
local FP_KILL_BASE               = 6
local FP_KILL_LEVEL_SCALE        = 0.50
local FP_KILL_MIN                = 1
local FP_KILL_MAX                = 30

-- Mission/dungeon
local FP_MISSION_BASE            = 180
local FP_MISSION_TIER_BONUS      = 90
local FP_DUNGEON_BASE            = 800
local FP_DUNGEON_TIER_BONUS      = 300

-- Group bonus
local GROUP_STEP                 = 0.12
local GROUP_MAX                  = 2.0

-- Holocron compatibility vars
NUMBEROFPROFESSIONSTOMASTER                  = 3
MAXIMUMNUMBEROFPROFESSIONSTOSHOWWITHHOLOCRON = 1

HologrindJediManager = JediManager:new {
  screenplayName      = jediManagerName,
  jediManagerName     = jediManagerName,
  jediProgressionType = HOLOGRINDJEDIPROGRESSION,
  startingEvent       = nil,
}

-- =========================================================
-- Profession list (legacy/combat)
-- =========================================================
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

-- =========================================================
-- Data keys
-- =========================================================
local PREFIX = "hgj:"
local function oid(creo) return SceneObject(creo):getObjectID() end
local function K(creo, suffix) return PREFIX .. tostring(oid(creo)) .. ":" .. suffix end
-- keys:
--   fp         current Force Points
--   regen      1 if passive regen enabled
--   migrated   1 if legacy hologrind check done
--   villagemig 1 if village→hologrind migration handled

function HologrindJediManager:getForcePoints(pCreature)
  local v = tonumber(readData(K(pCreature, "fp")))
  return v or 0
end

function HologrindJediManager:setForcePoints(pCreature, val)
  if not pCreature then return end
  if not val then val = 0 end
  if val < 0 then val = 0 end
  writeData(K(pCreature, "fp"), math.floor(val))
end

function HologrindJediManager:addForcePoints(pCreature, amt)
  if not pCreature or not amt or amt == 0 then return end
  local now = self:getForcePoints(pCreature) + math.floor(amt)
  self:setForcePoints(pCreature, now)
  self:checkAndConvertIfReady(pCreature)
end

-- =========================================================
-- Group bonus helpers
-- =========================================================
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

-- =========================================================
-- Jedi conversion/regen
-- =========================================================
function HologrindJediManager:isJedi(pCreature)
  local pGhost = CreatureObject(pCreature):getPlayerObject()
  return pGhost and PlayerObject(pGhost):isJedi() or false
end

function HologrindJediManager:convertToJedi(pCreature)
  local pGhost = CreatureObject(pCreature):getPlayerObject()
  if not pGhost or self:isJedi(pCreature) then return end

  -- core jedi state
  PlayerObject(pGhost):setJediState(2)  -- unlocked

  -- set progression type only if this branch supports it
  local po = PlayerObject(pGhost)
  if po ~= nil and po.setJediProgressionType ~= nil then
    po:setJediProgressionType(4)
  end

  -- publish-9 style titles
  awardSkill(pCreature, "force_title_jedi_novice")
  awardSkill(pCreature, "force_title_jedi_rank_02")

  -- ensure padawan tree is started
  if not CreatureObject(pCreature):hasSkill("jedi_padawan_novice") then
    awardSkill(pCreature, "jedi_padawan_novice")
  end

  -- give robe if possible
  local pInventory = SceneObject(pCreature):getSlottedObject("inventory")
  if pInventory ~= nil and not SceneObject(pInventory):isContainerFullRecursive() then
    giveItem(pInventory, "object/tangible/wearables/robe/robe_jedi_padawan.iff", -1)
  else
    CreatureObject(pCreature):sendSystemMessage("@jedi_spam:inventory_full_jedi_robe")
  end

  -- fx/music
  CreatureObject(pCreature):playEffect("clienteffect/trap_electric_01.cef", "")
  CreatureObject(pCreature):playMusicMessage("sound/music_become_jedi.snd")

  -- enable regen
  writeData(K(pCreature, "regen"), 1)
  self:scheduleForceRegenTick(pCreature)

  -- existing flavor line
  CreatureObject(pCreature):sendSystemMessage("The Force flows through you. You have become a Jedi.")
  -- explicit unlock notification
  CreatureObject(pCreature):sendSystemMessage("You have unlocked the Jedi path. Visit a Force Shrine to continue your Trials.")

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
  if tonumber(readData(K(pCreature, "regen"))) == 1 then
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

-- =========================================================
-- Legacy autogrant (old hologrind finished)
-- =========================================================
function HologrindJediManager:legacyAutograntIfEligible(pCreature)
  if tonumber(readData(K(pCreature, "migrated"))) == 1 then return end
  local pGhost = CreatureObject(pCreature):getPlayerObject()
  if not pGhost then return end
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

-- =========================================================
-- Village→hologrind migration for existing Jedi
-- =========================================================
function HologrindJediManager:shouldOfferVillageMigration(pCreature)
  if not pCreature then return false end
  if tonumber(readData(K(pCreature, "villagemig"))) == 1 then return false end

  local pGhost = CreatureObject(pCreature):getPlayerObject()
  if not pGhost then return false end
  if not PlayerObject(pGhost):isJedi() then return false end

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
  local text = "This character uses the older Village Jedi unlock.\n\nDo you want to migrate to the hologrind/FRS-compatible progression?"
  sui:sendMessageBox(
    pCreature,
    pCreature,
    "Jedi Migration",
    text,
    "@yes",
    "@no",
    "HologrindJediManager",
    "onVillageMigrationChoice"
  )
end

function HologrindJediManager:onVillageMigrationChoice(pCreature, pSui, eventIndex, ...)
  if not pCreature then return end
  if eventIndex == 0 then
    local pGhost = CreatureObject(pCreature):getPlayerObject()
    if pGhost then
      local po = PlayerObject(pGhost)
      if po ~= nil and po.setJediProgressionType ~= nil then
        po:setJediProgressionType(4)
      end
      writeData(K(pCreature, "regen"), 1)
      self:scheduleForceRegenTick(pCreature)
      CreatureObject(pCreature):sendSystemMessage("Your Jedi progression has been migrated to the hologrind/FRS path.")
    end
    writeData(K(pCreature, "villagemig"), 1)
  else
    writeData(K(pCreature, "villagemig"), 1)
    CreatureObject(pCreature):sendSystemMessage("Village-style Jedi progression retained.")
  end
end

-- =========================================================
-- Kill/missions/dungeons
-- =========================================================
function HologrindJediManager:onKilledCreature(pCreature, pVictim)
  if not pCreature or not pVictim then return 0 end
  if self:isJedi(pCreature) then return 0 end
  if SceneObject(pVictim):isPlayerCreature() then return 0 end

  local fp = FP_KILL_BASE
  local lvlP, lvlV
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

-- =========================================================
-- Holocrons
-- =========================================================
local function isHolocronTemplate(path)
  if not path then return false end
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
    if pGhost == nil then return false end

    local professions = PlayerObject(pGhost):getHologrindProfessions()
    for i = 1, #professions do
      if not PlayerObject(pGhost):hasBadge(professions[i]) then
        local professionText = self:getProfessionStringIdFromBadgeNumber(professions[i])
        CreatureObject(pCreatureObject):sendSystemMessageWithTO("@jedi_spam:holocron_light_information", "@skl_n:" .. professionText)
      end
    end

    return false
  end
end

function HologrindJediManager:useItem(pSceneObject, _, pCreature)
  if not pCreature or not pSceneObject then return end
  local template = SceneObject(pSceneObject):getTemplateObjectPath()
  if not isHolocronTemplate(template) then return end

  self:sendHolocronMessage(pCreature)

  SceneObject(pSceneObject):destroyObjectFromWorld()
  SceneObject(pSceneObject):destroyObjectFromDatabase()
end

-- =========================================================
-- Login: auto-grant if not unlocked
-- =========================================================
function HologrindJediManager:onPlayerLoggedIn(pCreature)
  if not pCreature then return end

  -- auto-force to 10k if not Jedi
  if not self:isJedi(pCreature) then
    local curFP = self:getForcePoints(pCreature)
    if curFP < FORCE_TO_UNLOCK then
      self:setForcePoints(pCreature, FORCE_TO_UNLOCK)
      if not CreatureObject(pCreature):hasSkill("jedi_padawan_novice") then
        awardSkill(pCreature, "jedi_padawan_novice")
      end
    end
  end

  -- legacy one-time check
  self:legacyAutograntIfEligible(pCreature)

  -- resume regen
  if tonumber(readData(K(pCreature, "regen"))) == 1 then
    self:scheduleForceRegenTick(pCreature)
  end

  -- convert now if we have the FP
  self:checkAndConvertIfReady(pCreature)

  -- observe kills
  createObserver(KILLEDCREATURE, "HologrindJediManager", "onKilledCreature", pCreature)

  -- for already-Jedi players, try to normalize progression to 4 if available
  local pGhost = CreatureObject(pCreature):getPlayerObject()
  if pGhost and PlayerObject(pGhost):isJedi() then
    local po = PlayerObject(pGhost)
    if po ~= nil and po.setJediProgressionType ~= nil then
      po:setJediProgressionType(4)
    end
  end

  -- offer village→hologrind migration if applicable
  if self:shouldOfferVillageMigration(pCreature) then
    self:showVillageMigrationChoice(pCreature)
  end
end

function HologrindJediManager:onPlayerCreated(pCreature)
  -- no heavy work here
end

function HologrindJediManager:canLearnSkill()
  return true
end

registerScreenPlay("HologrindJediManager", true)
return HologrindJediManager
