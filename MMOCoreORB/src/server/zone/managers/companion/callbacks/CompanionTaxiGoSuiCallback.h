/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-30, "GO button" pass, per user request: "so the
	vehicle only starts moving once the player actually clicks it, instead of
	always waiting exactly 5 seconds regardless of whether the player is
	ready"). SUI callback for the Companion Taxi departure confirmation popup:
	startTaxiRide() no longer pre-arms a flat 5-second taxiDepartureTime -- it
	holds the driver in STAY with taxiAwaitingGoConfirm set and pops this box
	instead (see CompanionObjectImplementation.cpp). Pressing GO just clears
	that flag and sets taxiDepartureTime to "now" -- updateTaxiTick()'s
	existing timer+proximity gate (completely unchanged) still requires the
	owner to be close before the driver actually departs, it just no longer
	has to wait out a fixed clock first. Cancelling retires the ride the same
	way every other abort path already does -- see the stopTaxiRide(false)
	call sites in CompanionStayCommand.h, CompanionDialogMenuSuiCallback.h,
	et al.
*/

#ifndef COMPANIONTAXIGOSUICALLBACK_H_
#define COMPANIONTAXIGOSUICALLBACK_H_

#include "server/zone/objects/player/sui/SuiCallback.h"
#include "server/zone/objects/player/sui/messagebox/SuiMessageBox.h"
#include "server/zone/objects/player/sui/SuiWindowType.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/objects/companion/CompanionObject.h"

class CompanionTaxiGoSuiCallback : public SuiCallback {
	ManagedReference<CompanionObject*> companion;

public:
	CompanionTaxiGoSuiCallback(ZoneServer* server, CompanionObject* comp)
		: SuiCallback(server) {
		companion = comp;
	}

	void run(CreatureObject* player, SuiBox* suiBox, uint32 eventIndex, Vector<UnicodeString>* args) {
		if (player == nullptr) {
			return;
		}

		ManagedReference<CompanionObject*> strongCompanion = companion;

		if (strongCompanion == nullptr) {
			return;
		}

		Locker clocker(strongCompanion, player);

		// Stale popup guard: the ride may already have ended (arrival, combat,
		// /companionstay, a store, etc.) or already departed by the time this
		// box is answered -- do nothing in either case.
		if (!strongCompanion->isTaxiActive() || !strongCompanion->isTaxiAwaitingGoConfirm()) {
			return;
		}

		bool cancelPressed = (eventIndex == 1);

		if (cancelPressed) {
			strongCompanion->setTaxiAwaitingGoConfirm(false);
			strongCompanion->stopTaxiRide(false);
			player->sendSystemMessage("Your companion's taxi ride is cancelled.");
			return;
		}

		// GO pressed: hand departure off to updateTaxiTick()'s existing,
		// unchanged timer+proximity gate -- setting taxiDepartureTime to "now"
		// makes the very next tick see its window as already passed; the
		// owner-proximity half of that check still applies exactly as before.
		strongCompanion->setTaxiAwaitingGoConfirm(false);
		strongCompanion->setTaxiDepartureTime(System::getMiliTime());
	}

};

#endif // COMPANIONTAXIGOSUICALLBACK_H_
