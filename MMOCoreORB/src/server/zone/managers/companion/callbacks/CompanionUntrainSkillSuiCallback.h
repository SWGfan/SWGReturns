/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System -- SUI callback for the untrain-skill list (spec 4D).
*/

#ifndef COMPANIONUNTRAINSKILLSUICALLBACK_H_
#define COMPANIONUNTRAINSKILLSUICALLBACK_H_

#include "server/zone/objects/player/sui/SuiCallback.h"
#include "server/zone/objects/player/sui/listbox/SuiListBox.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/managers/companion/CompanionSkillTrainer.h"

class CompanionUntrainSkillSuiCallback : public SuiCallback {
	ManagedReference<CompanionObject*> companion;
	Vector<String> candidateSkills;

public:
	CompanionUntrainSkillSuiCallback(ZoneServer* server, CompanionObject* comp, const Vector<String>& candidates)
		: SuiCallback(server) {
		companion = comp;
		candidateSkills = candidates;
	}

	void run(CreatureObject* player, SuiBox* suiBox, uint32 eventIndex, Vector<UnicodeString>* args) {
		if (eventIndex == 1 || player == nullptr || args == nullptr || args->size() <= 0) {
			return;
		}

		int menuSelection = Integer::valueOf(args->get(0).toString());

		if (menuSelection < 0 || menuSelection >= candidateSkills.size()) {
			return;
		}

		ManagedReference<CompanionObject*> strongCompanion = companion;

		if (strongCompanion == nullptr) {
			return;
		}

		Locker clocker(strongCompanion, player);

		CompanionSkillTrainer::instance()->untrainSkill(player, strongCompanion, candidateSkills.get(menuSelection));

		// Companion System (2026-08-09, v3 dynamic mirroring pass):
		// recompute the owner's full datapad-wide mirrored ability set
		// immediately after a real, interactive untrain -- this is what
		// actually revokes a stale companion_* ability once nothing left
		// in the datapad still grants it. See CompanionSkillTrainer.h's
		// doc comment on syncOwnerMirrorAbilities().
		CompanionSkillTrainer::instance()->syncOwnerMirrorAbilities(player);
	}

};

#endif // COMPANIONUNTRAINSKILLSUICALLBACK_H_
