/*
                Copyright <SWGEmu>
        See file COPYING for copying conditions. */

#include "server/zone/objects/creature/events/CloakEventObserver.h"

#include "server/zone/objects/creature/events/CloakEvent.h"

int CloakEventObserverImplementation::notifyObserverEvent(unsigned int eventType, Observable* observable, ManagedObject* arg1, long long arg2) {
    ManagedReference<CreatureObject*> strongRef = getPlayer().get();
    
    if (strongRef == nullptr)
        return 0;

    Reference<Task*> task = strongRef->getPendingTask("cloakevent");

    if (task != nullptr) {
        Reference<CloakEvent*> cloakEvent = task.castTo<CloakEvent*>();
        if (cloakEvent == nullptr) {
            return 0;
        }

        cloakEvent->notifyDamageReceived(arg1);

        return 1;
    }

    return 0;
}