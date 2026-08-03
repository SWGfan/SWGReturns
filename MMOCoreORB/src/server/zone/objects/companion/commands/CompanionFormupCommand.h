/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-14, "Form Up" macro pass) -- dedicated
	/companionformup command, giving FormationManager its own real,
	hotbar-draggable owner ability, exactly the same way /companionfollow,
	/companionstay, /companionpatrol, and /companionstore already do for
	their own order commands -- see CompanionStayCommand.h and
	docs/companion_system/NOTES.md.

	2026-07-17 ("militant formations" pass, per user request) -- no longer
	hard-locked to "line": a typed argument picks the formation directly
	(/companionformup wedge -- macro-friendly), and a bare click (hotbar
	abilities can't prompt for arguments) opens a SUI list of all six
	formation shapes instead (CompanionFormationSuiCallback.h).
*/

#ifndef COMPANIONFORMUPCOMMAND_H_
#define COMPANIONFORMUPCOMMAND_H_

#include "server/zone/objects/creature/commands/QueueCommand.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/objects/player/sui/listbox/SuiListBox.h"
#include "server/zone/managers/companion/FormationManager.h"
#include "server/zone/managers/companion/CompanionChatter.h"
#include "server/zone/managers/companion/callbacks/CompanionFormationSuiCallback.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"

class CompanionFormupCommand : public QueueCommand {
public:

	CompanionFormupCommand(const String& name, ZoneProcessServer* server)
		: QueueCommand(name, server) {

	}

	/** See CompanionFollowCommand.h's identical helper for the rationale. */
	void resolveActiveCompanions(CreatureObject* player, Vector<ManagedReference<CompanionObject*>>& companions) const {
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

			companions.add(companion);
		}
	}

	int doQueueCommand(CreatureObject* creature, const uint64& target, const UnicodeString& arguments) const {
		if (!checkStateMask(creature)) {
			return INVALIDSTATE;
		}

		if (!checkInvalidLocomotions(creature)) {
			return INVALIDLOCOMOTION;
		}

		String arg = arguments.toString().trim().toLowerCase();

		if (!arg.isEmpty()) {
			if (!FormationManager::instance()->isValidFormation(arg)) {
				String valid = "";

				for (int i = 0; i < FormationManager::FORMATION_COUNT; ++i) {
					if (i > 0) {
						valid += ", ";
					}

					valid += FormationManager::FORMATION_NAMES[i];
				}

				creature->sendSystemMessage("Unknown formation '" + arg + "'. Valid formations: " + valid + ".");
				return INVALIDPARAMETERS;
			}

			// FormationManager::formUp() already does its own null/zone/
			// no-followers checks and sends the real "@companion:
			// formup_no_followers"/"@companion:formup_complete" messages.
			FormationManager::instance()->formUp(creature, arg);

			Vector<ManagedReference<CompanionObject*>> companions;
			resolveActiveCompanions(creature, companions);
			CompanionChatter::announceOrder(creature, "Squad -- form up! " + arg.toUpperCase() + " formation!", "formup", companions);

			return SUCCESS;
		}

		// Bare click (hotbar) -- open the formation picker instead.
		ManagedReference<PlayerObject*> ghost = creature->getPlayerObject();

		if (ghost == nullptr) {
			return GENERALERROR;
		}

		ghost->closeSuiWindowType(SuiWindowType::COMPANION_FORMUP_SELECT);

		ManagedReference<SuiListBox*> listBox = new SuiListBox(creature, SuiWindowType::COMPANION_FORMUP_SELECT);
		listBox->setPromptTitle("-=COMPANION=- : Choose Formation");
		listBox->setPromptText("Choose the formation your squad should assume and hold while following you. Macro tip: /companionformup <name> picks one directly.");

		listBox->addMenuItem("Line -- single rank abreast behind you");
		listBox->addMenuItem("Wedge -- V shape trailing you");
		listBox->addMenuItem("Box -- 3-wide marching grid behind you");
		listBox->addMenuItem("Column -- single file, for streets and caves");
		listBox->addMenuItem("Vanguard -- full rank in FRONT, walking point");
		listBox->addMenuItem("Escort -- protective diamond around you");

		listBox->setCallback(new CompanionFormationSuiCallback(creature->getZoneServer()));

		ghost->addSuiBox(listBox);
		creature->sendMessage(listBox->generateMessage());

		return SUCCESS;
	}
};

#endif // COMPANIONFORMUPCOMMAND_H_
