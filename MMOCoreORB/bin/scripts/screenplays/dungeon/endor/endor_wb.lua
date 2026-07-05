endor_wb = ScreenPlay:new {
	numberOfActs = 1,
	scriptName = "endor_wb",
  	planet = "endor",
	earthquakeRadius = 7.5,
}
registerScreenPlay("endor_wb", true)

function endor_wb:start()
	if (isZoneEnabled(self.planet)) then
		self:spawnMobiles()
	end
end


function endor_wb:spawnMobiles()
		local pBoss = spawnMobile("endor", "frenzied_gorax", -1, -4334-8, 249.4, 7438.5, 169, 19500000)
		local creature = CreatureObject(pBoss)
		createObserver(DAMAGERECEIVED, "endor_wb", "npcDamageObserver", pBoss)    
		createObserver(OBJECTDESTRUCTION, "endor_wb", "bossDead", pBoss)
end

function endor_wb:npcDamageObserver(bossObject, playerObject, damage)

	local player = LuaCreatureObject(playerObject)
	local boss = LuaCreatureObject(bossObject)
	
	health = boss:getHAM(0)
	action = boss:getHAM(3)
	mind = boss:getHAM(6)
	
	maxHealth = boss:getMaxHAM(0)
	maxAction = boss:getMaxHAM(3)
	maxMind = boss:getMaxHAM(6)

	if (((health <= (maxHealth * 0.9))) and readData("endor_wb:spawnState") == 0) then
			writeData("endor_wb:spawnState",1)
			createEvent(0 * 1000, "endor_wb", "knockdown", playerObject, "")
 			createEvent(5 * 1000, "endor_wb", "earthquake", playerObject, "")
			createEvent(15 * 1000, "endor_wb", "tremor", playerObject, "")
			createEvent(20 * 1000, "endor_wb", "tremor", playerObject, "")
			CreatureObject(playerObject):sendSystemMessage("The Gorax slams his feet into the ground, sending tremors and knocking you down!")			
      		CreatureObject(bossObject):playEffect("clienteffect/attacker_berserk.cef", "")
	end

	if (((health <= (maxHealth * 0.7))) and readData("endor_wb:spawnState") == 1) then
      		writeData("endor_wb:spawnState",2)
			createEvent(0 * 1000, "endor_wb", "knockdown", playerObject, "")
			createEvent(5 * 1000, "endor_wb", "earthquake", playerObject, "")
			createEvent(13 * 1000, "endor_wb", "knockdown", playerObject, "")
			createEvent(15 * 1000, "endor_wb", "earthquake", playerObject, "")
 			createEvent(5 * 1000, "endor_wb", "tremor", playerObject, "")
			createEvent(15 * 1000, "endor_wb", "tremor_mid", playerObject, "")
			createEvent(20 * 1000, "endor_wb", "tremor_mid", playerObject, "")       
      			CreatureObject(playerObject):sendSystemMessage("The ground feels softer under barrage of the Gorax")
      			CreatureObject(bossObject):playEffect("clienteffect/attacker_berserk.cef", "")
	end

	if (((health <= (maxHealth * 0.5))) and readData("endor_wb:spawnState") == 2) then
			writeData("endor_wb:spawnState",3)
			createEvent(0 * 1000, "endor_wb", "knockdown", playerObject, "")
			createEvent(0 * 1000, "endor_wb", "earthquake", playerObject, "")
			createEvent(17 * 1000, "endor_wb", "earthquake", playerObject, "")
 			createEvent(5 * 1000, "endor_wb", "tremor_mid", playerObject, "")
			createEvent(15 * 1000, "endor_wb", "tremor_mid", playerObject, "")
			createEvent(20 * 1000, "endor_wb", "tremor_mid", playerObject, "") 
			createEvent(25 * 1000, "endor_wb", "earthquake", playerObject, "")
			createEvent(45 * 1000, "endor_wb", "earthquake", playerObject, "")
			createEvent(65 * 1000, "endor_wb", "earthquake", playerObject, "")
			
      		CreatureObject(playerObject):sendSystemMessage("The ground gives way to a volcanic geyser! Move away now!")
      		CreatureObject(bossObject):playEffect("clienteffect/attacker_berserk.cef", "")
	end

	if (((health <= (maxHealth * 0.3))) and readData("endor_wb:spawnState") == 3) then
      		writeData("endor_wb:spawnState",4)
			createEvent(20 * 1000, "endor_wb", "knockdown", playerObject, "")
			createEvent(0 * 1000, "endor_wb", "earthquake", playerObject, "")
			createEvent(17 * 1000, "endor_wb", "earthquake", playerObject, "")
 			createEvent(5 * 1000, "endor_wb", "tremor_mid", playerObject, "")
			createEvent(15 * 1000, "endor_wb", "tremor_mid", playerObject, "")
			createEvent(20 * 1000, "endor_wb", "tremor_mid", playerObject, "") 
			createEvent(20 * 1000, "endor_wb", "tremor_mid", playerObject, "") 
			createEvent(25 * 1000, "endor_wb", "earthquake", playerObject, "")
			createEvent(45 * 1000, "endor_wb", "earthquake", playerObject, "")
			createEvent(65 * 1000, "endor_wb", "earthquake", playerObject, "")			
      		CreatureObject(playerObject):sendSystemMessage("Another breach beneath your feet!")
      		CreatureObject(bossObject):playEffect("clienteffect/attacker_berserk.cef", "")
	end

	if (((health <= (maxHealth * 0.1))) and readData("endor_wb:spawnState") == 4) then
				CreatureObject(playerObject):sendSystemMessage("The Gorax roars in defiance!")
				self:awardToken(bossObject)
      			writeData("endor_wb:spawnState",5)  			
      			CreatureObject(bossObject):playEffect("clienteffect/attacker_berserk.cef", "")
		end
	return 0

