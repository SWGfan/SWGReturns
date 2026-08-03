/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-30, "Theater Mode" dialog section -- per Nick:
	"how do i access the theater plays? can we add a section here called
	Theater Mode and put our plays in there"). Handles a selection from the
	Theater Mode submenu SuiListBox built by
	CompanionDialogMenuSuiCallback.h's case 14. Currently offers just "The
	Landing" -- an ordered index -> show dispatch, so future shows can be
	appended as new menu items + new case labels here without disturbing
	this one's index or the main dialog's numbering.

	Re-validates EVERYTHING at click time (not in combat, has active
	companions, per-companion cooldown) rather than trusting whatever was
	true when the submenu was opened -- same shape as every other
	scheduled/deferred companion action in this codebase, and specifically
	mirrors CompanionTheLandingCommand::doQueueCommand()'s own checks exactly
	(this is a second entry point into the SAME show, not a parallel
	implementation).
*/

#ifndef COMPANIONTHEATERMODESUICALLBACK_H_
#define COMPANIONTHEATERMODESUICALLBACK_H_

#include "server/zone/objects/player/sui/SuiCallback.h"
#include "server/zone/objects/player/sui/listbox/SuiListBox.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/objects/companion/commands/CompanionTheLandingCommand.h"
#include "server/zone/managers/companion/CompanionFireworksShow.h"
#include "server/zone/objects/companion/commands/CompanionBattleTheaterCommand.h" // BATTLE_THEATER_2026_07_30
#include "server/zone/objects/companion/commands/CompanionBirthdayCommand.h" // BIRTHDAY_SHOW_2026_07_30

class CompanionTheaterModeSuiCallback : public SuiCallback {

	/**
	 * All of the player's currently summoned, living companions. Duplicated
	 * (rather than shared) from CompanionDialogMenuSuiCallback.h's identical
	 * helper, per this project's per-file-copy convention for this exact
	 * scan.
	 */
	void resolveAllActiveCompanions(CreatureObject* player, Vector<ManagedReference<CompanionObject*> >& out) const {
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

			CompanionObject* comp = device->getCompanionObject();

			if (comp == nullptr || comp->getZone() == nullptr) {
				continue;
			}

			if (comp->getLinkedCreature().get() != player) {
				continue;
			}

			out.add(comp);
		}
	}

	// OVERLAPPING_THEATER_SHOWS_FIX_2026_07_30 -- the companion whose dialog this
	// Theater Mode submenu was opened from (needed for single-companion
	// shows like Fireworks; whole-roster shows like The Landing
	// re-resolve independently and ignore this).
	ManagedReference<CompanionObject*> dialogCompanion;

public:
	CompanionTheaterModeSuiCallback(ZoneServer* server, CompanionObject* comp)
			: SuiCallback(server) {
		dialogCompanion = comp;
	}

	void run(CreatureObject* player, SuiBox* suiBox, uint32 eventIndex, Vector<UnicodeString>* args) {
		if (eventIndex == 1 || player == nullptr || args == nullptr || args->size() <= 0) {
			return;
		}

		int showSelection = Integer::valueOf(args->get(0).toString());

		if (player->isInCombat()) {
			player->sendSystemMessage("Not while there's a fight going on!");
			return;
		}

		// OVERLAPPING_THEATER_SHOWS_FIX_2026_07_30 -- refuse a new show while one is
		// already active for this owner (see
		// CompanionTheLandingCommand.h's start()/finishShow() for the
		// matching arm/clear).
		if (!player->checkCooldownRecovery("companion_theater_mode_busy")) {
			player->sendSystemMessage("A theater show is already in progress -- try again once it's finished.");
			return;
		}

		Vector<ManagedReference<CompanionObject*> > companions;
		resolveAllActiveCompanions(player, companions);

		if (companions.size() == 0) {
			player->sendSystemMessage("@companion:no_active_companion"); // You have no active companion.
			return;
		}

		switch (showSelection) {
		case 0: { // The Landing
			for (int i = 0; i < companions.size(); ++i) {
				CompanionObject* companion = companions.get(i);

				if (companion == nullptr) {
					continue;
				}

				if (!companion->checkCooldownRecovery("companion_thelanding")) {
					player->sendSystemMessage("Your companions are still recovering from the last show.");
					return;
				}

				if (companion->isInCombat()) {
					player->sendSystemMessage("Not while there's a fight going on!");
					return;
				}
			}

			CompanionTheLandingShow::start(player, companions);
			break;
		}
		case 1: { // Fireworks Show (2026-07-30 addition -- single
			// companion, the one whose dialog this Theater Mode submenu
			// was opened from, same companion
			// CompanionDialogMenuSuiCallback.h's case 12 uses).
			CompanionObject* strongDialogCompanion = dialogCompanion;

			if (strongDialogCompanion == nullptr) {
				player->sendSystemMessage("@companion:no_active_companion"); // You have no active companion.
				break;
			}

			Locker clocker(strongDialogCompanion, player);

			CompanionFireworksShow::start(player, strongDialogCompanion);
			break;
		}
		case 2: { // BIRTHDAY_SHOW_2026_07_30 (whole-roster, per Nick's confirmed
			// premise: "the player character's birthday" -- the squad
			// celebrates for the OWNER, so this re-resolves the roster
			// independently just like The Landing does, ignoring
			// dialogCompanion).
			CompanionBirthdayShow::start(player, companions);
			break;
		}
		case 3: { // BATTLE_THEATER_2026_07_30 (no companions required -- pure
			// spawned-NPC spectacle, deployable anywhere near the player.
			// The companions.size()==0 gate earlier in this function is a
			// pre-existing structural limit of the whole Theater Mode SUI
			// entry point -- not new to this show. The raw
			// /companionbattletheater command works with zero companions.)
			CompanionBattleTheaterShow::start(player);
			break;
		}
		default:
			break;
		}
	}

};

#endif // COMPANIONTHEATERMODESUICALLBACK_H_
