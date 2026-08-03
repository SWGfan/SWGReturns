/*
 * VehicleControlDeviceImplementation.cpp
 *
 *  Created on: 10/04/2010
 *      Author: victor
 */

#include "server/zone/objects/intangible/VehicleControlDevice.h"
#include "server/zone/objects/intangible/VehicleControlObserver.h"
#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/creature/VehicleObject.h"
#include "server/zone/objects/creature/events/VehicleDecayTask.h"
#include "server/zone/packets/scene/AttributeListMessage.h"
#include "server/zone/ZoneServer.h"
#include "server/zone/Zone.h"
#include "tasks/CallMountTask.h"
#include "server/zone/objects/region/CityRegion.h"
#include "server/zone/objects/player/sessions/TradeSession.h"
#include "server/zone/managers/player/PlayerManager.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"

namespace {

	// Companion Vehicle Mimicry (2026-07-15): true if the owner or any of
	// their currently summoned companions is in combat right now.
	// Duplicated (per this project's per-file-copy convention -- see
	// CompanionFollowCommand.h et al.) into CompanionDialogMenuSuiCallback.h
	// and CompanionTaxiWaypointSuiCallback.h, which need the identical
	// check for the Taxi dialog flow.
	bool isCompanionGroupInCombat(CreatureObject* player) {
		if (player == nullptr) {
			return false;
		}

		if (player->isInCombat()) {
			return true;
		}

		ManagedReference<SceneObject*> datapad = player->getSlottedObject("datapad");

		if (datapad == nullptr) {
			return false;
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

			if (companion->isInCombat()) {
				return true;
			}
		}

		return false;
	}

	// Companion Vehicle Mimicry (2026-07-15, "vehicle mimicry redesign" --
	// see CompanionObject.idl's taxiVehicle doc comment and NOTES.md):
	// whenever the OWNER calls out or stores their own real vehicle, every
	// companion they currently have summoned should cosmetically mirror
	// that same vehicle template alongside them -- independent of, and
	// using the same underlying mechanism as, the Companion Taxi feature
	// (see CompanionDialogMenuSuiCallback.h's case 9). Scans the player's
	// datapad for every living, linked companion exactly like every
	// Companion*Command.h file already does. Also switches every companion
	// to FOLLOW (2026-07-15 follow-up, per user request: calling out a
	// vehicle means you're about to travel, so nobody should be left
	// behind parked in STAY/PATROL) -- UNLESS the group is in combat
	// (2026-07-15, second follow-up, per user request: never interrupt a
	// fight -- if the owner or any companion is currently fighting, skip
	// the whole escort silently and leave everyone fighting until the
	// group is fully clear; the owner's own real vehicle still spawns
	// normally regardless, only the companion-side mimicry is gated).
	void startCompanionVehicleMimicry(CreatureObject* player, unsigned int vehicleTemplateCRC) {
		if (player == nullptr || vehicleTemplateCRC == 0) {
			return;
		}

		if (isCompanionGroupInCombat(player)) {
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

			ManagedReference<CompanionObject*> companion = device->getCompanionObject();

			if (companion == nullptr || companion->getZone() == nullptr) {
				continue;
			}

			if (companion->getLinkedCreature().get() != player) {
				continue;
			}

			Locker clocker(companion, player);

			// 2026-07-15 (user request): calling out a vehicle means
			// you're about to go somewhere, so switch every companion to
			// FOLLOW first -- same call shape as CompanionFollowCommand.h
			// -- rather than leaving a companion parked in STAY/PATROL
			// behind while everyone else rides off.
			companion->setCompanionState(CompanionObject::FOLLOW);
			companion->setFollowObject(player);

			// DEFERRED (genesis port): the companion fork also nudged a
			// resting companion into motion here. That used isResting() /
			// setMovementState(), neither of which exists on this base --
			// restore once those are ported. setCompanionState(FOLLOW) +
			// setFollowObject() above are unaffected.

			// No destination -- pure "escort while the owner's own
			// vehicle exists" mode; startTaxiRide() itself only touches
			// the vehicle/pace, not companionState, when hasDestination
			// is false, so the FOLLOW order set just above sticks.
			companion->startTaxiRide(player, 0, 0, false, vehicleTemplateCRC);
		}
	}