end

function endor_wb:knockdown(bossObject)
	if (bossObject == nil) then
			return
	end
	local playerTable = SceneObject(bossObject):getPlayersInRange(80)
	if (playerTable == nil) then
		return
	end

	for i = 1, #playerTable, 1 do
		local pPlayer = playerTable[i]
		if (pPlayer ~= nil) then
				CreatureObject(pPlayer):setPosture(KNOCKEDDOWN)
		end
	end
end

function endor_wb:tremor(playerObject)
if (CreatureObject(playerObject):isGrouped()) then
	local groupSize = CreatureObject(playerObject):getGroupSize()
	for i = 0, groupSize - 1, 1 do
			local pMember = CreatureObject(playerObject):getGroupMember(i)
			if pMember ~= nil and SceneObject(pMember):isInRangeWithObject(playerObject, 200) then
			local trapDmg = getRandomNumber(500, 600)
			CreatureObject(pMember):inflictDamage(pMember, 0, trapDmg, 1)
		end
	end
else
	local trapDmg = getRandomNumber(500, 600)
		CreatureObject(playerObject):inflictDamage(playerObject, 0, trapDmg, 1)
	end
end

function endor_wb:tremor_mid(playerObject)
if (CreatureObject(playerObject):isGrouped()) then
	local groupSize = CreatureObject(playerObject):getGroupSize()
	for i = 0, groupSize - 1, 1 do
			local pMember = CreatureObject(playerObject):getGroupMember(i)
			if pMember ~= nil and SceneObject(pMember):isInRangeWithObject(playerObject, 200) then
			local trapDmg = getRandomNumber(800, 1000)
			CreatureObject(pMember):inflictDamage(pMember, 0, trapDmg, 1)
		end
	end
else
	local trapDmg = getRandomNumber(800, 1000)
		CreatureObject(playerObject):inflictDamage(playerObject, 0, trapDmg, 1)
	end
end

function endor_wb:earthquake(bossObject, playerObject)
	if bossObject == nil then
        return
    end

    local playerTable = SceneObject(bossObject):getPlayersInRange(100)
    if playerTable == nil then
        return
    end

    if #playerTable > 0 then
        for i = 1, #playerTable do
            local currentPlayer = playerTable[i]
            if currentPlayer ~= nil then
				self:createBreach(currentPlayer)
            end
        end
    end
