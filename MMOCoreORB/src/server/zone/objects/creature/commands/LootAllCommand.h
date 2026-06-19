/*
Copyright <SWGEmu>
See file COPYING for copying conditions.*/

#ifndef LOOTALL_H_
#define LOOTALL_H_

#include "server/zone/managers/player/PlayerManager.h"
#include "server/zone/objects/creature/ai/AiAgent.h"

class LootAllCommand : public QueueCommand {
public:

LootAllCommand(const String& name, ZoneProcessServer* server)
: QueueCommand(name, server) {
}

int doQueueCommand(CreatureObject* creature, const uint64& target, const UnicodeString& arguments) const {

if (!checkStateMask(creature))
return INVALIDSTATE;

if (!checkInvalidLocomotions(creature))
return INVALIDLOCOMOTION;

Zone* zone = creature->getZone();
if (zone == nullptr)
return GENERALERROR;

PlayerManager* playerManager = server->getZoneServer()->getPlayerManager();
if (playerManager == nullptr)
return GENERALERROR;

SortedVector<ManagedReference<QuadTreeEntry*>> closeObjects;
zone->getInRangeObjects(creature->getPositionX(), creature->getPositionY(), 10, &closeObjects, true);

for (int i = 0; i < closeObjects.size(); i++) {
ManagedReference<AiAgent*> ai = closeObjects.get(i).castTo<AiAgent*>();
if (ai == nullptr || !ai->isDead())
continue;

SceneObject* lootContainer = ai->getSlottedObject("inventory");
if (lootContainer == nullptr)
continue;

uint64 ownerID = lootContainer->getContainerPermissions()->getOwnerID();
if (ownerID != creature->getObjectID() && ownerID != creature->getGroupID())
continue;

Locker locker(ai, creature);
playerManager->lootAll(creature, ai);
}

return SUCCESS;
}
};

#endif //LOOTALL_H_
