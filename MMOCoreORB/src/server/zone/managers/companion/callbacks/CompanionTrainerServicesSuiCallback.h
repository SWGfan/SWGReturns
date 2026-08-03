/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-18, per user request -- see NOTES.md): the
	Companion Handler trainer's SERVICES menu. Replaces the earlier
	auto-claim-on-converse behavior (live feedback: "don't just give a new
	one if a user clicks on the trainer -- the user needs to specify") with
	an explicit menu offered alongside the normal training conversation:

	  - Claim a new companion  -> CompanionSkillTrainer::
	    claimAdditionalCompanion() (unchanged slot-cap logic)
	  - Permanently dismiss a companion -> pick-a-companion list ->
	    yes/no confirmation box -> device + companion (and everything it
	    is carrying/wearing) destroyed from the database for good.

	All three windows are header-only callbacks in this one file (new .cpp
	files need a cmake reconfigure; headers don't).
*/

#ifndef COMPANIONTRAINERSERVICESSUICALLBACK_H_
#define COMPANIONTRAINERSERVICESSUICALLBACK_H_

#include "server/zone/objects/player/sui/SuiCallback.h"
#include "server/zone/objects/player/sui/listbox/SuiListBox.h"
#include "server/zone/objects/player/sui/messagebox/SuiMessageBox.h"
#include "server/zone/objects/player/sui/SuiWindowType.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/managers/companion/CompanionSkillTrainer.h"
#include "server/zone/ZoneServer.h"

// ---------------------------------------------------------------------------
// Step 3: the yes/no confirmation, then the irreversible deletion.
// ---------------------------------------------------------------------------
class CompanionDeleteConfirmSuiCallback : public SuiCallback {
	uint64 deviceID;

public:
	CompanionDeleteConfirmSuiCallback(ZoneServer* server, uint64 device)
		: SuiCallback(server), deviceID(device) {
	}

	static void sendConfirmBox(CreatureObject* player, CompanionControlDevice* device, const String& companionName) {
		if (player == nullptr || device == nullptr) {
			return;
		}

		ManagedReference<PlayerObject*> ghost = player->getPlayerObject();

		if (ghost == nullptr) {
			return;
		}

		ghost->closeSuiWindowType(SuiWindowType::COMPANION_DELETE_CONFIRM);

		ManagedReference<SuiMessageBox*> sui = new SuiMessageBox(player, SuiWindowType::COMPANION_DELETE_CONFIRM);
		sui->setPromptTitle("Permanently Dismiss Companion");
		sui->setPromptText("Are you SURE you want to permanently dismiss " + companionName
				+ "?\n\nThis cannot be undone. The companion, its skills, and everything it is carrying or wearing will be destroyed for good.\n\nSelect Yes to delete this companion forever.");
		sui->setOkButton(true, "@yes");
		sui->setCancelButton(true, "@no");
		sui->setCallback(new CompanionDeleteConfirmSuiCallback(player->getZoneServer(), device->getObjectID()));

		ghost->addSuiBox(sui);
		player->sendMessage(sui->generateMessage());
	}

	void run(CreatureObject* player, SuiBox* suiBox, uint32 eventIndex, Vector<UnicodeString>* args) {
		// eventIndex 0 = OK ("Yes"); anything else = cancelled.
		if (eventIndex != 0 || player == nullptr) {
			return;
		}

		ZoneServer* zoneServer = player->getZoneServer();

		if (zoneServer == nullptr) {
			return;
		}

		ManagedReference<SceneObject*> deviceObj = zoneServer->getObject(deviceID);
		CompanionControlDevice* device = deviceObj != nullptr ? deviceObj.castTo<CompanionControlDevice*>().get() : nullptr;

		if (device == nullptr) {
			return;
		}

		// Sanity: the device must still be in THIS player's datapad.
		if (device->getRootParent() != player) {
			return;
		}

		String name = device->getDisplayedName();

		Locker dlocker(device, player);

		ManagedReference<CompanionObject*> companion = device->getCompanionObject();

		// Store first if summoned -- reuses every teardown hook (taxi stop,
		// group leave, observer cleanup) instead of duplicating them here.
		if (companion != nullptr && companion->getZone() != nullptr) {
			device->storeObject(player, true);
		}

		// Destroy the companion (and, via container cascade, everything it
		// carries/wears) from the database for good.
		if (companion != nullptr) {
			Locker clocker(companion, player);

			companion->destroyObjectFromWorld(true);
			companion->destroyObjectFromDatabase(true);
		}

		// And the datapad device itself.
		device->destroyObjectFromWorld(true);
		device->destroyObjectFromDatabase(true);

		player->sendSystemMessage(name + " has been permanently dismissed.");
	}

};

// ---------------------------------------------------------------------------
// Step 2: pick which companion to dismiss.
// ---------------------------------------------------------------------------
class CompanionDeleteSelectSuiCallback : public SuiCallback {
	Vector<uint64> deviceIDs;

public:
	CompanionDeleteSelectSuiCallback(ZoneServer* server, const Vector<uint64>& devices)
		: SuiCallback(server) {
		deviceIDs = devices;
	}

	static void sendDeleteList(CreatureObject* player) {
		if (player == nullptr) {
			return;
		}

		ManagedReference<PlayerObject*> ghost = player->getPlayerObject();
		ManagedReference<SceneObject*> datapad = player->getSlottedObject("datapad");

		if (ghost == nullptr || datapad == nullptr) {
			return;
		}

		Vector<uint64> devices;
		Vector<String> labels;

		for (int i = 0; i < datapad->getContainerObjectsSize(); ++i) {
			ManagedReference<SceneObject*> obj = datapad->getContainerObject(i);

			if (obj == nullptr || !obj->isCompanionControlDevice()) {
				continue;
			}

			CompanionControlDevice* device = cast<CompanionControlDevice*>(obj.get());

			if (device == nullptr) {
				continue;
			}

			CompanionObject* companion = device->getCompanionObject();
			String status = companion != nullptr && companion->getZone() != nullptr ? "  [summoned]" : "  [stored]";

			devices.add(device->getObjectID());
			labels.add(device->getDisplayedName() + status);
		}

		if (devices.size() == 0) {
			player->sendSystemMessage("You have no companions to dismiss.");
			return;
		}

		ghost->closeSuiWindowType(SuiWindowType::COMPANION_DELETE_SELECT);

		ManagedReference<SuiListBox*> sui = new SuiListBox(player, SuiWindowType::COMPANION_DELETE_SELECT);
		sui->setPromptTitle("Permanently Dismiss a Companion");
		sui->setPromptText("Choose the companion to dismiss FOREVER. You will be asked to confirm.");
		sui->setCancelButton(true, "@ui:cancel");
		sui->setOkButton(true, "@ui:ok");
		sui->setCallback(new CompanionDeleteSelectSuiCallback(player->getZoneServer(), devices));

		for (int i = 0; i < labels.size(); ++i) {
			sui->addMenuItem(labels.get(i));
		}

		ghost->addSuiBox(sui);
		player->sendMessage(sui->generateMessage());
	}

	void run(CreatureObject* player, SuiBox* suiBox, uint32 eventIndex, Vector<UnicodeString>* args) {
		if (eventIndex == 1 || player == nullptr || args == nullptr || args->size() <= 0) {
			return;
		}

		int menuSelection = Integer::valueOf(args->get(0).toString());

		if (menuSelection < 0 || menuSelection >= deviceIDs.size()) {
			return;
		}

		ZoneServer* zoneServer = player->getZoneServer();

		if (zoneServer == nullptr) {
			return;
		}

		ManagedReference<SceneObject*> deviceObj = zoneServer->getObject(deviceIDs.get(menuSelection));
		CompanionControlDevice* device = deviceObj != nullptr ? deviceObj.castTo<CompanionControlDevice*>().get() : nullptr;

		if (device == nullptr || device->getRootParent() != player) {
			return;
		}

		CompanionDeleteConfirmSuiCallback::sendConfirmBox(player, device, device->getDisplayedName());
	}

};

// ---------------------------------------------------------------------------
// Step 1: the trainer's services menu (opened alongside the normal training
// conversation -- see AiAgentImplementation::sendConversationStartTo()).
// ---------------------------------------------------------------------------
class CompanionTrainerServicesSuiCallback : public SuiCallback {
public:
	CompanionTrainerServicesSuiCallback(ZoneServer* server)
		: SuiCallback(server) {
	}

	static void sendServicesMenu(CreatureObject* player) {
		if (player == nullptr) {
			return;
		}

		ManagedReference<PlayerObject*> ghost = player->getPlayerObject();

		if (ghost == nullptr) {
			return;
		}

		ghost->closeSuiWindowType(SuiWindowType::COMPANION_TRAINER_SERVICES);

		ManagedReference<SuiListBox*> sui = new SuiListBox(player, SuiWindowType::COMPANION_TRAINER_SERVICES);
		sui->setPromptTitle("Companion Handler Services");
		sui->setPromptText("Besides training, what can I do for you?\n(Cancel if you're only here to train.)");
		sui->setCancelButton(true, "@ui:cancel");
		sui->setOkButton(true, "@ui:ok");
		sui->setCallback(new CompanionTrainerServicesSuiCallback(player->getZoneServer()));

		sui->addMenuItem("Claim a new companion"); // 0
		sui->addMenuItem("Permanently dismiss a companion..."); // 1

		ghost->addSuiBox(sui);
		player->sendMessage(sui->generateMessage());
	}

	void run(CreatureObject* player, SuiBox* suiBox, uint32 eventIndex, Vector<UnicodeString>* args) {
		if (eventIndex == 1 || player == nullptr || args == nullptr || args->size() <= 0) {
			return;
		}

		int menuSelection = Integer::valueOf(args->get(0).toString());

		switch (menuSelection) {
		case 0:
			// Explicit claim (2026-07-18: was automatic on every converse).
			CompanionSkillTrainer::instance()->claimAdditionalCompanion(player);
			break;
		case 1:
			CompanionDeleteSelectSuiCallback::sendDeleteList(player);
			break;
		default:
			break;
		}
	}

};

#endif // COMPANIONTRAINERSERVICESSUICALLBACK_H_
