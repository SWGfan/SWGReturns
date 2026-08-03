/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-15, "vehicle mimicry redesign" -- see
	CompanionDialogMenuSuiCallback.h's case 9 and NOTES.md). SUI callback for
	the Companion Taxi destination picker: the owner has already called out
	their own real vehicle (checked before this list was even offered -- see
	resolveSpawnedVehicleTemplateCRC() in CompanionDialogMenuSuiCallback.h)
	and just chose which of their own waypoints on this planet to be driven
	to. Starts the ride on the leader's own companion, then replicates the
	same "drive to this destination" order to every other group member's
	own summoned companion in this zone -- but each member's companion
	mimics THAT MEMBER's own currently-spawned vehicle (not the leader's),
	matching the user's "ask the owner to spawn their vehicle of choice...
	drive in separate vehicles together like a convoy" spec. A member who
	hasn't called out their own vehicle is skipped with an explanatory
	message rather than silently left behind.
*/

#ifndef COMPANIONTAXIWAYPOINTSUICALLBACK_H_
#define COMPANIONTAXIWAYPOINTSUICALLBACK_H_

#include "server/zone/objects/player/sui/SuiCallback.h"
#include "server/zone/objects/player/sui/listbox/SuiListBox.h"
#include "server/zone/objects/player/sui/SuiWindowType.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/objects/intangible/VehicleControlDevice.h"
#include "server/zone/objects/group/GroupObject.h"
#include "server/zone/Zone.h"

class CompanionTaxiWaypointSuiCallback : public SuiCallback {
	ManagedReference<CompanionObject*> companion;
	Vector<float> waypointX;
	Vector<float> waypointY;
	Vector<String> waypointLabels;
	unsigned int vehicleTemplateCRC;

	/**
	 * Duplicated (per this project's per-file-copy convention for these
	 * lookups -- see CompanionFollowCommand.h et al.) from
	 * CompanionDialogMenuSuiCallback.h's identical helper: the CRC of the
	 * given player's own currently spawned/summoned vehicle, or 0 if they
	 * have none out right now.
	 */
	unsigned int resolveSpawnedVehicleTemplateCRC(CreatureObject* player) const {
		if (player == nullptr) {
			return 0;
		}

		ManagedReference<SceneObject*> datapad = player->getSlottedObject("datapad");

		if (datapad == nullptr) {
			return 0;
		}

		for (int i = 0; i < datapad->getContainerObjectsSize(); ++i) {
			ManagedReference<SceneObject*> obj = datapad->getContainerObject(i);

			if (obj == nullptr || !obj->isVehicleControlDevice()) {
				continue;
			}

			VehicleControlDevice* device = cast<VehicleControlDevice*>(obj.get());
			ManagedReference<SceneObject*> vehicle = device->getControlledObject();

			if (vehicle != nullptr && vehicle->isInQuadTree()) {
				return vehicle->getServerObjectCRC();
			}
		}

		return 0;
	}

	/**
	 * Duplicated from CompanionDialogMenuSuiCallback.h's identical helper.
	 */
	CompanionObject* resolveActiveCompanion(CreatureObject* player) const {
		if (player == nullptr) {
			return nullptr;
		}

		ManagedReference<SceneObject*> datapad = player->getSlottedObject("datapad");

		if (datapad == nullptr) {
			return nullptr;
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

			CompanionObject* comp = device->getCompanionObject();

			if (comp == nullptr || comp->getZone() == nullptr) {
				continue;
			}

			if (comp->getLinkedCreature().get() != player) {
				continue;
			}

			return comp;
		}

		return nullptr;
	}

	/**
	 * Companion Vehicle Mimicry (2026-07-15, per user request: "if any of
	 * the companions are being attacked, or the user, do not stop
	 * attacking until the group is out of combat"). Duplicated from
	 * VehicleControlDeviceImplementation.cpp's identical helper -- true if
	 * the given player or any of their currently summoned companions is
	 * in combat right now.
	 */
	bool isCompanionGroupInCombat(CreatureObject* player) const {
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

			CompanionObject* comp = device->getCompanionObject();

			if (comp == nullptr || comp->getZone() == nullptr) {
				continue;
			}

			if (comp->getLinkedCreature().get() != player) {
				continue;
			}

			if (comp->isInCombat()) {
				return true;
			}
		}

		return false;
	}

