local ObjectManager = require("managers.object.object_manager")

RareLewtBoxMenuComponent = {
    cooldownDuration = 30,  -- Cooldown duration in seconds (e.g., 5 minutes)
    playerCooldowns = {},  -- Table to track the last interaction time for each player
}

function RareLewtBoxMenuComponent:fillObjectMenuResponse(pSceneObject, pMenuResponse, pPlayer)
    if pSceneObject == nil or pMenuResponse == nil or pPlayer == nil then
        return
    end

    local menuResponse = LuaObjectMenuResponse(pMenuResponse)
    menuResponse:addRadialMenuItem(220, 3, "Open Box")
end

function RareLewtBoxMenuComponent:handleObjectMenuSelect(pSceneObject, pPlayer, selectedID)
    if pPlayer == nil or pSceneObject == nil then
        return 0
    end

    if selectedID == 220 then
        local displayedName = SceneObject(pSceneObject):getDisplayedName()
        if displayedName == nil then
            return 0
        end

    local inventory = SceneObject(pPlayer):getSlottedObject("inventory")
    local LewtChest = getContainerObjectByTemplate(inventory, "object/tangible/loot/lewt_chests/lewt_chest_rare.iff", false)
    
    if (inventory ~= nil) then
            createLoot(inventory, "rarelootsystem", 350, true)
            createLoot(inventory, "legendary_comp_group", 400, true)
            createLoot(inventory, "boss_rare", 425, true)
            createLoot(inventory, "resource_deed_loot", 450, true)
            createLoot(inventory, "resource_crate_loot", 450, true)
        end

        CreatureObject(pPlayer):sendSystemMessage("Lewt Chest Opened.")
        CreatureObject(pPlayer):playEffect("clienteffect/pl_force_generic.cef", "")
        SceneObject(LewtChest):destroyObjectFromWorld(true)
        SceneObject(LewtChest):destroyObjectFromDatabase(true)
	end
end