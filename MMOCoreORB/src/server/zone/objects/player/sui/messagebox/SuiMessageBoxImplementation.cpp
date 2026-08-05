/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.*/

#include "server/zone/objects/player/sui/messagebox/SuiMessageBox.h"

BaseMessage* SuiMessageBoxImplementation::generateMessage() {
	SuiCreatePageMessage* message = new SuiCreatePageMessage(boxID, "Script.messageBox");

	//Declare Headers:
	addHeader("Prompt.lblPrompt", "Text");
	addHeader("bg.caption.lblTitle", "Text");

	//Set Body Options:
	addSetting("3", "Prompt.lblPrompt", "Text", promptText);
	addSetting("3", "bg.caption.lblTitle", "Text", promptTitle);

	// SUI_MESSAGEBOX_BUTTON_TEXT_FIX_2026_08_05-- these three used to only send the custom button
	// text to the client when it started with "@" (an STF key), so a literal
	// label like "GO" was silently dropped and the client fell back to the
	// widget's own built-in default ("Yes"/"No"). promptText/promptTitle
	// above never had this gate and always send -- bringing the buttons in
	// line with that fixes it, for both STF-key and literal-text labels.
	if (cancelButton) {
		addSetting("3", "btnCancel", "Enabled", "True");
		addSetting("3", "btnCancel", "Visible", "True");

		if (!cancelButtonText.isEmpty())
			addSetting("3", "btnCancel", "Text", cancelButtonText);
	} else {
		addSetting("3", "btnCancel", "Enabled", "False");
		addSetting("3", "btnCancel", "Visible", "False");
	}

	if (!okButtonText.isEmpty())
		addSetting("3", "btnOk", "Text", okButtonText);

	if (otherButton) {
		addSetting("3", "btnRevert","Enabled","True");
		addSetting("3", "btnRevert","Visible","True");

		if (!otherButtonText.isEmpty())
			addSetting("3", "btnRevert","Text", otherButtonText);
	} else {
		addSetting("3", "btnRevert", "Enabled", "False");
		addSetting("3", "btnRevert", "Visible", "False");
	}

	setHandlerText("handleSUI");

	//Generate Packet:
	generateHeader(message);
	generateBody(message);
	generateFooter(message);
	hasGenerated = true;

	return message;
}