	// Symmetric teardown -- called when the owner stores/dismisses their
	// own real vehicle. Tears down ANY active ride on that companion,
	// whether it's a plain escort or a Taxi-with-destination ride still in
	// progress -- the owner storing their own real vehicle is a clear
	// enough "I'm done with vehicles for now" signal either way, and
	// startTaxiRide() is fully idempotent if something needs to start a
	// new ride again right after.
	void stopCompanionVehicleMimicry(CreatureObject* player) {
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

			ManagedReference<CompanionObject*> companion = device->getCompanionObject();

			if (companion == nullptr || companion->getZone() == nullptr) {
				continue;
			}

			if (companion->getLinkedCreature().get() != player) {
				continue;
			}

			// True auto-taxi (2026-07-16): only ESCORT rides end when the
			// owner's real vehicle is stored -- a running DESTINATION taxi
			// trip stores that vehicle ITSELF at ride start (see
			// CompanionObjectImplementation::startOwnerAutoDrive()), so
			// killing it here would cancel the very ride that triggered us.
			// Checked BEFORE the cross-lock (the getters are @dirty reads):
			// startOwnerAutoDrive() already HOLDS this very companion's
			// lock while it stores the vehicle -- re-locking it here would
			// self-deadlock.
			if (!companion->isTaxiActive() || companion->isTaxiDestinationRide()) {
				continue;
			}

			Locker clocker(companion, player);

			// 2026-07-16 rider-flip redesign: the companion is parked
			// in STAY while riding, so the escort teardown must resume
			// FOLLOW itself or the companion just stands where it
			// dismounted (pre-redesign, escort mode never touched
			// companionState and false was correct here).
			companion->stopTaxiRide(true);
		}
	}

}

void VehicleControlDeviceImplementation::generateObject(CreatureObject* player) {
	if (player->isDead() || player->isIncapacitated())
		return;

	if (!isASubChildOf(player))
		return;

	if (player->getParent() != nullptr || player->isInCombat()) {
		player->sendSystemMessage("@pet/pet_menu:cant_call_vehicle"); // You can only unpack vehicles while Outside and not in Combat.
		return;
	}

	ManagedReference<TangibleObject*> controlledObject = this->controlledObject.get();

	if (controlledObject == nullptr)
		return;

	if (controlledObject->isInQuadTree())
		return;

	ManagedReference<TradeSession*> tradeContainer = player->getActiveSession(SessionFacadeType::TRADE).castTo<TradeSession*>();

	if (tradeContainer != nullptr) {
		server->getZoneServer()->getPlayerManager()->handleAbortTradeMessage(player);
	}

	if(player->getPendingTask("call_mount") != nullptr) {
		StringIdChatParameter waitTime("pet/pet_menu", "call_delay_finish_vehicle");
		AtomicTime nextExecution;
		Core::getTaskManager()->getNextExecutionTime(player->getPendingTask("call_mount"), nextExecution);
		int timeLeft = (nextExecution.getMiliTime() / 1000) - System::getTime();
		waitTime.setDI(timeLeft);

		player->sendSystemMessage(waitTime);
		return;
	}

	ManagedReference<SceneObject*> datapad = player->getSlottedObject("datapad");

	if (datapad == nullptr)
		return;

	int currentlySpawned = 0;

	for (int i = 0; i < datapad->getContainerObjectsSize(); ++i) {
		ManagedReference<SceneObject*> object = datapad->getContainerObject(i);

		if (object->isVehicleControlDevice()) {
			VehicleControlDevice* device = cast<VehicleControlDevice*>( object.get());

			ManagedReference<SceneObject*> vehicle = device->getControlledObject();

			if (vehicle != nullptr && vehicle->isInQuadTree()) {
				if (++currentlySpawned > 2)
					player->sendSystemMessage("@pet/pet_menu:has_max_vehicle");

				return;
			}
		}
	}

	if(player->getCurrentCamp() == nullptr && player->getCityRegion() == nullptr) {

		Reference<CallMountTask*> callMount = new CallMountTask(_this.getReferenceUnsafeStaticCast(), player, "call_mount");

		StringIdChatParameter message("pet/pet_menu", "call_vehicle_delay");
		message.setDI(5);
		player->sendSystemMessage(message);

		player->addPendingTask("call_mount", callMount, 5 * 1000);

		if (vehicleControlObserver == nullptr) {
			vehicleControlObserver = new VehicleControlObserver(_this.getReferenceUnsafeStaticCast());
			vehicleControlObserver->deploy();
		}

		player->registerObserver(ObserverEventType::STARTCOMBAT, vehicleControlObserver);

	} else {

		Locker clocker(controlledObject, player);
		spawnObject(player);
	}

}

