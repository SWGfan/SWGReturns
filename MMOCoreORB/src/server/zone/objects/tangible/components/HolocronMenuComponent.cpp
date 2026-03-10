/*
* HolocronMenuComponent.cpp
*
*  Created on: 09/16/2019
*      Author: TOXIC
*
*  Modified: Aftermath Server
*      Added: "Use for Studies" (221) - awards Jedi unlock points, visible to all players
*      Added: "Speak to the Gatekeeper" (222) - visible only when threshold is hit and player is pre-Padawan
*/

#include "HolocronMenuComponent.h"
#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/scene/SceneObject.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/packets/object/ObjectMenuResponse.h"
#include "server/zone/objects/player/sui/messagebox/SuiMessageBox.h"
#include "server/zone/managers/skill/SkillManager.h"
#include "server/zone/managers/player/PlayerManager.h"
#include "server/zone/packets/player/PlayMusicMessage.h"
#include "server/zone/managers/creature/CreatureManager.h"
#include "server/zone/objects/region/CityRegion.h"
#include "server/zone/ZoneServer.h"
#include "server/chat/ChatManager.h"
#include "server/zone/managers/jedi/JediManager.h"
#include "server/zone/managers/director/DirectorManager.h"

// ============================================================
// RADIAL MENU IDs
// 213-220: existing Aftermath menu items (unchanged)
// 221:     "Use for Studies"       - all players, awards Jedi points
// 222:     "Speak to the Gatekeeper" - shown only when threshold hit + pre-Padawan
// ============================================================

void HolocronMenuComponent::fillObjectMenuResponse(SceneObject* sceneObject, ObjectMenuResponse* menuResponse, CreatureObject* player) const {

	TangibleObjectMenuComponent::fillObjectMenuResponse(sceneObject, menuResponse, player);
	ManagedReference<PlayerObject*> ghost = player->getPlayerObject();

	if (ghost == nullptr)
		return;

	// ---- EXISTING ITEMS (unchanged) ----
	if (ghost->getJediState() >= 1) {
		menuResponse->addRadialMenuItem(213, 3, "Reveal Encrypted Data");
		menuResponse->addRadialMenuItemToRadialID(213, 215, 3, "Regenerate Full Force");
		menuResponse->addRadialMenuItemToRadialID(213, 216, 3, "Visibility");
		menuResponse->addRadialMenuItemToRadialID(213, 220, 3, "Unlock Gray Jedi");
	}

	// ---- AFTERMATH JEDI UNLOCK ITEMS ----
	// All eligibility checks handled in Lua, not here

	menuResponse->addRadialMenuItem(221, 3, "Use for Studies");
	menuResponse->addRadialMenuItem(222, 3, "Speak to the Gatekeeper");
	menuResponse->addRadialMenuItem(223, 3, "Reset Jedi Progress");
}

