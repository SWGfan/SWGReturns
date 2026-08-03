/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-18, ranger auto-harvest -- see NOTES.md). The
	ask-ONCE resource picker: the first time a ranger-trained companion's
	post-combat sweep reaches a harvestable creature corpse and no harvest
	preference has ever been chosen, this list box asks the owner which
	resource (meat / hide / bone) their ranger should collect from then on.
	The answer persists on the companion (CompanionObject::harvestPreference,
	stored as CreatureManagerImplementation::harvest()'s own selectedID codes
	234/235/236) and is changeable any time from the companion's radial
	Harvesting submenu (CompanionMenuComponent) -- this box never re-opens on
	its own once a choice is stored.
*/

#ifndef COMPANIONHARVESTCHOICESUICALLBACK_H_
#define COMPANIONHARVESTCHOICESUICALLBACK_H_

#include "server/zone/objects/player/sui/SuiCallback.h"
#include "server/zone/objects/player/sui/listbox/SuiListBox.h"
#include "server/zone/objects/player/sui/SuiWindowType.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/objects/companion/CompanionObject.h"

class CompanionHarvestChoiceSuiCallback : public SuiCallback {
	ManagedReference<CompanionObject*> companion;

public:
	CompanionHarvestChoiceSuiCallback(ZoneServer* server, CompanionObject* comp)
		: SuiCallback(server) {
		companion = comp;
	}

	/** The one-and-only opener -- shared by the sweep's first-encounter ask
	 * and the radial Harvesting submenu's "Choose..." parent option. */
	static void sendChoiceBox(CreatureObject* player, CompanionObject* comp) {
		if (player == nullptr || comp == nullptr) {
			return;
		}

		ManagedReference<PlayerObject*> ghost = player->getPlayerObject();

		if (ghost == nullptr) {
			return;
		}

		// If it's already open (several corpses in one sweep), don't stack.
		if (ghost->hasSuiBoxWindowType(SuiWindowType::COMPANION_HARVEST_CHOICE)) {
			return;
		}

		ManagedReference<SuiListBox*> sui = new SuiListBox(player, SuiWindowType::COMPANION_HARVEST_CHOICE);
		sui->setPromptTitle(comp->getDisplayedName() + " -=COMPANION=- : Harvesting");
		sui->setPromptText("Your companion can harvest creature corpses after combat. Which resource should it collect? (You can change this later from its radial Harvesting menu.)");
		sui->setCancelButton(true, "@ui:cancel");
		sui->setOkButton(true, "@ui:ok");
		sui->setCallback(new CompanionHarvestChoiceSuiCallback(player->getZoneServer(), comp));

		sui->addMenuItem("Meat"); // 0 -> 234
		sui->addMenuItem("Hide"); // 1 -> 235
		sui->addMenuItem("Bone"); // 2 -> 236

		ghost->addSuiBox(sui);
		player->sendMessage(sui->generateMessage());
	}

	void run(CreatureObject* player, SuiBox* suiBox, uint32 eventIndex, Vector<UnicodeString>* args) {
		if (eventIndex == 1 || player == nullptr || args == nullptr || args->size() <= 0) {
			return;
		}

		int menuSelection = Integer::valueOf(args->get(0).toString());

		// 0/1/2 -> the real harvest radial selectedID codes.
		int preference = 0;

		switch (menuSelection) {
		case 0:
			preference = 234;
			break;
		case 1:
			preference = 235;
			break;
		case 2:
			preference = 236;
			break;
		default:
			return;
		}

		ManagedReference<CompanionObject*> strongCompanion = companion;

		if (strongCompanion == nullptr) {
			return;
		}

		Locker clocker(strongCompanion, player);
		strongCompanion->setHarvestPreference(preference);

		String label = preference == 234 ? "meat" : (preference == 235 ? "hide" : "bone");
		player->sendSystemMessage("Your companion will harvest " + label + " from creature corpses after combat.");
	}

};

#endif // COMPANIONHARVESTCHOICESUICALLBACK_H_
