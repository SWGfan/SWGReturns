dant_discipleScreenplay = ScreenPlay:new {
	numberOfActs = 1,
  	planet = "dantooine",
}
registerScreenPlay("dant_discipleScreenplay", true)

function dant_discipleScreenplay:start()
	if (isZoneEnabled(self.planet)) then
		self:spawnMobiles()
	end
end


function dant_discipleScreenplay:spawnMobiles()
		local pBoss = spawnMobile("dantooine", "dant_disciple",-1,4331.3,25.8,3907.1,-14,15098000)
		spawnMobile("dantooine", "janta_scion",900,4335.6,25.7,3910.4,-28,15098000)
		spawnMobile("dantooine", "janta_scion",900,4326.1,25.0,3907.9,14,15098000)
		local creature = CreatureObject(pBoss)
		createObserver(DAMAGERECEIVED, "dant_discipleScreenplay", "npcDamageObserver", pBoss)    
		createObserver(OBJECTDESTRUCTION, "dant_discipleScreenplay", "bossDead", pBoss)
end

function dant_discipleScreenplay:npcDamageObserver(bossObject, playerObject, damage)

	local player = LuaCreatureObject(playerObject)
	local boss = LuaCreatureObject(bossObject)
	
	health = boss:getHAM(0)
	action = boss:getHAM(3)
	mind = boss:getHAM(6)
	
	maxHealth = boss:getMaxHAM(0)
	maxAction = boss:getMaxHAM(3)
	maxMind = boss:getMaxHAM(6)
	
	if (((health <= (maxHealth * 0.99))) and readData("dant_discipleScreenplay:spawnState") == 0) then
	writeData("dant_discipleScreenplay:spawnState",1)
      		spatialChat(bossObject, "It was foolish of you to challenge me.")
			createEvent(5 * 1000, "dant_discipleScreenplay", "rockthrow_last", playerObject, "")
 			createEvent(10 * 1000, "dant_discipleScreenplay", "rockthrow_last", playerObject, "")   			
      		CreatureObject(bossObject):playEffect("clienteffect/mustafar/som_dark_jedi_laugh.cef", "")
	end

	if (((health <= (maxHealth * 0.95))) and readData("dant_discipleScreenplay:spawnState") == 1) then
			writeData("dant_discipleScreenplay:spawnState",2)
			createEvent(5 * 1000, "dant_discipleScreenplay", "rockthrow", playerObject, "")
 			createEvent(10 * 1000, "dant_discipleScreenplay", "rockthrow", playerObject, "")
			createEvent(20 * 1000, "dant_discipleScreenplay", "rockthrow", playerObject, "")
			createEvent(30 * 1000, "dant_discipleScreenplay", "rockthrow", playerObject, "")
			createEvent(50 * 1000, "dant_discipleScreenplay", "rockthrow", playerObject, "")       
      		CreatureObject(bossObject):playEffect("clienteffect/mustafar/som_dark_jedi_laugh.cef", "")
	end

	if (((health <= (maxHealth * 0.7))) and readData("dant_discipleScreenplay:spawnState") == 2) then
      		writeData("dant_discipleScreenplay:spawnState",3)
			createEvent(0 * 1000, "dant_discipleScreenplay", "rockthrow", playerObject, "")
 			createEvent(5 * 1000, "dant_discipleScreenplay", "rockthrow", playerObject, "")
			createEvent(10 * 1000, "dant_discipleScreenplay", "rockthrow", playerObject, "")
			createEvent(15 * 1000, "dant_discipleScreenplay", "rockthrow", playerObject, "")
			createEvent(20 * 1000, "dant_discipleScreenplay", "rockthrow", playerObject, "")        
			self:spawnSupport(playerObject)
      			CreatureObject(playerObject):sendSystemMessage("The Disciple flings rocks at you using the Force.")
      			CreatureObject(bossObject):playEffect("clienteffect/mustafar/som_dark_jedi_laugh.cef", "")
	end

	if (((health <= (maxHealth * 0.5))) and readData("dant_discipleScreenplay:spawnState") == 3) then
      		spatialChat(bossObject, "My Scions will destroy and desecrate you!")
			writeData("dant_discipleScreenplay:spawnState",4)
			createEvent(0 * 1000, "dant_discipleScreenplay", "rockthrow_mid", playerObject, "")
 			createEvent(2 * 1000, "dant_discipleScreenplay", "rockthrow_mid", playerObject, "")
			createEvent(5 * 1000, "dant_discipleScreenplay", "rockthrow_mid", playerObject, "")
			createEvent(7 * 1000, "dant_discipleScreenplay", "rockthrow_mid", playerObject, "")
			createEvent(8 * 1000, "dant_discipleScreenplay", "rockthrow_mid", playerObject, "")     
			self:spawnSupport(playerObject)
      		CreatureObject(bossObject):playEffect("clienteffect/mustafar/som_dark_jedi_laugh.cef", "")
	end

	if (((health <= (maxHealth * 0.3))) and readData("dant_discipleScreenplay:spawnState") == 4) then
      		writeData("dant_discipleScreenplay:spawnState",5)
			createEvent(0 * 1000, "dant_discipleScreenplay", "rockthrow_last", playerObject, "")
			createEvent(2 * 1000, "dant_discipleScreenplay", "rockthrow_last", playerObject, "")
			createEvent(5 * 1000, "dant_discipleScreenplay", "rockthrow_last", playerObject, "")
			createEvent(7 * 1000, "dant_discipleScreenplay", "rockthrow_last", playerObject, "")
			createEvent(9 * 1000, "dant_discipleScreenplay", "rockthrow_last", playerObject, "")      
			self:spawnSupport(playerObject)
      		CreatureObject(playerObject):sendSystemMessage("A barrage of rocks heads your way.")
      		CreatureObject(bossObject):playEffect("clienteffect/mustafar/som_dark_jedi_laugh.cef", "")
	end

	if (((health <= (maxHealth * 0.1))) and readData("dant_discipleScreenplay:spawnState") == 5) then
				spatialChat(bossObject, "Your life force .. is mine!")
				self:awardToken(bossObject)
      			writeData("dant_discipleScreenplay:spawnState",6)
				createEvent(0 * 1000, "dant_discipleScreenplay", "finisher", playerObject, "")  		
      			CreatureObject(bossObject):playEffect("clienteffect/mustafar/som_dark_jedi_laugh.cef", "")
		end
	return 0

