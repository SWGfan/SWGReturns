/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-17, "militant formations" pass, per user
	request) -- SUI callback for the formation picker /companionformup opens
	when clicked with no argument (real hotbar abilities can't prompt for a
	typed argument, so the button pops this list instead of being locked to
	one hard-coded shape the way the 2026-07-14 version was).
	Modeled on CompanionStarterProfessionSuiCallback.h. The chosen formation
	is applied immediately via FormationManager::formUp() (which also
	remembers it as the owner's active formation for every later
	/companionfollow) and announced with squad chatter.
*/

#ifndef COMPANIONFORMATIONSUICALLBACK_H_
#define COMPANIONFORMATIONSUICALLBACK_H_

#include "server/zone/objects/player/sui/SuiCallback.h"
#include "server/zone/objects/player/sui/listbox/SuiListBox.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/managers/companion/FormationManager.h"
#include "server/zone/managers/companion/CompanionChatter.h"

class CompanionFormationSuiCallback : public SuiCallback {
public:
	CompanionFormationSuiCallback(ZoneServer* serv)
		: SuiCallback(serv) {
	}

	void run(CreatureObject* player, SuiBox* suiBox, uint32 eventIndex, Vector<UnicodeString>* args) {
		if (eventIndex == 1 || player == nullptr || args == nullptr || args->size() <= 0) {
			return;
		}

		int menuSelection = Integer::valueOf(args->get(0).toString());

		if (menuSelection < 0 || menuSelection >= FormationManager::FORMATION_COUNT) {
			return;
		}

		String formation = FormationManager::FORMATION_NAMES[menuSelection];

		FormationManager::instance()->formUp(player, formation);

		// Squad chatter -- resolve the summoned companions the same datapad
		// way every Companion*Command does (see CompanionFollowCommand.h's
		// resolveActiveCompanions for the canonical copy of this scan).
		Vector<ManagedReference<CompanionObject*>> companions;
		ManagedReference<SceneObject*> datapad = player->getSlottedObject("datapad");

		if (datapad != nullptr) {
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

				companions.add(companion);
			}
		}

		CompanionChatter::announceOrder(player, "Squad -- form up! " + formation.toUpperCase() + " formation!", "formup", companions);
	}
};

#endif // COMPANIONFORMATIONSUICALLBACK_H_
