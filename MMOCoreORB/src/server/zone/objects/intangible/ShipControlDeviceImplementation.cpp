/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.
*/

#include "server/zone/objects/intangible/ShipControlDevice.h"
#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/packets/object/ObjectMenuResponse.h"
#include "server/zone/Zone.h"
#include "server/zone/managers/player/PlayerManager.h"
#include "server/zone/managers/radial/RadialOptions.h"
#include "server/zone/objects/ship/ShipObject.h"
#include "server/zone/objects/ship/PobShipObject.h"

void ShipControlDeviceImplementation::generateObject(CreatureObject* player) {
	ZoneServer* zoneServer = getZoneServer();

	ManagedReference<TangibleObject*> controlledObject = this->controlledObject.get();

	if (controlledObject == nullptr)
		return;

	Locker clocker(controlledObject, player);

	controlledObject->initializePosition(player->getPositionX(), player->getPositionZ() + 10, player->getPositionY());

	player->getZone()->transferObject(controlledObject, -1, true);

	controlledObject->transferObject(player, 5, true);
	player->setState(CreatureState::PILOTINGSHIP);

	updateStatus(1);

	PlayerObject* ghost = player->getPlayerObject();

	if (ghost != nullptr)
		ghost->setTeleporting(true);
}

void ShipControlDeviceImplementation::storeObject(CreatureObject* player, bool force) {
	player->clearState(CreatureState::PILOTINGSHIP);

	ManagedReference<TangibleObject*> controlledObject = this->controlledObject.get();

	if (controlledObject == nullptr)
		return;

	Locker clocker(controlledObject, player);

	if (!controlledObject->isInQuadTree())
		return;

	Zone* zone = player->getZone();

	if (zone == nullptr)
		return;

	zone->transferObject(player, -1, false);

	controlledObject->destroyObjectFromWorld(true);

	transferObject(controlledObject, 4, true);

	updateStatus(0);
}

// NOTE: This previously drove a "Jump to Lightspeed" style menu (rename ship, deed ship,
// launch into a space zone / land from space). That depended on PlanetManager::getJtlZoneName(),
// GroupObject::updateMemberShip(), and PlayerObject::setSpaceLaunch*() which no longer exist
// anywhere in this codebase, so that flow can't work as written. This now mirrors the working
// VehicleControlDevice pattern instead: a simple generate/store radial option.
ShipObject* ShipControlDeviceImplementation::launchShip(CreatureObject* player, const String& zoneName, const Vector3& position) {
	return nullptr;
}

void ShipControlDeviceImplementation::fillObjectMenuResponse(ObjectMenuResponse* menuResponse, CreatureObject* player) {
	if (player == nullptr)
		return;

	ManagedReference<TangibleObject*> controlledObject = this->controlledObject.get();

	if (controlledObject == nullptr)
		return;

	if (!controlledObject->isInQuadTree()) {
		menuResponse->addRadialMenuItem(RadialOptions::VEHICLE_GENERATE, 3, "Launch Ship");
	} else {
		menuResponse->addRadialMenuItem(RadialOptions::VEHICLE_STORE, 3, "Land Ship");
	}
}

int ShipControlDeviceImplementation::handleObjectMenuSelect(CreatureObject* player, byte selectedID) {
	if (player == nullptr)
		return 1;

	if (selectedID == RadialOptions::VEHICLE_GENERATE) {
		generateObject(player);
		return 0;
	} else if (selectedID == RadialOptions::VEHICLE_STORE) {
		storeObject(player, false);
		return 0;
	}

	return 1;
}

void ShipControlDeviceImplementation::fillAttributeList(AttributeListMessage* alm, CreatureObject* object) {
	alm->insertAttribute("parking_spot", getParkingLocation());
}

bool ShipControlDeviceImplementation::canBeTradedTo(CreatureObject* player, CreatureObject* receiver, int numberInTrade) {
	ManagedReference<SceneObject*> datapad = receiver->getSlottedObject("datapad");

	if (datapad == nullptr)
		return false;

	ManagedReference<PlayerManager*> playerManager = player->getZoneServer()->getPlayerManager();

	int shipsInDatapad = numberInTrade;
	int maxStoredShips = playerManager->getBaseStoredShips();

	for (int i = 0; i < datapad->getContainerObjectsSize(); i++) {
		Reference<SceneObject*> obj =  datapad->getContainerObject(i).castTo<SceneObject*>();

		if (obj != nullptr && obj->isShipControlDevice() ){
			shipsInDatapad++;
		}
	}

	if( shipsInDatapad >= maxStoredShips){
		player->sendSystemMessage("That person has too many ships in their datapad");
		receiver->sendSystemMessage("You already have the maximum number of ships that you can own.");
		return false;
	}

	return true;
}

int ShipControlDeviceImplementation::canBeDestroyed(CreatureObject* player) {
	ManagedReference<TangibleObject*> controlledObject = this->controlledObject.get();

	if (controlledObject != nullptr) {
		if (controlledObject->isInQuadTree())
			return 1;
	}

	return IntangibleObjectImplementation::canBeDestroyed(player);
}

void ShipControlDeviceImplementation::destroyObjectFromDatabase(bool destroyContainedObjects) {
	ManagedReference<TangibleObject*> controlledObject = this->controlledObject.get();

	if (controlledObject != nullptr) {
		Locker locker(controlledObject);

		controlledObject->destroyObjectFromDatabase(true);
	}

	IntangibleObjectImplementation::destroyObjectFromDatabase(destroyContainedObjects);
}

void ShipControlDeviceImplementation::setStoredLocationData(CreatureObject* player) {
	// No-op: space launch/parking-location tracking depended on the removed JTL subsystem.
}

Vector3 ShipControlDeviceImplementation::getStoredPosition(bool randomPosition) {
	Vector3 random = randomPosition ? Vector3(System::random(10) - 5, System::random(10) - 5, 0) : Vector3::ZERO;
	return storedPosition + random;
}

bool ShipControlDeviceImplementation::isShipLaunched() {
	ManagedReference<TangibleObject*> controlledObject = this->controlledObject.get();

	if (controlledObject == nullptr)
		return false;

	return controlledObject->isInQuadTree();
}
