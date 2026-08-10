/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System -- SUI callback for the one-time first-launch starter
	profession picker. On a valid selection, grants the chosen profession's
	real novice skill box (e.g. "combat_marksman_novice") to the companion's
	isolated skill ledger.
	2026-07-24 FIX: now routed through CompanionSkillTrainer::trainSkill()
	(0 SP by construction, same as every other companion skill grant) instead
	of a bare CompanionObject::grantSkill() call, so any prerequisite skill
	box the chosen profession depends on is granted first, bottom-up -- see
	the inline comment at the actual call site for the full root-cause note.
	Then marks the companion's one-time first-launch flow complete. If the player
	cancels (eventIndex == 1) or the SUI is otherwise dismissed without a
	valid selection, firstLaunchComplete is deliberately left false so the
	picker is shown again on the next summon -- see
	CompanionControlDeviceImplementation::spawnObject() and
	docs/companion_system/NOTES.md.
*/

#ifndef COMPANIONSTARTERPROFESSIONSUICALLBACK_H_
#define COMPANIONSTARTERPROFESSIONSUICALLBACK_H_

#include "server/zone/objects/player/sui/SuiCallback.h"
#include "server/zone/objects/player/sui/listbox/SuiListBox.h"
#include "server/zone/objects/player/sui/inputbox/SuiInputBox.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/managers/companion/CompanionSkillTrainer.h"
#include "server/zone/managers/companion/callbacks/CompanionRenameSuiCallback.h"
#include "server/zone/managers/player/creation/PlayerCreationManager.h"

class CompanionStarterProfessionSuiCallback : public SuiCallback {
	ManagedReference<CompanionObject*> companion;
	Vector<String> candidateProfessions;

	// Companion System (2026-07-13, "starting loadout" pass -- see
	// NOTES.md): COMPANION_STARTER_PROFESSIONS (CompanionSkillTrainer.cpp)
	// are real novice skill-box strings (e.g. "combat_marksman_novice") --
	// PlayerCreationManager's professionDefaultsInfo table is keyed by the
	// bare profession root name instead (e.g. "marksman"), the same keys
	// real character creation uses. Maps between the two; falls through to
	// professionDefaultsInfo's own index-0 generic-default entry (via
	// grantStartingGearTo()) for anything unrecognized, so an unmapped
	// string is a degraded default kit, never a crash.
	static String resolveProfessionRootName(const String& skillBoxName) {
		if (skillBoxName == "crafting_artisan_novice")
			return String("artisan");
		if (skillBoxName == "combat_brawler_novice")
			return String("brawler");
		if (skillBoxName == "combat_marksman_novice")
			return String("marksman");
		if (skillBoxName == "science_medic_novice")
			return String("medic");
		if (skillBoxName == "outdoors_scout_novice")
			return String("scout");
		if (skillBoxName == "social_entertainer_novice")
			return String("entertainer");

		return skillBoxName;
	}

	// 2026-07-20 FIX ("artisan spawned with a dress, no tool"):
	// professionDefaultsInfo is keyed by the CHARACTER-CREATION profession
	// names from creation/profession_defaults.iff -- "crafting_artisan",
	// "combat_brawler", etc. (getStarterProfession()'s own format) -- NOT
	// the short roots resolveProfessionRootName() returns for display. The
	// old call passed "artisan", the lookup missed, and the index-0
	// fallback kit (the maiden's dress) was granted with no tool. The
	// defaults key is simply the novice skill box minus "_novice".
	static String resolveProfessionDefaultsKey(const String& skillBoxName) {
		if (skillBoxName.endsWith("_novice")) {
			return skillBoxName.subString(0, skillBoxName.length() - 7);
		}

		return skillBoxName;
	}

public:
	CompanionStarterProfessionSuiCallback(ZoneServer* server, CompanionObject* comp, const Vector<String>& candidates)
		: SuiCallback(server) {
		companion = comp;
		candidateProfessions = candidates;
	}

