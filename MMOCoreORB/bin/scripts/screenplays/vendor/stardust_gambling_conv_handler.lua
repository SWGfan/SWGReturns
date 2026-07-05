StardustGamblingConvoHandler = conv_handler:new {
	--Vendor Setup
	GamblingLogicType = StardustGamblingLogic,
	--Vendor Greeting
	initialDialog = "Welcome! I trade Stardust Tokens for lewt boxes! Would you like to gamble?!",
	--Player selection to start buying, vendor spatial.
	buyWindow = "Sure, I like to gamble.",
	openSUItext = "",
	gambleWindow = "test Dialog (GambleWindow)",
	gambleLuck = "Good Luck!",
	saleOption_as = "Test Sale Dialog2",
	--Player selection to get vendor information, vendor dialog.
	informationFirstOption = "",
	informationFirstDialog = "",
	--Player selection to get more vendor information, vendor dialog.
	informationSecondOption = "",
	informationSecondDialog = "",
	--Set faction restriction (FACTIONIMPERIAL or FACTIONREBEL)
	factionRestriction = "",

}

function StardustGamblingConvoHandler:getInitialScreen(pPlayer, pNpc, pConvTemplate)
	local convoTemplate = LuaConversationTemplate(pConvTemplate)
	
	if (self.factionRestriction ~= "") then
		if (self.factionRestriction == FACTIONIMPERIAL and not CreatureObject(pPlayer):getFaction() == FACTIONIMPERIAL) or 
		(self.factionRestriction == FACTIONREBEL and not CreatureObject(pPlayer):getFaction() == FACTIONREBEL) or
		(CreatureObject(pPlayer):getFactionStanding(self.factionRestriction) < 200) then
			return convoTemplate:getScreen("factionRestriction")
		end
	end
	
	return convoTemplate:getInitialScreen()
end


function StardustGamblingConvoHandler:runScreenHandlers(pConvTemplate, pPlayer, pNpc, selectedOption, pConvScreen)
	local screen = LuaConversationScreen(pConvScreen)
	local screenID = screen:getScreenID()
	local pConvScreen = screen:cloneScreen()
	local clonedScreen = LuaConversationScreen(pConvScreen)
	
	if (screenID == "initial") then
		clonedScreen:setCustomDialogText(self.initialDialog)
		clonedScreen:addOption(self.buyWindow, "gamble_sui")
	end
	if (screenID == "gamble_sui") then
		clonedScreen:setCustomDialogText(self.gambleLuck)
		StardustGamblingLogic:openSUIGambling(pPlayer)
	end
	return pConvScreen
end