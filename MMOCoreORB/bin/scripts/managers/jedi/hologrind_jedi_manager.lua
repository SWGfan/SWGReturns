JediManager = require("managers.jedi.jedi_manager")
local ObjectManager = require("managers.object.object_manager")

jediManagerName = "HologrindJediManager"
NUMBEROFPROFESSIONSTOMASTER = 4

HologrindJediManager = JediManager:new {
	screenplayName = jediManagerName,
	jediManagerName = jediManagerName,
	jediProgressionType = HOLOGRINDJEDIPROGRESSION,
	startingEvent = nil,
}

-- Return a list of all professions and their badge number that are available for the hologrind
function HologrindJediManager:getGrindableProfessionList()
	local grindableProfessions = {
		{ "crafting_architect_master",		CRAFTING_ARCHITECT_MASTER  },		--1
		{ "crafting_armorsmith_master",		CRAFTING_ARMORSMITH_MASTER  },		--2
		{ "crafting_artisan_master",		CRAFTING_ARTISAN_MASTER  },		--3
		{ "outdoors_bio_engineer_master",	OUTDOORS_BIO_ENGINEER_MASTER  },	--4
		{ "combat_bountyhunter_master",		COMBAT_BOUNTYHUNTER_MASTER  },		--5
		{ "combat_brawler_master",		COMBAT_BRAWLER_MASTER  },		--6
		{ "combat_carbine_master",		COMBAT_CARBINE_MASTER  },		--7
		{ "crafting_chef_master",		CRAFTING_CHEF_MASTER  },		--8
		{ "science_combatmedic_master",		SCIENCE_COMBATMEDIC_MASTER  },		--9
		{ "combat_commando_master",		COMBAT_COMMANDO_MASTER  },		--10
		{ "outdoors_creaturehandler_master",	OUTDOORS_CREATUREHANDLER_MASTER  },	--11
		{ "social_dancer_master",		SOCIAL_DANCER_MASTER  },		--12
		{ "science_doctor_master",		SCIENCE_DOCTOR_MASTER  },		--13
		{ "crafting_droidengineer_master",	CRAFTING_DROIDENGINEER_MASTER  },	--14
		{ "social_entertainer_master",		SOCIAL_ENTERTAINER_MASTER  },		--15
		{ "combat_1hsword_master",		COMBAT_1HSWORD_MASTER  },		--16
		{ "social_imagedesigner_master",	SOCIAL_IMAGEDESIGNER_MASTER  },		--17
		{ "combat_marksman_master",		COMBAT_MARKSMAN_MASTER  },		--18
		{ "science_medic_master",		SCIENCE_MEDIC_MASTER  },		--19
		{ "crafting_merchant_master",		CRAFTING_MERCHANT_MASTER  },		--20
		{ "social_musician_master",		SOCIAL_MUSICIAN_MASTER  },		--21
		{ "combat_polearm_master",		COMBAT_POLEARM_MASTER  },		--22
		{ "combat_pistol_master",		COMBAT_PISTOL_MASTER  },		--23
		{ "outdoors_ranger_master",		OUTDOORS_RANGER_MASTER  },		--24
		{ "combat_rifleman_master",		COMBAT_RIFLEMAN_MASTER  },		--25
		{ "outdoors_scout_master",		OUTDOORS_SCOUT_MASTER  },		--26
		{ "combat_smuggler_master",		COMBAT_SMUGGLER_MASTER  },		--27
		{ "outdoors_squadleader_master",	OUTDOORS_SQUADLEADER_MASTER  },		--28
		{ "combat_2hsword_master",		COMBAT_2HSWORD_MASTER  },		--29
		{ "crafting_tailor_master",		CRAFTING_TAILOR_MASTER  },		--30
		{ "crafting_weaponsmith_master",	CRAFTING_WEAPONSMITH_MASTER  },		--31
		{ "combat_unarmed_master",		COMBAT_UNARMED_MASTER  },		--32
	}
	return grindableProfessions
end

-- Assign random hologrind professions on player creation.
function HologrindJediManager:onPlayerCreated(pCreatureObject)
	local skillList = self:getGrindableProfessionList()
	local pGhost = CreatureObject(pCreatureObject):getPlayerObject()
	if (pGhost == nil) then
		return
	end
	for i = 1, NUMBEROFPROFESSIONSTOMASTER, 1 do
		local numberOfSkillsInList = #skillList
		local skillNumber = getRandomNumber(1, numberOfSkillsInList)
		PlayerObject(pGhost):addHologrindProfession(skillList[skillNumber][2])
		table.remove(skillList, skillNumber)
	end
