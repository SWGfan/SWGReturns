/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-18 second camp revision -- see
	CampDeploymentManager.cpp's rewrite doc comment and NOTES.md). The tent
	picker: lists every camp tier the companion's training allows, each row
	marked either "[carried -- ready to deploy]" or "[craft: <recipe>]".
	Selecting a row dispatches to CampDeploymentManager::deployCampTier(),
	which deploys the carried kit or starts the recipe-checked crafting
	theater for that exact tier.
*/

#ifndef COMPANIONCAMPCHOICESUICALLBACK_H_
#define COMPANIONCAMPCHOICESUICALLBACK_H_

#include "server/zone/objects/player/sui/SuiCallback.h"
#include "server/zone/objects/player/sui/listbox/SuiListBox.h"
#include "server/zone/objects/player/sui/SuiWindowType.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/objects/companion/CompanionObject.h"

namespace server {
namespace zone {
namespace managers {
namespace companion {
	class CampDeploymentManager;
}
}
}
}

class CompanionCampChoiceSuiCallback : public SuiCallback {
	ManagedReference<CompanionObject*> companion;
	Vector<int> tierIndexes; // row -> CAMP_KIT_TIERS index

public:
	CompanionCampChoiceSuiCallback(ZoneServer* server, CompanionObject* comp, const Vector<int>& tiers)
		: SuiCallback(server) {
		companion = comp;
		tierIndexes = tiers;
	}

	static void sendChoiceBox(CreatureObject* player, CompanionObject* comp);

	void run(CreatureObject* player, SuiBox* suiBox, uint32 eventIndex, Vector<UnicodeString>* args);

};

#endif // COMPANIONCAMPCHOICESUICALLBACK_H_