end

function dant_discipleScreenplay:rockthrow(playerObject)
if (CreatureObject(playerObject):isGrouped()) then
	local groupSize = CreatureObject(playerObject):getGroupSize()
	for i = 0, groupSize - 1, 1 do
			local pMember = CreatureObject(playerObject):getGroupMember(i)
			if pMember ~= nil and SceneObject(pMember):isInRangeWithObject(playerObject, 200) then
			local trapDmg = getRandomNumber(500, 700)
			CreatureObject(pMember):inflictDamage(pMember, 0, trapDmg, 1)
      		CreatureObject(playerObject):playEffect("clienteffect/mustafar/dark_jedi_rock_attack_1.cef", "")
		end
	end
else
	local trapDmg = getRandomNumber(500, 700)
		CreatureObject(playerObject):inflictDamage(playerObject, 0, trapDmg, 1)
      	CreatureObject(playerObject):playEffect("clienteffect/mustafar/dark_jedi_rock_attack_1.cef", "")
	end
end

function dant_discipleScreenplay:rockthrow_mid(playerObject)
if (CreatureObject(playerObject):isGrouped()) then
	local groupSize = CreatureObject(playerObject):getGroupSize()
	for i = 0, groupSize - 1, 1 do
			local pMember = CreatureObject(playerObject):getGroupMember(i)
			if pMember ~= nil and SceneObject(pMember):isInRangeWithObject(playerObject, 200) then
			local trapDmg = getRandomNumber(800, 900)
			CreatureObject(pMember):inflictDamage(pMember, 0, trapDmg, 1)
      		CreatureObject(playerObject):playEffect("clienteffect/mustafar/dark_jedi_rock_attack_1.cef", "")
		end
	end