end 

function endor_wb:createBreach(playerObject)
	if (playerObject == nil) then
		return
	end
	local xPos = CreatureObject(playerObject):getPositionX()
	local yPos = CreatureObject(playerObject):getPositionY()
	local zPos = CreatureObject(playerObject):getPositionZ()
	local pSceneObject = spawnSceneObject(self.planet, "object/static/particle/pt_lok_lava_spirt.iff", xPos, zPos, yPos, 0, 0)

	createEvent(15 * 1000, self.scriptName, "destroyObject", pSceneObject, "")
	createEvent(5 * 1000, self.scriptName, "earthquakeDamage", pSceneObject, "")
end

function endor_wb:earthquakeDamage(pSceneObject)
	if (pSceneObject == nil) then
		return
	end

	local playerTable = SceneObject(pSceneObject):getPlayersInRange(self.earthquakeRadius)

	if (playerTable == nil) then
		createEvent(1 * 1000, self.scriptName, "earthquakeDamage", pSceneObject, "")
		return
	end

	for i = 1, #playerTable, 1 do
		local playerObject = playerTable[i]
		local curDist = SceneObject(playerObject):getDistanceTo(pSceneObject)

		if (curDist <= self.earthquakeRadius) then
			CreatureObject(playerObject):addDotState(pSceneObject, BLEEDING, 450, HEALTH, 60, 100, SceneObject(pSceneObject):getObjectID(), 0)
		end
	end

	createEvent(2 * 1000, self.scriptName, "earthquakeDamage", pSceneObject, "")
end

function endor_wb:destroyObject(pObject)
	if (pObject == nil) then
		return
	end

	if (SceneObject(pObject):isAiAgent()) then
		CreatureObject(pObject):setPvpStatusBitmask(0)
		forcePeace(pObject)
	end
	SceneObject(pObject):destroyObjectFromWorld()
end

function endor_wb:bossDead(pBoss)
	local creature = CreatureObject(pBoss)
	local respawn = math.random(7200,10800)
	createEvent(120 * 1000, "endor_wb", "KillBoss", pBoss, "") -- Corpse Despawn
	createEvent(respawn * 1000, "endor_wb", "KillSpawn", pBoss, "") -- Respawn
	return 0
end

function endor_wb:KillSpawn()
		local pBoss = spawnMobile("endor", "frenzied_gorax", -1, -4334-8, 249.4, 7438.5, 169, 19500000)
		createObserver(DAMAGERECEIVED, "endor_wb", "npcDamageObserver", pBoss)
		createObserver(OBJECTDESTRUCTION, "endor_wb", "bossDead", pBoss)
end

function endor_wb:awardToken(bossObject)
    if bossObject == nil then
        return
    end

    local playerTable = SceneObject(bossObject):getPlayersInRange(100)
    if playerTable == nil then
        return
    end

    if #playerTable > 0 then
        for i = 1, #playerTable do
            local currentPlayer = playerTable[i]
            if currentPlayer ~= nil then
                local pInventory = SceneObject(currentPlayer):getSlottedObject("inventory")
                if pInventory ~= nil then
                    if not SceneObject(pInventory):isContainerFullRecursive() then
                        giveItem(pInventory, "object/tangible/item/stardust_pvp_token_generic.iff", -1)
                    else
                        CreatureObject(currentPlayer):sendSystemMessage("You did not receive a boss token because your inventory is full.")
                    end
                else
                    CreatureObject(currentPlayer):sendSystemMessage("You did not receive a boss token because your inventory is full.")
                end

				local pGhost = CreatureObject(currentPlayer):getPlayerObject()
                if (pGhost ~= nil and not PlayerObject(pGhost):hasBadge(164)) then
                    PlayerObject(pGhost):awardBadge(164)
                end
            end
        end
    end
end

function endor_wb:KillBoss(pBoss)
      	writeData("endor_wb:spawnState",0)  
	dropObserver(pBoss, OBJECTDESTRUCTION)
	if SceneObject(pBoss) then
		SceneObject(pBoss):destroyObjectFromWorld()
	end
	return 0
end
