/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-13, "custom companion name" pass) -- SUI
	callback for the "Rename Companion" option on the Talk-to-Companion
	dialog menu (see CompanionDialogMenuSuiCallback.h, case 8). Validates the
	typed name through NameManager's own profanity/reserved-word filter, via
	the companion-specific validateCompanionName() (2026-07-16) -- unlike
	real character names, companion names are allowed digits and spaces
	(e.g. "Unit 7"), so this deliberately does NOT use the strict
	species-keyed validateName() every player character name goes through.
	Then sets the companion's customName to the chosen name plus
	" (<Owner>'s Companion -=COMPANION=-)" appended on the same line.
	SceneObject::getDisplayedName() just returns customName verbatim, so
	this is exactly the same single-line "name + suffix" concatenation
	pattern already used elsewhere in this codebase (e.g. LootValues.h's
	" (Legendary)" suffix, PlaceStructureSessionImplementation.cpp's
	"'s House" suffix) -- deliberately not an embedded newline: this
	codebase has no existing precedent anywhere of a real two-line
	nameplate actually rendering client-side, so a single-line suffix is
	the safe, proven-working choice.
*/

#ifndef COMPANIONRENAMESUICALLBACK_H_
#define COMPANIONRENAMESUICALLBACK_H_

#include "server/zone/objects/player/sui/SuiCallback.h"
#include "server/zone/objects/player/sui/inputbox/SuiInputBox.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/managers/name/NameManager.h"

class CompanionRenameSuiCallback : public SuiCallback {
	ManagedReference<CompanionObject*> companion;

public:
	CompanionRenameSuiCallback(ZoneServer* serv, CompanionObject* comp)
		: SuiCallback(serv) {
		companion = comp;
	}

	void run(CreatureObject* player, SuiBox* suiBox, uint32 eventIndex, Vector<UnicodeString>* args) {
		if (eventIndex == 1 || player == nullptr || args == nullptr || args->size() <= 0) {
			return;
		}

		ManagedReference<CompanionObject*> strongCompanion = companion;

		if (strongCompanion == nullptr) {
			return;
		}

		String chosenName = args->get(0).toString().trim();

		if (chosenName.isEmpty() || chosenName.length() > 40) {
			player->sendSystemMessage("@companion:rename_invalid"); // That name is not valid. Companion names must be 1-40 characters.
			return;
		}

		NameManager* nameManager = server->getNameManager();

		// Companion System (2026-07-16, per user request): companions use
		// their own, more permissive validateCompanionName() now instead of
		// the strict real-character validateName() -- allows numbers and
		// spaces (e.g. "Unit 7"), still runs the same profanity filter.
		if (nameManager != nullptr && nameManager->validateCompanionName(chosenName) != NameManagerResult::ACCEPTED) {
			player->sendSystemMessage("@companion:rename_rejected"); // That name was rejected by the name filter.
			return;
		}

		Locker clocker(strongCompanion, player);

		// Owner display name for the appended suffix -- getFirstName() (not
		// the full "First Last") matches the same convention
		// PlaceStructureSessionImplementation.cpp's "'s House" suffix uses.
		String ownerName = player->getFirstName();

		if (ownerName.isEmpty()) {
			ownerName = player->getDisplayedName();
		}

		// Companion System (2026-07-15, per user request): dropped the
		// redundant "Companion" word -- the "-=COMPANION=-" tag already says
		// it. Was: "<name> (<owner>'s Companion -=COMPANION=-)".
		String newNameplate = chosenName + " (" + ownerName + "'s -=COMPANION=-)";

		strongCompanion->setCustomObjectName(newNameplate, true);

		// Companion System (2026-07-15, per user request): keep the datapad
		// DEVICE's name in sync with the companion's chosen name, so
		// multiple companions are tellable apart in the datapad and can be
		// called individually.
		ManagedReference<CompanionControlDevice*> device = strongCompanion->getCompanionControlDevice();

		if (device != nullptr) {
			Locker dlocker(device, player);

			device->setCustomObjectName(chosenName, true);
		}

		player->sendSystemMessage("@companion:rename_success"); // Your companion's name has been updated.
	}

};

#endif // COMPANIONRENAMESUICALLBACK_H_
