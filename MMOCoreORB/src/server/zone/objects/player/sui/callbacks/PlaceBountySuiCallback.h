/*
 * PlaceBountySuiCallback.h
 */

#ifndef PLACEBOUNTYSUICALLBACK_H_
#define PLACEBOUNTYSUICALLBACK_H_

#include "server/zone/managers/mission/MissionManager.h"
#include "server/zone/objects/player/sui/SuiCallback.h"
#include "server/zone/objects/player/sui/inputbox/SuiInputBox.h"
#include "server/chat/ChatManager.h"

class PlaceBountySuiCallback : public SuiCallback {
private:
	ManagedReference<CreatureObject*> killerPlayer;

public:
	PlaceBountySuiCallback(ZoneServer* server, CreatureObject* killer) :
			SuiCallback(server), killerPlayer(killer) {
	}

	void run(CreatureObject* player, SuiBox* suiBox, uint32 eventIndex, Vector<UnicodeString>* args) {
		if (!suiBox->isInputBox() || eventIndex == 1 || args == nullptr || args->isEmpty())
			return;

		int reward = Integer::valueOf(args->get(0).toString());
		const int minBounty = 100000;
		const int maxBounty = 500000;

		reward = Math::max(minBounty, Math::min(maxBounty, reward));

		MissionManager* missionManager = server->getMissionManager();
		PlayerObject* killerGhost = killerPlayer != nullptr ? killerPlayer->getPlayerObject() : nullptr;

		if (missionManager == nullptr || killerGhost == nullptr)
			return;

		if (reward > player->getBankCredits()) {
			player->sendSystemMessage("You have insufficient credits to place a bounty on " + killerPlayer->getFirstName() + ".");
			return;
		}

		player->subtractBankCredits(reward);
		killerGhost->setBountyReward(reward);
		missionManager->addPlayerToBountyList(killerPlayer->getObjectID(), reward);
		killerGhost->setVisibility(8000);

		player->sendSystemMessage("You have successfully placed a bounty on " + killerPlayer->getFirstName() + ".");
		killerPlayer->sendSystemMessage("Warning!! " + player->getFirstName() + " has placed a bounty on you.");

		StringBuffer broadcast;
		broadcast << "\\#66B3FF[Spynet Alert] \\#FFFFFFThe Guild has posted a new target.";
		killerPlayer->getZoneServer()->getChatManager()->broadcastGalaxy(nullptr, broadcast.toString());
	}
};

#endif /* PLACEBOUNTYSUICALLBACK_H_ */
