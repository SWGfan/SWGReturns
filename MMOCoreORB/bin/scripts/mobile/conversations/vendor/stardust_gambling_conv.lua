StardustGamblingConvoTemplate = ConvoTemplate:new {
	initialScreen = "initial",
	templateType = "Lua",
	luaClassHandler = "StardustGamblingConvoHandler",
	screens = {}
}

initial = ConvoScreen:new {
	id = "initial",
	customDialogText = "",
	stopConversation = "false",
	options = {
	}
}
StardustGamblingConvoTemplate:addScreen(initial);

gamble_sui = ConvoScreen:new {
	id = "gamble_sui",
	customDialogText = "",
	stopConversation = "true",
	options = {}
}
StardustGamblingConvoTemplate:addScreen(gamble_sui);

addConversationTemplate("StardustGamblingConvoTemplate", StardustGamblingConvoTemplate);