int HolocronMenuComponent::handleObjectMenuSelect(SceneObject* sceneObject, CreatureObject* creature, byte selectedID) const {
	ManagedReference<PlayerObject*> ghost = creature->getPlayerObject();

	if (ghost == nullptr)
		return 0;

	ZoneServer* zserv = creature->getZoneServer();

	if (zserv == nullptr)
		return 0;

	// ---- EXISTING HANDLERS (unchanged) ----

	if (selectedID == 213) {
		if (ghost->getJediState() >= 1) {
			JediManager::instance()->useItem(sceneObject, JediManager::ITEMHOLOCRON, creature);
		}
	}

	if (selectedID == 215 && (ghost->getJediState() >= 1)) {
		ManagedReference<PlayerObject*> playerObject = creature->getPlayerObject();
		if (!creature->checkCooldownRecovery("force_replenish_cooldown")) {
			if (playerObject->getForcePower() >= playerObject->getForcePowerMax()) {
				creature->sendSystemMessage("@jedi_spam:holocron_force_max");
			} else {
				StringIdChatParameter stringId;
				Time* cdTime = creature->getCooldownTime("force_replenish_cooldown");
				int timeLeft = floor((float)cdTime->miliDifference() / 1000) * -1;
				stringId.setStringId("@innate:equil_wait");
				stringId.setDI(timeLeft);
				creature->sendSystemMessage(stringId);
				error("Cooldown In Effect You May Not Replenish Force: " + creature->getFirstName());
				return 0;
			}
			return 0;
		}
		if (playerObject != nullptr && playerObject->getJediState() >= 1) {
			if (playerObject->getForcePower() < playerObject->getForcePowerMax()) {
				creature->sendSystemMessage("@jedi_spam:holocron_force_replenish");
				playerObject->setForcePower(playerObject->getForcePowerMax(), true);
				creature->addCooldown("force_replenish_cooldown", 3600 * 1000);
				sceneObject->destroyObjectFromWorld(true);
				creature->playEffect("clienteffect/pl_force_absorb_hit.cef");
				PlayMusicMessage* pmm = new PlayMusicMessage("sound/music_become_light_jedi.snd");
				playerObject->sendMessage(pmm);
			} else {
				creature->sendSystemMessage("Your force pool is currently full");
			}
		} else {
			JediManager::instance()->useItem(sceneObject, JediManager::ITEMHOLOCRON, creature);
		}
		return 0;
	}

	if (selectedID == 216 && (ghost->getJediState() >= 1)) {
		ManagedReference<SuiMessageBox*> box = new SuiMessageBox(creature, SuiWindowType::NONE);
		box->setPromptTitle("Jedi Visibility");
		int jediVis1 = ghost->getVisibility();
		StringBuffer promptText;
		String playerName = creature->getFirstName();
		promptText << "\\#00ff00 " << playerName << " Has " << "\\#000000 " << "(" << "\\#ffffff " << jediVis1 << "\\#000000 " << ")" << "\\#00ff00 " << " Jedi Visibility" << endl;
		box->setPromptText(promptText.toString());
		ghost->addSuiBox(box);
		creature->sendMessage(box->generateMessage());
	}

	if (selectedID == 220 && (ghost->getJediState() >= 1) && (ghost->getSkillPoints() == 250)) {
		ManagedReference<SuiMessageBox*> box = new SuiMessageBox(creature, SuiWindowType::CITY_ADMIN_CONFIRM_UPDATE_TYPE);
		creature->sendSystemMessage("You Have Unlocked Gray Jedi");
		int jediVis1 = ghost->getVisibility();
		box->setPromptTitle("Gray Jedi Progress");
		StringBuffer promptText;
		String playerName = creature->getFirstName();
		promptText << "\\#ffffff " << playerName << "\\#00ff00 Your Visibility is at: " << jediVis1;
		box->setPromptText(promptText.toString());
		ghost->addSuiBox(box);
		creature->sendMessage(box->generateMessage());
		SkillManager::instance()->awardSkill("combat_jedi_novice", creature, true, true, true);
		Vector3 coords(5294.95, -4123.03, 0);
		String zoneName = "dathomir";
		ghost->setTrainerCoordinates(coords);
		ghost->setTrainerZoneName(zoneName);
		creature->sendExecuteConsoleCommand("/pause 10;/findmytrainer");
		box->setForceCloseDistance(5.f);
	}

	if (selectedID == 220 && (ghost->getJediState() >= 1) && (ghost->getSkillPoints() < 250) && !creature->hasSkill("combat_jedi_novice")) {
		creature->sendSystemMessage("You do not meet the requirements for this feature, Force Sensitive and 250 skill points must be free to become gray jedi");
		return 0;
	}

	// ---- AFTERMATH JEDI UNLOCK HANDLERS ----

	// 221: "Use for Studies"
	if (selectedID == 221) {
		callLuaHolocronFunction(sceneObject, creature, "holocron_use_for_studies");
		return 0;
	}

	// 222: "Speak to the Gatekeeper"  Lua handles all eligibility checks
	if (selectedID == 222) {
		callLuaHolocronFunction(sceneObject, creature, "holocron_speak_to_gatekeeper");
		return 0;
	}

	// 223: "Reset Jedi Progress"
	if (selectedID == 223) {
		callLuaHolocronFunction(sceneObject, creature, "holocron_reset_progress");
		return 0;
	}

	return 0;
}

// ============================================================
// PRIVATE HELPER: Call a Lua function with (pSceneObject, pCreature, pGhost)
// Used by both "Use for Studies" and "Speak to the Gatekeeper"
// ============================================================

void HolocronMenuComponent::callLuaHolocronFunction(SceneObject* sceneObject, CreatureObject* creature, const String& functionName) const {

	if (creature == nullptr || sceneObject == nullptr)
		return;

	Lua* lua = DirectorManager::instance()->getLuaInstance();
	if (lua == nullptr)
		return;

	// Use LuaFunction with << operator for proper SWIG wrapping.
	// Only push creature and sceneObject  Lua retrieves ghost via getPlayerObject().
	Reference<LuaFunction*> luaFunc = lua->createFunction(functionName, 0);
	if (luaFunc == nullptr) {
		error("HolocronMenuComponent: Lua function not found: " + functionName);
		return;
	}

	*luaFunc << creature;    // pCreature (player)
	*luaFunc << sceneObject; // pTarget  (the holocron item)

	luaFunc->callFunction();
}