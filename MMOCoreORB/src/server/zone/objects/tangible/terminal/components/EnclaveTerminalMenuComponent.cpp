// Coded by BoosterSteel 19-03-2026
#include "EnclaveTerminalMenuComponent.h"
#include "server/zone/Zone.h"
#include "server/zone/packets/object/ObjectMenuResponse.h"
#include "server/zone/objects/scene/SceneObject.h"
#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/objects/building/BuildingObject.h"
#include "server/zone/managers/frs/FrsManager.h"
#include "server/zone/objects/tangible/TangibleObject.h"
#include "server/zone/objects/player/variables/FrsData.h"
#include "server/zone/managers/director/DirectorManager.h"

void EnclaveTerminalMenuComponent::fillObjectMenuResponse(SceneObject* sceneObject, ObjectMenuResponse* menuResponse, CreatureObject* player) const {
	ManagedReference<BuildingObject*> building = sceneObject->getParentRecursively(SceneObjectType::BUILDING).castTo<BuildingObject*>();

	if (building == nullptr || player->isDead() || player->isIncapacitated())
		return;

	ZoneServer* zServ = building->getZoneServer();

	if (zServ == nullptr)
		return;

	FrsManager* frsManager = zServ->getFrsManager();

	if (frsManager == nullptr)
		return;

	int enclaveType = frsManager->getEnclaveType(building);

	if (enclaveType == 0)
		return;

	int terminalType = getTerminalType(sceneObject);

	if (terminalType == 0)
		return;

	PlayerObject* ghost = player->getPlayerObject();

	if (ghost == nullptr)
		return;

	// ---- GHOSTS JEDI UNLOCK TRIAL OPTIONS ----
	// Show to any Force Sensitive — eligibility checked fully in Lua
	if (ghost->getJediState() >= 1) {
		if (terminalType == LIGHT_CHALLENGE) {
			menuResponse->addRadialMenuItem(81, 3, "Begin Jedi Master Trial");
		} else if (terminalType == DARK_CHALLENGE) {
			menuResponse->addRadialMenuItem(81, 3, "Begin Dark Jedi Master Trial");
		}
	}
	// ---- END GHOSTS JEDI UNLOCK TRIAL OPTIONS ----

	FrsData* frsData = ghost->getFrsData();
	int playerRank = frsData->getRank();

	if (playerRank < 0 && !ghost->isPrivileged()) {
		player->sendSystemMessage("@force_rank:insufficient_rank_vote");
		return;
	}

	if (frsData->getCouncilType() == 0 && !ghost->isPrivileged())
		return;

	if (frsManager->isPlayerFightingInArena(player->getObjectID()))
		return;

	if (terminalType == VOTING || terminalType == LIGHT_CHALLENGE || terminalType == DARK_CHALLENGE) {
		// ---- GHOSTS DISCIPLINE TRAINING OPTIONS ----
		if (terminalType == LIGHT_CHALLENGE) {
			menuResponse->addRadialMenuItem(83, 3, "Train Lightsaber Mastery");
			menuResponse->addRadialMenuItem(84, 3, "Train Force Powers");
			menuResponse->addRadialMenuItem(85, 3, "Train Defence");
			menuResponse->addRadialMenuItem(86, 3, "Train Guardian Arts");
		} else if (terminalType == DARK_CHALLENGE) {
			menuResponse->addRadialMenuItem(83, 3, "Train Lightsaber Mastery");
			menuResponse->addRadialMenuItem(84, 3, "Train Force Powers");
			menuResponse->addRadialMenuItem(85, 3, "Train Defence");
			menuResponse->addRadialMenuItem(86, 3, "Train Tyrant Arts");
		}
		// ---- END GHOSTS DISCIPLINE TRAINING OPTIONS ----

		// Ghosts custom promotion system
		menuResponse->addRadialMenuItem(82, 3, "Request Promotion");
		menuResponse->addRadialMenuItem(74, 3, "@force_rank:recover_jedi_items");
	}
}

