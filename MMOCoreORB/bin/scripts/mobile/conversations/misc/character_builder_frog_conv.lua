characterBuilderFrogConvoTemplate = ConvoTemplate:new {
	initialScreen = "cbf_welcome",
	templateType = "Lua",
	luaClassHandler = "characterBuilderFrogConvoHandler",
	screens = {}
}

cbf_welcome = ConvoScreen:new {
	id = "cbf_welcome",
	leftDialog = "",
	customDialogText = "Greetings. Nasty has asked me to help the Jedi.",
	stopConversation = "false",
	options = {
		{"Staff: Grant me Jedi Master rank.", "cbf_staff_menu"},
		{"No thank you.", "cbf_goodbye"},
	}
}
characterBuilderFrogConvoTemplate:addScreen(cbf_welcome)

cbf_staff_menu = ConvoScreen:new {
	id = "cbf_staff_menu",
	leftDialog = "",
	customDialogText = "Which council?",
	stopConversation = "false",
	options = {
		{"Dark Jedi Master", "cbf_grant_dark_master"},
		{"Light Jedi Master", "cbf_grant_light_master"},
		{"Never mind.", "cbf_goodbye"},
	}
}
characterBuilderFrogConvoTemplate:addScreen(cbf_staff_menu)

cbf_grant_dark_master = ConvoScreen:new {
	id = "cbf_grant_dark_master",
	leftDialog = "",
	customDialogText = "It is done. You now stand among the Dark Jedi Masters.",
	stopConversation = "true",
	options = {}
}
characterBuilderFrogConvoTemplate:addScreen(cbf_grant_dark_master)

cbf_grant_light_master = ConvoScreen:new {
	id = "cbf_grant_light_master",
	leftDialog = "",
	customDialogText = "It is done. You now stand among the Light Jedi Masters.",
	stopConversation = "true",
	options = {}
}
characterBuilderFrogConvoTemplate:addScreen(cbf_grant_light_master)

cbf_goodbye = ConvoScreen:new {
	id = "cbf_goodbye",
	leftDialog = "",
	customDialogText = "Very well. Come back any time.",
	stopConversation = "true",
	options = {}
}
characterBuilderFrogConvoTemplate:addScreen(cbf_goodbye)

addConversationTemplate("characterBuilderFrogConvoTemplate", characterBuilderFrogConvoTemplate)
