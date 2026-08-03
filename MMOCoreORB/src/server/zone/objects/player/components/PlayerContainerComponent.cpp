/*
 * PlayerContainerComponent.cpp
 *
 *  Created on: 26/05/2011
 *      Author: victor
 */

#include "PlayerContainerComponent.h"
#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/objects/player/FactionStatus.h"
#include "server/zone/objects/tangible/wearables/ArmorObject.h"
#include "server/zone/objects/tangible/weapon/WeaponObject.h"
#include "server/zone/managers/player/PlayerManager.h"
#include "server/zone/ZoneServer.h"
#include "server/zone/managers/visibility/VisibilityManager.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/managers/companion/CompanionSkillTrainer.h"

int PlayerContainerComponent::canAddObject(SceneObject* sceneObject, SceneObject* object, int containmentType, String& errorDescription) const {
	CreatureObject* creo = dynamic_cast<CreatureObject*>(sceneObject);

	if (creo == nullptr) {
		return TransferErrorCode::PLAYERUSEMASKERROR;
	}

	if (object->isTangibleObject() && containmentType == 4) {
		// Companion System (2026-07-15, per user request): the Companion
		// Loadout backpack is a system container, not wearable gear -- the
		// client's default Equip radial can't be removed for its wearable
		// container type, so the equip attempt is refused here instead.
		if (object->getServerObjectCRC() == STRING_HASHCODE("object/tangible/inventory/companion_loadout_backpack.iff")) {
			errorDescription = "The Companion Loadout cannot be worn.";

			return TransferErrorCode::PLAYERUSEMASKERROR;
		}

		TangibleObject* wearable = cast<TangibleObject*>(object);

		SharedTangibleObjectTemplate* tanoData = dynamic_cast<SharedTangibleObjectTemplate*>(wearable->getObjectTemplate());

		// Companion System (2026-07-12): gate the race/species check the same
		// way the faction check just below it already is. This component is
		// reused as-is (via inheritance) by CompanionContainerComponent (see
		// server/zone/objects/companion/components/CompanionContainerComponent.h)
		// so a live CompanionObject can run through the exact same validated
		// "Wear" logic a real player does instead of a duplicated copy. A
		// companion's own object template is the synthetic NPC appearance
		// shell object/mobile/companion_actor.iff (see
		// object/mobile/companion_actor.lua), which never appears in any
		// item's SharedTangibleObjectTemplate playerRaces whitelist (those
		// lists are populated exclusively with real object/creature/player/
		// *.iff race template paths) -- so, unchanged, this check would
		// silently reject every wearable for every companion, always,
		// regardless of the item. Gating it behind isPlayerCreature(),
		// exactly like the faction check two blocks down, is a no-op for
		// every real player (isPlayerCreature() is always true for them, so
		// this is byte-for-byte the same behavior they had before) and only
		// changes anything for non-player CreatureObject subclasses --
		// currently just CompanionObject. See docs/companion_system/NOTES.md
		// ("Companion Auto-Equip") for the full writeup and the explicitly
		// accepted limitation this creates (companions are not currently
		// species/race-restricted on what they can wear).
		if (creo->isPlayerCreature() && tanoData != nullptr) {
			const auto races = tanoData->getPlayerRaces();
			String race = creo->getObjectTemplate()->getFullTemplateString();

			if (!races->contains(race.hashCode())) {
				errorDescription = "You lack the necessary requirements to wear this object";

				return TransferErrorCode::PLAYERUSEMASKERROR;
			}
		}

		if (creo->isPlayerCreature()) {
			if (!wearable->isNeutral()) {
				if (wearable->isImperial() && (creo->getFactionStatus() == FactionStatus::ONLEAVE || !creo->isImperial())) {
					errorDescription = "You lack the necessary requirements to wear this object";

					return TransferErrorCode::PLAYERUSEMASKERROR;
				}

				if (wearable->isRebel() && (creo->getFactionStatus() == FactionStatus::ONLEAVE || !creo->isRebel())) {
					errorDescription = "You lack the necessary requirements to wear this object";

					return TransferErrorCode::PLAYERUSEMASKERROR;
				}
			}
		}

		// Companion System bug fix (2026-07-13, "armor silently fails to
		// auto-equip" -- see docs/companion_system/NOTES.md): gated behind
		// isPlayerCreature() the same way the race check above and the
		// faction check below already are. checkEncumbrancies() compares the
		// armor's Health/Action/Mind encumbrance costs against the wearer's
		// raw STRENGTH/CONSTITUTION/QUICKNESS/STAMINA/FOCUS/WILLPOWER HAM
		// pools -- a companion's HAM is migrated to a flat, non-combat
		// "master entertainer" baseline (see
		// CompanionObjectImplementation::migrateBaselineStats()), not a real
		// combat character's stat spread, so this check failed for
		// essentially any real armor piece, silently: attemptAutoEquip()
		// (CompanionContainerComponent.cpp) treats any non-zero canAddObject()
		// result as "not equippable right now" and quietly leaves the item in
		// inventory (by design, since this is a passive auto-reaction, not a
		// player-initiated command) -- and even the one system message this
		// function does send on failure (equip_armor_fail) went to
		// player->sendSystemMessage(), which for a companion is a no-op (no
		// AiAgent/companion ever has a client `owner` reference -- see
		// NOTES.md's "K" skills-window research), so nothing was ever visible
		// to the player either. Unchanged for every real player
		// (isPlayerCreature() is always true for them).
		if (creo->isPlayerCreature() && object->isArmorObject()) {
			PlayerManager* playerManager = sceneObject->getZoneServer()->getPlayerManager();

			if (!playerManager->checkEncumbrancies(creo, cast<ArmorObject*>(object))) {
				errorDescription = "You lack the necessary secondary stats to equip this item";

				return TransferErrorCode::NOTENOUGHENCUMBRANCE;
			}
		}

		// Companion System bug fix (2026-07-13, same pass as the encumbrance
		// gate above -- see docs/companion_system/NOTES.md): also gated
		// behind isPlayerCreature(). creo->hasSkill() here is
		// CreatureObject::hasSkill() (CreatureObject.idl), which checks the
		// real skillList field populated by SkillManager -- CompanionObject::
		// grantSkill() (CompanionObjectImplementation.cpp) deliberately never
		// touches that field, only its own isolated learnedSkills ledger (see
		// CompanionObject.idl's field doc: "isolated from the player
		// SkillManager skill tree"). Left ungated, hasSkill() would return
		// false unconditionally for every companion, so any wearable with a
		// non-empty certificationsRequired list could never auto-equip onto a
		// companion no matter what it had "learned" -- same silent-failure
		// shape as the encumbrance bug just above (attemptAutoEquip() treats
		// any non-zero canAddObject() result as "not equippable right now"
		// and quietly leaves the item in inventory). Same accepted limitation
		// already documented for the race check further up: companions are
		// not currently skill/cert-restricted on what they can wear. Unchanged
		// for every real player.
		if (creo->isPlayerCreature() && object->isWearableObject()) {
			if (tanoData != nullptr) {
				const Vector<String>& skillsRequired = tanoData->getCertificationsRequired();

				if (skillsRequired.size() > 0) {
					bool hasSkill = false;

					for (int i = 0; i < skillsRequired.size(); i++) {
						const String& skill = skillsRequired.get(i);

						if (!skill.isEmpty() && creo->hasSkill(skill)) {
							hasSkill = true;
							break;
						}
					}

					if (!hasSkill) {
						errorDescription = "@error_message:insufficient_skill"; // You lack the skill to use this item.

						return TransferErrorCode::PLAYERUSEMASKERROR;
					}
				}
				
				if ((wearable->getMaxCondition() - wearable->getConditionDamage()) <= 0) {
					errorDescription = "This object has been damaged to the point of uselessness.";
						return TransferErrorCode::PLAYERUSEMASKERROR;
				}
			}
		}

		if (object->isWeaponObject()) {
			WeaponObject* weapon = cast<WeaponObject*>(object);
			int bladeColor = weapon->getBladeColor();
			PlayerObject* ghost = creo->getPlayerObject();

			if (weapon->isJediWeapon()) {
				if (bladeColor == 31) {
					errorDescription = "@jedi_spam:lightsaber_no_color";
					return TransferErrorCode::PLAYERUSEMASKERROR;
				}

				// Companion System bug fix (JEDI_SKILL_GATING_FIX_2026_07_29, "Jedi skill gating" pass --
				// see docs/companion_system/NOTES.md): the crafter check below
				// this branch is meaningless for a companion -- ghost is always
				// null (companions never have a PlayerObject), so dereferencing it
				// here would fault outright, and the only real gate left is the
				// crafter comparison -- and CompanionCraftingManager::craftItem()
				// stamps the companion's own identity as crafter on everything it
				// crafts, so a companion that crafts (or is simply handed) a
				// lightsaber could always equip it, no training required. Requires
				// the same badge-based isJediEligible() check jedi_* skill training
				// itself uses instead. Real players are untouched --
				// isCompanionObject() is always false for them, so they fall straight
				// through to the original crafter/isPrivileged() check (now
				// additionally null-guarded on ghost, which is a no-op for a real
				// player and keeps a non-player CreatureObject from faulting here).
				if (creo->isCompanionObject()) {
					CompanionObject* companion = cast<CompanionObject*>(creo);

					if (!CompanionSkillTrainer::instance()->isJediEligible(companion)) {
						errorDescription = "@jedi_spam:not_your_lightsaber";
						return TransferErrorCode::PLAYERUSEMASKERROR;
					}
				} else if (weapon->getCraftersName() != creo->getFirstName() && (ghost == nullptr || !ghost->isPrivileged())) {
					errorDescription = "@jedi_spam:not_your_lightsaber";
					return TransferErrorCode::PLAYERUSEMASKERROR;
				}
			}
		}
	}

	return ContainerComponent::canAddObject(sceneObject, object, containmentType, errorDescription);
}

