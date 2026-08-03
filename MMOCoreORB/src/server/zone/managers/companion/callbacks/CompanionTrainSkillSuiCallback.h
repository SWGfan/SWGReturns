/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System -- SUI callback for the train-skill list (spec 4D).
*/

#ifndef COMPANIONTRAINSKILLSUICALLBACK_H_
#define COMPANIONTRAINSKILLSUICALLBACK_H_

#include "server/zone/objects/player/sui/SuiCallback.h"
#include "server/zone/objects/player/sui/listbox/SuiListBox.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/managers/companion/CompanionSkillTrainer.h"

class CompanionTrainSkillSuiCallback : public SuiCallback {
	ManagedReference<CompanionObject*> companion;
	Vector<String> candidateSkills;

public:
	// 2026-07-20: reused by the colored skill-tree window too -- when
	// treeMode is true, the callback re-opens the tree after training (so
	// the just-trained box flips green immediately) instead of doing
	// nothing. An empty-string candidate (a header/branch label row in the
	// tree) trains nothing and just re-opens.
	bool treeMode = false;

	CompanionTrainSkillSuiCallback(ZoneServer* server, CompanionObject* comp, const Vector<String>& candidates, bool asTree = false)
		: SuiCallback(server) {
		companion = comp;
		candidateSkills = candidates;
		treeMode = asTree;
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

		const String& picked = candidateSkills.get(menuSelection);

		if (!picked.isEmpty()) {
			Locker clocker(strongCompanion, player);
			CompanionSkillTrainer::instance()->trainSkill(player, strongCompanion, picked);
		}

		// Tree mode: reopen so the newly-green box (and its whole trained
		// prerequisite chain) shows immediately.
		if (treeMode) {
			CompanionSkillTrainer::instance()->sendSkillTree(player, strongCompanion);
		}
	}

};

#endif // COMPANIONTRAINSKILLSUICALLBACK_H_
