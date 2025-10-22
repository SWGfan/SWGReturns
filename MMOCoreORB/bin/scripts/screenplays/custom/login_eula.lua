-- scripts/screenplays/custom/login_eula.lua
-- First-login EULA prompt (per-account), reading from MOTD.
-- Locks movement until accepted. If not accepted within TIMEOUT_SECONDS, kick.
-- Uses readData/writeData (no SQL, no objvars).

LoginEULA = ScreenPlay:new {
    screenplayName = "LoginEULA",

    -- ===== Config =====
    ENABLE_MOVEMENT_LOCK = true,            -- lock movement until accepted
    TIMEOUT_SECONDS = 45,                   -- kick after this many seconds if not accepted
    MOTD_PATHS = {                          -- we’ll try these in order
        "conf/motd.txt",
        "scripts/motd.txt",
        "conf/motd",
        "scripts/motd"
    },
    -- Wrap your MOTD EULA with these markers:
    -- [EULA-BEGIN] ... terms ... [EULA-END]
    EULA_BEGIN_MARK = "[EULA-BEGIN]",
    EULA_END_MARK   = "[EULA-END]",

    -- SUI limits
    MAX_SUI_TEXT = 3500,
    TITLE = "SWG Returns — End User License Agreement",
    ACCEPT_BUTTON = "@ok",
    CANCEL_BUTTON = "@cancel",

    -- Storage keys
    KEY_ACCEPT_PREFIX  = "eulaAccepted:",    -- + accountId
    KEY_PENDING_PREFIX = "eulaPending:",     -- + accountId (set on show; cleared on accept/decline/kick)
}

registerScreenPlay("LoginEULA", true)

-- ===== Helpers =====

local function getGhost(pCreature)
    if (pCreature == nil) then return nil end
    return CreatureObject(pCreature):getPlayerObject()
end

local function getAccountId(pCreature)
    local pGhost = getGhost(pCreature)
    if (pGhost == nil) then return nil end
    local id = nil
    local ok, val = pcall(function() return PlayerObject(pGhost):getAccountID() end)
    if ok and val ~= nil then id = tonumber(val) end
    if id == nil then
        ok, val = pcall(function() return PlayerObject(pGhost):getAccountId() end)
        if ok and val ~= nil then id = tonumber(val) end
    end
    return id
end

local function kAccept(accId)  return LoginEULA.KEY_ACCEPT_PREFIX  .. tostring(accId) end
local function kPending(accId) return LoginEULA.KEY_PENDING_PREFIX .. tostring(accId) end

local function hasAcceptedEula(accId)
    return readData(kAccept(accId)) ~= nil
end

local function setAcceptedEula(accId)
    writeData(kAccept(accId), 1)
end

local function setPending(accId, val)
    if val then writeData(kPending(accId), 1) else deleteData(kPending(accId)) end
end

local function isPending(accId)
    return readData(kPending(accId)) ~= nil
end

local function lockMovement(pCreature, yes)
    if pCreature == nil then return end
    if not LoginEULA.ENABLE_MOVEMENT_LOCK then return end
    pcall(function()
        CreatureObject(pCreature):setState(STATE_IMMOBILIZED, yes)
    end)
end

local function tryReadFile(paths)
    for i = 1, #paths do
        local f = io.open(paths[i], "r")
        if f ~= nil then
            local content = f:read("*a")
            f:close()
            if content and #content > 0 then
                return content, paths[i]
            end
        end
    end
    return nil, nil
end

local function extractEulaFromMotd(motd, beginMark, endMark)
    if not motd or motd == "" then return "" end
    motd = motd:gsub("\r\n", "\n"):gsub("\r", "\n")
    local bStart, bEnd = motd:find(beginMark, 1, true)
    local eStart, eEnd = motd:find(endMark, 1, true)
    if bStart and eEnd and eEnd > bEnd then
        local segment = motd:sub(bEnd + 1, eStart - 1)
        return segment:gsub("^%s+", ""):gsub("%s+$", "")
    else
        return motd:gsub("^%s+", ""):gsub("%s+$", "")
    end
end

local function fitForSui(text, maxLen)
    if not text then return "" end
    if #text <= maxLen then return text end
    local note = "\n\n… (truncated; see full MOTD for complete terms)"
    local allowed = maxLen - #note
    if allowed < 0 then allowed = 0 end
    return text:sub(1, allowed) .. note
