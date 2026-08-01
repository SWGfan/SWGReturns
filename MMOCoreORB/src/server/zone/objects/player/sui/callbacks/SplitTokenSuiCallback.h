#ifndef SPLIT_TOKEN_SUI_CALLBACK_H_
#define SPLIT_TOKEN_SUI_CALLBACK_H_

#include "server/zone/objects/player/sui/SuiCallback.h"
#include "server/zone/objects/tangible/misc/VendorToken.h"

class SplitTokenSuiCallback : public SuiCallback {
	ManagedWeakReference<SceneObject*> tokenObject;

public:
	SplitTokenSuiCallback(ZoneServer* server, SceneObject* sceneObject)
			: SuiCallback(server) {
		tokenObject = sceneObject;
	}

	void run(CreatureObject* player, SuiBox* suiBox, uint32 eventIndex, Vector<UnicodeString>* args) {
		if (player == nullptr || !player->isPlayerCreature() || suiBox == nullptr)
			return;

		auto inventory = player->getSlottedObject("inventory");

		if (inventory == nullptr || eventIndex == 1 || !suiBox->isTransferBox() || args == nullptr || args->size() <= 1)
			return;

		auto original = tokenObject.get().castTo<TangibleObject*>();

		if (original == nullptr || !original->isASubChildOf(inventory))
			return;

		int amount = Integer::valueOf(args->get(1).toString());

		if (amount <= 0 || amount >= original->getUseCount())
			return;

		String templatePath = original->getObjectTemplate()->getFullTemplateString();
		auto splitToken = player->getZoneServer()->createObject(templatePath.hashCode(), 1).castTo<TangibleObject*>();

		if (splitToken == nullptr)
			return;

		Locker locker(original, player);
		Locker splitLocker(splitToken, original);

		if (!inventory->transferObject(splitToken, -1, true)) {
			splitToken->destroyObjectFromDatabase(true);
			return;
		}

		original->setUseCount(original->getUseCount() - amount, true);
		splitToken->setUseCount(amount, true);
		splitToken->sendTo(player, true);
		player->sendSystemMessage("You've successfully split your items.");
	}
};

#endif
