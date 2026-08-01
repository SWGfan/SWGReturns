DDDropOffsScreenPlay = ScreenPlay:new {
	numberOfActs = 1,
	-- Add further item templates here to extend the drop-off rewards.
	rewardItems = {
		"object/tangible/veteran_reward/resource.iff",
		"object/tangible/veteran_reward/resource.iff",
	},
}

registerScreenPlay("DDDropOffsScreenPlay", true)

function DDDropOffsScreenPlay:start()
	if isZoneEnabled("tatooine") then
		self:spawnDropOffs()
	end
end

function DDDropOffsScreenPlay:spawnDropOffs()
	-- Beside the welcome Darth Vader outside Mos Eisley starport.
	local pNpc = spawnMobile("tatooine", "commoner", 1, 3524, 5, -4820, 220, 0)

	if pNpc == nil then
		return
	end

	CreatureObject(pNpc):setCustomObjectName("DD's Drop Offs")
	createObserver(OBJECTRADIALOPENED, "DDDropOffsScreenPlay", "claimDropOff", pNpc)
end

function DDDropOffsScreenPlay:claimDropOff(pNpc, pPlayer)
	if pNpc == nil or pPlayer == nil then
		return 0
	end

	local player = CreatureObject(pPlayer)

	if player:hasScreenPlayState(1, "dd_drop_offs_rewards") then
		player:sendSystemMessage("You have already collected this character's DD's Drop Offs package.")
		return 0
	end

	local pInventory = SceneObject(pPlayer):getSlottedObject("inventory")

	if pInventory == nil then
		player:sendSystemMessage("DD's Drop Offs could not find your inventory.")
		return 0
	end

	if SceneObject(pInventory):isContainerFullRecursive() then
		player:sendSystemMessage("Make at least two free inventory slots, then try DD's Drop Offs again.")
		return 0
	end

	local issued = {}

	for i = 1, #self.rewardItems do
		local pReward = giveItem(pInventory, self.rewardItems[i], -1)

		if pReward == nil then
			for j = 1, #issued do
				SceneObject(issued[j]):destroyObjectFromWorld()
				SceneObject(issued[j]):destroyObjectFromDatabase()
			end

			player:sendSystemMessage("DD's Drop Offs could not complete the package. Make inventory room and try again.")
			return 0
		end

		table.insert(issued, pReward)
	end

	player:setScreenPlayState(1, "dd_drop_offs_rewards")
	player:sendSystemMessage("DD's Drop Offs issued two 100,000-unit resource deeds. Select Ddayprimefuel on one and Ddayprimesteel on the other.")
	spatialChat(pNpc, "Drop-off complete. Use the deeds and choose the two DD capped resources.")

	return 0
end
