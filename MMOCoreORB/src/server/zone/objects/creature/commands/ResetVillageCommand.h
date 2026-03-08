#ifndef RESETVILLAGECOMMAND_H_
#define RESETVILLAGECOMMAND_H_
#include "server/zone/objects/scene/SceneObject.h"
#include "server/zone/managers/director/DirectorManager.h"
#include "server/zone/objects/player/variables/PlayerQuestData.h"

class ResetVillageCommand : public QueueCommand {
public:
	ResetVillageCommand(const String& name, ZoneProcessServer* server)
		: QueueCommand(name, server) {
	}

	int doQueueCommand(CreatureObject* creature, const uint64& target, const UnicodeString& arguments) const {
		if (!checkStateMask(creature))
			return INVALIDSTATE;

		if (!checkInvalidLocomotions(creature))
			return INVALIDLOCOMOTION;

		creature->error("ResetVillageCommand called, target=" + String::valueOf(target));

		ManagedReference<SceneObject*> object = server->getZoneServer()->getObject(target);

		if (object == nullptr || !object->isCreatureObject()) {
			creature->error("ResetVillageCommand: invalid target");
			return INVALIDTARGET;
		}

		CreatureObject* targetCreature = cast<CreatureObject*>(object.get());
		creature->error("ResetVillageCommand: got targetCreature " + targetCreature->getFirstName());

		Locker clocker(targetCreature, creature);

		ManagedReference<PlayerObject*> ghost = targetCreature->getPlayerObject();

		if (ghost == nullptr) {
			creature->error("ResetVillageCommand: ghost is null");
			return GENERALERROR;
		}

		creature->error("ResetVillageCommand: got ghost, clearing data");

		ghost->clearScreenPlayData("VillageJediProgression");
		creature->error("ResetVillageCommand: cleared VillageJediProgression");

		ghost->clearScreenPlayData("VillageDefense");
		ghost->clearScreenPlayData("VillageUnlockScreenPlay:combat");
		ghost->clearScreenPlayData("VillageUnlockScreenPlay:crafting");
		ghost->clearScreenPlayData("VillageUnlockScreenPlay:medic");
		ghost->clearScreenPlayData("VillageUnlockScreenPlay:enhance");

		ghost->clearActiveQuestsBit(PlayerQuestData::FS_VILLAGE_ELDER, true);
		ghost->clearCompletedQuestsBit(PlayerQuestData::FS_VILLAGE_ELDER, true);

		String playerID = String::valueOf(targetCreature->getObjectID());
		DirectorManager::instance()->setQuestStatus(playerID + ":village:lastActiveQuest", "");
		DirectorManager::instance()->setQuestStatus(playerID + ":village:activeQuestName", "");
		DirectorManager::instance()->setQuestStatus(playerID + ":village:lastCompletedQuest", "");

		creature->error("ResetVillageCommand: setting screenplay state to 4");
		targetCreature->setScreenPlayState("VillageJediProgression", 1);
		ghost->setScreenPlayData("VillageJediProgression", "FsIntroStep", "8");
		

		creature->sendSystemMessage("Village progress reset to pre-arrival state.");
		targetCreature->sendSystemMessage("Your village progress has been reset. Travel to Dathomir to receive your Force Sensitive skills.");

		creature->error("ResetVillageCommand: complete");
		return SUCCESS;
	}
};
#endif //RESETVILLAGECOMMAND_H_