/**
 * Is called when this object has been inserted with an object
 * @param object object that has been inserted
 */
int PlayerContainerComponent::notifyObjectInserted(SceneObject* sceneObject, SceneObject* object) const {
	CreatureObject* creo = dynamic_cast<CreatureObject*>(sceneObject);

	if (creo == nullptr) {
		return 0;
	}

	if (object->isArmorObject()) {
		PlayerManager* playerManager = sceneObject->getZoneServer()->getPlayerManager();
		playerManager->applyEncumbrancies(creo, cast<ArmorObject*>(object));
	}

	if (object->isTangibleObject()) {
		TangibleObject* tano = cast<TangibleObject*>(object);
		tano->applySkillModsTo(creo);
	}

	if (object->isInstrument() && creo->isEntertaining())
		creo->stopEntertaining();

	//this it to update the equipment list
	//we need a DeltaVector with all the slotted objects it seems
	/*CreatureObjectMessage6* msg6 = new CreatureObjectMessage6(creo);
	creo->broadcastMessage(msg6, true, true);*/

	if (object->isTangibleObject() && object->getArrangementDescriptorSize() != 0 && object->getArrangementDescriptor(0)->size() != 0) {
		const String& arrangement = object->getArrangementDescriptor(0)->get(0);

		if (arrangement != "mission_bag" && arrangement != "ghost" && arrangement != "bank") {
			creo->addWearableObject(object->asTangibleObject(), true);
		}
	}

	if (object->isTangibleObject()) {
		ManagedReference<TangibleObject*> tano = object->asTangibleObject();
		tano->addTemplateSkillMods(creo);
	}

	// Jedi stuff below.
	PlayerObject* ghost = creo->getPlayerObject();

	if (ghost && ghost->isJedi()) {

		if (object->isRobeObject()) {
			//Remove force power from clothing
			//ghost->setForcePowerMax(creo->getSkillMod("jedi_force_power_max"));
			//ghost->recalculateForcePower();
		} else if (object->isWeaponObject()) {
			WeaponObject* weaponObject = cast<WeaponObject*>(object);
			if (weaponObject->isJediWeapon()) {
				VisibilityManager::instance()->increaseVisibility(creo, VisibilityManager::SABERVISMOD);
			}
		}
	}

	return ContainerComponent::notifyObjectInserted(sceneObject, object);
}