end

local function showEulaSui(pCreature, title, body)
    local sui = LuaSuiManager()
    -- Try 2-button first
    local ok2 = pcall(function()
        sui:sendMessageBox(
            pCreature, pCreature,
            title, body,
            LoginEULA.ACCEPT_BUTTON, "LoginEULA", "onAcceptEula",
            LoginEULA.CANCEL_BUTTON, "LoginEULA", "onDeclineEula"
        )
    end)
    if ok2 then return end
    -- Fallback to 1-button (Accept only)
    pcall(function()
        sui:sendMessageBox(
            pCreature, pCreature,
            title, body,
            LoginEULA.ACCEPT_BUTTON, "LoginEULA", "onAcceptEula"
        )
    end)
end

-- ===== ScreenPlay hooks =====

function LoginEULA:start()
    -- no world spawning needed
end

function LoginEULA:onPlayerLoggedIn(pCreatureObject)
    if (pCreatureObject == nil) then return end
    local accId = getAccountId(pCreatureObject)
    if accId == nil then return end

    if hasAcceptedEula(accId) then
        return
    end

    lockMovement(pCreatureObject, true)

    local motd, pathUsed = tryReadFile(self.MOTD_PATHS)
    local body = nil

    if motd == nil then
        body =
            "Effective Date: October 21, 2025\n\n" ..
            "This server is a non-commercial, fan-made emulator. Not affiliated with Lucasfilm/Disney/SOE.\n" ..
            "Age 13+ (under 18 requires parental consent). No harassment, cheating, exploits, or RMT.\n" ..
            "Accounts are a revocable license; service provided “AS IS”.\n\n" ..
            "Press ACCEPT to continue."
    else
        local eulaText = extractEulaFromMotd(motd, self.EULA_BEGIN_MARK, self.EULA_END_MARK)
        if eulaText == "" then eulaText = motd end
        eulaText = fitForSui(eulaText, self.MAX_SUI_TEXT)
        local header = pathUsed and ("Source: " .. pathUsed .. "\n\n") or ""
        body = header .. eulaText .. "\n\nBy pressing ACCEPT you acknowledge this EULA."
    end

    -- Mark pending and show SUI
    setPending(accId, true)
    showEulaSui(pCreatureObject, self.TITLE, body)

    -- Schedule timeout kick
    local delay = math.max(5, self.TIMEOUT_SECONDS) * 1000  -- ms
    createEvent(delay, "LoginEULA", "onEulaTimeout", pCreatureObject, tostring(accId))
end

-- ===== Timer callback =====
function LoginEULA:onEulaTimeout(pCreatureObject, accIdStr)
    local accId = tonumber(accIdStr or "0")
    if accId == nil then return end
    if hasAcceptedEula(accId) then
        -- Already accepted; nothing to do
        setPending(accId, false)
        return
    end
    if not isPending(accId) then
        -- Was handled by decline path
        return
    end
    -- Still pending => kick
    if (pCreatureObject ~= nil) then
        CreatureObject(pCreatureObject):sendSystemMessage("EULA not accepted in time. Disconnecting…")
    end
    setPending(accId, false)
    self:disconnectPlayer(pCreatureObject, "")
end

-- ===== SUI callbacks =====
function LoginEULA:onAcceptEula(pCreatureObject, eventIndex, args)
    if (pCreatureObject == nil) then return end
    local accId = getAccountId(pCreatureObject)
    if accId == nil then return end

    setAcceptedEula(accId)
    setPending(accId, false)
    lockMovement(pCreatureObject, false)
    CreatureObject(pCreatureObject):sendSystemMessage("EULA accepted. Welcome to SWG Returns!")
end

function LoginEULA:onDeclineEula(pCreatureObject, eventIndex, args)
    if (pCreatureObject == nil) then return end
    local accId = getAccountId(pCreatureObject)
    if accId ~= nil then setPending(accId, false) end
    CreatureObject(pCreatureObject):sendSystemMessage("You must accept the EULA to play. Disconnecting...")
    createEvent(1500, "LoginEULA", "disconnectPlayer", pCreatureObject, "")
end

function LoginEULA:disconnectPlayer(pCreatureObject, args)
    if (pCreatureObject == nil) then return end
    pcall(function()
        SceneObject(pCreatureObject):destroyObjectFromWorld()
    end)
end