void VehicleControlDeviceImplementation::spawnObject(CreatureObject* player) {
	ZoneServer* zoneServer = getZoneServer();

	ManagedReference<TangibleObject*> controlledObject = this->controlledObject.get();

	if (controlledObject == nullptr)
		return;

	if (!isASubChildOf(player))
		return;

	if (player->getParent() != nullptr || player->isInCombat()) {
		player->sendSystemMessage("@pet/pet_menu:cant_call_vehicle"); // You can only unpack vehicles while Outside and not in Combat.
		return;
	}

	ManagedReference<TradeSession*> tradeContainer = player->getActiveSession(SessionFacadeType::TRADE).castTo<TradeSession*>();

	if (tradeContainer != nullptr) {
		server->getZoneServer()->getPlayerManager()->handleAbortTradeMessage(player);
	}

	controlledObject->initializePosition(player->getPositionX(), player->getPositionZ(), player->getPositionY());
	ManagedReference<CreatureObject*> vehicle = nullptr;

	if (controlledObject->isCreatureObject()) {
		vehicle = cast<CreatureObject*>(controlledObject.get());
		vehicle->setCreatureLink(player);
		vehicle->setControlDevice(_this.getReferenceUnsafeStaticCast());
	}

	Zone* zone = player->getZone();

	if (zone == nullptr)
		return;

	//controlledObject->insertToZone(player->getZone());
	zone->transferObject(controlledObject, -1, true);
	Reference<VehicleDecayTask*> decayTask = new VehicleDecayTask(controlledObject);
	decayTask->execute();

	if (vehicle != nullptr && controlledObject->getServerObjectCRC() == 0x32F87A54) // Jetpack
	{
		controlledObject->setCustomizationVariable("/private/index_hover_height", 40, true); // Illusion of flying.
		player->executeObjectControllerAction(STRING_HASHCODE("mount"), controlledObject->getObjectID(), ""); // Auto mount.
	}

	// Companion Vehicle Mimicry (2026-07-15, per direct user request -- see
	// the anonymous-namespace helper above and NOTES.md): every companion
	// this player currently has summoned pulls out a cosmetic copy of this
	// same vehicle and rides along, whatever they're currently doing.
	startCompanionVehicleMimicry(player, controlledObject->getServerObjectCRC());

	updateStatus(1);

	if (vehicleControlObserver != nullptr)
		player->dropObserver(ObserverEventType::STARTCOMBAT, vehicleControlObserver);
}

void VehicleControlDeviceImplementation::cancelSpawnObject(CreatureObject* player) {

	Reference<Task*> mountTask = player->getPendingTask("call_mount");
	if(mountTask) {
		mountTask->cancel();
		player->removePendingTask("call_mount");
	}

	if (vehicleControlObserver != nullptr)
		player->dropObserver(ObserverEventType::STARTCOMBAT, vehicleControlObserver);
}

