/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-30) -- "The Landing" GO-button confirm popup.
	Per Nick's request: the show's opening walk/tour beat
	(CompanionTheLandingShow::beatCrateConverge(), phase 1) no longer fires
	automatically on a 10s timer right after the director's opening line --
	the owner now gets a real SuiMessageBox with a single GO button first,
	the same "confirm with a button" pattern FlashSpeederCommand.h /
	ReplaceFlashSpeederSuiCallback.h already use in this codebase, adapted
	to this project's companion-callback constructor convention (see
	CompanionTaxiWaypointSuiCallback.h / CompanionCampChoiceSuiCallback.h --
	ZoneServer* + the resolved ids/state needed to act, stored as
	ManagedReference/Reference members, re-resolved and null-checked in
	run() rather than trusted raw across the SUI round-trip).

	ORDERING: this header is a private implementation detail of
	CompanionTheLandingCommand.h and must ONLY ever be #include-d from
	there (see the #include line right after CompanionTheLandingShow's
	closing brace in that file), AFTER CompanionTheLandingState and
	CompanionTheLandingShow are both fully defined -- this header needs
	CompanionTheLandingState's full definition for its Reference<> member
	and needs CompanionTheLandingShow::scheduleStep() to hand off to phase
	1. It deliberately does NOT #include CompanionTheLandingCommand.h back,
	to avoid a circular include between the two files -- do not add one.
*/

#ifndef COMPANIONTHELANDINGGOSUICALLBACK_H_
#define COMPANIONTHELANDINGGOSUICALLBACK_H_

#include "server/zone/objects/player/sui/SuiCallback.h"
#include "server/zone/objects/player/sui/SuiWindowType.h"
#include "server/zone/objects/player/PlayerObject.h"

class CompanionTheLandingGoSuiCallback : public SuiCallback {
	uint64 ownerID;
	Reference<CompanionTheLandingState*> state;

public:
	CompanionTheLandingGoSuiCallback(ZoneServer* server, uint64 owner, CompanionTheLandingState* landingState)
		: SuiCallback(server) {
		ownerID = owner;
		state = landingState;
	}

	void run(CreatureObject* player, SuiBox* suiBox, uint32 eventIndex, Vector<UnicodeString>* args) {
		if (player == nullptr || player->getObjectID() != ownerID) {
			return;
		}

		Reference<CompanionTheLandingState*> strongState = state;

		if (strongState == nullptr || !strongState->awaitingGoConfirm) {
			// Already resumed or aborted (e.g. the pending-confirm watchdog
			// fired first because a cast member died/went into combat, or
			// the owner zoned out, while this box sat open) -- nothing
			// left to do.
			return;
		}

		strongState->awaitingGoConfirm = false;

		Reference<CreatureObject*> ownerRef = player;

		// Hands off to the exact same phase-1 entry point the show used to
		// reach automatically on a 10s timer -- scheduled with a 0ms delay
		// rather than called directly here, per this project's documented
		// rule that heavy work (spawnObject et al., inside
		// beatCrateConverge) must never run inline inside a SUI callback
		// while the player's lock is held.
		CompanionTheLandingShow::scheduleStep(1, server, ownerRef, strongState, 0);
	}

};

#endif // COMPANIONTHELANDINGGOSUICALLBACK_H_
