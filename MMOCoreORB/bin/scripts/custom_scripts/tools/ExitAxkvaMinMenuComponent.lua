local ObjectManager = require("managers.object.object_manager")

ExitAxkvaMinMenuComponent = {
    cooldownDuration = 30,  -- Cooldown duration in seconds (e.g., 5 minutes)
    playerCooldowns = {},  -- Table to track the last interaction time for each player
}

function ExitAxkvaMinMenuComponent:fillObjectMenuResponse(pSceneObject, pMenuResponse, pPlayer)
    if pSceneObject == nil or pMenuResponse == nil or pPlayer == nil then
        return
    end

    local menuResponse = LuaObjectMenuResponse(pMenuResponse)
    menuResponse:addRadialMenuItem(120, 3, "Exit Dungeon")
end

function ExitAxkvaMinMenuComponent:handleObjectMenuSelect(pSceneObject, pPlayer, selectedID)
    if pPlayer == nil or pSceneObject == nil then
        return 0
    end

    if selectedID == 120 then
            -- what todo here
	end
end