public:
	CompanionTaxiWaypointSuiCallback(ZoneServer* server, CompanionObject* comp, const Vector<float>& destXList, const Vector<float>& destYList, const Vector<String>& labelList, unsigned int vehicleCRC)
		: SuiCallback(server) {
		companion = comp;
		waypointX = destXList;
		waypointY = destYList;
		waypointLabels = labelList;
		vehicleTemplateCRC = vehicleCRC;
	}

	void run(CreatureObject* player, SuiBox* suiBox, uint32 eventIndex, Vector<UnicodeString>* args) {
		if (eventIndex == 1 || player == nullptr || args == nullptr || args->size() <= 0) {
			return;
		}

		int menuSelection = Integer::valueOf(args->get(0).toString());

		if (menuSelection < 0 || menuSelection >= waypointX.size()) {
			return;
		}

		float destX = waypointX.get(menuSelection);
		float destY = waypointY.get(menuSelection);

		ManagedReference<CompanionObject*> strongCompanion = companion;

		if (strongCompanion == nullptr) {
			return;
		}

		Zone* zone = player->getZone();

		// Re-check combat here (not just when the dialog was first opened
		// -- see CompanionDialogMenuSuiCallback.h's case 9): time passed
		// while the player picked a waypoint, and per the user's request
		// a fight always takes priority over starting the ride.
		if (isCompanionGroupInCombat(player)) {
			player->sendSystemMessage("You can't call a taxi while you or your companions are in combat.");
			return;
		}

		// Multi-stop route (2026-07-16, per user request): if a destination
		// ride is already running, the pick APPENDS a stop to the route
		// instead of restarting the ride from scratch.
		bool appended = false;

		{
			Locker clocker(strongCompanion, player);

			appended = strongCompanion->addTaxiWaypoint(destX, destY);

			if (!appended && !strongCompanion->startTaxiRide(player, destX, destY, true, vehicleTemplateCRC)) {
				player->sendSystemMessage("Your companion can't start the ride right now.");
				return;
			}
		}

		if (appended) {
			player->sendSystemMessage("Your companion adds the stop to its route.");
		} else {
			player->sendSystemMessage("Your companion pulls out a matching vehicle and waits 5 seconds -- click it and /follow, then enjoy the ride!");
		}

		// Group convoy: every other member's own active companion in this
		// zone drives to the same destination, mimicking THAT MEMBER's own
		// currently-spawned vehicle (skipped if they haven't called one out).
		ManagedReference<GroupObject*> group = player->getGroup();

		if (group != nullptr && zone != nullptr) {
			for (int i = 0; i < group->getGroupSize(); ++i) {
				ManagedReference<CreatureObject*> member = group->getGroupMember(i);

				if (member == nullptr || member == player || !member->isPlayerCreature()) {
					continue;
				}

				// Cross-zone members are skipped outright, mirroring the
				// original taxi feature's documented companion-stranding
				// risk gate.
				if (member->getZone() != zone) {
					continue;
				}

				unsigned int memberVehicleCRC = resolveSpawnedVehicleTemplateCRC(member);

				if (memberVehicleCRC == 0) {
					member->sendSystemMessage("Call out your own vehicle to join the convoy.");
					continue;
				}

				if (isCompanionGroupInCombat(member)) {
					member->sendSystemMessage("You can't join the convoy while you or your companions are in combat.");
					continue;
				}

				Locker memberLocker(member, player);

				CompanionObject* memberCompanion = resolveActiveCompanion(member);

				if (memberCompanion == nullptr || memberCompanion == strongCompanion || memberCompanion->getZone() != zone) {
					continue;
				}

				Locker memberCompanionLocker(memberCompanion, member);

				// Same append-or-start shape as the leader's own companion,
				// so mid-route added stops propagate through the convoy.
				if (memberCompanion->addTaxiWaypoint(destX, destY)) {
					member->sendSystemMessage("Your companion adds the group's next stop to its route.");
				} else if (memberCompanion->startTaxiRide(member, destX, destY, true, memberVehicleCRC)) {
					member->sendSystemMessage("Your companion joins the group convoy and waits 5 seconds -- click it and /follow, then enjoy the ride!");
				}
			}
		}

		// Chain the picker (2026-07-16, per user request "pick multiple
		// waypoints"): after every successful pick, immediately re-offer the
		// same waypoint list so the owner can queue the next stop -- Cancel
		// simply ends the picking, the ride keeps running with whatever
		// route was built. A fresh callback instance carries the same data.
		ManagedReference<PlayerObject*> ghost = player->getPlayerObject();

		if (ghost != nullptr && waypointLabels.size() > 0) {
			ghost->closeSuiWindowType(SuiWindowType::COMPANION_TAXI_WAYPOINT);

			ManagedReference<SuiListBox*> routeSui = new SuiListBox(player, SuiWindowType::COMPANION_TAXI_WAYPOINT);
			routeSui->setPromptTitle(strongCompanion->getDisplayedName() + " -=COMPANION=- : Taxi Route");
			routeSui->setPromptText("Stop added. Pick another waypoint to extend the route, or Cancel to finish -- the ride is already underway.");
			routeSui->setCancelButton(true, "@ui:cancel");
			routeSui->setOkButton(true, "@ui:ok");
			routeSui->setCallback(new CompanionTaxiWaypointSuiCallback(player->getZoneServer(), strongCompanion, waypointX, waypointY, waypointLabels, vehicleTemplateCRC));

			for (int i = 0; i < waypointLabels.size(); ++i) {
				routeSui->addMenuItem(waypointLabels.get(i));
			}

			ghost->addSuiBox(routeSui);
			player->sendMessage(routeSui->generateMessage());
		}
	}

};

#endif // COMPANIONTAXIWAYPOINTSUICALLBACK_H_