	void run(CreatureObject* player, SuiBox* suiBox, uint32 eventIndex, Vector<UnicodeString>* args) {
		if (eventIndex == 1 || player == nullptr || args == nullptr || args->size() <= 0) {
			return;
		}

		int menuSelection = Integer::valueOf(args->get(0).toString());

		if (menuSelection < 0 || menuSelection >= candidateProfessions.size()) {
			return;
		}

		ManagedReference<CompanionObject*> strongCompanion = companion;

		if (strongCompanion == nullptr) {
			return;
		}

		Locker clocker(strongCompanion, player);

		String chosenProfession = candidateProfessions.get(menuSelection);

		// Companion System (2026-07-24 FIX, "bone armor / missing prereq
		// schematics" bug -- per c3rr's root-cause research, confirmed via
		// direct read of this file + CompanionSkillTrainer.cpp): the OLD code
		// here called CompanionObject::grantSkill(chosenProfession) directly,
		// which only adds that ONE exact skill string to learnedSkills with
		// zero recursion. CompanionSkillTrainer::trainSkill() is the one
		// place that walks Skill::getSkillsRequired() depth-first and grants
		// every unmet prerequisite bottom-up before granting the requested
		// box (see its own doc comment). Any draft schematic that lives on a
		// prerequisite/intermediate skill box below the granted novice tier
        // was therefore invisible to Skill::getSchematicsGranted()-based
		// enumeration for a companion that only ever went through THIS
		// starter picker (never the real per-skill training UI, which always
		// went through trainSkill() correctly). Switching to trainSkill()
		// here closes that gap generally, for every starter profession, not
		// just armorsmith/bone armor. trainSkill() also calls
		// grantOwnerAbilitiesForSkill() internally for every skill it grants
		// (idempotent -- see that method's own hasAbility() guard), so the
		// separate call further below for chosenProfession alone is now
		// redundant for that one skill but is left in place (harmless) since
		// it's still the only call covering the always-available baseline
		// order abilities path and costs nothing extra to leave.
		CompanionSkillTrainer::instance()->trainSkill(player, strongCompanion, chosenProfession);
		strongCompanion->setFirstLaunchComplete(true);

		// Companion System (2026-07-13, "starting loadout" pass -- see
		// NOTES.md): grant the same starting equipment + clutter a real new
		// character gets for this profession. "human_male" matches
		// companion_actor.lua's own deliberate, fixed appearance choice
		// (every companion is a human male -- see that file's header
		// comment). Passing strongCompanion as both the equip target and the
		// loose-item container is still correct even after the companion
		// gained a real separate "inventory" bag child object (2026-07-13,
		// "item vanishes when taken back out" fix): any loose (clutter)
		// item transferred here at containmentType -1 goes through the same
		// notifyObjectInserted()/attemptAutoEquip() hook as any other item
		// given to the companion, which now relocates non-equippable loose
		// items into that bag automatically -- no need to resolve/pass the
		// bag directly here.
		// 2026-07-20 FIX, take 2 (verified against the actual TRE data +
		// both readObject() implementations this time): the race key in
		// BOTH tables is the SHARED client template path -- the PTMP/NAME
		// chunks store "object/creature/player/shared_human_male.iff"
		// verbatim (only the ITEM values get their shared_ stripped;
		// confirmed in creation/profession_defaults_crafting_artisan.iff
		// extracted from stardust_01.tre, and in ProfessionDefaultsInfo.h:76
		// + PlayerCreationManager.cpp:216 which .put() the NAME unmodified).
		PlayerCreationManager::instance()->grantStartingGearTo(strongCompanion, strongCompanion, resolveProfessionDefaultsKey(chosenProfession), "object/creature/player/shared_human_male.iff");

		// Companion System (2026-07-13, "macro list" pass; comment updated
		// 2026-07-24 now that the grant above goes through trainSkill(),
		// which already calls grantOwnerAbilitiesForSkill() internally for
		// every skill it grants, incl. any prereqs -- see the call site
		// above). This call is now redundant for chosenProfession itself
		// (idempotent, so harmless to leave), but this is also the exact
		// one-time "first ever companion" moment, so the five always-
		// available baseline order abilities (companion_follow/_stay/
		// _patrol/_store/_attack) are granted here as well -- see
		// CompanionSkillTrainer::grantBaselineOwnerOrderAbilities().
		CompanionSkillTrainer::instance()->grantOwnerAbilitiesForSkill(player, chosenProfession);
		CompanionSkillTrainer::instance()->grantBaselineOwnerOrderAbilities(player);

		// Companion System (2026-07-15, "test 5 companions at once" pass --
		// see NOTES.md and the user's own explicit choice of "same starter
		// profession/loadout each" over answering this picker separately per
		// companion). This picker is bound to exactly one CompanionObject
		// (see the constructor above) and, without this block, would only
		// ever resolve firstLaunchComplete for that one -- every other
		// freshly-granted companion in the datapad would independently pop
		// its own copy of this same SUI the next time IT gets summoned
		// (CompanionControlDeviceImplementation::spawnObject() checks
		// hasCompletedFirstLaunch() per companion). Instead, cascade the exact
		// same profession + starting gear + ability grants onto every OTHER
		// companion in the player's own datapad that hasn't completed its
		// first-launch flow yet, so answering this picker once covers the
		// whole batch. grantOwnerAbilitiesForSkill()/
		// grantBaselineOwnerOrderAbilities()/grantAllAbilitiesForTesting()
		// are all idempotent (hasAbility() guards), so re-granting the same
		// owner-side abilities here on top of the calls above is harmless.
		ManagedReference<SceneObject*> datapad = player->getSlottedObject("datapad");

		if (datapad != nullptr) {
			for (int i = 0; i < datapad->getContainerObjectsSize(); ++i) {
				ManagedReference<SceneObject*> obj = datapad->getContainerObject(i);

				if (obj == nullptr || !obj->isCompanionControlDevice()) {
					continue;
				}

				CompanionControlDevice* otherDevice = cast<CompanionControlDevice*>(obj.get());
				CompanionObject* otherCompanion = otherDevice != nullptr ? otherDevice->getCompanionObject() : nullptr;

				if (otherCompanion == nullptr || otherCompanion == strongCompanion) {
					continue;
				}

				if (otherCompanion->hasCompletedFirstLaunch()) {
					continue;
				}

				Locker otherLocker(otherCompanion, player);

				// Companion System (2026-07-27 FIX, "crafting companions never
				// reach top tier" -- this cascade block was missed by the
				// 2026-07-24 trainSkill() fix above, which only covers the ONE
				// companion directly answering this SUI. Every OTHER companion in
				// the datapad was still granted the bare chosenProfession string with
				// zero prerequisite backfill -- same root cause, same fix.
				CompanionSkillTrainer::instance()->trainSkill(player, otherCompanion, chosenProfession);
				otherCompanion->setFirstLaunchComplete(true);

				PlayerCreationManager::instance()->grantStartingGearTo(otherCompanion, otherCompanion, resolveProfessionDefaultsKey(chosenProfession), "object/creature/player/shared_human_male.iff");
			}
		}

		// Companion System (2026-08-09, v3 dynamic mirroring pass -- see
		// NOTES.md and CompanionSkillTrainer.h's doc comment on
		// syncOwnerMirrorAbilities()). Replaces the old
		// grantAllAbilitiesForTesting() bench-testing shortcut with a real
		// recompute of the owner's mirrored ability set, unioned across
		// every companion in the datapad (summoned or stored). Placed here,
		// AFTER the cascade loop above, so this one call captures both the
		// primary companion answering this SUI and any other companions
		// just cascaded into the same starting profession in a single pass.
		CompanionSkillTrainer::instance()->syncOwnerMirrorAbilities(player);

		player->sendSystemMessage("@companion:starter_profession_chosen"); // Your companion has chosen its starting profession.

		// Companion System (2026-07-20, "ask BEFORE the companion spawns"
		// rework -- see CompanionControlDeviceImplementation::spawnObject()):
		// this picker now runs pre-spawn, so the actual summon happens HERE,
		// after the profession + loadout grants -- the companion steps into
		// the world already holding its profession gear, like a fresh
		// character. firstLaunchComplete was set above, so this re-entry
		// sails straight through spawnObject()'s pre-spawn intercept.
		ManagedReference<CompanionControlDevice*> spawnDevice = strongCompanion->getCompanionControlDevice();

		clocker.release();

		// CRASH/HANG FIX (2026-07-20, "server hangs on spawn/profession
		// pick"): the pre-spawn rework moved the actual summon here, but
		// running spawnObject() INLINE inside this SUI callback -- which
		// executes with the player lock already held -- does heavy locked
		// work (createObject/zone transfers/observer registration) and
		// deadlocked against other threads waiting on the player lock. The
		// device's own activate path always deferred spawnObject to a task
		// for exactly this reason; do the same here. Defer to a task with
		// the same clean lock order the activate lambda uses (companion
		// locked; spawnObject asserts that).
		if (spawnDevice != nullptr && strongCompanion->getZone() == nullptr) {
			ManagedReference<CompanionControlDevice*> deviceRef = spawnDevice;
			ManagedReference<CompanionObject*> companionRef = strongCompanion;
			ManagedReference<CreatureObject*> playerRef = player;

			Core::getTaskManager()->executeTask([deviceRef, companionRef, playerRef] () {
				CompanionControlDevice* device = deviceRef.get();
				CompanionObject* comp = companionRef.get();
				CreatureObject* owner = playerRef.get();

				if (device == nullptr || comp == nullptr || owner == nullptr || comp->getZone() != nullptr) {
					return;
				}

				// CRASH FIX (2026-07-20, live SIGABRT after profession pick):
				// the device/companion cross-locks below use `owner` as the
				// cross, which asserts owner is already locked by this
				// thread -- but this task worker holds nothing. Lock the
				// owner (root) first.
				Locker olocker(owner);
				Locker dlocker(device, owner);
				Locker clocker(comp, owner);

				device->spawnObject(owner);
			}, "CompanionFirstSpawnLambda");
		}

		// Companion System (2026-07-15, per user request): naming the
		// companion is part of the first-launch flow now -- the same rename
		// input box the Talk-to-Companion dialog's "Rename Companion" option
		// opens pops automatically right after the profession choice.
		//
		// Companion System (2026-07-16, per user request): default the input
		// box's text to the chosen profession's name in caps (e.g.
		// "MARKSMAN", "MEDIC") instead of the companion's current displayed
		// name -- gives the player something profession-appropriate to
		// start from, which they're still free to overwrite entirely before
		// confirming.
		ManagedReference<PlayerObject*> ghost = player->getPlayerObject();

		if (ghost != nullptr) {
			ghost->closeSuiWindowType(SuiWindowType::COMPANION_RENAME);

			String defaultName = resolveProfessionRootName(chosenProfession).toUpperCase();

			ManagedReference<SuiInputBox*> inputBox = new SuiInputBox(player, SuiWindowType::COMPANION_RENAME, 0x00);
			inputBox->setPromptTitle(strongCompanion->getDisplayedName() + " -=COMPANION=- : Name Your Companion");
			inputBox->setPromptText("@companion:rename_prompt"); // Enter a new name for your companion.
			inputBox->setMaxInputSize(40);
			inputBox->setDefaultInput(defaultName);
			inputBox->setCallback(new CompanionRenameSuiCallback(player->getZoneServer(), strongCompanion));

			ghost->addSuiBox(inputBox);
			player->sendMessage(inputBox->generateMessage());
		}
	}

};

#endif // COMPANIONSTARTERPROFESSIONSUICALLBACK_H_
