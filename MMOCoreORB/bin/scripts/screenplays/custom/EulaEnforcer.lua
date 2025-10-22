-- EulaEnforcer.lua — SWG Returns
-- SUI EULA enforcement with Accept / Decline.
-- Stores acceptance via readData/writeData (per-character), no objvars required.

EulaEnforcer = {
    className = "EulaEnforcer",

    -- === CONFIG ===
    EULA_VERSION = "2025-10-21",                -- bump this to re-prompt everyone
    EULA_TITLE   = "SWG Returns - End User License Agreement",
    EULA_TEXT = [[
SWG Returns is a non-commercial, fan-operated SWG emulator built on SWGEmu Core3 (AGPLv3).
It is not affiliated with Lucasfilm, Disney, SOE, or Daybreak.

By continuing you confirm:
• You are 13+ (parental consent required if under 18)
• You will play respectfully and follow community rules
• You understand gameplay/data may reset at any time
• You play at your own risk; no warranties are provided

Full EULA: https://swgreturns.com/legal/eula
Contact: bentonus@gmail.com

Do you accept the EULA?
    ]],

    -- Optional: where to “park” players who decline if your core can’t kick.
    PARK_PLANET = "tutorial",  -- set to a harmless/internal area you have
    PARK_CELLID = 0,
    PARK_X = 0, PARK_Y = 0, PARK_Z = 0,
}

-- ===== internal key helpers (readData/writeData) =====
local PREFIX = "eula:"
local function oid(creo) return SceneObject(creo):getObjectID() end
local function K(creo, suffix)
    return PREFIX .. tostring(oid(creo)) .. ":" .. suffix
end
-- Keys:
--   K(creo, "accepted") -> string version (e.g., "2025-10-21") or nil

-- ===== lifecycle =====
function EulaEnforcer:onPlayerLoggedIn(pCreature)
    if not pCreature then return end
    -- If already accepted this version, do nothing
    local acceptedVer = readStringData(K(pCreature, "accepted"))
    if acceptedVer == self.EULA_VERSION then
        return
    end

    -- SUI: Accept / Decline
    local sui = LuaSuiManager()
    -- Signature used here supports 2 buttons on most Core3 builds:
    -- sendMessageBox(owner, target, title, text, okLabel, okScript, okCallback, cancelLabel, cancelScript, cancelCallback)
    sui:sendMessageBox(
        pCreature, pCreature,
        self.EULA_TITLE, self.EULA_TEXT,
        "@yes",  "EulaEnforcer", "onAccept",
        "@no",   "EulaEnforcer", "onDecline"
    )
end

-- ===== SUI callbacks =====
function EulaEnforcer:onAccept(pCreature)
    if not pCreature then return end
    writeStringData(K(pCreature, "accepted"), self.EULA_VERSION)
    -- UNGATE movement:
    CreatureObject(pCreature):setSpeedMultiplierBase(1.0)
    CreatureObject(pCreature):sendSystemMessage("Thank you. EULA accepted. Enjoy SWG Returns!")
end


function EulaEnforcer:onDecline(pCreature)
    if not pCreature then return end
    CreatureObject(pCreature):sendSystemMessage("You declined the EULA. Disconnecting…")

    -- Try to cleanly disconnect across common Core3 variants:
    local function tryKick()
        local pGhost = CreatureObject(pCreature):getPlayerObject()
        if pGhost and PlayerObject(pGhost) and PlayerObject(pGhost).disconnect then
            PlayerObject(pGhost):disconnect()
            return true
        end
        if CreatureObject(pCreature).disconnect then
            CreatureObject(pCreature):disconnect()
            return true
        end
        if PlayerObject and PlayerObject(pGhost) and PlayerObject(pGhost).forceLogout then
            PlayerObject(pGhost):forceLogout()
            return true
        end
        return false
    end

    if not tryKick() then
        -- Fallback: park the player in a limbo area and keep re-prompting on next login.
        SceneObject(pCreature):switchZone(self.PARK_PLANET, self.PARK_X, self.PARK_Z, self.PARK_Y, self.PARK_CELLID)
        CreatureObject(pCreature):sendSystemMessage("Your session could not be closed automatically. You have been moved out of play. Please /logout.")
    end
end

-- Required stub for single-button SUI compatibility (some cores call this)
function EulaEnforcer:notifyOkPressed()
    -- no-op
end

registerScreenPlay("EulaEnforcer", true)