else
	local trapDmg = getRandomNumber(800, 900)
		CreatureObject(playerObject):inflictDamage(playerObject, 0, trapDmg, 1)
      	CreatureObject(playerObject):playEffect("clienteffect/mustafar/dark_jedi_rock_attack_1.cef", "")
	end
end

function dant_discipleScreenplay:rockthrow_last(playerObject)
if (CreatureObject(playerObject):isGrouped()) then
	local groupSize = CreatureObject(playerObject):getGroupSize()
	for i = 0, groupSize - 1, 1 do
			local pMember = CreatureObject(playerObject):getGroupMember(i)
			if pMember ~= nil and SceneObject(pMember):isInRangeWithObject(playerObject, 200) then
			local trapDmg = getRandomNumber(900, 1100)
			CreatureObject(pMember):inflictDamage(pMember, 0, trapDmg, 1)
      		CreatureObject(playerObject):playEffect("clienteffect/mustafar/dark_jedi_rock_attack_1.cef", "")
		end
	end
else
	local trapDmg = getRandomNumber(900, 1100)
		CreatureObject(playerObject):inflictDamage(playerObject, 0, trapDmg, 1)
      	CreatureObject(playerObject):playEffect("clienteffect/mustafar/dark_jedi_rock_attack_1.cef", "")
	end
end


function dant_discipleScreenplay:finisher(playerObject)
	local trapDmg = getRandomNumber(2400, 3200)
		CreatureObject(playerObject):inflictDamage(playerObject, 0, trapDmg, 1)
      	CreatureObject(playerObject):playEffect("clienteffect/mustafar/som_force_crystal_drain.cef", "")
end

function dant_discipleScreenplay:spawnSupport(bossObject, playerObject)
	local boss = LuaCreatureObject(bossObject)
	local bossX = boss:getPositionX()
	local bossY = boss:getPositionY()
	local bossZ = boss:getPositionZ()
	local cell = boss:getParentID()
	
	local pGuard1 = spawnMobile("dantooine", "janta_scion", -1, bossX, bossZ, bossY, 93, cell) 
		CreatureObject(pGuard1):engageCombat(playerObject)
		spatialChat(pGuard1, "We .. serve ..")
	local pGuard2 = spawnMobile("dantooine", "janta_scion", -1, bossX, bossZ, bossY, 141, cell) 
		CreatureObject(pGuard2):engageCombat(playerObject)
	local pGuard3 = spawnMobile("dantooine", "janta_scion", -1, bossX, bossZ, bossY, 173, cell) 
		CreatureObject(pGuard3):engageCombat(playerObject)
	local pGuard4 = spawnMobile("dantooine", "janta_scion", -1, bossX, bossZ, bossY, 173, cell) 
		CreatureObject(pGuard4):engageCombat(playerObject)

end

function dant_discipleScreenplay:awardToken(bossObject)
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
                if (pGhost ~= nil and not PlayerObject(pGhost):hasBadge(162)) then
                    PlayerObject(pGhost):awardBadge(162)
                end
            end
        end
    end
end

function dant_discipleScreenplay:bossDead(pBoss)
	local creature = CreatureObject(pBoss)
	local respawn = math.random(7200,10800)
	createEvent(120 * 1000, "dant_discipleScreenplay", "KillBoss", pBoss, "") -- Corpse Despawn
	createEvent(respawn * 1000, "dant_discipleScreenplay", "KillSpawn", pBoss, "") -- Respawn
	return 0
end

function dant_discipleScreenplay:KillSpawn()
		self:spawnMobiles()
end

function dant_discipleScreenplay:KillBoss(pBoss)
      	writeData("dant_discipleScreenplay:spawnState",0)  
	dropObserver(pBoss, OBJECTDESTRUCTION)
	if SceneObject(pBoss) then
		SceneObject(pBoss):destroyObjectFromWorld()
	end
	return 0
end
