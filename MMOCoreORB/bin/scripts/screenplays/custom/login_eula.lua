-- scripts/screenplays/custom/login_eula.lua
-- First-login EULA prompt (per-account), reading text from your MOTD file.
-- Movement is locked until accepted. Uses readData/writeData (no SQL, no objvars).

LoginEULA = ScreenPlay:new {
    screenplayName = "LoginEULA",

    -- ===== Config =====
    ENABLE_MOVEMENT_LOCK = true,            -- lock movement until accepted
    MOTD_PATHS = {                          -- we’ll try these in order
        "conf/motd.txt",
        "scripts/motd.txt",
        "conf/motd",
        "scripts/motd"
    },
    -- Put these markers around the EULA portion of your MOTD:
    -- [EULA-BEGIN] ... your terms ... [EULA-END]
    EULA_BEGIN_MARK = "[EULA-BEGIN]",
    EULA_END_MARK   = "[EULA-END]",

    -- SUI limits: message boxes can choke on huge bodies; keep it sane
    MAX_SUI_TEXT = 3500,                    -- soft limit; we trim & append notice
    TITLE = "SWG Returns — End User License Agreement",
    ACCEPT_BUTTON = "@ok",                  -- shows as "OK" in many clients; treated as Accept
    CANCEL_BUTTON = "@cancel",              -- only used if your engine supports 2-button message box

    -- Storage
    KEY_ACCEPT_PREFIX = "eulaAccepted:",    -- + accountId
}

registerScreenPlay("LoginEULA", true)

-- ===== Internal helpers =====

local function getGhost(pCreature)
    if (pCreature == nil) then return nil end
    return CreatureObject(pCreature):getPlayerObject()
end

local function getAccountId(pCreature)
    local pGhost = getGhost(pCreature)
    if (pGhost == nil) then return nil end
    local id = nil
    -- Some forks expose getAccountID(), others getAccountId()
    local ok, val = pcall(function() return PlayerObject(pGhost):getAccountID() end)
    if ok and val ~= nil then id = tonumber(val) end
    if id == nil then
        ok, val = pcall(function() return PlayerObject(pGhost):getAccountId() end)
        if ok and val ~= nil then id = tonumber(val) end
    end
    return id
end

local function acceptedKeyFor(accId)
    return LoginEULA.KEY_ACCEPT_PREFIX .. tostring(accId)
end

local function hasAcceptedEula(accId)
    local v = readData(acceptedKeyFor(accId))
    return v ~= nil
end

local function setAcceptedEula(accId)
    writeData(acceptedKeyFor(accId), 1)
end

local function lockMovement(pCreature, yes)
    if pCreature == nil then return end
    if not LoginEULA.ENABLE_MOVEMENT_LOCK then return end
    -- Try to use a common immobilize state; ignore if not supported
    pcall(function()
        CreatureObject(pCreature):setState(STATE_IMMOBILIZED, yes)
    end)
end

-- File reading with multiple candidate paths
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

-- Extract EULA subsection if markers exist; else return full text
local function extractEulaFromMotd(motd, beginMark, endMark)
    if not motd or motd == "" then return "" end

    -- Normalize line endings
    motd = motd:gsub("\r\n", "\n"):gsub("\r", "\n")

    local bStart, bEnd = motd:find(beginMark, 1, true)
    local eStart, eEnd = motd:find(endMark, 1, true)

    if bStart and eEnd and eEnd > bEnd then
        local segment = motd:sub(bEnd + 1, eStart - 1)
        segment = segment:gsub("^%s+", ""):gsub("%s+$", "")
        return segment
    else
        -- No markers; use entire file
        local trimmed = motd:gsub("^%s+", ""):gsub("%s+$", "")
        return trimmed
    end
end

-- Trim to safe SUI size with note
local function fitForSui(text, maxLen)
    if not text then return "" end
    if #text <= maxLen then return text end
    local note = "\n\n… (truncated; see full MOTD for complete terms)"
    local allowed = maxLen - #note
    if allowed < 0 then allowed = 0 end
    return text:sub(1, allowed) .. note
end

-- Show EULA via SUI message box.
-- Some forks allow 2-button message box; others only 1 button.
-- We attempt 2-button first with pcall; on failure, fallback to 1-button (Accept only).
local function showEulaSui(pCreature, title, body)
    local sui = LuaSuiManager()

    -- Attempt 2-button signature:
    -- sendMessageBox(Creature, Owner, Title, Prompt, OkBtn, Script, OkCb, CancelBtn, Script, CancelCb)
    local ok2 = pcall(function()
        sui:sendMessageBox(
            pCreature, pCreature,
            title, body,
            LoginEULA.ACCEPT_BUTTON, "LoginEULA", "onAcceptEula",
            LoginEULA.CANCEL_BUTTON, "LoginEULA", "onDeclineEula"
        )
    end)

    if ok2 then return end

    -- Fallback: 1-button (Accept).
    pcall(function()
        sui:sendMessageBox(
            pCreature, pCreature,
            title, body,
            LoginEULA.ACCEPT_BUTTON, "LoginEULA", "onAcceptEula"
        )
    end)
end

-- ===== ScreenPlay entry points =====

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
        -- Hard fallback text if MOTD missing
        body =
            "Effective Date: October 21, 2025\n\n" ..
            "This server is a non-commercial, fan-made emulator. Not affiliated with Lucasfilm/Disney/SOE.\n" ..
            "Age 13+ (under 18 requires parental consent). No harassment, cheating, exploits, or RMT.\n" ..
            "Accounts are a revocable license; service provided “AS IS”.\n\n" ..
            "Press ACCEPT to continue."
    else
        local eulaText = extractEulaFromMotd(motd, self.EULA_BEGIN_MARK, self.EULA_END_MARK)
        if eulaText == "" then
            -- MOTD exists but no markers or empty section; show the whole MOTD
            eulaText = motd
        end

        -- Cap to safe SUI size
        eulaText = fitForSui(eulaText, self.MAX_SUI_TEXT)

        -- Add a small header with path for admin clarity
        local header = ""
        if pathUsed ~= nil then
            header = "Source: " .. pathUsed .. "\n\n"
        end
        body = header .. eulaText .. "\n\nBy pressing ACCEPT you acknowledge this EULA."
    end

    showEulaSui(pCreatureObject, self.TITLE, body)
end

-- ===== SUI callbacks =====

function LoginEULA:onAcceptEula(pCreatureObject, eventIndex, args)
    if (pCreatureObject == nil) then return end
    local accId = getAccountId(pCreatureObject)
    if accId == nil then return end

    setAcceptedEula(accId)
    lockMovement(pCreatureObject, false)
    CreatureObject(pCreatureObject):sendSystemMessage("EULA accepted. Welcome to SWG Returns!")
end

function LoginEULA:onDeclineEula(pCreatureObject, eventIndex, args)
    if (pCreatureObject == nil) then return end
    CreatureObject(pCreatureObject):sendSystemMessage("You must accept the EULA to play. Disconnecting...")
    createEvent(1500, "LoginEULA", "disconnectPlayer", pCreatureObject, "")
end

function LoginEULA:disconnectPlayer(pCreatureObject, args)
    if (pCreatureObject == nil) then return end
    -- Graceful removal; adjust if your core exposes a direct disconnect API.
    pcall(function()
        SceneObject(pCreatureObject):destroyObjectFromWorld()
    end)
end
