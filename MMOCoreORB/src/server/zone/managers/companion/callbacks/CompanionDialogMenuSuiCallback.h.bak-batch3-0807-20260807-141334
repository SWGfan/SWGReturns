/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System -- root dialogue/SUI menu callback (spec 4D:
	"Interacting with or targeting the companion opens a dynamic
	conversation/SUI dialog box"). Options: basic movement commands, skill
	sheet, train/untrain, the Contextual Help SUI ListBox, and (2026-07-12,
	"stats window" pass) the Stats Sheet.
*/

#ifndef COMPANIONDIALOGMENUSUICALLBACK_H_
#define COMPANIONDIALOGMENUSUICALLBACK_H_

#include "server/zone/objects/player/sui/SuiCallback.h"
#include "server/zone/objects/player/sui/listbox/SuiListBox.h"
#include "server/zone/objects/player/sui/inputbox/SuiInputBox.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/objects/waypoint/WaypointObject.h"
#include "server/zone/objects/group/GroupObject.h"
#include "server/zone/objects/intangible/VehicleControlDevice.h"
#include "server/zone/Zone.h"
#include "server/zone/managers/companion/CompanionSkillTrainer.h"
#include "server/zone/managers/combat/CombatManager.h"
#include "server/zone/managers/companion/CampDeploymentManager.h"
#include "server/zone/managers/companion/CompanionFireworksShow.h"
#include "server/zone/managers/companion/callbacks/CompanionRenameSuiCallback.h"
#include "server/zone/managers/companion/callbacks/CompanionTaxiWaypointSuiCallback.h"
#include "server/zone/managers/companion/callbacks/CompanionTheaterModeSuiCallback.h"
#include "server/zone/objects/creature/buffs/PrivateBuff.h"
#include "server/zone/objects/creature/buffs/BuffType.h"
#include "templates/params/creature/CreatureAttribute.h"

class CompanionDialogMenuSuiCallback : public SuiCallback {
	ManagedReference<CompanionObject*> companion;

	/**
	 * Companion Taxi (2026-07-15, group convoy): scans an arbitrary
	 * player's datapad for their summoned, living companion. Duplicated
	 * (rather than shared) from CompanionFollowCommand's identical helper,
	 * per this project's per-file-copy convention for this exact lookup.
	 */
	CompanionObject* resolveActiveCompanion(CreatureObject* player) const {
		if (player == nullptr) {
			return nullptr;
		}

		ManagedReference<SceneObject*> datapad = player->getSlottedObject("datapad");

		if (datapad == nullptr) {
			return nullptr;
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

			return comp;
		}

		return nullptr;
	}

	/**
	 * Companion Vehicle Mimicry (2026-07-15, "vehicle mimicry redesign" --
	 * see NOTES.md): the CRC of the given player's own currently
	 * spawned/summoned vehicle, or 0 if they have none out right now.
	 * Mirrors the exact "currentlySpawned" scan
	 * VehicleControlDeviceImplementation::generateObject() already does,
	 * just returning the template CRC instead of a count. Duplicated (not
	 * shared) into CompanionTaxiWaypointSuiCallback.h per this project's
	 * per-file-copy convention for these lookups.
	 */
	unsigned int resolveSpawnedVehicleTemplateCRC(CreatureObject* player) const {
		if (player == nullptr) {
			return 0;
		}

		ManagedReference<SceneObject*> datapad = player->getSlottedObject("datapad");

		if (datapad == nullptr) {
			return 0;
		}

		for (int i = 0; i < datapad->getContainerObjectsSize(); ++i) {
			ManagedReference<SceneObject*> obj = datapad->getContainerObject(i);

			if (obj == nullptr || !obj->isVehicleControlDevice()) {
				continue;
			}

			VehicleControlDevice* device = cast<VehicleControlDevice*>(obj.get());
			ManagedReference<SceneObject*> vehicle = device->getControlledObject();

			if (vehicle != nullptr && vehicle->isInQuadTree()) {
				return vehicle->getServerObjectCRC();
			}
		}

		return 0;
	}

	/**
	 * Companion Vehicle Mimicry (2026-07-15, per user request: "if any of
	 * the companions are being attacked, or the user, do not stop
	 * attacking until the group is out of combat"). True if the given
	 * player or any of their currently summoned companions is in combat
	 * right now. Duplicated from
	 * VehicleControlDeviceImplementation.cpp's identical helper into this
	 * file and CompanionTaxiWaypointSuiCallback.h, per this project's
	 * per-file-copy convention.
	 */
	bool isCompanionGroupInCombat(CreatureObject* player) const {
		if (player == nullptr) {
			return false;
		}

		if (player->isInCombat()) {
			return true;
		}

		ManagedReference<SceneObject*> datapad = player->getSlottedObject("datapad");

		if (datapad == nullptr) {
			return false;
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

			if (comp->isInCombat()) {
				return true;
			}
		}

		return false;
	}

/**
 * Companion System (2026-07-27, "Doctor Buff Radial" -- per Fable's
 * design doc claude/design-companion-skill-commands-2026-07-24.md,
 * build-order item 1). True if this companion has learned ANY
 * science_medic_* skill. Same shape as
 * CompanionObjectImplementation.cpp's companionHasRangerTraining() --
 * duplicated here rather than shared, per this file's own established
 * per-file-copy convention for small companion-lookup helpers.
 */
bool companionHasDoctorTraining(CompanionObject* comp) const {
if (comp == nullptr) {
return false;
}

for (int i = 0; i < comp->getLearnedSkillCount(); ++i) {
const String& skill = comp->getLearnedSkill(i);

if (skill.beginsWith("science_medic_")) {
return true;
}
}

return false;
}

/** True if comp has learned the master doctor skill (top buff tier). */
bool companionIsMasterDoctor(CompanionObject* comp) const {
if (comp == nullptr) {
return false;
}

for (int i = 0; i < comp->getLearnedSkillCount(); ++i) {
if (comp->getLearnedSkill(i) == "science_medic_master") {
return true;
}
}

return false;
}

/**
 * All of the player's currently summoned, living companions (same
 * datapad-scan shape as CompanionStayCommand.h's resolveActiveCompanions()
 * -- duplicated here per this file's per-file-copy convention).
 */
void resolveAllActiveCompanions(CreatureObject* player, Vector<ManagedReference<CompanionObject*>>& out) const {
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

/**
 * Companion System (2026-07-27, "Doctor Buff Radial", FIRST PASS --
 * per Fable's design; c3rr/Nick should re-verify tier VALUES once this
 * is live, the shape/plumbing is what's being tested here). Applies a
 * real medical PrivateBuff (health/strength/constitution/action/
 * quickness/stamina) to recipient, modeled directly on
 * CampDeploymentManager::applyDanceBuff()'s proven construct+addBuff
 * pattern (2026-07-20, live). Tier scales master-doctor vs
 * any-other-doctor-skill; NOT yet wired to consume a real EnhancePack
 * item even if the doctor is carrying one -- current fallback is
 * always the free-with-cooldown path (packs-if-carried is a follow-up,
 * see NOTES.md). Per-recipient 5-minute cooldown, keyed by recipient
 * object ID so the squad-wide call doesn't clobber a single cooldown.
 */
void applyDoctorBuff(CreatureObject* doctor, CreatureObject* recipient) const {
if (doctor == nullptr || recipient == nullptr) {
return;
}

String cooldownKey = "companion_doctor_buff_" + String::valueOf(recipient->getObjectID());

if (!recipient->checkCooldownRecovery(cooldownKey)) {
return;
}

recipient->updateCooldownTimer(cooldownKey, 5 * 60 * 1000);

bool masterTier = companionIsMasterDoctor(cast<CompanionObject*>(doctor));

// Provisional tier values (2026-07-27) -- top tier modeled on the
// 2026-07-14 EnhancePack research in NOTES.md (max pack: power 800
// / duration 14200). Non-master tier is a conservative fraction
// pending real per-skill-level confirmation.
int power = masterTier ? 800 : 300;
int duration = masterTier ? 14200 : 3600;

uint32 buffCRC = STRING_HASHCODE("companion_doctor_enhance");

// No outer recipient lock here -- matches CampDeploymentManager::
// applyDanceBuff()'s proven pattern exactly (only the buff object
// itself is locked, right before addBuff()).
PrivateBuff* existing = cast<PrivateBuff*>(recipient->getBuff(buffCRC));

if (existing != nullptr && existing->getAttributeModifierValue(CreatureAttribute::HEALTH) >= power) {
recipient->sendSystemMessage("You are already under the effects of a stronger (or equal) medical buff.");
return;
}

ManagedReference<PrivateBuff*> buff = new PrivateBuff(recipient, buffCRC, duration, BuffType::MEDICAL);

buff->setAttributeModifier(CreatureAttribute::HEALTH, power);
buff->setAttributeModifier(CreatureAttribute::STRENGTH, power);
buff->setAttributeModifier(CreatureAttribute::CONSTITUTION, power);
buff->setAttributeModifier(CreatureAttribute::ACTION, power);
buff->setAttributeModifier(CreatureAttribute::QUICKNESS, power);
buff->setAttributeModifier(CreatureAttribute::STAMINA, power);

Locker buffLocker(buff);
recipient->addBuff(buff);

recipient->sendSystemMessage(doctor->getDisplayedName() + " applies a medical enhancement.");
}

public:
CompanionDialogMenuSuiCallback(ZoneServer* server, CompanionObject* comp)
		: SuiCallback(server) {
		companion = comp;
	}

	void run(CreatureObject* player, SuiBox* suiBox, uint32 eventIndex, Vector<UnicodeString>* args) {
		if (eventIndex == 1 || player == nullptr || args == nullptr || args->size() <= 0) {
			return;
		}

		int menuSelection = Integer::valueOf(args->get(0).toString());

		ManagedReference<CompanionObject*> strongCompanion = companion;

		if (strongCompanion == nullptr) {
			return;
		}

		Locker clocker(strongCompanion, player);

		switch (menuSelection) {
		case 0: // Follow
			strongCompanion->setCompanionState(CompanionObject::FOLLOW);
			strongCompanion->setFollowObject(player);
			strongCompanion->setStandingOrder(CompanionObject::FOLLOW);
			strongCompanion->setEscortTarget(nullptr);
			strongCompanion->setGuardTarget(nullptr);
			break;
		case 1: { // Stay
			// BUG FIX (2026-07-25, live report: "stay does nothing, that
			// companion still follows me") -- this SUI case was a stale,
			// simplified duplicate of CompanionStayCommand.h predating the
			// 2026-07-20 "massive battlefield" pass. It only called
			// setCompanionState()/setFollowObject(nullptr) -- missing
			// setOblivious() (which is what actually detaches the AI's
			// internal follow/target tracking; without it the movement
			// state keeps chasing the owner regardless of companionState)
			// and setStandingOrder() (which is what the post-combat/
			// interruption recovery path reads to decide where to send the
			// companion back to -- leaving it unset meant any combat blip
			// silently reverted the companion to FOLLOW). Now mirrors the
			// real command's full sequence exactly.
			if (strongCompanion->isInCombat()) {
				CombatManager::instance()->attemptPeace(strongCompanion);
			}

			if (strongCompanion->isTaxiActive()) {
				strongCompanion->stopTaxiRide(false);
			}

			Vector3 home = strongCompanion->getWorldPosition();
			strongCompanion->setHomeLocation(home.getX(), home.getZ(), home.getY());

			strongCompanion->setCompanionState(CompanionObject::STAY);
			strongCompanion->setFollowObject(nullptr);
			strongCompanion->setOblivious();
			strongCompanion->setStandingOrder(CompanionObject::STAY);
			strongCompanion->setEscortTarget(nullptr);
			strongCompanion->setGuardTarget(nullptr);
			break;
		}
		case 2: // Patrol
			strongCompanion->setCompanionState(CompanionObject::PATROL);
			break;
		case 3: // Skill Sheet
			CompanionSkillTrainer::instance()->sendSkillSheet(player, strongCompanion);
			break;
		case 4: // Train
			CompanionSkillTrainer::instance()->sendTrainList(player, strongCompanion);
			break;
		case 5: // Untrain
			CompanionSkillTrainer::instance()->sendUntrainList(player, strongCompanion);
			break;
		case 6: // Contextual Help
			CompanionSkillTrainer::instance()->sendHelpSheet(player, strongCompanion);
			break;
		case 7: // Stats Sheet (Companion System, "stats window" pass)
			CompanionSkillTrainer::instance()->sendStatsSheet(player, strongCompanion);
			break;
		case 8: { // Rename Companion (Companion System, "custom companion name" pass)
			ManagedReference<PlayerObject*> ghost = player->getPlayerObject();

			if (ghost == nullptr) {
				break;
			}

			ghost->closeSuiWindowType(SuiWindowType::COMPANION_RENAME);

			ManagedReference<SuiInputBox*> inputBox = new SuiInputBox(player, SuiWindowType::COMPANION_RENAME, 0x00);
			inputBox->setPromptTitle(strongCompanion->getDisplayedName() + " -=COMPANION=- : Rename");
			inputBox->setPromptText("@companion:rename_prompt"); // Enter a new name for your companion.
			inputBox->setMaxInputSize(40);
			inputBox->setDefaultInput(strongCompanion->getDisplayedName());
			inputBox->setCallback(new CompanionRenameSuiCallback(player->getZoneServer(), strongCompanion));

			ghost->addSuiBox(inputBox);
			player->sendMessage(inputBox->generateMessage());

			break;
		}
		case 9: { // Companion Taxi (2026-07-15, REDESIGNED per direct user
			// request -- see CompanionObjectImplementation::startTaxiRide()
			// and NOTES.md's "vehicle mimicry redesign" entry). The owner
			// must call out their OWN real vehicle first (matches the
			// spec: "ask the owner to spawn their vehicle of choice") --
			// the companion's cosmetic ride then matches that same
			// vehicle template instead of always being an x31.
			// Destination is chosen from a live SUI list of the owner's
			// own waypoints on this planet: the server has no way to
			// force-open the native client Datapad Waypoints window
			// pictured in the reference screenshot, only a custom SUI
			// dialog like this one -- the closest achievable equivalent
			// to "select the waypoint to goto" short of that literal
			// native window.
			ManagedReference<PlayerObject*> ghost = player->getPlayerObject();
			Zone* zone = player->getZone();

			if (ghost == nullptr || zone == nullptr) {
				break;
			}

			if (strongCompanion->getZone() != zone) {
				player->sendSystemMessage("Your companion must be on this planet to drive you somewhere.");
				break;
			}

			if (isCompanionGroupInCombat(player)) {
				player->sendSystemMessage("You can't call a taxi while you or your companions are in combat.");
				break;
			}

			unsigned int vehicleTemplateCRC = resolveSpawnedVehicleTemplateCRC(player);

			if (vehicleTemplateCRC == 0) {
				player->sendSystemMessage("Call out your own vehicle first, then ask your companion for a ride.");
				break;
			}

			// Build the list of the owner's own waypoints on this planet
			// (not just the first ACTIVE one -- a real pick list, closer
			// to the pictured native panel).
			uint32 planetCRC = zone->getZoneCRC();
			Vector<float> waypointX;
			Vector<float> waypointY;
			Vector<String> waypointLabels;

			for (int i = 0; i < ghost->getWaypointListSize(); ++i) {
				WaypointObject* waypoint = ghost->getWaypoint(i);

				if (waypoint == nullptr || waypoint->getPlanetCRC() != planetCRC) {
					continue;
				}

				waypointX.add(waypoint->getPositionX());
				waypointY.add(waypoint->getPositionY());
				waypointLabels.add(waypoint->getDisplayedName());
			}

			if (waypointX.size() == 0) {
				player->sendSystemMessage("You have no waypoints on this planet yet -- create one from your datapad first.");
				break;
			}

			ghost->closeSuiWindowType(SuiWindowType::COMPANION_TAXI_WAYPOINT);

			ManagedReference<SuiListBox*> taxiSui = new SuiListBox(player, SuiWindowType::COMPANION_TAXI_WAYPOINT);
			taxiSui->setPromptTitle(strongCompanion->getDisplayedName() + " -=COMPANION=- : Taxi");
			taxiSui->setPromptText("Choose a waypoint on this planet to be driven to:");
			taxiSui->setCancelButton(true, "@ui:cancel");
			taxiSui->setOkButton(true, "@ui:ok");
			taxiSui->setCallback(new CompanionTaxiWaypointSuiCallback(player->getZoneServer(), strongCompanion, waypointX, waypointY, waypointLabels, vehicleTemplateCRC));

			for (int i = 0; i < waypointLabels.size(); ++i) {
				taxiSui->addMenuItem(waypointLabels.get(i));
			}

			ghost->addSuiBox(taxiSui);
			player->sendMessage(taxiSui->generateMessage());

			break;
		}
		case 10: { // Camp: Set Up Tent ("wild camp & buff" phase 1,
			// 2026-07-18 -- see CampDeploymentManager.cpp's rewrite doc
			// comment and NOTES.md). All gating (ranger/scout training,
			// kit-vs-craft fallback, placement checks) lives in the
			// manager; this is just the dispatch.
			Locker clocker(strongCompanion, player);

			CampDeploymentManager::instance()->deployCamp(player, strongCompanion);
			break;
		}
		case 11: { // Camp: Pack Up Tent (same pass).
			Locker clocker(strongCompanion, player);

			CampDeploymentManager::instance()->packUpCamp(player, strongCompanion);
			break;
		}
		case 12: { // Fun: Fireworks Show (2026-07-19 -- see
			// CompanionFireworksShow.h: real FireworkObject items from the
			// companion's bag, sibling delivery if it has none).

			// OVERLAPPING_THEATER_SHOWS_FIX_2026_07_30 -- don't let fireworks launch while
			// a multi-companion Theater Mode show (e.g. The Landing) is
			// already using this owner's roster.
			if (!player->checkCooldownRecovery("companion_theater_mode_busy")) {
				player->sendSystemMessage("A theater show is already in progress -- try again once it's finished.");
				break;
			}

			Locker clocker(strongCompanion, player);

			CompanionFireworksShow::start(player, strongCompanion);
			break;
		}
		case 13: { // Skill Tree (colored) (2026-07-20 -- see sendSkillTree).
			CompanionSkillTrainer::instance()->sendSkillTree(player, strongCompanion);
			break;
		}
		case 14: { // Theater Mode (2026-07-30, per Nick: "add a section here
			// called Theater Mode and put our plays in there") -- opens a
			// dedicated submenu listing the available scripted companion
			// shows. Currently just "The Landing"; append future shows here
			// as new menu items + new CompanionTheaterModeSuiCallback.h case
			// labels, without disturbing this dialog's own numbering.
			ManagedReference<PlayerObject*> theaterGhost = player->getPlayerObject();

			if (theaterGhost == nullptr) {
				break;
			}

			theaterGhost->closeSuiWindowType(SuiWindowType::COMPANION_THEATER_MODE_LIST);

			ManagedReference<SuiListBox*> theaterSui = new SuiListBox(player, SuiWindowType::COMPANION_THEATER_MODE_LIST);
			theaterSui->setPromptTitle(strongCompanion->getDisplayedName() + " -=COMPANION=- : Theater Mode");
			theaterSui->setPromptText("Choose a show for your companions to perform:");
			theaterSui->setCancelButton(true, "@ui:cancel");
			theaterSui->setOkButton(true, "@ui:ok");
			theaterSui->setCallback(new CompanionTheaterModeSuiCallback(player->getZoneServer(), strongCompanion));

			theaterSui->addMenuItem("The Landing (2 min -- prop-heavy skirmish reveal)"); // 0
			theaterSui->addMenuItem("Fireworks Show (about 90s, single companion)"); // 1 (2026-07-30 addition, THEATER_MENU_TEXT_HOTFIX_2026_07_31)
			theaterSui->addMenuItem("Birthday Show (about 1 min -- cake, dance, banner)"); // 2 (BIRTHDAY_SHOW_2026_07_30, THEATER_MENU_TEXT_HOTFIX_2026_07_31)
			theaterSui->addMenuItem("Battle Theater (about 2.5 min -- 180 NPCs, 6 factions)"); // 3 (BATTLE_THEATER_2026_07_30, THEATER_MENU_TEXT_HOTFIX_2026_07_31)

			theaterGhost->addSuiBox(theaterSui);
			player->sendMessage(theaterSui->generateMessage());

			break;
		}
		case 15: { // Medical: Buff Me (Doctor Buff Radial, 2026-07-27, first pass) (shifted from 14 by the 2026-07-30 Theater Mode insertion)
			if (!companionHasDoctorTraining(strongCompanion)) {
				break;
			}

			applyDoctorBuff(strongCompanion, player);
			break;
		}
		case 16: { // Medical: Buff The Squad (same pass) (shifted from 15)
			if (!companionHasDoctorTraining(strongCompanion)) {
				break;
			}

			applyDoctorBuff(strongCompanion, player);

			Vector<ManagedReference<CompanionObject*>> squad;
			resolveAllActiveCompanions(player, squad);

			for (int i = 0; i < squad.size(); ++i) {
				CompanionObject* member = squad.get(i);

				if (member == strongCompanion) {
					continue;
				}

				applyDoctorBuff(strongCompanion, member);
			}

			break;
		}
		default:
			break;
		}
	}

};

#endif // COMPANIONDIALOGMENUSUICALLBACK_H_
