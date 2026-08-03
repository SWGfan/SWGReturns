
#ifndef STORESPAWNEDCHILDRENTASK_H_
#define STORESPAWNEDCHILDRENTASK_H_

#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/objects/companion/CompanionObject.h"

class StoreSpawnedChildrenTask : public Task {
	ManagedWeakReference<CreatureObject*> play;
	Vector<ManagedReference<CreatureObject*> > children;

	// Companion System (2026-07-15, "companion left behind on logout" fix --
	// see PlayerObjectImplementation::unloadSpawnedChildren() and NOTES.md).
	// CompanionControlDevice deliberately extends IntangibleObject directly,
	// not ControlDevice (so it can't cross-recognize with the real pet/
	// vehicle/ship system -- see HANDOFF.md's earlier cross-zone-transfer
	// research), so it can never live in the `children` vector above. Stored
	// in a separate vector instead and processed the same way, right after
	// the real control devices, under the same player lock this task already
	// takes below.
	Vector<ManagedReference<CompanionControlDevice*> > companionDevices;

public:
	StoreSpawnedChildrenTask(CreatureObject* creo,
			Vector<ManagedReference<CreatureObject*> >&& ch,
			Vector<ManagedReference<CompanionControlDevice*> >&& companionDev = Vector<ManagedReference<CompanionControlDevice*> >()) :
		play(creo), children(std::move(ch)), companionDevices(std::move(companionDev)) {

	}

	void run() {
		ManagedReference<CreatureObject*> player = play.get();

		if (player == nullptr)
			return;

		Locker locker(player);

		for (int i = 0; i < children.size(); ++i) {
			CreatureObject* child = children.get(i);

			if (child == nullptr)
				continue;

			Locker clocker(child, player);

			ManagedReference<ControlDevice*> controlDevice = child->getControlDevice().get();

			if (controlDevice != nullptr) {
				Locker deviceLocker(controlDevice);
				controlDevice->storeObject(player, true);
			}
		}

		// Companion System (2026-07-15, "companion left behind on logout"
		// fix -- see NOTES.md). Mirrors CompanionStoreCommand.h's own
		// locking order exactly (companion locked first, cross-locked to
		// player, then the device) -- storeObject() ends by calling
		// companion->destroyObjectFromWorld(), which asserts the companion
		// is already locked by this thread. force=true (matches the real
		// control-device storeObject() call above, bypassing storeObject()'s
		// "can't store while in combat" gate -- a logging-out player has no
		// way to resolve combat state, so a companion should never be left
		// stranded in the world over that).
		for (int i = 0; i < companionDevices.size(); ++i) {
			ManagedReference<CompanionControlDevice*> companionDevice = companionDevices.get(i).get();

			if (companionDevice == nullptr) {
				continue;
			}

			ManagedReference<CompanionObject*> companion = companionDevice->getCompanionObject();

			if (companion != nullptr) {
				Locker clocker(companion, player);
				Locker deviceLocker(companionDevice, player);

				companionDevice->storeObject(player, true);
			} else {
				Locker deviceLocker(companionDevice, player);

				companionDevice->storeObject(player, true);
			}
		}

	}
};



#endif /* STORESPAWNEDCHILDRENTASK_H_ */