void VehicleControlDeviceImplementation::storeObject(CreatureObject* player, bool force) {
	ManagedReference<TangibleObject*> controlledObject = this->controlledObject.get();

	if (controlledObject == nullptr)
		return;

	/*if (!controlledObject->isInQuadTree())
		return;*/

	if (player->isRidingMount() && player->getParent() == controlledObject) {

		if (!force && !player->checkCooldownRecovery("mount_dismount"))
			return;

		player->executeObjectControllerAction(STRING_HASHCODE("dismount"));

		if (player->isRidingMount())
			return;
	}

	Locker crossLocker(controlledObject, player);

	Reference<Task*> decayTask = controlledObject->getPendingTask("decay");

	if (decayTask != nullptr) {
		decayTask->cancel();
		controlledObject->removePendingTask("decay");
	}

	// Companion Vehicle Mimicry (2026-07-15, per direct user request -- see
	// the anonymous-namespace helper above and NOTES.md): the owner is
	// storing their own real vehicle, so every companion's matching
	// cosmetic escort ride ends too.
	stopCompanionVehicleMimicry(player);

	controlledObject->destroyObjectFromWorld(true);

	if (controlledObject->isCreatureObject())
		(cast<CreatureObject*>(controlledObject.get()))->setCreatureLink(nullptr);

	updateStatus(0);
}

void VehicleControlDeviceImplementation::destroyObjectFromDatabase(bool destroyContainedObjects) {
	ManagedReference<TangibleObject*> controlledObject = this->controlledObject.get();

	if (controlledObject != nullptr) {
		Locker locker(controlledObject);

		ManagedReference<CreatureObject*> object = controlledObject->getSlottedObject("rider").castTo<CreatureObject*>();

		if (object != nullptr) {
			Locker clocker(object, controlledObject);

			object->executeObjectControllerAction(STRING_HASHCODE("dismount"));

			object = controlledObject->getSlottedObject("rider").castTo<CreatureObject*>();

			if (object != nullptr) {
				controlledObject->removeObject(object, nullptr, true);

				Zone* zone = getZone();

				if (zone != nullptr)
					zone->transferObject(object, -1, false);
			}
		}

		controlledObject->destroyObjectFromDatabase(true);
	}

	IntangibleObjectImplementation::destroyObjectFromDatabase(destroyContainedObjects);
}

int VehicleControlDeviceImplementation::canBeDestroyed(CreatureObject* player) {
	ManagedReference<TangibleObject*> controlledObject = this->controlledObject.get();

	if (controlledObject != nullptr) {
		if (controlledObject->isInQuadTree())
			return 1;
	}

	return IntangibleObjectImplementation::canBeDestroyed(player);
}

bool VehicleControlDeviceImplementation::canBeTradedTo(CreatureObject* player, CreatureObject* receiver, int numberInTrade) {
	ManagedReference<SceneObject*> datapad = receiver->getSlottedObject("datapad");

	if (datapad == nullptr)
		return false;

	ManagedReference<PlayerManager*> playerManager = player->getZoneServer()->getPlayerManager();

	int vehiclesInDatapad = numberInTrade;
	int maxStoredVehicles = playerManager->getBaseStoredVehicles();

	for (int i = 0; i < datapad->getContainerObjectsSize(); i++) {
		Reference<SceneObject*> obj =  datapad->getContainerObject(i).castTo<SceneObject*>();

		if (obj != nullptr && obj->isVehicleControlDevice() ){
			vehiclesInDatapad++;
		}
	}

	if( vehiclesInDatapad >= maxStoredVehicles){
		player->sendSystemMessage("That person has too many vehicles in their datapad");
		receiver->sendSystemMessage("@pet/pet_menu:has_max_vehicle"); // You already have the maximum number of vehicles that you can own.
		return false;
	}

	return true;
}

void VehicleControlDeviceImplementation::fillAttributeList(AttributeListMessage* alm, CreatureObject* object) {
	SceneObjectImplementation::fillAttributeList(alm, object);

	if( this->controlledObject == nullptr )
		return;

	ManagedReference<VehicleObject*> vehicle = this->controlledObject.get().castTo<VehicleObject*>();
	if( vehicle == nullptr )
		return;

	if (vehicle->getPaintCount() > 0){
		alm->insertAttribute("customization_cnt", vehicle->getPaintCount());
	}

}
