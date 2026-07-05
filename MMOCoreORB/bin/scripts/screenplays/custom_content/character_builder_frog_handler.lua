characterBuilderFrogConvoHandler = conv_handler:new {}

function characterBuilderFrogConvoHandler:runScreenHandlers(pConvTemplate, pPlayer, pNpc, selectedOption, pConvScreen)
	local screen = LuaConversationScreen(pConvScreen)
	local screenID = screen:getScreenID()

	if screenID == "cbf_grant_dark_master" then
		local pGhost = CreatureObject(pPlayer):getPlayerObject()
		if pGhost ~= nil then
			PlayerObject(pGhost):setJediState(8)
			PlayerObject(pGhost):setFrsCouncil(2)
			PlayerObject(pGhost):setFrsRank(11)
			PlayerObject(pGhost):addSkillPoints(5000)
		end
		if not CreatureObject(pPlayer):hasSkill("force_title_jedi_novice") then
			FsIntro:completeVillageIntroFrog(pPlayer)
			FsOutro:completeVillageOutroFrog(pPlayer)
		end

		local jediTitles = { "force_title_jedi_rank_02", "force_title_jedi_rank_03" }
		for _, title in ipairs(jediTitles) do
			if not CreatureObject(pPlayer):hasSkill(title) then
				CreatureObject(pPlayer):addSkillDirect(title)
			end
		end
		local skills = {
			"force_rank_dark_novice","force_rank_dark_rank_01","force_rank_dark_rank_02",
			"force_rank_dark_rank_03","force_rank_dark_rank_04","force_rank_dark_rank_05",
			"force_rank_dark_rank_06","force_rank_dark_rank_07","force_rank_dark_rank_08",
			"force_rank_dark_rank_09","force_rank_dark_rank_10","force_rank_dark_master"
		}
		for _, skill in ipairs(skills) do
			if not CreatureObject(pPlayer):hasSkill(skill) then
				awardSkill(pPlayer, skill)
			end
		end
		local pInventory = SceneObject(pPlayer):getSlottedObject("inventory")
		if pInventory ~= nil and not SceneObject(pInventory):isContainerFullRecursive() then
			giveItem(pInventory, "object/tangible/wearables/robe/robe_jedi_dark_s05.iff", -1)
		end
		CreatureObject(pPlayer):sendSystemMessage("You have been granted Dark Jedi Master rank.")

	elseif screenID == "cbf_grant_light_master" then
		local pGhost = CreatureObject(pPlayer):getPlayerObject()
		if pGhost ~= nil then
			PlayerObject(pGhost):setJediState(4)
			PlayerObject(pGhost):setFrsCouncil(1)
			PlayerObject(pGhost):setFrsRank(11)
			PlayerObject(pGhost):addSkillPoints(5000)
		end
		if not CreatureObject(pPlayer):hasSkill("force_title_jedi_novice") then
			FsIntro:completeVillageIntroFrog(pPlayer)
			FsOutro:completeVillageOutroFrog(pPlayer)
		end

		local jediTitles = { "force_title_jedi_rank_02", "force_title_jedi_rank_03" }
		for _, title in ipairs(jediTitles) do
			if not CreatureObject(pPlayer):hasSkill(title) then
				CreatureObject(pPlayer):addSkillDirect(title)
			end
		end
		local skills = {
			"force_rank_light_novice","force_rank_light_rank_01","force_rank_light_rank_02",
			"force_rank_light_rank_03","force_rank_light_rank_04","force_rank_light_rank_05",
			"force_rank_light_rank_06","force_rank_light_rank_07","force_rank_light_rank_08",
			"force_rank_light_rank_09","force_rank_light_rank_10","force_rank_light_master"
		}
		for _, skill in ipairs(skills) do
			if not CreatureObject(pPlayer):hasSkill(skill) then
				awardSkill(pPlayer, skill)
			end
		end
		local pInventory = SceneObject(pPlayer):getSlottedObject("inventory")
		if pInventory ~= nil and not SceneObject(pInventory):isContainerFullRecursive() then
			giveItem(pInventory, "object/tangible/wearables/robe/robe_jedi_light_s05.iff", -1)
		end
		CreatureObject(pPlayer):sendSystemMessage("You have been granted Light Jedi Master rank.")
	end

	return pConvScreen
end
