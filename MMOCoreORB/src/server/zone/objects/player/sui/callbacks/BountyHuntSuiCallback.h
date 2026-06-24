#ifndef PLAYER_BH_SUI_CALLBACK
#define PLAYER_BH_SUI_CALLBACK

#include "server/zone/objects/player/sui/SuiCallback.h"
#include "server/zone/objects/player/sui/inputbox/SuiInputBox.h"
#include "server/zone/managers/mission/MissionManager.h"
#include "server/zone/managers/visibility/VisibilityManager.h"
#include "server/zone/objects/player/PlayerObject.h"

class BountyHuntSuiCallback : public SuiCallback {
private:
    static const int MIN_BOUNTY = 20000;
    static const int MAX_BOUNTY = 1000000;

    void resendInputBox(CreatureObject* placer, CreatureObject* target) {
        if (placer == nullptr || target == nullptr)
            return;

        PlayerObject* placerGhost = placer->getPlayerObject();

        if (placerGhost == nullptr)
            return;

        ManagedReference<SuiInputBox*> input = new SuiInputBox(placer, SuiWindowType::PLAYER_BOUNTY_OFFER);

        input->setPromptTitle("Bounty Hunters Guild");

        StringBuffer prompt;
        prompt << "Enter the bounty amount you want to place on " << target->getFirstName() << ".\n\n"
            << "Bounties must be between 20,000 and 1,000,000 credits.";

        input->setPromptText(prompt.toString());
        input->setUsingObject(target);
        input->setCallback(new BountyHuntSuiCallback(placer->getZoneServer()));
        input->setCancelButton(true, "@cancel");

        placerGhost->addSuiBox(input);
        placer->sendMessage(input->generateMessage());
    }

public:
    BountyHuntSuiCallback(ZoneServer* serv) : SuiCallback(serv) {
    }

    void run(CreatureObject* placer, SuiBox* sui, uint32 eventIndex, Vector<UnicodeString>* args) {
        bool cancelPressed = (eventIndex == 1);

        if (sui == nullptr || !sui->isInputBox() || placer == nullptr || cancelPressed || args == nullptr || args->size() <= 0)
            return;

        ManagedReference<SceneObject*> suiObject = sui->getUsingObject().get();
        CreatureObject* target = cast<CreatureObject*>(suiObject.get());

        if (target == nullptr || target == placer)
            return;

        String arg1 = args->get(0).toString();
        int value = 0;

        try {
            value = Integer::valueOf(arg1);
        } catch (Exception& e) {
            value = 0;
        }

        if (arg1 == "" || value < MIN_BOUNTY || value > MAX_BOUNTY) {
            placer->sendSystemMessage("Bounty amount must be between 20,000 and 1,000,000 credits.");
            resendInputBox(placer, target);
            return;
        }

        int bank = placer->getBankCredits();
        int cash = placer->getCashCredits();

        if (bank + cash < value) {
            placer->sendSystemMessage("You have insufficient funds!");
            return;
        }

        MissionManager* missionManager = placer->getZoneServer()->getMissionManager();

        if (missionManager == nullptr)
            return;

        uint64 targetId = target->getObjectID();
        bool existingBounty = missionManager->hasPlayerBountyTargetInList(targetId);

        if (bank >= value) {
            placer->subtractBankCredits(value);
        } else {
            if (bank > 0)
                placer->subtractBankCredits(bank);

            placer->subtractCashCredits(value - bank);
        }

        if (existingBounty) {
            missionManager->increasePlayerBountyReward(targetId, value);
        } else {
            missionManager->addPlayerToBountyList(targetId, value);
        }

        PlayerObject* targetGhost = target->getPlayerObject();

        if (targetGhost != nullptr) {
            targetGhost->updatePlayerBountyTimestamp(172800000); // 48 hours
            targetGhost->setBountyPlacerId(placer->getObjectID());

            if (!existingBounty)
                targetGhost->setBountyReward(value);

            if (targetGhost->isOnline())
                missionManager->updatePlayerBountyOnlineStatus(targetId, true);
        }

        target->playEffect("clienteffect/ui_missile_aquiring.cef", "");
        placer->playEffect("clienteffect/holoemote_haunted.cef", "head");
        placer->sendSystemMessage("Bounty has been successfully placed!");

        VisibilityManager::instance()->increaseVisibility(target, 8000);
        VisibilityManager::instance()->addToVisibilityList(target);

        StringBuffer zBroadcast;
        zBroadcast << "\\#ffb90f" << target->getFirstName() << " is now on the bounty hunter \\#e51b1bTerminal!";
        target->getZoneServer()->getChatManager()->broadcastGalaxy(nullptr, zBroadcast.toString());
    }
};

#endif
