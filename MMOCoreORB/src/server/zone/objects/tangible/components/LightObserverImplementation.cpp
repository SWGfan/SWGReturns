/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.
 */

#include "server/zone/objects/tangible/components/LightObserver.h"
#include "server/zone/ZoneServer.h"

// NOTE: LightObject (server/zone/objects/tangible/misc/LightObject.h) does not exist in this
// codebase and nothing constructs a LightObject or attaches a LightObserver to anything, so this
// observer is currently a no-op stub to keep the build working.
int LightObserverImplementation::notifyObserverEvent(unsigned int eventType, Observable* observable, ManagedObject* arg1, int64 arg2) {
	return 1;
}