end

-- Count how many hologrind professions the player has mastered.
function HologrindJediManager:getNumberOfMasteredProfessions(pCreatureObject)
	local pGhost = CreatureObject(pCreatureObject):getPlayerObject()
	if (pGhost == nil) then
		return 0
	end
	local professions = PlayerObject(pGhost):getHologrindProfessions()
	local masteredNumberOfProfessions = 0
	for i = 1, #professions, 1 do
		if PlayerObject(pGhost):hasBadge(professions[i]) then
			masteredNumberOfProfessions = masteredNumberOfProfessions + 1
		end
	end
	return masteredNumberOfProfessions
end

-- Check if the player is jedi.
function HologrindJediManager:isJedi(pCreatureObject)
	local pGhost = CreatureObject(pCreatureObject):getPlayerObject()
	if (pGhost == nil) then
		return false
	end
	return PlayerObject(pGhost):isJedi()
end

-- Sui window ok pressed callback.
function HologrindJediManager:notifyOkPressed()
	-- Do nothing.
end

-- Send the Returns-specific jedi unlock meditation SUI window.
function HologrindJediManager:sendSuiWindow(pCreatureObject)
	local suiManager = LuaSuiManager()
	suiManager:sendMessageBox(
		pCreatureObject,
		pCreatureObject,
		"Returns: Awakening",
		"A tremor stirs within you  -  something long dormant has begun to surface.\n\n" ..
		"The Force does not call out. It waits.\n\n" ..
		"Seek solitude. Clear your mind of conflict and ambition. " ..
		"In the silence between heartbeats, you may yet hear what the galaxy has always whispered to you.\n\n" ..
		"Your path back begins now.",
		"@ok",
		"HologrindJediManager",
		"notifyOkPressed"
	)
end

-- Award jedi skill and state to the player.
function HologrindJediManager:awardJediStatusAndSkill(pCreatureObject)
	local pGhost = CreatureObject(pCreatureObject):getPlayerObject()
	if (pGhost == nil) then
		return
	end
	awardSkill(pCreatureObject, "force_title_jedi_novice")
	PlayerObject(pGhost):setJediState(1)
end

-- Check progression and award jedi status if all professions are mastered.
function HologrindJediManager:checkIfProgressedToJedi(pCreatureObject)
	if self:getNumberOfMasteredProfessions(pCreatureObject) >= NUMBEROFPROFESSIONSTOMASTER and not self:isJedi(pCreatureObject) then
		self:sendSuiWindow(pCreatureObject)
		self:awardJediStatusAndSkill(pCreatureObject)
	end
end

-- Badge awarded event handler.
function HologrindJediManager:badgeAwardedEventHandler(pCreatureObject, pCreatureObject2, badgeNumber)
	if (pCreatureObject == nil) then
		return 0
	end
	self:checkIfProgressedToJedi(pCreatureObject)
	return 0
end

-- Register badge observer on the player.
function HologrindJediManager:registerObservers(pCreatureObject)
	createObserver(BADGEAWARDED, "HologrindJediManager", "badgeAwardedEventHandler", pCreatureObject)
end

-- On login: check progression and re-register observers.
function HologrindJediManager:onPlayerLoggedIn(pCreatureObject)
	if (pCreatureObject == nil) then
		return
	end
	self:checkIfProgressedToJedi(pCreatureObject)
	self:registerObservers(pCreatureObject)
end

-- Get profession string ID from badge number.
function HologrindJediManager:buildBadgeNameMap()
	local skillList = self:getGrindableProfessionList()
	local map = {}
	for i = 1, #skillList, 1 do
		map[tostring(skillList[i][2])] = skillList[i][1]
	end
	return map
end

function HologrindJediManager:getProfessionStringIdFromBadgeNumber(badgeNumber)
	local map = self:buildBadgeNameMap()
	return map[tostring(badgeNumber)]
end