/**
 * Is called when an object was removed
 * @param object object that has been inserted
 */
int PlayerContainerComponent::notifyObjectRemoved(SceneObject* sceneObject, SceneObject* object, SceneObject* destination) const {
	CreatureObject* creo = dynamic_cast<CreatureObject*>(sceneObject);

	if (creo == nullptr) {
		return 0;
	}

	if (object->isArmorObject()) {
		PlayerManager* playerManager = creo->getZoneServer()->getPlayerManager();
		playerManager->removeEncumbrancies(creo, cast<ArmorObject*>(object));
	}

	if (object->isTangibleObject()) {
		TangibleObject* tano = cast<TangibleObject*>(object);
		tano->removeSkillModsFrom(creo);
	}

	if (object->isInstrument()) {
		if (creo->isPlayingMusic())
			creo->stopEntertaining();
	}

	//this it to update the equipment list
	//we need a DeltaVector with all the slotted objects it seems
	/*CreatureObjectMessage6* msg6 = new CreatureObjectMessage6(creo);
	creo->broadcastMessage(msg6, true, true);*/

	if (object->isTangibleObject() && object->getArrangementDescriptorSize() != 0 && object->getArrangementDescriptor(0)->size() != 0) {
		const String& arrangement = object->getArrangementDescriptor(0)->get(0); //CHK

		if (arrangement != "mission_bag" && arrangement != "ghost" && arrangement != "bank") {
			creo->removeWearableObject(object->asTangibleObject(), true);
		}
	}

	if (object->isTangibleObject()) {
		ManagedReference<TangibleObject*> tano = object->asTangibleObject();
		tano->removeTemplateSkillMods(creo);
	}

	// Jedi stuff below.
	PlayerObject* ghost = creo->getPlayerObject();

	if (ghost && ghost->isJedi()) {
		if (object->isRobeObject()) {
			ghost->recalculateForcePower();
		}
	}

	return ContainerComponent::notifyObjectRemoved(sceneObject, object, destination);
}
