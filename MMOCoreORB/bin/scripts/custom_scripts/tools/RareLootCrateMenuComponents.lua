RareLootCrateMenuComponent = {}

function RareLootCrateMenuComponent:addOpenMenu(pSceneObject, pMenuResponse, pPlayer)
	if pSceneObject == nil or pMenuResponse == nil or pPlayer == nil then
		return
	end

	local menuResponse = LuaObjectMenuResponse(pMenuResponse)
	menuResponse:addRadialMenuItem(220, 3, "Open Crate")
end

function RareLootCrateMenuComponent:openCrate(pSceneObject, pPlayer, selectedID, rewardGroups, message)
	if pPlayer == nil or pSceneObject == nil then
		return 0
	end

	if selectedID ~= 220 then
		return 0
	end

	local inventory = SceneObject(pPlayer):getSlottedObject("inventory")
	if inventory == nil then
		CreatureObject(pPlayer):sendSystemMessage("You need an inventory to open this crate.")
		return 0
	end

	for i = 1, #rewardGroups do
		local reward = rewardGroups[i]
		createLoot(inventory, reward.group, reward.level, true)
	end

	CreatureObject(pPlayer):sendSystemMessage(message)
	CreatureObject(pPlayer):playEffect("clienteffect/pl_force_generic.cef", "")
	SceneObject(pSceneObject):destroyObjectFromWorld(true)
	SceneObject(pSceneObject):destroyObjectFromDatabase(true)

	return 0
end

RareLootCrateMenuComponent1 = {}

function RareLootCrateMenuComponent1:fillObjectMenuResponse(pSceneObject, pMenuResponse, pPlayer)
	RareLootCrateMenuComponent:addOpenMenu(pSceneObject, pMenuResponse, pPlayer)
end

function RareLootCrateMenuComponent1:handleObjectMenuSelect(pSceneObject, pPlayer, selectedID)
	local rewardGroups = {
		{group = "rarelootsystem", level = 300},
		{group = "rarelootsystem", level = 325},
		{group = "lootcollectiontierthree", level = 350},
		{group = "armor_attachments", level = 300},
		{group = "clothing_attachments", level = 300},
	}

	return RareLootCrateMenuComponent:openCrate(pSceneObject, pPlayer, selectedID, rewardGroups, "Rare Loot Crate opened.")
end

RareLootCrateMenuComponent2 = {}

function RareLootCrateMenuComponent2:fillObjectMenuResponse(pSceneObject, pMenuResponse, pPlayer)
	RareLootCrateMenuComponent:addOpenMenu(pSceneObject, pMenuResponse, pPlayer)
end

function RareLootCrateMenuComponent2:handleObjectMenuSelect(pSceneObject, pPlayer, selectedID)
	local rewardGroups = {
		{group = "rarelootsystem", level = 350},
		{group = "lootcollectiontierthree", level = 400},
		{group = "lootcollectiontierdiamond", level = 425},
		{group = "legendary_comp_group", level = 425},
		{group = "boss_rare", level = 425},
		{group = "resource_deed_loot", level = 425},
	}

	return RareLootCrateMenuComponent:openCrate(pSceneObject, pPlayer, selectedID, rewardGroups, "Rare Loot Crate Exceptional opened.")
end

RareLootCrateMenuComponent3 = {}

function RareLootCrateMenuComponent3:fillObjectMenuResponse(pSceneObject, pMenuResponse, pPlayer)
	RareLootCrateMenuComponent:addOpenMenu(pSceneObject, pMenuResponse, pPlayer)
end

function RareLootCrateMenuComponent3:handleObjectMenuSelect(pSceneObject, pPlayer, selectedID)
	local rewardGroups = {
		{group = "lootcollectiontierdiamond", level = 500},
		{group = "lootcollectiontierdiamond", level = 525},
		{group = "legendary_comp_group", level = 550},
		{group = "boss_rare", level = 550},
		{group = "g_rifle_t21_legendary", level = 550},
		{group = "g_pistol_fwg5_legendary", level = 550},
		{group = "g_baton_stun_legendary", level = 550},
		{group = "g_lance_nightsister_legendary", level = 550},
		{group = "resource_deed_loot", level = 550},
		{group = "resource_crate_loot", level = 550},
	}

	return RareLootCrateMenuComponent:openCrate(pSceneObject, pPlayer, selectedID, rewardGroups, "Rare Loot Crate Legendary opened.")
end