-- Human-readable names for profession badge numbers.
function HologrindJediManager:getProfessionDisplayName(badgeNumber)
	local names = {
		[CRAFTING_ARCHITECT_MASTER] = "Master Architect",
		[CRAFTING_ARMORSMITH_MASTER] = "Master Armorsmith",
		[CRAFTING_ARTISAN_MASTER] = "Master Artisan",
		[OUTDOORS_BIO_ENGINEER_MASTER] = "Master Bio-Engineer",
		[COMBAT_BOUNTYHUNTER_MASTER] = "Master Bounty Hunter",
		[COMBAT_BRAWLER_MASTER] = "Master Brawler",
		[COMBAT_CARBINE_MASTER] = "Master Carbineer",
		[CRAFTING_CHEF_MASTER] = "Master Chef",
		[SCIENCE_COMBATMEDIC_MASTER] = "Master Combat Medic",
		[COMBAT_COMMANDO_MASTER] = "Master Commando",
		[OUTDOORS_CREATUREHANDLER_MASTER] = "Master Creature Handler",
		[SOCIAL_DANCER_MASTER] = "Master Dancer",
		[SCIENCE_DOCTOR_MASTER] = "Master Doctor",
		[CRAFTING_DROIDENGINEER_MASTER] = "Master Droid Engineer",
		[SOCIAL_ENTERTAINER_MASTER] = "Master Entertainer",
		[COMBAT_1HSWORD_MASTER] = "Master Swordsman",
		[SOCIAL_IMAGEDESIGNER_MASTER] = "Master Image Designer",
		[COMBAT_MARKSMAN_MASTER] = "Master Marksman",
		[SCIENCE_MEDIC_MASTER] = "Master Medic",
		[CRAFTING_MERCHANT_MASTER] = "Master Merchant",
		[SOCIAL_MUSICIAN_MASTER] = "Master Musician",
		[COMBAT_POLEARM_MASTER] = "Master Pikeman",
		[COMBAT_PISTOL_MASTER] = "Master Pistoleer",
		[OUTDOORS_RANGER_MASTER] = "Master Ranger",
		[COMBAT_RIFLEMAN_MASTER] = "Master Rifleman",
		[OUTDOORS_SCOUT_MASTER] = "Master Scout",
		[COMBAT_SMUGGLER_MASTER] = "Master Smuggler",
		[OUTDOORS_SQUADLEADER_MASTER] = "Master Squad Leader",
		[COMBAT_2HSWORD_MASTER] = "Master Swordsman",
		[CRAFTING_TAILOR_MASTER] = "Master Tailor",
		[CRAFTING_WEAPONSMITH_MASTER] = "Master Weaponsmith",
		[COMBAT_UNARMED_MASTER] = "Master Unarmed Combatant",
	}
	return names[badgeNumber] or "Unknown Profession"
end

-- Send holocron hint message to the player.
-- Holocrons are NEVER silent  -  they always give a hint until all professions are mastered.
function HologrindJediManager:sendHolocronMessage(pCreatureObject)
	local mastered = self:getNumberOfMasteredProfessions(pCreatureObject)

	if mastered >= NUMBEROFPROFESSIONSTOMASTER then
		CreatureObject(pCreatureObject):sendSystemMessage("The holocron shimmers and dissolves into the Force. Your training is complete.")
		return true
	end

	local pGhost = CreatureObject(pCreatureObject):getPlayerObject()
	if (pGhost == nil) then
		return false
	end

	local unmastered = {}
	local professions = PlayerObject(pGhost):getHologrindProfessions()
	for i = 1, #professions, 1 do
		if not PlayerObject(pGhost):hasBadge(professions[i]) then
			table.insert(unmastered, professions[i])
		end
	end

	if #unmastered > 0 then
		local pick = unmastered[getRandomNumber(1, #unmastered)]
		local displayName = self:getProfessionDisplayName(pick)
		CreatureObject(pCreatureObject):sendSystemMessage("A mysterious voice speaks from the holocron: \"To continue your path to enlightenment, you must master " .. displayName .. ".\"")
	end

	return false
end

-- Use item handler  -  consumes the holocron after giving a hint.
function HologrindJediManager:useItem(pSceneObject, itemType, pCreatureObject)
	if (pCreatureObject == nil or pSceneObject == nil) then
		return
	end
	if itemType == ITEMHOLOCRON then
		local isSilent = self:sendHolocronMessage(pCreatureObject)
		if not isSilent then
			-- Holocron consumed after delivering its message
			SceneObject(pSceneObject):destroyObjectFromWorld()
			SceneObject(pSceneObject):destroyObjectFromDatabase()
		end
	end
end

function HologrindJediManager:canLearnSkill(pPlayer, skillName)
	return true
end

registerScreenPlay("HologrindJediManager", true)
return HologrindJediManager