int EnclaveTerminalMenuComponent::handleObjectMenuSelect(SceneObject* sceneObject, CreatureObject* player, byte selectedID) const {
	if (sceneObject == nullptr || !sceneObject->isTangibleObject() || player == nullptr || player->isDead() || player->isIncapacitated())
		return 0;

	if (player->getDistanceTo(sceneObject) > 15) {
		player->sendSystemMessage("@pvp_rating:ch_terminal_too_far");
		return 1;
	}

	ManagedReference<BuildingObject*> building = sceneObject->getParentRecursively(SceneObjectType::BUILDING).castTo<BuildingObject*>();

	if (building == nullptr)
		return 1;

	ZoneServer* zServ = building->getZoneServer();

	if (zServ == nullptr)
		return 1;

	FrsManager* frsManager = zServ->getFrsManager();

	if (frsManager == nullptr)
		return 1;

	int enclaveType = frsManager->getEnclaveType(building);

	if (enclaveType == 0)
		return 1;

	int terminalType = getTerminalType(sceneObject);

	if (terminalType == 0)
		return 1;

	PlayerObject* ghost = player->getPlayerObject();

	if (ghost == nullptr)
		return 1;

	// ---- GHOSTS JEDI UNLOCK TRIAL HANDLER (selectedID 81) ----
	if (selectedID == 81) {
		Lua* lua = DirectorManager::instance()->getLuaInstance();
		if (lua == nullptr)
			return 1;

		String funcName = "";
		if (terminalType == LIGHT_CHALLENGE)
			funcName = "enclave_terminal_master_light";
		else if (terminalType == DARK_CHALLENGE)
			funcName = "enclave_terminal_master_dark";

		if (funcName.isEmpty())
			return 1;

		Reference<LuaFunction*> luaFunc = lua->createFunction(funcName, 0);
		if (luaFunc == nullptr)
			return 1;

		*luaFunc << player;
		*luaFunc << sceneObject;
		*luaFunc << ghost;
		luaFunc->callFunction();
		return 0;
	}

	// ---- GHOSTS DISCIPLINE TRAINING HANDLERS (selectedID 83-86) ----
	if (selectedID >= 83 && selectedID <= 86) {
		Lua* lua = DirectorManager::instance()->getLuaInstance();
		if (lua == nullptr)
			return 1;

		// Map selectedID to branch index (1=Lightsaber, 2=Force, 3=Defence, 4=Guardian/Tyrant)
		int branchIndex = selectedID - 82; // 83->1, 84->2, 85->3, 86->4

		String funcName = "";
		if (terminalType == LIGHT_CHALLENGE)
			funcName = "enclave_terminal_train_light_branch";
		else if (terminalType == DARK_CHALLENGE)
			funcName = "enclave_terminal_train_dark_branch";

		if (funcName.isEmpty())
			return 1;

		Reference<LuaFunction*> luaFunc = lua->createFunction(funcName, 0);
		if (luaFunc == nullptr)
			return 1;

		*luaFunc << player;
		*luaFunc << sceneObject;
		*luaFunc << ghost;
		*luaFunc << branchIndex;
		luaFunc->callFunction();
		return 0;
	}

	// ---- FRS PROMOTION HANDLER (selectedID 82) ----
	if (selectedID == 82) {
		Lua* lua = DirectorManager::instance()->getLuaInstance();
		if (lua == nullptr)
			return 1;

		Reference<LuaFunction*> luaFunc = lua->createFunction("frs_request_promotion", 0);
		if (luaFunc == nullptr)
			return 1;

		*luaFunc << player;
		*luaFunc << sceneObject;
		*luaFunc << ghost;
		luaFunc->callFunction();
		return 0;
	}

	// ---- END GHOSTS HANDLERS ----

	FrsData* frsData = ghost->getFrsData();
	int playerRank = frsData->getRank();

	if (playerRank < 0 && !ghost->isPrivileged()) {
		player->sendSystemMessage("@force_rank:insufficient_rank_vote");
		return 1;
	}

	if (frsData->getCouncilType() == 0 && !ghost->isPrivileged())
		return 1;

	if (frsManager->isPlayerFightingInArena(player->getObjectID()))
		return 1;

	if (terminalType == VOTING) {
		if (selectedID == 69)
			frsManager->sendVoteSUI(player, sceneObject, FrsManager::SUI_VOTE_STATUS, enclaveType);
		else if (selectedID == 70)
			frsManager->sendVoteSUI(player, sceneObject, FrsManager::SUI_VOTE_RECORD, enclaveType);
		else if (selectedID == 71)
			frsManager->sendVoteSUI(player, sceneObject, FrsManager::SUI_VOTE_ACCEPT_PROMOTE, enclaveType);
		else if (selectedID == 75)
			frsManager->sendVoteSUI(player, sceneObject, FrsManager::SUI_VOTE_DEMOTE, enclaveType);
		else if (selectedID == 73)
			frsManager->sendVoteSUI(player, sceneObject, FrsManager::SUI_VOTE_PETITION, enclaveType);
#if FRS_TESTING
		else if (selectedID == 76 && ghost->isPrivileged())
			frsManager->sendVoteSUI(player, sceneObject, FrsManager::SUI_FORCE_PHASE_CHANGE, enclaveType);
#endif
		else if (selectedID == 74)
			frsManager->recoverJediItems(player);
	} else if (terminalType == LIGHT_CHALLENGE) {
		if (selectedID == 69)
			frsManager->sendChallengeVoteSUI(player, sceneObject, FrsManager::SUI_CHAL_VOTE_STATUS, enclaveType);
		else if (selectedID == 70)
			frsManager->sendChallengeVoteSUI(player, sceneObject, FrsManager::SUI_CHAL_VOTE_RECORD, enclaveType);
		else if (selectedID == 71)
			frsManager->sendChallengeVoteSUI(player, sceneObject, FrsManager::SUI_CHAL_VOTE_ISSUE, enclaveType);
	} else if (terminalType == DARK_CHALLENGE) {
		if (selectedID == 69)
			frsManager->sendArenaChallengeSUI(player, sceneObject, FrsManager::SUI_ARENA_CHAL_SCORES, enclaveType);
		else if (selectedID == 70)
			frsManager->sendArenaChallengeSUI(player, sceneObject, FrsManager::SUI_ARENA_CHAL_STATUS, enclaveType);
		else if (selectedID == 71)
			frsManager->sendArenaChallengeSUI(player, sceneObject, FrsManager::SUI_ARENA_CHAL_VIEW, enclaveType);
		else if (selectedID == 72)
			frsManager->sendArenaChallengeSUI(player, sceneObject, FrsManager::SUI_ARENA_CHAL_ACCEPT, enclaveType);
		else if (selectedID == 73)
			frsManager->sendArenaChallengeSUI(player, sceneObject, FrsManager::SUI_ARENA_CHAL_ISSUE, enclaveType);
#if FRS_TESTING
		else if (selectedID == 76 && ghost->isPrivileged())
			frsManager->forceArenaOpen(player);
#endif
	}

	return 0;
}

int EnclaveTerminalMenuComponent::getTerminalType(SceneObject* terminal) const {
	if (terminal == nullptr)
		return 0;

	uint64 terminalCRC = terminal->getServerObjectCRC();

	if (terminalCRC == STRING_HASHCODE("object/tangible/terminal/terminal_light_enclave_voting.iff"))
		return VOTING;
	else if (terminalCRC == STRING_HASHCODE("object/tangible/terminal/terminal_light_enclave_challenge.iff"))
		return LIGHT_CHALLENGE;
	else if (terminalCRC == STRING_HASHCODE("object/tangible/terminal/terminal_dark_enclave_voting.iff"))
		return VOTING;
	else if (terminalCRC == STRING_HASHCODE("object/tangible/terminal/terminal_dark_enclave_challenge.iff"))
		return DARK_CHALLENGE;

	return 0;
}