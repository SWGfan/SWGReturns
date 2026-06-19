/*
	EventZombiesCommand.h
	Custom staff command: /eventzombies [start|stop|status]
	Delegates entirely to the ZombieEventScreenplay Lua screenplay.
*/

#ifndef EVENTZOMBIESCOMMAND_H_
#define EVENTZOMBIESCOMMAND_H_

#include "server/zone/objects/scene/SceneObject.h"

class EventZombiesCommand : public QueueCommand {
public:
	EventZombiesCommand(const String& name, ZoneProcessServer* server)
		: QueueCommand(name, server) {

		// Admin-only: godLevel 15 means only CSR level 15+ can use it
		logLevel = 15;
	}

	int doQueueCommand(CreatureObject* creature, const uint64& target, const UnicodeString& arguments) const {
		if (!checkStateMask(creature))
			return INVALIDSTATE;

		if (!checkInvalidLocomotions(creature))
			return INVALIDLOCOMOTION;

		// Delegate entirely to the Lua screenplay function
		Lua* lua = creature->getZoneServer()->getLuaInstance();
		if (lua == nullptr)
			return GENERALERROR;

		// Call: eventzombies(pCreature, pTarget, args)
		UniqueReference<LuaFunction*> luaFunc(lua->createFunction("eventzombies", 0));
		if (luaFunc == nullptr)
			return GENERALERROR;

		*luaFunc << creature;

		SceneObject* targetObj = creature->getZoneServer()->getObject(target);
		if (targetObj != nullptr)
			*luaFunc << targetObj;
		else
			*luaFunc << (SceneObject*)nullptr;

		*luaFunc << arguments.toCharArray();

		luaFunc->callFunction();

		return SUCCESS;
	}
};

#endif // EVENTZOMBIESCOMMAND_H_
