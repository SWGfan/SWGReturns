/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System -- dedicated /companionstore command. Modeled on
	server/zone/objects/creature/commands/pet/PetStoreCommand.h, but calls
	straight into CompanionControlDevice::storeObject() (native, already
	implemented -- syncs vitality back to the device, drops observers,
	destroys the world object) rather than queuing a separate store task,
	since the companion equivalent is a single already-locked native call
	and does not need the pet system's task indirection -- see
	CompanionFollowCommand.h and docs/companion_system/NOTES.md.
*/

#ifndef COMPANIONSTORECOMMAND_H_
#define COMPANIONSTORECOMMAND_H_

#include "server/zone/objects/creature/commands/QueueCommand.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/managers/combat/CombatManager.h"
#include "server/zone/managers/companion/CompanionChatter.h"

class CompanionStoreCommand : public QueueCommand {
public:

	CompanionStoreCommand(const String& name, ZoneProcessServer* server)
		: QueueCommand(name, server) {

	}

	/**
	 * Companion System (2026-07-15, "test 5 companions at once" pass) --
	 * resolves EVERY summoned, living companion's CONTROL DEVICE (not the
	 * CompanionObject itself, since storing is a call on the device) --
	 * was resolveActiveCompanionDevice() (singular) before the user asked
	 * for order commands to affect every summoned companion at once.
	 */
	void resolveActiveCompanionDevices(CreatureObject* player, Vector<ManagedReference<CompanionControlDevice*>>& devices) const {
		if (player == nullptr) {
			return;
		}

		ManagedReference<SceneObject*> datapad = player->getSlottedObject("datapad");

		if (datapad == nullptr) {
			return;
		}

		for (int i = 0; i < datapad->getContainerObjectsSize(); ++i) {
			ManagedReference<SceneObject*> obj = datapad->getContainerObject(i);

			if (obj == nullptr || !obj->isCompanionControlDevice()) {
				continue;
			}

			CompanionControlDevice* device = cast<CompanionControlDevice*>(obj.get());

			if (device->isCompanionDead()) {
				continue;
			}

			CompanionObject* companion = device->getCompanionObject();

			if (companion == nullptr || companion->getZone() == nullptr) {
				continue;
			}

			if (companion->getLinkedCreature().get() != player) {
				continue;
			}

			devices.add(device);
		}
	}

	int doQueueCommand(CreatureObject* creature, const uint64& target, const UnicodeString& arguments) const {
		if (!checkStateMask(creature)) {
			return INVALIDSTATE;
		}

		Vector<ManagedReference<CompanionControlDevice*>> devices;
		resolveActiveCompanionDevices(creature, devices);

		if (devices.size() == 0) {
			creature->sendSystemMessage("@companion:no_active_companion"); // You have no active companion.
			return GENERALERROR;
		}

		// Companion System (2026-07-17, "command flair" pass) -- chatter has
		// to fire BEFORE the store loop despawns everyone (immediateReplies:
		// a scheduled reply would find the companion already out of the
		// world and drop it). See CompanionChatter.h.
		{
			Vector<ManagedReference<CompanionObject*>> talkers;

			for (int i = 0; i < devices.size(); ++i) {
				ManagedReference<CompanionObject*> companion = devices.get(i)->getCompanionObject();

				if (companion != nullptr && companion->getZone() != nullptr) {
					talkers.add(companion);
				}
			}

			CompanionChatter::announceOrder(creature, "Squad -- stand down, you've earned it.", "store", talkers, true);
		}

		for (int i = 0; i < devices.size(); ++i) {
			CompanionControlDevice* device = devices.get(i);
			ManagedReference<CompanionObject*> companion = device->getCompanionObject();

			// Bug fix: companion's lock must still be held when
			// device->storeObject() runs below, since that call ends by calling
			// companion->destroyObjectFromWorld() (AiAgent.cpp's
			// isLockedByCurrentThread() assertion). Previously clocker was
			// scoped to only this isInCombat()/attemptPeace() check and had
			// already gone out of scope (releasing the companion's lock) by the
			// time storeObject() ran below, crashing the server with SIGABRT
			// the moment a player actually stored a companion (confirmed via a
			// live gdb backtrace). Both companion and device are now kept
			// locked for the whole remainder of the command -- matching the
			// established multi-object Locker pattern used elsewhere in this
			// system (see CompanionControlDeviceImplementation.cpp's summon
			// lambda, which locks player/device/companion in sequence the same
			// way).
			if (companion != nullptr) {
				Locker clocker(companion, creature);

				if (companion->isInCombat()) {
					CombatManager::instance()->attemptPeace(companion);
				}

				Locker dlocker(device, creature);

				device->storeObject(creature, false);
			} else {
				Locker dlocker(device, creature);

				device->storeObject(creature, false);
			}
		}

		return SUCCESS;
	}
};

#endif // COMPANIONSTORECOMMAND_H_
