/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System -- see docs/companion_system/NOTES.md.
*/

#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include <cmath> // 2026-07-20, "taxi greeting" pass -- std::sin/std::cos for the greet-point heading math in startTaxiRide()
#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/managers/player/PlayerManager.h"
#include "server/zone/managers/objectcontroller/ObjectController.h"
#include "server/zone/ZoneServer.h"
#include "server/zone/objects/scene/SceneObject.h"
#include "server/zone/objects/scene/SceneObjectType.h" // 2026-08-10, seat-search: SceneObjectType::FURNITURE
#include "server/zone/objects/scene/TransferErrorCode.h"
#include "server/zone/objects/tangible/TangibleObject.h"
#include "server/zone/objects/tangible/weapon/WeaponObject.h"
#include "server/zone/objects/intangible/VehicleControlDevice.h"
#include "server/zone/objects/creature/ai/Creature.h"
#include "server/zone/objects/resource/ResourceSpawn.h"
#include "server/zone/managers/resource/ResourceManager.h"
#include "server/zone/managers/player/PlayerManager.h"
#include "server/zone/objects/creature/events/DespawnCreatureTask.h"
#include "server/chat/ChatManager.h"
#include "server/chat/StringIdChatParameter.h"
#include "server/zone/managers/companion/callbacks/CompanionHarvestChoiceSuiCallback.h"
#include "server/zone/managers/companion/callbacks/CompanionTaxiGoSuiCallback.h"
#include "server/zone/objects/player/sui/messagebox/SuiMessageBox.h"
#include "server/zone/objects/creature/commands/CombatQueueCommand.h"
#include "server/zone/managers/combat/CombatManager.h"
#include "server/zone/objects/creature/ai/variables/CreatureAttackMap.h"
#include "server/zone/objects/creature/ai/PatrolPoint.h"
#include "server/zone/managers/companion/CompanionSkillTrainer.h"
#include "server/zone/managers/companion/CompanionGearExchangeManager.h"
#include "server/zone/managers/companion/CampDeploymentManager.h" // 2026-08-11, cantina ambiance: changeIntoCampClothes()/restoreArmorFromCamp()
#include "server/zone/objects/cell/CellObject.h"
#include "server/zone/Zone.h"
// COMPANION_TAXI_CHAIN_2026_08_07 -- getWaypointListSize()/getWaypoint() and
// the mission_bag scan below need these two; see collectOwnerPlanetWaypointIds().
#include "server/zone/objects/waypoint/WaypointObject.h"
#include "server/zone/objects/mission/MissionObject.h"
#include "templates/params/creature/CreatureAttribute.h"
#include "templates/params/creature/CreatureState.h"
// genesis port: no PlayerArrangement enum on this base; MountCommand.h:89
// passes the raw arrangement index 4 for a rider, so RIDER == 4 here.
#include "templates/params/OptionBitmask.h"
#include "templates/manager/TemplateManager.h"
#include "templates/creature/SharedCreatureObjectTemplate.h"
#include "server/zone/objects/group/GroupObject.h"
#include "server/zone/managers/skill/SkillManager.h"
#include "server/zone/managers/skill/SkillModManager.h" // COMPANION_SKILLMOD_GRANT_HOTFIX_2026_07_31
#include "server/zone/objects/creature/variables/Skill.h"
#include "server/zone/managers/crafting/schematicmap/SchematicMap.h"
#include "server/zone/managers/crafting/schematicmap/DraftSchematicGroup.h"
#include "server/zone/objects/draftschematic/DraftSchematic.h"
#include "server/zone/managers/companion/CompanionCraftingManager.h"
#include "server/zone/managers/companion/CompanionCraftingRangeIndicator.h"
#include "server/zone/objects/tangible/pharmaceutical/PharmaceuticalObject.h"
#include "server/zone/objects/tangible/pharmaceutical/StimPack.h"
#include "server/zone/objects/factorycrate/FactoryCrate.h"

// Companion Personality/Growth/Flee/Self-buff/Idle-emote patch (2026-07-30).
#include "server/zone/managers/companion/CompanionChatter.h"
#include "server/zone/managers/companion/CompanionCraftTheater.h"
#include "server/zone/objects/tangible/consumable/Consumable.h"
#include "templates/params/creature/CreaturePosture.h"

// Per-xp-type cap applied to companion-local experience, mirrors the general
// shape of player combat xp caps without touching PlayerObject's xp system at
// all (see NOTES.md, "Experience & isolation").
#define COMPANION_MAX_XP_PER_TYPE 5000000

// Companion Taxi / Vehicle Mimicry (2026-07-15 -- see CompanionObject.idl's
// taxiVehicle doc comment and NOTES.md's cosmetic-taxi research entries):
// tuning constants. TAXI_SPEED matches the real x31 landspeeder's run speed
// (object/mobile/vehicle/objects.lua, speed {11, 6}); confirmed no
// speed-hack check applies to AI-driven movement. Arrival radius is 8m
// (squared for the cheap distance compare).
// Speed raised 11.0 -> 14.0 (2026-07-15 live feedback: at exactly x31 pace
// the companion fell behind the owner's own x31 due to pathing overhead --
// 14 keeps it a touch AHEAD so the owner can comfortably /follow).
//
// REDESIGN (2026-07-15, "vehicle mimicry redesign" -- see NOTES.md): the
// intervening "real mount" experiment (mounting the companion onto a
// static vehicle shell via MOUNTEDCREATURE/RIDER) is REVERTED here -- live
// testing confirmed it caused the companion to teleport instead of
// driving (a mounted creature isn't normally locomoted by AI patrol code,
// so the two fought each other). Back to the original, previously-working
// pure cosmetic position-mirror, with the tick tightened 500ms -> 200ms
// for noticeably smoother visual tracking. The vehicle template is no
// longer hardcoded -- COMPANION_TAXI_VEHICLE_TEMPLATE is now only the
// fallback used when no explicit template CRC is supplied.
//
// SECOND REDESIGN (2026-07-16, "the vehicle drives, the companion rides" --
// user-picked option after the cosmetic mirror kept looking detached): the
// two earlier attempts each solved half the problem. The real mount glued
// rider and vehicle perfectly (native CREO RIDER-slot containment, client
// renders child-on-parent with zero server ticks) but broke locomotion
// because the MOUNTED COMPANION was still the AI mover; the cosmetic
// mirror moved correctly (the companion's own proven AI pathing) but could
// never look truly attached (discrete 200ms server position writes vs. the
// client's locally-interpolated companion movement). This pass flips the
// roles so each half lands on its proven side: a NEW, non-persistent
// "driver" agent -- created from the companion's own battle-tested
// object/mobile/companion_actor.iff template (CompanionObject class, the
// "companion" AI map, the leash override, everything already live-proven
// for follow/patrol movement) with its clientObjectCRC re-stamped to the
// mimicked vehicle's client template (the exact re-stamp trick the
// companion itself uses for shared_human_male) -- does ALL the moving via
// its own AI (PATROL to the destination, or FOLLOW the owner in escort
// mode), while the actual companion is mounted ONTO it as a real RIDER
// child (MOUNTEDCREATURE/RIDER/RIDINGMOUNT, the glue half that already
// worked). The companion is parked in STAY/oblivious for the ride so its
// own AI never fights the parent's movement -- the exact conflict that
// caused the original teleport bug. updateTaxiTick() no longer mirrors
// positions at all: it only watches for arrival, combat (dismount and
// fight -- combat always takes priority, per spec), the lead/catch-up
// speed throttle (now applied to the DRIVER's speeds), and driver loss.
#define COMPANION_TAXI_SPEED 14.0f
#define COMPANION_TAXI_ARRIVAL_RADIUS_SQ 64.0f
#define COMPANION_TAXI_TICK_MS 200
#define COMPANION_TAXI_VEHICLE_TEMPLATE "object/mobile/vehicle/landspeeder_x31.iff"

// Taxi pacing rules (2026-07-16 revision, per user request -- replaces the
// earlier 15%/70m/25m slow-down throttle):
// - Base pace: 5% faster than the mimicked vehicle's own real speed, so
//   the taxi always stays a touch AHEAD of the owner without running away.
// - Hard 85m leash: if the taxi gets more than 85m ahead of the owner it
//   STOPS COMPLETELY and waits; it only resumes driving once the owner has
//   closed back to within 35m (taxiThrottled remembers the paused state).
// - Catch-up boost: if the OWNER somehow gets ahead of the taxi (closer to
//   the destination than it is) and the gap exceeds 35m, the taxi speeds
//   up 20% over its base pace until it has regained the lead -- "the
//   companion should always keep up with the user."
// - Departure: the driver holds at the pickup spot until BOTH the 5-second
//   click-and-follow window has passed AND the owner is within 35m.
#define COMPANION_TAXI_SPEED_MULTIPLIER 1.02f   // COMPANION_TAXI_PACING_2026_08_04: owner's speed +2%
#define COMPANION_TAXI_LEASH_DISTANCE_SQ 8100.0f // 90m, squared
// How hard to throttle once past the leash. NOT a full stop: the old
// behaviour called setOblivious(), and MovePetBase refuses to move at all
// while OBLIVIOUS, so recovery depended on the resume branch firing --
// the same fragile shape behind several bugs found on 2026-08-04.
#define COMPANION_TAXI_THROTTLED_MULTIPLIER 0.30f
#define COMPANION_TAXI_RESUME_DISTANCE_SQ 2500.0f // 50m, squared
#define COMPANION_TAXI_CATCHUP_MULTIPLIER 1.2f
// Companion System (2026-08-07, per user request "the companion is
// falling too far behind while i drive, have them speed up to always be
// within 20 meters"): deliberately a SEPARATE constant from
// COMPANION_TAXI_RESUME_DISTANCE_SQ above -- that one is shared by 3
// unrelated checks (the post-arrival pickup-watch proximity test, the
// departure-hold "is the owner close enough to depart" gate, and the
// 85m-leash throttle-resume check), so retargeting it to 20m would have
// tightened all four behaviors at once instead of just the live driving
// catch-up pace below.
#define COMPANION_TAXI_CATCHUP_TRIGGER_DISTANCE_SQ 400.0f // 20m, squared

// Companion System (2026-08-10, per Nick: "my companion goes through
// walls" when catching up at speed after taking a taxi into a city, plus
// his follow-up "lets do [the tighter city leash] as well"). Both the taxi
// driver's own leash/catch-up pacing AND the plain-FOLLOW keep-up boost
// (see runKeepUpTick() below) now check isNearDenseBuildings() every tick
// and swap to these tighter numbers only while near real building
// clusters -- open-terrain pacing is completely untouched, since that's
// not where the clipping was reported. Chosen deliberately smaller than
// the existing 90m/50m taxi leash pair and the keep-up's 1.8x/25m/10m
// triple, not replacing them outright, so a normal empty-terrain taxi
// ride or open-desert follow keeps its existing proven pace.
#define COMPANION_TAXI_CITY_LEASH_DISTANCE_SQ 900.0f // 30m, squared -- trips the crawl-and-wait sooner near buildings
#define COMPANION_TAXI_CITY_RESUME_DISTANCE_SQ 225.0f // 15m, squared
#define COMPANION_TAXI_CITY_CATCHUP_MULTIPLIER 1.05f // was COMPANION_TAXI_CATCHUP_MULTIPLIER's 1.2x
#define COMPANION_KEEPUP_CITY_BOOST_MULTIPLIER 1.15f // was the plain-FOLLOW keep-up's flat 1.8x
#define COMPANION_KEEPUP_CITY_LEASH_DISTANCE_SQ 900.0f // 30m, squared -- beyond this near buildings, CRAWL (don't rush) and wait
#define COMPANION_KEEPUP_CITY_RESUME_DISTANCE_SQ 225.0f // 15m, squared
#define COMPANION_KEEPUP_CITY_CRAWL_MULTIPLIER 0.6f // slow, deliberate pace instead of a boosted rush through tight geometry

namespace {

	// Forward declaration -- isNearDenseBuildings() is DEFINED much further
	// down (2026-08-10, alongside the seat-search helpers), but
	// updateTaxiTick() below needs to call it and is defined earlier in
	// this file than that. All `namespace { ... }` blocks in this file are
	// the same translation-unit-local unnamed namespace, so this
	// declaration and that later definition refer to the same function --
	// same fix shape as any free-function forward-use case, just noted
	// explicitly since this file's helpers are conventionally NOT
	// forward-declared (see the "NOT CompanionObjectImplementation::
	// members" comment on the later helper block for why they're free
	// functions in the first place).
	bool isNearDenseBuildings(Zone* zone, float x, float y);

	// Reschedules the taxi mirror/arrival tick. Free function (not an idl
	// method) so the generated header doesn't need a private-helper
	// declaration; takes a strong reference so the lambda keeps the
	// companion alive across the delay.
	void scheduleCompanionTaxiTick(CompanionObject* companion) {
		if (companion == nullptr) {
			return;
		}

		Reference<CompanionObject*> companionRef = companion;

		Core::getTaskManager()->scheduleTask([companionRef] () {
			CompanionObject* companion = companionRef.get();

			if (companion == nullptr) {
				return;
			}

			Locker locker(companion);

			companion->updateTaxiTick();
		}, "CompanionTaxiTickLambda", COMPANION_TAXI_TICK_MS);
	}

	// COMPANION_TAXI_ARRIVAL_WAIT_2026_08_05 -- watches a taxi-parked companion for the owner
	// catching up, then resumes following on its own so the player never has
	// to remember to press Follow. Same idiom as scheduleCompanionTaxiTick
	// just above. Bails out at the first sign anything more specific already
	// happened -- a manual order, a new ride, the companion or owner gone --
	// so it can never fight a more recent action.
#define COMPANION_TAXI_PICKUP_WATCH_MS 2000
#define COMPANION_TAXI_PICKUP_WATCH_MAX_TICKS 300 // 300 * 2s = 10 minutes

	void scheduleTaxiPickupWatch(CompanionObject* companion, int elapsed = 0) {
		if (companion == nullptr) {
			return;
		}

		Reference<CompanionObject*> companionRef = companion;

		Core::getTaskManager()->scheduleTask([companionRef, elapsed] () {
			CompanionObject* companion = companionRef.get();

			if (companion == nullptr) {
				return;
			}

			Locker locker(companion);

			if (companion->isDead() || companion->getZone() == nullptr) {
				return;
			}

			// Something else already handled this companion since arrival --
			// a manual /follow, a new order, a fresh taxi ride. Don't
			// second-guess it.
			if (companion->getStandingOrder() != CompanionObject::STAY || companion->isTaxiActive()) {
				return;
			}

			CreatureObject* owner = companion->getLinkedCreature().get();

			if (owner == nullptr || owner->getZone() == nullptr) {
				return;
			}

			float dx = owner->getPositionX() - companion->getPositionX();
			float dy = owner->getPositionY() - companion->getPositionY();

			if ((dx * dx + dy * dy) <= COMPANION_TAXI_RESUME_DISTANCE_SQ) {
				companion->setStandingOrder(CompanionObject::FOLLOW);
				companion->setCompanionState(CompanionObject::FOLLOW);
				companion->setFollowObject(owner);
				companion->setFollowState(AiAgent::FOLLOWING);
				companion->activateMovementEvent();
				return;
			}

			if (elapsed >= COMPANION_TAXI_PICKUP_WATCH_MAX_TICKS) {
				// Owner never showed up -- stop polling forever rather than
				// ticking indefinitely. The companion stays exactly where the
				// ride left it; /follow still works normally at any time.
				return;
			}

			scheduleTaxiPickupWatch(companion, elapsed + 1);
		}, "CompanionTaxiPickupWatchLambda", COMPANION_TAXI_PICKUP_WATCH_MS);
	}

}

// Companion System (2026-07-15, "companion stops following / leashes back
// home" fix -- see the matching CompanionObject.idl doc comment,
// ai/companion.lua and NOTES.md): customAiMap is transient, so re-assign
// the dedicated companion AI map on every load BEFORE AiAgent's own
// initializeTransientMembers() assembles the behavior trees via
// setAITemplate().
void CompanionObjectImplementation::initializeTransientMembers() {
	// genesis port: DEFERRED -- the customAiMap field does not exist on genesis's
	// AiAgent and adding it would require an AiAgent.idl change (out of scope), so
	// the per-load re-assignment of the dedicated "companion" AI map is dropped.
	// Part of the already-known behaviour-tree gap: genesis uses Lua behaviour trees
	// and the native leaf classes the Companion System expects do not exist here.
	// customAiMap = STRING_HASHCODE("companion");

	// Companion Taxi (2026-07-15): a ride never survives a despawn/reload --
	// defensive reset alongside the constructor's own initialization.
	taxiActive = false;
	taxiHasDestination = false;
	taxiThrottled = false;
	taxiVehicle = nullptr;
	taxiOwnerCarriage = nullptr;
	taxiDepartureTime = 0;
	taxiOwnerWasMounted = false;
	lootSweepActive = false;
	keepUpMonitorActive = false;
	keepUpBoosted = false;
	keepUpBaseRunSpeed = 0;
	keepUpBaseWalkSpeed = 0;
	taxiSavedRunSpeed = 0;
	taxiSavedWalkSpeed = 0;
	taxiBoostedRunSpeed = 0;
	taxiBoostedWalkSpeed = 0;

	AiAgentImplementation::initializeTransientMembers();
}

void CompanionObjectImplementation::setVitality(int value) {
	if (value < 0) {
		value = 0;
	}

	if (value > maxVitality) {
		value = maxVitality;
	}

	vitality = value;

	// Persistence fix (2026-07-13, "companion state has no structural save
	// guarantee" -- see NOTES.md): a hand-written native setter does NOT get
	// automatic dirty-tracking the way a plain idlc-generated setter does --
	// zero call sites anywhere in objects/companion/ ever marked a companion
	// dirty before this pass, so vitality/skills/XP/state changes could be
	// silently lost if nothing else happened to touch the object again
	// before the next save sweep/shutdown. updateToDatabase() itself is
	// confirmed vestigial (empty body in every class in this codebase, not
	// just this one) -- zoneServer->updateObjectToDatabase() is the real,
	// traced-to-_setUpdated(true) call.
	ZoneServer* zoneServer = getZoneServer();

	if (zoneServer != nullptr) {
		zoneServer->updateObjectToDatabase(_this.getReferenceUnsafeStaticCast());
	}
}

void CompanionObjectImplementation::healVitality(int amount) {
	if (amount <= 0) {
		return;
	}

	int newVitality = vitality + amount;

	if (newVitality > maxVitality) {
		newVitality = maxVitality;
	}

	vitality = newVitality;

	ZoneServer* zoneServer = getZoneServer();

	if (zoneServer != nullptr) {
		zoneServer->updateObjectToDatabase(_this.getReferenceUnsafeStaticCast());
	}

	// Keep the persisted copy on the control device in sync so a store/restore
	// cycle (or a server crash) never loses the heal.
	ManagedReference<CompanionControlDevice*> device = companionControlDevice.get();

	if (device != nullptr) {
		Locker clocker(device, _this.getReferenceUnsafeStaticCast());

		// device->setVitality() marks itself dirty internally (see
		// CompanionControlDeviceImplementation.cpp) -- no separate dirty-mark
		// call needed here.
		device->setVitality(vitality);
	}
}

void CompanionObjectImplementation::grantSkill(const String& skillName) {
	if (skillName.isEmpty() || learnedSkills.contains(skillName)) {
		return;
	}

	learnedSkills.add(skillName);

	// COMPANION_SKILLMOD_GRANT_HOTFIX_2026_07_31 -- apply this skill's real skill modifiers to the
	// companion itself (e.g. "healing_ability"), mirroring SkillManager::
	// awardSkill()'s own player-side logic. Without this, grantSkill() only
	// ever recorded the skill's NAME -- getSkillMod() stayed permanently 0
	// for every companion regardless of what it had "learned", silently
	// failing any gate that checks it (confirmed live: a fully
	// Master-Medic-trained companion crafted every Wound Pack correctly
	// but could never actually use one to heal). notifyClient is false --
	// companions have no client session of their own to push a delta to.
	Skill* grantedSkillMods = SkillManager::instance()->getSkill(skillName);

	if (grantedSkillMods != nullptr) {
		const VectorMap<String, int>* skillModifiers = grantedSkillMods->getSkillModifiers();

		for (int i = 0; i < skillModifiers->size(); ++i) {
			const auto& entry = skillModifiers->elementAt(i);
			addSkillMod(SkillModManager::SKILLBOX, entry.getKey(), entry.getValue(), false);
		}
	}

	CompanionGearExchangeManager::offerWeaponAfterSkillGrant(getLinkedCreature().get(), _this.getReferenceUnsafeStaticCast(), skillName);

	recalculateCombatLevel();

	// Persistence fix (see setVitality() above for the full rationale).
	ZoneServer* zoneServer = getZoneServer();

	if (zoneServer != nullptr) {
		zoneServer->updateObjectToDatabase(_this.getReferenceUnsafeStaticCast());
	}

	info(true) << "Companion " << getObjectID() << " granted skill '" << skillName << "' (0 SP, isolated ledger)";
}

void CompanionObjectImplementation::removeSkill(const String& skillName) {
	if (!learnedSkills.removeElement(skillName)) {
		return;
	}

	// COMPANION_SKILLMOD_GRANT_HOTFIX_2026_07_31 -- mirror image of the grantSkill() fix above:
	// remove this skill's real skill modifiers from the companion too, so
	// untrain/retrain cycles (and multi-skill overlap) stay consistent
	// with what a real player's own surrenderSkill() does.
	Skill* revokedSkillMods = SkillManager::instance()->getSkill(skillName);

	if (revokedSkillMods != nullptr) {
		const VectorMap<String, int>* skillModifiers = revokedSkillMods->getSkillModifiers();

		for (int i = 0; i < skillModifiers->size(); ++i) {
			const auto& entry = skillModifiers->elementAt(i);
			removeSkillMod(SkillModManager::SKILLBOX, entry.getKey(), entry.getValue(), false);
		}
	}

	recalculateCombatLevel();

	// Persistence fix (see setVitality() above for the full rationale).
	ZoneServer* zoneServer = getZoneServer();

	if (zoneServer != nullptr) {
		zoneServer->updateObjectToDatabase(_this.getReferenceUnsafeStaticCast());
	}
}

void CompanionObjectImplementation::recalculateCombatLevel() {
	// Companions have no XP-driven "player level" table of their own; combat
	// level is derived purely from how many combat skill boxes have been
	// trained, mirroring the way a player's displayed combat level is a
	// function of their learned skill boxes rather than raw xp.
	//
	// NOTE: this is a simplified heuristic (skill count only). A
	// production-quality implementation should weight skill boxes by their
	// column position within their tree (novice/1/2/3/4/master) the same way
	// the client-side skills.iff "combatLevel" column does; that requires
	// resolving each learned skill string against SkillManager's loaded Skill
	// tree (SkillManager::instance()->getSkill(name)) to read a per-skill
	// level contribution. Left as a TODO -- see NOTES.md.
	int level = 0;

	for (int i = 0; i < learnedSkills.size(); ++i) {
		const String& skill = learnedSkills.get(i);

		// Master-tier boxes are worth more than intermediate tiers.
		if (skill.endsWith("_master")) {
			level += 4;
		} else if (skill.beginsWith("jedi_")) {
			level += 3;
		} else {
			level += 1;
		}
	}

	combatLevel = level;

	// Bug fix (first in-game test): the companion actor is created via a
	// raw createObject() call rather than the normal CreatureTemplate-driven
	// mob-spawn path (CreatureManagerImplementation::spawnCreature() /
	// AiAgentImplementation::loadTemplateData(CreatureTemplate*)), which is
	// the ONLY place the engine's own CreatureObject "level" field normally
	// gets populated. Left unset, it stays at its raw constructor default
	// of 0 -- and a level-0 creature is what caused the client's Examine
	// window to show "Combat Difficulty: ... looks like instant death" (its
	// most extreme danger-tier fallback) instead of a sane novice rating.
	// setLevel(..., false) is safe to call here: AiAgentImplementation::
	// setLevel() only randomizes HAM when `npcTemplate` is non-null, and
	// companions never have one (same reason -- no CreatureTemplate spawn
	// path), so this call only ever updates the visible level number, never
	// touches the HAM baseline migrateBaselineStats() already set. Floor of
	// 1 because setLevel() silently no-ops for lvl <= 0.
	setLevel(level > 0 ? level : 1, false);

	// Persistence fix (see setVitality() above for the full rationale).
	// Defense in depth: both current callers (grantSkill()/removeSkill())
	// already mark dirty themselves, but this is a public native method a
	// future caller could invoke directly -- an extra dirty-mark call here
	// is harmless (idempotent), not fixing something already broken twice.
	ZoneServer* zoneServer = getZoneServer();

	if (zoneServer != nullptr) {
		zoneServer->updateObjectToDatabase(_this.getReferenceUnsafeStaticCast());
	}
}

void CompanionObjectImplementation::setCompanionState(int state) {
	companionState = state;

	// Persistence fix (see setVitality() above for the full rationale).
	ZoneServer* zoneServer = getZoneServer();

	if (zoneServer != nullptr) {
		zoneServer->updateObjectToDatabase(_this.getReferenceUnsafeStaticCast());
	}
}

// Auto Skill-Training Walkup (AUTO_SKILL_TRAIN_WALKUP_2026_07_30) -- forward declarations.
// Full definitions live further down in this file's existing
// "Companion Personality/Flee/Self-buff/Idle-emote patch" anonymous
// namespace, next to runFleeCheckTick()/runSelfBuffTick()/etc. --
// reused here because addExperience() (the correct, event-driven
// trigger point: it's the only place experiencePools can ever
// change) is defined much earlier in this file.
namespace {
	bool isCompanionBusyForTraining(CompanionObject* companion);
	bool findReadyUntrainedSkill(CompanionObject* companion, CreatureObject* owner, String& outSkillName);
	bool tryInitiateSkillTrainWalkup(CompanionObject* companion, CreatureObject* owner);

	// Build fix (2026-07-30, build-fix-2) -- TRAINING_WALKUP_TIMEOUT_MS
	// (marker: TRAINING_WALKUP_TIMEOUT_MS_MOVED_2026_07_30_BUILD_FIX_2)
	// defined HERE, not down in the later anonymous-namespace block near
	// runMedicAutoCareTick(), because an anonymous namespace only makes
	// its members visible for the rest of the translation unit starting
	// right after the specific { } block that declares them -- it is NOT
	// hoisted/retroactive. addExperience() (which uses this constant) is
	// defined right after THIS EARLIER block, so the constant has to live
	// here to be visible at its point of use. See addExperience()'s Auto
	// Skill-Training Walkup block below. Value: 75 seconds, same budget
	// documented where this used to sit.
	extern const uint64 TRAINING_WALKUP_TIMEOUT_MS_MOVED_2026_07_30_BUILD_FIX_2;
	const uint64 TRAINING_WALKUP_TIMEOUT_MS = 75000;

	// Companion System (2026-08-07, live bug report: "the pop up box is
	// popping up a new box every 2 seconds and making it difficult to
	// click"): fireTrainingSuiSend() unconditionally clears
	// trainingReadyUntil once it sends the SUI, so if the same skill is
	// STILL "ready" (the player hasn't acted on the box yet -- which is
	// exactly the point of the complaint), the very next ~2000ms keep-up
	// tick's tryInitiateSkillTrainWalkup() sees trainingReadyUntil == 0
	// again, re-arms it, and the walkup tick resends the SUI, replacing
	// the one the player was just trying to click -- repeating forever
	// until they win the race or the skill gets trained. Fixed via
	// trainSuiLastShownMs() below: once the SUI has actually been shown,
	// suppress re-triggering for this long, so the box stays put until
	// the player closes it (or trains) instead of getting yanked out
	// from under them every tick.
	const uint64 TRAIN_SUI_RESHOW_COOLDOWN_MS = 60000;

	// Companion System (2026-08-07, per user request "their xp needs to cap
	// out like a real character's xp"): the flat COMPANION_MAX_XP_PER_TYPE
	// ceiling (see its #define near the top of this file) was an arbitrary
	// placeholder never tied to real game data. Real players are capped per
	// xpType by SkillManager::updateXpLimits(), which takes the MAX
	// Skill::getXpCap() among the player's own currently-learned skills for
	// that xpType (real data straight from skills.iff), falling back to
	// SkillManager's defaultXpLimits table when nothing they've learned yet
	// defines one. Mirrored here against the companion's OWN learnedSkills
	// list -- companions never touch a real PlayerObject's skill list or
	// updateXpLimits() itself (this file's "skill point isolation"
	// convention) -- so a companion's real ceiling tracks the exact same
	// numbers a player would see for that xpType, recomputed on demand (same
	// cost class as findReadyUntrainedSkill()'s scan below) rather than
	// persisted. Falls back to the old flat constant only if NEITHER the
	// companion's own skills nor SkillManager's defaults define a real cap
	// at all (e.g. this project's own companion-only xp types with no live
	// skills.iff equivalent, such as "companion_master_xp").
	int computeCompanionRealXpCap(CompanionObject* companion, const String& xpType) {
		int cap = 0;

		if (companion != nullptr) {
			for (int i = 0; i < companion->getLearnedSkillCount(); ++i) {
				Skill* learned = SkillManager::instance()->getSkill(companion->getLearnedSkill(i));

				if (learned == nullptr || learned->getXpType() != xpType || learned->getXpCap() == 0) {
					continue;
				}

				if (learned->getXpCap() > cap) {
					cap = learned->getXpCap();
				}
			}
		}

		if (cap == 0) {
			cap = SkillManager::instance()->getDefaultXpLimit(xpType);
		}

		if (cap == 0) {
			cap = COMPANION_MAX_XP_PER_TYPE;
		}

		return cap;
	}
}

int CompanionObjectImplementation::addExperience(const String& xpType, int amount) {
	if (amount == 0 || xpType.isEmpty()) {
		return 0;
	}

	int current = experiencePools.get(xpType);
	int newTotal = current + amount;

	int realXpCap = computeCompanionRealXpCap(_this.getReferenceUnsafeStaticCast(), xpType);

	if (newTotal > realXpCap) {
		newTotal = realXpCap;
	}

	if (newTotal < 0) {
		newTotal = 0;
	}

	int granted = newTotal - current;

	experiencePools.put(xpType, newTotal);

	// Persistence fix (see setVitality() above for the full rationale).
	ZoneServer* zoneServer = getZoneServer();

	if (zoneServer != nullptr) {
		zoneServer->updateObjectToDatabase(_this.getReferenceUnsafeStaticCast());
	}

	// Growth/Level-up check (2026-07-30 patch, spec part 4). Hooked here
	// rather than into a periodic free-function tick because
	// experiencePools has no public enumeration accessor (only
	// getExperience(xpType), a get-by-key lookup) -- a free function
	// outside this class cannot sum it, and this member function is the
	// only place the total can ever change anyway, so this is strictly
	// more correct (event-driven, not polling) as well as buildable.
	int totalForGrowth = 0;

	for (int i = 0; i < experiencePools.size(); ++i) {
		if (experiencePools.elementAt(i).getKey() == "companion_master_xp") {
			continue;
		}

		totalForGrowth += experiencePools.elementAt(i).getValue();
	}

	int targetLevel = 1 + (totalForGrowth / 1000);

	if (targetLevel > 50) {
		targetLevel = 50;
	}

	if (targetLevel > getCompanionLevel()) {
		for (int lvl = getCompanionLevel() + 1; lvl <= targetLevel; ++lvl) {
			if (lvl <= 20) {
				int newMaxVitality = (int) (getMaxVitality() * 1.02 + 0.5);
				setMaxVitality(newMaxVitality);
			}

			setCompanionLevel(lvl);
		}

		// Announce once with the FINAL level reached rather than once per
		// level crossed (spec part 4 leaves this choice open) -- avoids
		// spamming chat with several level-up messages back to back when
		// a single large XP grant (e.g. a crafting batch) crosses more
		// than one 1000-xp threshold at once.
		CreatureObject* growthOwner = getLinkedCreature().get();

		if (growthOwner != nullptr) {
			CompanionChatter::announceLevelUp(_this.getReferenceUnsafeStaticCast(), growthOwner, targetLevel);
		}
	}

	// Auto Skill-Training Walkup (AUTO_SKILL_TRAIN_WALKUP_2026_07_30, spec: walk to owner +
	// auto-open the trainer Skill Tree SUI once a newly-granted XP
	// amount pushes an untrained skill in the companion's current tree
	// to 100%+ of its real cost). Extracted into tryInitiateSkillTrainWalkup()
	// (2026-08-07, per user request "as soon as the user and companions are
	// out of battle, they walk up... right away") so the SAME check also
	// runs from the regular keep-up tick, not just here -- see that
	// function's doc comment for why the event-only trigger could miss a
	// skill that became ready mid-combat. One-shot per trainingReadyUntil
	// (see CompanionObject.idl doc comment) -- do nothing if a walk-over is
	// already pending; runSkillTrainWalkupTick() (called from the 2s
	// keep-up tick) owns the rest of the lifecycle (busy-abandon, arrival,
	// timeout).
	tryInitiateSkillTrainWalkup(_this.getReferenceUnsafeStaticCast(), getLinkedCreature().get());

	return granted;
}

// Companion System (2026-07-19, per user request) -- see CompanionObject.idl's
// doc comment on this override. Skips the entire stock NPC-death pipeline
// (corpse/loot/XP/faction, none of which applies to a companion) and instead
// stores the companion + marks it dead-until-resummoned on its control
// device, so spawnObject() can auto-revive it (weak, regenerating) on the
// next summon instead of it staying gone forever.
int CompanionObjectImplementation::notifyObjectDestructionObservers(TangibleObject* attacker, int condition, bool isCombatAction) {
	ManagedReference<CompanionControlDevice*> device = getCompanionControlDevice();

	if (device != nullptr) {
		Locker dlocker(device, _this.getReferenceUnsafeStaticCast());

		device->handleCompanionDeath(getLinkedCreature().get());
	}

	return 1;
}

// Companion System (2026-07-17, "pet command port" pass) -- friend-list
// accessors for /companionfriend. See CompanionObject.idl's friendIds doc
// comment and CompanionFriendCommand.h.
bool CompanionObjectImplementation::isCompanionFriend(unsigned long long playerID) const {
	for (int i = 0; i < friendIds.size(); ++i) {
		if (friendIds.get(i) == playerID) {
			return true;
		}
	}

	return false;
}

bool CompanionObjectImplementation::toggleCompanionFriend(unsigned long long playerID) {
	for (int i = 0; i < friendIds.size(); ++i) {
		if (friendIds.get(i) == playerID) {
			friendIds.remove(i);
			return false;
		}
	}

	friendIds.add(playerID);
	return true;
}

namespace {

	// Personality System (2026-07-30 patch). personalityType/PERSONALITY_*
	// constants land on CompanionObject via a parallel patch to
	// CompanionObject.idl (0 = unassigned). Kept as free functions in their
	// own anonymous namespace -- same convention every other small helper
	// in this file uses (see medicIsTrained() etc. further down) -- so
	// nothing here requires a new CompanionObject.idl-declared member.
	int getPersonalityInterceptMod(int personalityType) {
		switch (personalityType) {
		case CompanionObject::PERSONALITY_BRAVE:
			return 20;
		case CompanionObject::PERSONALITY_STEADY:
			return 0;
		case CompanionObject::PERSONALITY_CAUTIOUS:
			return -15;
		case CompanionObject::PERSONALITY_RECKLESS:
			return 30;
		case CompanionObject::PERSONALITY_VIGILANT:
			return 10;
		default:
			return 0;
		}
	}

	int getFleeHealthThresholdPercent(int personalityType) {
		switch (personalityType) {
		case CompanionObject::PERSONALITY_BRAVE:
			return 10;
		case CompanionObject::PERSONALITY_STEADY:
			return 20;
		case CompanionObject::PERSONALITY_CAUTIOUS:
			return 35;
		case CompanionObject::PERSONALITY_RECKLESS:
			return 5;
		case CompanionObject::PERSONALITY_VIGILANT:
			return 20;
		default:
			return 20;
		}
	}

}

void CompanionObjectImplementation::interceptThreatToOwner(CreatureObject* attacker) {
	if (attacker == nullptr || isDead() || isIncapacitated()) {
		return;
	}

	// Only auto-intercept out of the three passive states; if we're already
	// attacking something (or STAY/PATROL... note STAY intentionally still
	// intercepts per the spec -- "protect the master" overrides STAY, but a
	// companion mid-PATROL or already fighting should not be yanked off task
	// unless it is currently idle-passive).
	if (companionState == CompanionObject::ATTACK || companionState == CompanionObject::THEATER) {
		return;
	}

	if (!isAttackableBy(attacker)) {
		return;
	}

	// Companion System (2026-07-17, "pet command port" pass) -- players the
	// owner marked via /companionfriend are never auto-intercepted, even
	// when they attack the owner (the duel-with-a-buddy case). Mirrors the
	// real pet friend list's whole purpose (PetFriendCommand.h).
	if (attacker->isPlayerCreature() && isCompanionFriend(attacker->getObjectID())) {
		return;
	}

	// Companion System spec 4A / Vigilance branch tie-in: "raises how
	// readily the companion intercepts threats to its owner" (see
	// companion_master_vigilance_01..04 doc comment in
	// CompanionSkillTrainer.cpp and NOTES.md). Untrained companions still
	// have a real baseline chance to intercept (40 + vigilanceRank*15 below
	// -- 40% at rank 0, meant as a starting safety net rather than
	// something that requires training to work at all), and each learned
	// Vigilance rank makes interception more reliable, capping at a
	// guaranteed intercept by rank 4. Mirrors the linear per-rank step
	// pattern already used by the Resilience branch in
	// CompanionControlDeviceImplementation::handleCompanionDeath().
	CreatureObject* owner = getLinkedCreature().get();
	int vigilanceRank = 0;

	if (owner != nullptr) {
		for (int i = 1; i <= 4; ++i) {
			if (owner->hasSkill("companion_master_vigilance_0" + String::valueOf(i))) {
				vigilanceRank = i;
			}
		}
	}

	int interceptChance = 40 + (vigilanceRank * 15); // 40/55/70/85/100

	// Personality System (2026-07-30 patch) -- BRAVE/RECKLESS intercept
	// more readily, CAUTIOUS holds back, STEADY/VIGILANT are neutral/small;
	// see getPersonalityInterceptMod() above. Clamped the same way the
	// rest of this file clamps a percentage (Math::max already used
	// elsewhere in this file, e.g. the harvest-extraction floor below).
	interceptChance += getPersonalityInterceptMod(getPersonalityType());
	interceptChance = Math::max(0, Math::min(100, interceptChance));

	if (System::random(99) >= interceptChance) {
		// Companion hesitates this time -- it noticed the threat but
		// wasn't quick/trained enough to react. Does not change state, so
		// it will simply get another roll next time this observer fires.
		return;
	}

	companionState = CompanionObject::ATTACK;

	addDefender(attacker);
	// COMPANION_INTERCEPT_FIX_2026_08_04 -- was setFollowObject(nullptr), the
	// identical bug already fixed in CompanionAttackCommand.h. On the Lua
	// behaviour-tree base the follow object IS the movement system
	// (MovePetBase:checkConditions requires followState ~= OBLIVIOUS, and
	// clearing the follow object is exactly how you become OBLIVIOUS), and
	// AiAgentImplementation::enqueueAttack() silently no-ops when
	// getFollowObject() is null. So auto-defence could neither move nor swing.
	// Stock GetTargetBase does setFollowObject(target); setDefender(target).
	setFollowObject(attacker);

	info(true) << "Companion " << getObjectID() << " intercepting threat " << attacker->getObjectID() << " to protect owner";

	// Route through the standard combat pipeline exactly as if the companion
	// itself had queued a basic attack -- this reuses all of the engine's
	// existing pool/HAM/cooldown handling instead of re-implementing it.
	// COMPANION_ATTACK_SPEED_FIX_2026_08_04_INTERCEPT -- see CompanionAttackCommand.h.
	// executeObjectControllerAction() bypasses the command queue, which is the
	// only writer of `nextAction`, so auto-defence had no attack cooldown either.
	if (isNextActionPast()) {
		enqueueCommand(STRING_HASHCODE("attack"), 0, attacker->getObjectID(), "");
	}

	// Companion System (2026-07-29 fix, per Nick: "companion runs back to
	// where i spawned it out" after battle, and his own hypothesis "sent
	// to battle but never made it there in time before it died"). The
	// followObject==nullptr set above is only ever un-set by the unrelated
	// post-combat loot sweep (deferredStartPostCombatSweep() /
	// runPostCombatSweepCheck()), which polls on a ~3-second cadence and
	// only starts polling once combat has already been observed -- so if
	// this attacker dies (or otherwise stops being a valid target) before
	// the companion actually lands a hit and enters real combat,
	// AiAgentImplementation::setDestination()'s FOLLOWING case sees the
	// null follow object on the very next AI tick and immediately paths
	// the companion toward its stale, pre-intercept homeLocation -- often
	// several seconds before the loot sweep ever gets around to restoring
	// FOLLOW. This dedicated, much faster check runs well ahead of that
	// and restores the correct posture (honoring standingOrder exactly
	// like the loot sweep's own endSweep() does) the moment it's clear no
	// real fight ever happened, without touching the loot sweep's timing
	// for fights that actually do happen.
	Reference<CompanionObject*> interceptRecoveryRef = _this.getReferenceUnsafeStaticCast();

	Core::getTaskManager()->scheduleTask([interceptRecoveryRef] () {
		CompanionObject* companion = interceptRecoveryRef.get();

		if (companion == nullptr) {
			return;
		}

		Locker locker(companion);
		companion->recoverFromAbortedIntercept();
	}, "CompanionInterceptRecoveryLambda", 1000);
}

// Companion System (2026-07-29 fix -- see the "runs back to spawn after
// battle" comment above interceptThreatToOwner()'s scheduleTask call).
// Only proceeds if the intercept never actually turned into real combat
// (companionState is still ATTACK, but isInCombat() is false) -- if a
// real fight is underway, this is a no-op and the loot sweep handles
// things normally once it ends. Restoration mirrors runSweepStep()'s own
// endSweep() lambda exactly (same standingOrder-based STAY / GUARD /
// FOLLOW+escortTarget branches), just reached much faster.
void CompanionObjectImplementation::recoverFromAbortedIntercept() {
	if (getZone() == nullptr || isDead()) {
		return;
	}

	if (companionState != CompanionObject::ATTACK || isInCombat()) {
		return;
	}

	CreatureObject* owner = getLinkedCreature().get();

	if (owner == nullptr) {
		return;
	}

	int standing = getStandingOrder();

	if (standing == CompanionObject::STAY) {
		setCompanionState(CompanionObject::STAY);
		setFollowObject(nullptr);
		setOblivious();
	} else if (standing == CompanionObject::GUARD) {
		CreatureObject* guardTarget = getGuardTarget().get();

		if (guardTarget != nullptr && guardTarget->getZone() != nullptr) {
			setCompanionState(CompanionObject::GUARD);
			setFollowObject(guardTarget);
			setFollowState(AiAgent::FOLLOWING); // genesis port: was setMovementState()
		} else {
			setCompanionState(CompanionObject::FOLLOW);
			setFollowObject(owner);
			setFollowState(AiAgent::FOLLOWING); // genesis port: was setMovementState()
		}
	} else {
		setCompanionState(CompanionObject::FOLLOW);

		CreatureObject* escortTarget = getEscortTarget().get();

		if (escortTarget != nullptr && escortTarget != owner && escortTarget->getZone() != nullptr) {
			setFollowObject(escortTarget);
		} else {
			setFollowObject(owner);
		}

		setFollowState(AiAgent::FOLLOWING); // genesis port: was setMovementState()
	}
}

void CompanionObjectImplementation::deferredInterceptThreatToOwner(CreatureObject* attacker) {
	// Bug fix (2026-07-13, real live SIGABRT -- see NOTES.md, "fifth real
	// SIGABRT, CompanionThreatObserver locking"): interceptThreatToOwner()
	// is @preLocked (mutates this companion's own state), but
	// CompanionThreatObserver.idl's notifyObserverEvent() fires on whatever
	// thread is processing the *owner's* combat-state change (DAMAGERECEIVED)
	// -- that thread only guarantees the owner is locked, never this
	// companion. Defer to a locked task, matching the exact pattern already
	// proven in CompanionContainerComponent.cpp's attemptAutoEquip().
	if (attacker == nullptr) {
		return;
	}

	Reference<CompanionObject*> companionRef = _this.getReferenceUnsafeStaticCast();
	Reference<CreatureObject*> attackerRef = attacker;

	Core::getTaskManager()->executeTask([companionRef, attackerRef] () {
		Locker locker(companionRef);
		Locker crossLocker(attackerRef, companionRef);

		CompanionObject* companion = companionRef.get();
		CreatureObject* attacker = attackerRef.get();

		if (companion == nullptr || attacker == nullptr) {
			return;
		}

		companion->interceptThreatToOwner(attacker);
	}, "CompanionInterceptThreatLambda");
}

void CompanionObjectImplementation::deferredInterceptOwnerHostileAction(CreatureObject* owner) {
	// Same reasoning/pattern as deferredInterceptThreatToOwner() above --
	// the STARTCOMBAT sibling event.
	if (owner == nullptr) {
		return;
	}

	Reference<CompanionObject*> companionRef = _this.getReferenceUnsafeStaticCast();
	Reference<CreatureObject*> ownerRef = owner;

	Core::getTaskManager()->executeTask([companionRef, ownerRef] () {
		Locker locker(companionRef);
		Locker crossLocker(ownerRef, companionRef);

		CompanionObject* companion = companionRef.get();
		CreatureObject* owner = ownerRef.get();

		if (companion == nullptr || owner == nullptr) {
			return;
		}

		companion->interceptOwnerHostileAction(owner);
	}, "CompanionInterceptOwnerHostileLambda");
}

void CompanionObjectImplementation::interceptOwnerHostileAction(CreatureObject* owner) {
	if (owner == nullptr) {
		return;
	}

	ZoneServer* zoneServer = owner->getZoneServer();

	if (zoneServer == nullptr) {
		return;
	}

	ManagedReference<SceneObject*> target = zoneServer->getObject(owner->getTargetID(), true);

	if (target == nullptr || !target->isCreatureObject()) {
		return;
	}

	interceptThreatToOwner(cast<CreatureObject*>(target.get()));
}

void CompanionObjectImplementation::refreshCombatAttacks(WeaponObject* weapon) {
	ZoneServer* zoneServer = getZoneServer();

	if (zoneServer == nullptr) {
		return;
	}

	ObjectController* objectController = zoneServer->getObjectController();

	if (objectController == nullptr) {
		return;
	}

	// Companions never carry a real npcTemplate (see the doc comment on
	// refreshCombatAttacks() in CompanionObject.idl), so
	// AiAgentImplementation::setupAttackMaps() can never build real attack
	// maps for one. This resolves the actually-relevant weapon (the one
	// just equipped, or -- if none -- the innate unarmed weapon every
	// CreatureObject keeps in its "default_weapon" slot) and builds a
	// filtered attack map directly. Deliberately calls getSlottedObject()
	// here instead of getDefaultWeapon(): AiAgent overrides that method to
	// return its own `defaultWeapon` field, which is only ever populated
	// from npcTemplate and is therefore always null for a companion --
	// not the same thing as the real innate fists weapon.
	WeaponObject* effectiveWeapon = weapon;

	if (effectiveWeapon == nullptr) {
		effectiveWeapon = getSlottedObject("default_weapon").castTo<WeaponObject*>();
	}

	// Generic humanoid combat set -- the exact brawlermid + marksmanmid
	// groups (bin/scripts/mobile/creatureskills.lua) real, working
	// humanoid NPC templates like corsec_trooper merge together, covering
	// melee (1h/2h/polearm/unarmed) and ranged (rifle/carbine/pistol-style)
	// attacks. The weapon-bitmask filter below (mirroring
	// AiAgentImplementation::setupAttackMaps()'s own filter) automatically
	// narrows this down to whichever attacks actually match the weapon in
	// hand, so one static list covers every weapon type a player could
	// auto-equip onto a companion.
	CreatureAttackMap genericAttacks;
	genericAttacks.addAttack("melee1hlunge1", "");
	genericAttacks.addAttack("melee1hhit1", "");
	genericAttacks.addAttack("melee1hbodyhit1", "");
	genericAttacks.addAttack("melee2hlunge1", "");
	genericAttacks.addAttack("melee2hhit1", "");
	genericAttacks.addAttack("melee2hheadhit1", "");
	genericAttacks.addAttack("polearmlunge1", "");
	genericAttacks.addAttack("polearmhit1", "");
	genericAttacks.addAttack("polearmleghit1", "");
	genericAttacks.addAttack("unarmedlunge1", "");
	genericAttacks.addAttack("unarmedhit1", "");
	genericAttacks.addAttack("unarmedstun1", "");
	genericAttacks.addAttack("overchargeshot1", "");
	genericAttacks.addAttack("pointblanksingle1", "");
	genericAttacks.addAttack("pointblankarea1", "");
	genericAttacks.addAttack("headshot1", "");
	genericAttacks.addAttack("bodyshot1", "");
	genericAttacks.addAttack("legshot1", "");
	genericAttacks.addAttack("fullautosingle1", "");
	genericAttacks.addAttack("diveshot", "");
	genericAttacks.addAttack("kipupshot", "");
	genericAttacks.addAttack("rollshot", "");

	// Companion System (2026-07-20, "Master Jedi companion" pass, per user
	// request) -- lightsaber/force-power attacks. REAL blocker found before
	// this fix: a companion equipped with any lightsaber (ONEHANDJEDIWEAPON/
	// TWOHANDJEDIWEAPON/POLEARMJEDIWEAPON bitmasks -- an entirely separate
	// family from the ONEHANDMELEEWEAPON/TWOHANDMELEEWEAPON/POLEARMWEAPON
	// bitmasks every attack above is gated on) would equip it visually but
	// have ZERO usable attacks -- none of the above match a saber's bitmask.
	// Same weapon-bitmask auto-filter below already handles everything: no
	// new branching needed, just give it real attacks to filter from. Full
	// verbatim "lightsabermaster" (20) + "forcepowermaster" (8) attack-skill
	// groups, bin/scripts/mobile/creatureskills.lua -- the same real attack
	// set stock dark/light Jedi Master NPCs use (global_dark_jedi_master_*
	// etc.), not invented. Harmless to add for non-Jedi companions too --
	// the filter below only lets an attack through if its real weapon
	// bitmask matches whatever is actually equipped, so these simply never
	// resolve for a companion holding a blaster or vibroblade.
	// Companion System bug fix (JEDI_SKILL_GATING_FIX_2026_07_29, "Jedi skill gating" pass --
	// see docs/companion_system/NOTES.md): the saber/Force-power block
	// below only ever survived weapon-bitmask filtering, never a real
	// eligibility check -- any companion equipped with a lightsaber-
	// class weapon (crafted, looted, or admin-given, regardless of
	// training) got full lightsaber + Force-power attacks. Gated behind
	// CompanionSkillTrainer::isJediEligible(), the same badge-based
	// check jedi_* skill training itself uses (CompanionSkillTrainer.cpp)
	// -- reused verbatim, not reinvented. A non-eligible companion
	// holding a saber now simply gets no matching attacks for it,
	// same as holding any other weapon type this list has no entries
	// for (i.e. the pre-2026-07-20 behavior).
	if (CompanionSkillTrainer::instance()->isJediEligible(_this.getReferenceUnsafeStaticCast())) {
	genericAttacks.addAttack("saber1hheadhit1", "");
	genericAttacks.addAttack("saber1hheadhit2", "");
	genericAttacks.addAttack("saber1hhit3", "");
	genericAttacks.addAttack("saber1hcombohit3", "");
	genericAttacks.addAttack("saber1hflurry", "");
	genericAttacks.addAttack("saber1hflurry2", "");
	genericAttacks.addAttack("saber2hbodyhit2", "");
	genericAttacks.addAttack("saber2hbodyhit3", "");
	genericAttacks.addAttack("saber2hfrenzy", "");
	genericAttacks.addAttack("saber2hhit3", "");
	genericAttacks.addAttack("saber2hphantom", "");
	genericAttacks.addAttack("saber2hsweep3", "");
	genericAttacks.addAttack("saberpolearmdervish", "");
	genericAttacks.addAttack("saberpolearmdervish2", "");
	genericAttacks.addAttack("saberpolearmhit3", "");
	genericAttacks.addAttack("saberpolearmleghit3", "");
	genericAttacks.addAttack("saberpolearmspinattack3", "");
	genericAttacks.addAttack("saberslash1", "");
	genericAttacks.addAttack("saberslash2", "");
	genericAttacks.addAttack("saberthrow2", "");
	genericAttacks.addAttack("forcelightningsingle2", "");
	genericAttacks.addAttack("forcelightningcone2", "");
	genericAttacks.addAttack("mindblast2", "");
	genericAttacks.addAttack("forceknockdown2", "");
	genericAttacks.addAttack("forceweaken2", "");
	genericAttacks.addAttack("forcethrow2", "");
	genericAttacks.addAttack("forcechoke", "");
	genericAttacks.addAttack("forceintimidate2", "");
	}

	CreatureAttackMap* newAttacks = new CreatureAttackMap();

	if (effectiveWeapon != nullptr) {
		for (int i = 0; i < genericAttacks.size(); i++) {
			const CombatQueueCommand* attack = cast<const CombatQueueCommand*>(objectController->getQueueCommand(genericAttacks.getCommand(i)));

			if (attack == nullptr) {
				continue;
			}

			if (attack->getWeaponType() & effectiveWeapon->getWeaponBitmask()) {
				newAttacks->add(genericAttacks.get(i));
			}
		}
	}

	if (newAttacks->isEmpty()) {
		delete newAttacks;
		newAttacks = nullptr;
	}

	// genesis port: upstream's primaryAttackMap / setPrimaryWeapon() /
	// setCurrentWeapon() do not exist on this base. Genesis's AiAgent has
	// exactly two maps -- `attackMap` and `defaultAttackMap`
	// (AiAgent.idl:78-79) -- and getAttackMap() (AiAgent.idl:1080-1086)
	// picks between them with `getWeapon() == readyWeapon ? attackMap :
	// defaultAttackMap`. Upstream's setPrimaryWeapon()+setCurrentWeapon()
	// pair existed only to force that branch to resolve to the map we just
	// built; pointing BOTH genesis maps at the same CreatureAttackMap makes
	// the branch irrelevant, so getAttackMap() returns `newAttacks` either
	// way. That is the same end state the original comment described
	// ("both pointed at the same map ... defaultAttackMap is set too as a
	// safety net"), expressed with genesis's real fields.
	//
	// NOT ported: readyWeapon is deliberately left alone. On genesis it is a
	// PERSISTED, AI-owned weapon created by loadTemplateData() from an
	// npcTemplate (AiAgentImplementation.cpp:199), and selectWeapon()
	// (AiAgentImplementation.cpp:1066-1121) transfers it into the hand slot
	// and destroyObjectFromWorld()s whatever it replaces -- assigning a
	// player-given weapon to it risks destroying the player's item.
	// setupAttackMaps() (AiAgentImplementation.cpp:354) is likewise unusable
	// here: it dereferences npcTemplate, which a companion never has (see
	// the comment at the top of this function).
	// DEFERRED / capability lost: the companion's equipped weapon is not
	// registered as the AI's readyWeapon, so genesis's own weapon-selection
	// logic still treats the companion as having only its default_weapon.
	// Concretely: selectWeapon()'s ranged/ideal-range weapon swap and
	// hasRangedWeapon() (AiAgent.idl:1325, AiAgentImplementation.cpp:3234)
	// will not see the equipped weapon. Neither is reachable from any
	// companion code path today -- selectWeapon()/selectDefaultWeapon() are
	// only called from LuaAiAgent.cpp:465,470 and PetRangedAttackCommand.h:61
	// -- so combat attack selection, which reads getAttackMap(), is
	// unaffected.
	attackMap = newAttacks;
	defaultAttackMap = newAttacks;
}

void CompanionObjectImplementation::equipItemFromInventory(TangibleObject* item, CreatureObject* requester) {
	if (item == nullptr || requester == nullptr) {
		return;
	}

	// Locking fix, same shape as attemptAutoEquip() (CompanionContainerComponent.cpp)
	// and deferredInterceptThreatToOwner() above: this fires from whatever
	// thread is processing the player's object-menu radial selection, which
	// only guarantees the player/item are relevant to that thread, never
	// this companion.
	Reference<CompanionObject*> companionRef = _this.getReferenceUnsafeStaticCast();
	Reference<TangibleObject*> itemRef = item;
	Reference<CreatureObject*> requesterRef = requester;

	Core::getTaskManager()->executeTask([companionRef, itemRef, requesterRef] () {
		Locker locker(companionRef);
		Locker itemLocker(itemRef, companionRef);

		CompanionObject* companion = companionRef.get();
		TangibleObject* item = itemRef.get();
		CreatureObject* requester = requesterRef.get();

		if (companion == nullptr || item == nullptr || requester == nullptr) {
			return;
		}

		if (!companion->isAuthorizedActor(requester)) {
			return;
		}

		if (companion->getZone() == nullptr || companion->isDead()) {
			return;
		}

		// Re-validate under lock -- state may have changed between the
		// radial click and this task running (item moved/equipped/deleted,
		// companion stored, etc.). A loose item can legitimately be parented
		// either directly to the companion (the brief landing spot right
		// after a "give," before attemptAutoEquip() resolves it) or to the
		// companion's real "inventory" child bag (2026-07-13 fix -- see
		// NOTES.md, "item vanishes when taken back out" -- where a loose,
		// non-equipped item ends up living now) -- accept either.
		SceneObject* inventoryBag = companion->getSlottedObject("inventory");
		SceneObject* itemParent = item->getParent().get();
		bool isLooseInCompanionInventory = (itemParent == companion || (inventoryBag != nullptr && itemParent == inventoryBag))
				&& item->getContainmentType() == -1;

		if (!isLooseInCompanionInventory) {
			requester->sendSystemMessage("@companion:equip_not_in_inventory"); // That item isn't in your companion's inventory.
			return;
		}

		if ((!item->isWeaponObject() && !item->isWearableObject()) || item->getArrangementDescriptorSize() == 0) {
			requester->sendSystemMessage("@companion:equip_not_equippable"); // That item can't be equipped.
			return;
		}

		ZoneServer* zoneServer = companion->getZoneServer();

		if (zoneServer == nullptr) {
			return;
		}

		ObjectController* objectController = zoneServer->getObjectController();

		if (objectController == nullptr) {
			return;
		}

		// transferType 4 is the base arrangement-group offset every real
		// wearable/weapon "Wear" transfer starts from -- same constant
		// attemptAutoEquip() and TransferItemArmorCommand.h/
		// TransferItemWeaponCommand.h use.
		int transferType = 4;
		String errorDescription;

		int precheck = companion->canAddObject(item, transferType, errorDescription);

		if (precheck == TransferErrorCode::SLOTOCCUPIED) {
			int arrangementSize = item->getArrangementDescriptorSize();
			int arrangementGroupToUse = -1;

			// Probe every arrangement group this item could go into for one
			// that's entirely free -- same probing loop attemptAutoEquip()
			// uses, mirrored here rather than shared since this is a
			// different file/class and the two call sites diverge on what
			// happens next (silent no-op there, a real message here).
			for (int i = 0; i < arrangementSize && arrangementGroupToUse == -1; ++i) {
				const Vector<String>* descriptors = item->getArrangementDescriptor(i);
				bool allFree = true;

				for (int j = 0; j < descriptors->size(); ++j) {
					if (companion->getSlottedObject(descriptors->get(j)) != nullptr) {
						allFree = false;
						break;
					}
				}

				if (allFree) {
					arrangementGroupToUse = i;
				}
			}

			if (arrangementGroupToUse == -1) {
				// Companion System (2026-07-15, "always swap out the
				// occupied slot" per user request): no free arrangement
				// group -- displace whatever occupies this item's FIRST
				// arrangement group and equip into the freed slots, exactly
				// the policy the loadout backpack has always had. Uses the
				// same destroy-first / silent-transfer / deferred-re-create
				// client resync as unequipItemToInventory(). Displaced gear
				// goes into THIS companion's own storage bag (2026-07-15,
				// per user request -- each companion's stuff stays with the
				// companion), falling back to the requester's inventory if
				// the bag is missing/full.
				SceneObject* requesterInventory = companion->getSlottedObject("inventory");

				if (requesterInventory == nullptr || requesterInventory->isContainerFull()) {
					requesterInventory = requester->getSlottedObject("inventory");
				}

				if (requesterInventory == nullptr) {
					requester->sendSystemMessage("@companion:equip_slot_occupied");
					return;
				}

				if (requesterInventory->isContainerFull()) {
					requester->sendSystemMessage("There is no room to swap out your companion's currently equipped item.");
					return;
				}

				const Vector<String>* descriptors = item->getArrangementDescriptor(0);

				// Collect unique occupants first -- multi-slot items appear
				// under several slot names.
				SortedVector<ManagedReference<SceneObject*> > occupants;
				occupants.setNoDuplicateInsertPlan();

				for (int j = 0; j < descriptors->size(); ++j) {
					SceneObject* slotted = companion->getSlottedObject(descriptors->get(j));

					if (slotted != nullptr) {
						occupants.put(slotted);
					}
				}

				Locker requesterInventoryLocker(requesterInventory, companion);

				for (int j = 0; j < occupants.size(); ++j) {
					SceneObject* occupant = occupants.get(j);

					Locker occupantLocker(occupant, companion);

					bool wasCurrentWeapon = occupant->isWeaponObject() && occupant == companion->getWeapon().get();

					occupant->broadcastDestroy(occupant, true);

					if (!objectController->transferObject(occupant, requesterInventory, -1, false)) {
						companion->broadcastObject(occupant, true);
						requester->sendSystemMessage("@companion:equip_slot_occupied");
						return;
					}

					if (wasCurrentWeapon) {
						companion->setWeapon(nullptr, true);
						companion->refreshCombatAttacks(nullptr);
					}

					Reference<SceneObject*> occupantRef = occupant;
					Reference<CreatureObject*> requesterResendRef = requester;

					Core::getTaskManager()->scheduleTask([occupantRef, requesterResendRef] () {
						SceneObject* displaced = occupantRef.get();
						CreatureObject* displacedOwner = requesterResendRef.get();

						if (displaced == nullptr || displacedOwner == nullptr) {
							return;
						}

						Locker locker(displaced);

						displaced->sendTo(displacedOwner, true);
					}, "CompanionEquipDisplaceResendLambda", 400);
				}

				arrangementGroupToUse = 0;
			}

			transferType += arrangementGroupToUse;
		} else if (precheck != 0) {
			// Explicit player action, so -- unlike attemptAutoEquip() --
			// relay the real reason back (insufficient wearable skill
			// certification, armor encumbrance, jedi-only weapon
			// restriction, etc., all already localized by canAddObject()).
			if (errorDescription.length() > 1) {
				requester->sendSystemMessage(errorDescription);
			} else {
				requester->sendSystemMessage("@companion:equip_not_equippable");
			}

			return;
		}

		if (!objectController->transferObject(item, companion, transferType, true)) {
			requester->sendSystemMessage("@companion:equip_not_equippable");
			return;
		}

		if (item->isWeaponObject()) {
			WeaponObject* weapon = cast<WeaponObject*>(item);

			if (weapon != nullptr) {
				companion->setWeapon(weapon, true);
				companion->refreshCombatAttacks(weapon);
			}
		}

		requester->sendSystemMessage("@companion:equipped"); // Your companion equips the item.
	}, "CompanionManualEquipLambda");
}

void CompanionObjectImplementation::unequipItemToInventory(TangibleObject* item, CreatureObject* requester) {
	if (item == nullptr || requester == nullptr) {
		return;
	}

	// Locking fix, same shape as equipItemFromInventory() above. 2026-07-14
	// redesign now also needs the requester locked (not just referenced) --
	// it used to only ever mutate the companion's own bag; now it mutates
	// the requester's own inventory container too.
	Reference<CompanionObject*> companionRef = _this.getReferenceUnsafeStaticCast();
	Reference<TangibleObject*> itemRef = item;
	Reference<CreatureObject*> requesterRef = requester;

	Core::getTaskManager()->executeTask([companionRef, itemRef, requesterRef] () {
		Locker locker(companionRef);
		Locker itemLocker(itemRef, companionRef);
		Locker requesterLocker(requesterRef, companionRef);

		CompanionObject* companion = companionRef.get();
		TangibleObject* item = itemRef.get();
		CreatureObject* requester = requesterRef.get();

		if (companion == nullptr || item == nullptr || requester == nullptr) {
			return;
		}

		if (!companion->isAuthorizedActor(requester)) {
			return;
		}

		if (companion->getZone() == nullptr || companion->isDead()) {
			return;
		}

		// Re-validate under lock -- state may have changed between the
		// radial click and this task running. Must actually be equipped
		// (a real slot, containmentType >= 4) directly on the companion --
		// getContainmentType() is unsigned, so cast to signed before
		// comparing (same fix already applied elsewhere in this feature --
		// see CompanionContainerComponent.cpp -- a stored -1 sentinel would
		// otherwise always satisfy ">= 4" as an unsigned value).
		if (item->getParent().get() != companion || (int) item->getContainmentType() < 4) {
			requester->sendSystemMessage("@companion:unequip_not_equipped"); // Your companion doesn't have that item equipped.
			return;
		}

		ZoneServer* zoneServer = companion->getZoneServer();

		if (zoneServer == nullptr) {
			return;
		}

		ObjectController* objectController = zoneServer->getObjectController();

		if (objectController == nullptr) {
			return;
		}

		// Companion System (2026-07-14, "Pick Up" redesign -- see NOTES.md):
		// user reported taking gear off a companion into its own nested
		// "inventory" bag was confusing (a second container to discover,
		// easy to miss) and proposed sending it straight into the
		// requester's OWN inventory instead -- the same container a real
		// player's own "un-equip my gear" action already targets. Mirrors
		// TransferItemMiscCommand.h's resolution/error-handling shape
		// exactly (getSlottedObject("inventory") for the destination,
		// canAddObject() precheck relaying its own localized error,
		// transferObject() with containmentType -1 for a loose/non-slotted
		// destination) rather than inventing a new pattern. The companion's
		// own nested bag is unaffected and still used for the *other*
		// direction (a non-equippable gift to the companion that doesn't
		// auto-equip).
		// Companion System (2026-07-15, per user request -- supersedes the
		// 2026-07-14 "straight to the player" redesign above now that
		// multiple companions exist): retrieved gear goes into THAT
		// companion's own storage bag ("<name> Inventory"), keeping each
		// companion's stuff with the companion. Falls back to the
		// requester's own inventory only if the bag is missing/full, rather
		// than failing the retrieval.
		SceneObject* playerInventory = companion->getSlottedObject("inventory");

		if (playerInventory == nullptr || playerInventory->isContainerFull()) {
			playerInventory = requester->getSlottedObject("inventory");
		}

		if (playerInventory == nullptr) {
			requester->sendSystemMessage("@companion:unequip_failed"); // Couldn't take that item off your companion right now.
			return;
		}

		Locker inventoryLocker(playerInventory, requester);

		String errorDescription;
		int transferType = -1;

		int precheck = playerInventory->canAddObject(item, transferType, errorDescription);

		if (precheck != 0) {
			inventoryLocker.release();

			// Explicit player action -- relay the real reason back (e.g.
			// inventory full), same as equipItemFromInventory() does for
			// its own canAddObject() precheck.
			if (errorDescription.length() > 1) {
				requester->sendSystemMessage(errorDescription);
			} else {
				requester->sendSystemMessage("@companion:unequip_failed");
			}

			return;
		}

		// If this is the companion's current weapon, revert combat state to
		// unarmed/default before the transfer -- same "clearWeapon" pattern
		// TransferItemMiscCommand.h uses when a player un-equips their own
		// current weapon this way.
		bool wasCurrentWeapon = item->isWeaponObject() && item == companion->getWeapon().get();

		// Companion System (2026-07-14, "retrieved item invisible until
		// relog" fix, take 2 -- see NOTES.md): moving an item that is WORN
		// on another creature into the player's inventory desyncs the
		// client: the containment-link update transferObject() broadcasts
		// for the move is mishandled for this cross-creature case, and once
		// the client's bookkeeping for the object is corrupted, even a
		// follow-up destroy+re-create is ignored (live-confirmed: item lands
		// server-side, count goes up, invisible until relog -- while the
		// same destroy+re-create works fine for items coming out of the
		// companion's storage BAG, an ordinary container the client handles
		// natively). Fix is ordering: (1) destroy the client-side object for
		// ALL observers first, while its client state is still clean ("worn
		// on companion"), (2) do the server-side transfer silently
		// (notifyClient=false -- no confusing cross-creature link
		// broadcast), (3) re-create it fresh for the owner under its new
		// parent. Other players correctly stop seeing the worn item at (1)
		// and never need it again (a private inventory's contents aren't
		// visible to them anyway).
		item->broadcastDestroy(item, true);

		if (!objectController->transferObject(item, playerInventory, transferType, false)) {
			// Transfer failed with the item still equipped -- re-create it
			// for observers so it doesn't turn invisible on the companion.
			companion->broadcastObject(item, true);

			inventoryLocker.release();
			requester->sendSystemMessage("@companion:unequip_failed");
			return;
		}

		// Deferred re-create (2026-07-14, take 3 -- see NOTES.md): an
		// IMMEDIATE re-create of an object ID the client just destroyed
		// gets silently eaten -- live-confirmed, the item stayed invisible
		// until relog even with correct destroy-first ordering, while a
		// relog (a full re-create much later) always showed it. Give the
		// client time to fully process the destroy before sending the fresh
		// create. Client-initiated moves (dragging/picking up from an open
		// container window) never hit any of this because the client renders
		// its own action locally without waiting for server packets -- which
		// is exactly why bag pick-ups always looked fine.
		Core::getTaskManager()->scheduleTask([itemRef, requesterRef] () {
			TangibleObject* item = itemRef.get();
			CreatureObject* requester = requesterRef.get();

			if (item == nullptr || requester == nullptr) {
				return;
			}

			Locker locker(item);

			item->sendTo(requester, true);
		}, "CompanionUnequipResendLambda", 400);

		inventoryLocker.release();

		if (wasCurrentWeapon) {
			companion->setWeapon(nullptr, true);
			companion->refreshCombatAttacks(nullptr);
		}

		requester->sendSystemMessage("@companion:unequipped"); // You take the item back from your companion.
	}, "CompanionManualUnequipLambda");
}

// COMPANION_TAXI_CHAIN_2026_08_07 -- shared by startTaxiRide() (snapshot at
// departure) and updateTaxiTick() (rescan on arrival) so a newly-appeared
// waypoint (e.g. a "Closest Group Mission" waypoint that only exists once
// the owner gets near the first one) can be told apart from every waypoint
// that already existed before the ride began. Per user request (2026-08-07):
// "it needs to automatically pick it once it shows up and drive to it
// without having the user pick the new closest group waypoint."
//
// Mirrors CompanionDialogMenuSuiCallback.h's taxi-picker scan exactly (same
// two sources: the owner's personal datapad waypoints, plus each active
// mission's own waypointToMission) so "what the picker would show" and
// "what auto-chaining sees" never drift apart. Both waypoint kinds are
// persisted objects (WaypointObject, and MissionObject::waypointToMission),
// so their objectIDs are stable across calls and safe to snapshot/diff.
// COMPANION_TAXI_CHAIN_TYPE_FIX_2026_08_07 -- takes Vector<uint64> (= Vector<unsigned
// long long> on this engine, see platform.h), NOT Vector<unsigned long> -- a distinct
// C++ type despite being the same size on this platform. Matters here specifically
// because taxiSeenWaypointIds (the caller's storage, declared `unsigned long` in the
// IDL) is codegen'd as Vector<unsigned long long> -- the IDL compiler's TypeInfo
// primitive-serialization specializations are keyed to the fixed-width typedef, not
// the bare type, so a same-named-but-different Vector<unsigned long> instantiation
// (what this parameter used to be) doesn't share it and fails to compile the moment
// any Vector<unsigned long> local variable is declared anywhere in this file (its
// inherited toBinaryStream()/parseFromBinaryStream() are virtual-instantiated as
// part of the class, and only the long-long specialization exists). First live build
// after this feature landed failed with exactly that error.
static void collectOwnerPlanetWaypoints(CreatureObject* owner, uint32 planetCRC, Vector<uint64>& objectIds, Vector<float>* outX, Vector<float>* outY) {
	if (owner == nullptr) {
		return;
	}

	PlayerObject* ghost = owner->getPlayerObject();

	if (ghost != nullptr) {
		for (int i = 0; i < ghost->getWaypointListSize(); ++i) {
			WaypointObject* waypoint = ghost->getWaypoint(i);

			if (waypoint == nullptr || waypoint->getPlanetCRC() != planetCRC) {
				continue;
			}

			objectIds.add(waypoint->getObjectID());

			if (outX != nullptr && outY != nullptr) {
				outX->add(waypoint->getPositionX());
				outY->add(waypoint->getPositionY());
			}
		}
	}

	ManagedReference<SceneObject*> missionBag = owner->getSlottedObject("mission_bag");

	if (missionBag != nullptr) {
		for (int i = 0; i < missionBag->getContainerObjectsSize(); ++i) {
			ManagedReference<SceneObject*> obj = missionBag->getContainerObject(i);

			if (obj == nullptr || !obj->isMissionObject()) {
				continue;
			}

			MissionObject* mission = cast<MissionObject*>(obj.get());
			WaypointObject* missionWaypoint = mission->getWaypointToMission();

			if (missionWaypoint == nullptr || missionWaypoint->getPlanetCRC() != planetCRC) {
				continue;
			}

			objectIds.add(missionWaypoint->getObjectID());

			if (outX != nullptr && outY != nullptr) {
				outX->add(missionWaypoint->getPositionX());
				outY->add(missionWaypoint->getPositionY());
			}
		}
	}
}

// Companion Taxi / Vehicle Mimicry (2026-07-15) -- see CompanionObject.idl's
// method doc comments and NOTES.md's "vehicle mimicry redesign" entry for
// the design and the reasoning behind reverting the real-mount experiment.
bool CompanionObjectImplementation::startTaxiRide(CreatureObject* owner, float destX, float destY, bool hasDestination, unsigned int vehicleTemplateCRC) {
	if (owner == nullptr) {
		return false;
	}

	Zone* zone = getZone();

	if (zone == nullptr || isDead()) {
		return false;
	}

	ZoneServer* zoneServer = getZoneServer();

	if (zoneServer == nullptr) {
		return false;
	}

	// Idempotent: tear down any ride already running before starting anew.
	if (taxiActive) {
		stopTaxiRide(false);
	}

	// Vehicles are outdoor-only; a companion inside a building can't ride.
	if (getParent().get() != nullptr) {
		return false;
	}

	// Companion System (2026-07-20, "taxi greeting" pass, per user
	// request): "the companion acting as the taxi should go to the user
	// and stop 5 meters in front" -- a posted/strayed companion could be
	// anywhere on the map (especially now that /companionstay can hold a
	// companion far from the owner for a "battlefield" setup), so a taxi
	// call snap-teleports it to 5m directly in front of the owner's
	// current facing before the vehicle/driver ever spawns -- same
	// snapTeleport idiom FormationManager::arrangeFollowers() already uses
	// for /companionformup (forward = (sin, cos) of the target's heading).
	{
		float headingAngle = owner->getDirectionAngle();
		float forwardX = std::sin(headingAngle);
		float forwardY = std::cos(headingAngle);

		float greetX = owner->getPositionX() + forwardX * 5.0f;
		float greetY = owner->getPositionY() + forwardY * 5.0f;
		float greetZ = zone->getHeight(greetX, greetY);

		ManagedReference<SceneObject*> ownerParent = owner->getParent().get();
		uint64 ownerParentID = ownerParent != nullptr ? ownerParent->getObjectID() : 0;

		teleport(greetX, greetZ, greetY, ownerParentID);
		setNextPosition(greetX, greetZ, greetY, ownerParent.castTo<CellObject*>());
	}

	if (vehicleTemplateCRC == 0) {
		vehicleTemplateCRC = STRING_HASHCODE(COMPANION_TAXI_VEHICLE_TEMPLATE);
	}

	// Resolve the mimicked vehicle's SHARED template up front: its client
	// CRC is what makes the driver LOOK like that vehicle, and its real
	// speed array is what the 15%-over boost is computed from.
	// (vehicleTemplateCRC is a SERVER template CRC -- the mimicry hook reads
	// it via getServerObjectCRC() -- so the client CRC must come from the
	// template data, exactly like SceneObjectImplementation::
	// loadTemplateData() itself does at creation time.)
	SharedObjectTemplate* vehicleTemplateData = TemplateManager::instance()->getTemplate(vehicleTemplateCRC);
	SharedCreatureObjectTemplate* vehicleCreoTemplate = dynamic_cast<SharedCreatureObjectTemplate*>(vehicleTemplateData);

	float vehicleRealRunSpeed = COMPANION_TAXI_SPEED / COMPANION_TAXI_SPEED_MULTIPLIER;
	float vehicleRealWalkSpeed = vehicleRealRunSpeed;

	if (vehicleCreoTemplate != nullptr) {
		const auto& speedTempl = vehicleCreoTemplate->getSpeed();

		if (speedTempl.size() > 0 && speedTempl.get(0) > 0.f) {
			vehicleRealRunSpeed = speedTempl.get(0);
			vehicleRealWalkSpeed = speedTempl.size() > 1 && speedTempl.get(1) > 0.f ? (float) speedTempl.get(1) : vehicleRealRunSpeed;
		}
	}

	// 2026-07-16 rider-flip redesign (see the file-top comment block): the
	// vehicle is no longer a static cosmetic shell -- it's the DRIVER, a
	// NON-PERSISTENT (persistence level 0, structurally can never hit the
	// database) agent built from the companion's own proven
	// companion_actor.iff template, so it inherits the full live-tested
	// movement stack: CompanionObject class, AIENABLED, the "companion" AI
	// map (moveMode=RUN patrol/follow branches), and the leash override
	// that keeps long trips from snapping home.
	ManagedReference<SceneObject*> driverObj = zoneServer->createObject(STRING_HASHCODE("object/mobile/companion_actor.iff"), 0);
	CompanionObject* driver = driverObj != nullptr ? driverObj.castTo<CompanionObject*>().get() : nullptr;

	if (driver == nullptr) {
		if (driverObj != nullptr) {
			Locker cleanupLocker(driverObj, _this.getReferenceUnsafeStaticCast());
			driverObj->destroyObjectFromWorld(true);
		}

		error("CompanionSystem: could not create taxi driver agent -- ride aborted");
		return false;
	}

	float boostedRunSpeed = vehicleRealRunSpeed * COMPANION_TAXI_SPEED_MULTIPLIER;
	float boostedWalkSpeed = vehicleRealWalkSpeed * COMPANION_TAXI_SPEED_MULTIPLIER;

	Locker driverLocker(driver, _this.getReferenceUnsafeStaticCast());

	// Look like the owner's vehicle: re-stamp the client template BEFORE
	// the zone insert so the client's create packet already carries the
	// vehicle CRC (same trick the companion itself uses for
	// shared_human_male -- see CompanionControlDeviceImplementation.cpp).
	// OptionBitmask::VEHICLE is required or the client refuses to display
	// a vehicle-template object at all (see OptionBitmask.h's own comment);
	// INVULNERABLE keeps the ride from being attacked out from under the
	// rider; CONVERSE is cleared so the speeder doesn't offer the
	// companion's own Talk-to dialog radial.
	if (vehicleTemplateData != nullptr) {
		driver->setClientObjectCRC(vehicleTemplateData->getClientObjectCRC());
	}

	// 2026-07-16 (user request): the driver carries the COMPANION'S name --
	// the owner watches "their companion" drive off, their existing target
	// UI keeps showing a sensible name, and a /target <name>-style follow
	// macro keeps working for the whole ride (a blank name here made the
	// owner's target readout collapse to nothing after the waypoint pick).
	driver->setCustomObjectName(getCustomObjectName(), false);
	driver->setCreatureLink(owner);
	driver->setOptionBit(OptionBitmask::VEHICLE, false);
	driver->setOptionBit(OptionBitmask::INVULNERABLE, false);
	driver->clearOptionBit(OptionBitmask::CONVERSE, false);

	driver->setRunSpeed(boostedRunSpeed, true);
	// genesis port: dropped driver->setWalkSpeed(boostedWalkSpeed, true) -- genesis's
	// CreatureObject.idl exposes walkSpeed READ-ONLY (field :100, getWalkSpeed()
	// :1676); setRunSpeed() (:468) is the only speed setter, and it is already
	// called on the line(s) directly above with the matching run-speed value, so
	// the pace change still takes effect for RUN movement. DEFERRED: walk-mode
	// pacing cannot be tuned on this base.

	driver->initializePosition(getPositionX(), getPositionZ(), getPositionY());
	driver->setDirection(getDirectionW(), getDirectionX(), getDirectionY(), getDirectionZ());
	zone->transferObject(driver, -1, true);

	// Same activation order as the companion's own spawnObject(): home
	// anchor, then AI map, then tree assembly.
	driver->setHomeLocation(getPositionX(), getPositionZ(), getPositionY(), nullptr);
	// genesis port: DEFERRED -- setCustomAiMap()/customAiMap does not exist on this
	// base and adding it would require an AiAgent.idl change (out of scope). Dropped
	// the setCustomAiMap(STRING_HASHCODE("companion")) call; the companion falls back
	// to the default AiMap trees selected from creatureBitmask. This is part of the
	// already-known behaviour-tree gap: genesis drives AI from Lua behaviour trees and
	// the native leaf classes the Companion System expects do not exist here, so the
	// "companion" AI map (follow-at-a-run, no leash-home) is not applied.
	// genesis port: setAITemplate() -> setupBehaviorTree() (AiAgent.idl:1178,
	// autogen/.../AiAgent.h:805) -- same no-arg "assemble the default trees" call.
	// COMPANION_TAXI_DRIVER_TREE_FIX_2026_08_04 -- closes the deferral the
	// comment above describes.
	//
	// setupBehaviorTree() with no arguments composes the tree from the creature
	// bitmask. The driver never gets CreatureFlag::PET (nothing in the companion
	// tree calls setCreatureBitmask at all), so the idle slot fell to the NONE
	// row of templates.lua -- `idlewander`: GeneratePatrol, Walk, Wait. No
	// follow behaviour anywhere in it, so nothing consumed the follow object the
	// way MoveCreaturePet does; and its mover is WalkBase, which inherits
	// MoveBase:checkConditions and therefore the leash. The driver ended up with
	// a correct follow object, a correct FOLLOWING state, a live move event, and
	// a tree that could only wander near home and go home if it strayed.
	//
	// The comment above calls the missing piece "the 'companion' AI map
	// (follow-at-a-run, no leash-home)". That is exactly what companionfollow is
	// -- MoveCreaturePet in the idle slot, CombatMoveCreaturePet in the combat
	// slot, neither carrying MoveBase's leash check. It did not exist when this
	// was deferred; it was written on 2026-08-04 for this same class of bug.
	//
	// Works for both ride modes: MovePetBase:checkConditions only requires that
	// the follow state is not OBLIVIOUS, which the destination ride's PATROLLING
	// satisfies as readily as the escort ride's FOLLOWING.
	// TAXI_DRIVER_TREE_CORRECTION_2026_08_04 -- companionfollow was wrong for a
	// DRIVER. Its root selector puts the combat branch (GetTarget ->
	// SelectAttack -> CombatMoveCreaturePet) ahead of the idle mover, and
	// GetTargetBase acquires a target and calls setFollowObject(target) -- so a
	// driver heading for a waypoint grabbed the first hostile it saw and drove
	// at that instead. Escort mode was unaffected, because chasing the owner is
	// what the mover does anyway; that is exactly the split observed in game
	// (mounted follow fine, waypoint driving broken).
	//
	// Stock `follow` is the right template for this job:
	//     follow = { {"root", "MoveCreaturePet", "none", BEHAVIOR} }
	// the pure mover and nothing else. It honours the follow object in escort
	// mode and patrol points in destination mode, and MovePetBase carries no
	// leash check -- which was the whole reason idlewander could not be used.
	driver->activateLoad("follow");
	driver->activateRecovery();

	// Companion System (2026-07-20, "taxi greeting" pass, per user
	// request): spoken once, right as the vehicle arrives at the
	// owner's side -- same ChatManager::broadcastChatMessage() primitive
	// CompanionChatter.h uses for order-response flair, called inline
	// here since this file's own companionSweepSay() helper (identical
	// pattern) isn't declared until later in the file.
	{
		// COMPANION_TAXI_ESCORT_SILENT_BARK_FIX_2026_08_05 -- this used to fire unconditionally, including
		// for escort/mimicry rides (every time the owner calls out their own
		// vehicle, once per summoned companion). Escort mode is already
		// documented a few lines below as "SILENT cosmetic mimicry" -- it
		// auto-follows on its own after a short hold, nothing to click -- so
		// the bark was both wrong there and, because mimicry re-arms on every
		// vehicle call-out, endlessly repetitive. Only a real destination ride
		// (a waypoint the owner actually picked) gets it now.
		ChatManager* chatManager = zoneServer->getChatManager();

		if (hasDestination && chatManager != nullptr) {
			chatManager->broadcastChatMessage(driver, UnicodeString("Click on me and follow! I'll bring us there."), 0, 0, 0, 0, 1);
		}
	}

	if (hasDestination) {
		// Route via the SAME proven AI patrol pathing as before -- just on
		// the driver instead of the companion. The route (point A + any
		// stops appended by addTaxiWaypoint()) is loaded now, but the
		// driver HOLDS STILL in STAY for 5 seconds (2026-07-16, per user
		// request) so the owner has time to click it and /follow --
		// updateTaxiTick() flips it to PATROL once taxiDepartureTime
		// passes.
		driver->setFollowObject(nullptr);
		driver->clearPatrolPoints();

		float destZ = zone->getHeight(destX, destY);
		PatrolPoint destination(destX, destZ, destY);
		driver->addPatrolPoint(destination);

		driver->setCompanionState(CompanionObject::STAY);
		driver->setOblivious();

		taxiAwaitingGoConfirm = true;
	} else {
		// Escort mode: same 5-second departure hold as destination mode
		// (2026-07-20, "taxi greeting" pass) before the driver starts
		// following the owner -- see updateTaxiTick()'s matching
		// standalone branch (kept separate from the destination-mode hold
		// logic below to avoid touching its already-proven pacing/leash
		// handling).
		//
		// TAXI_GO_SCOPE_FIX_2026_07_30 -- back to a flat timer here, NOT the GO
		// popup (2026-07-30 fix, per Nick: only the companion he actually
		// asked to be the taxi should ask GO) -- escort mode is the
		// SILENT cosmetic mimicry every OTHER summoned companion enters
		// automatically the moment the owner calls out their own real
		// vehicle (see VehicleControlDeviceImplementation.cpp's
		// startCompanionVehicleMimicry()) -- with N companions summoned,
		// popping N GO confirmations at once for a ride the player never
		// explicitly chose is exactly the bug reported live today.
		driver->setCompanionState(CompanionObject::STAY);
		driver->setFollowObject(nullptr);
		driver->setOblivious();

		taxiDepartureTime = System::getMiliTime() + 5000;
	}

	// Companion System (2026-07-30, "GO button" pass, per user request:
	// "so the vehicle only starts moving once the player actually clicks
	// it"). taxiDepartureTime is no longer pre-armed at ride start in
	// EITHER branch above -- the driver just holds in STAY with
	// taxiAwaitingGoConfirm set (see updateTaxiTick()'s matching new
	// early-return gates) until the owner presses GO on this popup.
	// CompanionTaxiGoSuiCallback.h's OK handler is the only thing that
	// ever sets taxiDepartureTime now: it flips it to "now," which makes
	// the very next tick see the existing (unchanged) proximity+timer
	// check pass immediately.
	// TAXI_GO_SCOPE_FIX_2026_07_30 -- only the real, single, player-chosen taxi
	// (hasDestination == true) shows the GO popup. Escort-mode cosmetic
	// mimicry (hasDestination == false, see the branch above) never did
	// before today and shouldn't now either.
	if (hasDestination) {
		ManagedReference<SuiMessageBox*> goSuiBox = new SuiMessageBox(owner, SuiWindowType::COMPANION_TAXI_GO_CONFIRM);
		goSuiBox->setCallback(new CompanionTaxiGoSuiCallback(zoneServer, _this.getReferenceUnsafeStaticCast()));
		goSuiBox->setPromptTitle(getDisplayedName() + " -=COMPANION=- : Taxi");
		goSuiBox->setPromptText("Your ride is ready -- press GO when you're set to depart.");
		goSuiBox->setCancelButton(true, "@ui:cancel");
		goSuiBox->setOkButton(true, "GO");

		ManagedReference<PlayerObject*> ownerGhost = owner->getPlayerObject();

		// COMPANION_TAXI_GO_POPUP_DEFER_2026_08_04 -- this box used to land on
		// top of the waypoint picker. startTaxiRide() is called from INSIDE the
		// waypoint SuiListBox's own callback, so the GO box was generated and
		// sent while the client was still showing the list that launched it,
		// and the two stacked. A new player has no way to know one window is
		// hiding behind another.
		//
		// SWG's SUI protocol gives the server no way to position a window, so
		// "centre it" is not on the table. Not having two on screen at once is:
		// send this one from a short scheduled task, by which time the client
		// has processed the list box closing. Same deferred-task idiom as
		// CompanionLootResendLambda and CompanionAutoEquipDisplaceResendLambda
		// elsewhere in this file.
		if (ownerGhost != nullptr) {
			ManagedReference<SuiMessageBox*> deferredBox = goSuiBox;
			ManagedReference<CreatureObject*> deferredOwner = owner;

			Core::getTaskManager()->scheduleTask([deferredBox, deferredOwner] () {
				SuiMessageBox* box = deferredBox.get();
				CreatureObject* who = deferredOwner.get();

				if (box == nullptr || who == nullptr) {
					return;
				}

				Locker locker(who);

				ManagedReference<PlayerObject*> ghost = who->getPlayerObject();

				if (ghost == nullptr) {
					return;
				}

				ghost->addSuiBox(box);
				who->sendMessage(box->generateMessage());
			}, "CompanionTaxiGoPopupLambda", 700);
		}
	}

	// Mount the companion onto the driver -- the exact 3-step MountCommand
	// idiom whose GLUE half already live-proved itself in the earlier
	// real-mount experiment (only its locomotion half was wrong, and the
	// driver owns locomotion now).
	driver->setState(CreatureState::MOUNTEDCREATURE, true);
	driver->transferObject(_this.getReferenceUnsafeStaticCast(), 4 /* PlayerArrangement::RIDER */, true);
	setState(CreatureState::RIDINGMOUNT, true);

	// Park the companion's own AI for the duration of the ride so it can
	// never fight the parent's movement -- the exact conflict that caused
	// the original real-mount teleport bug. STAY + oblivious mirrors
	// CompanionStayCommand's own idiom, INCLUDING its home re-anchor: the
	// generic OBLIVIOUS logic paths a creature toward homeLocation whenever
	// it isn't "in range" of it (see the spawnObject() homeLocation fix
	// note), so home must ride along too -- set here to the mount point,
	// then re-anchored to the driver's position every updateTaxiTick().
	setCompanionState(CompanionObject::STAY);
	setFollowObject(nullptr);
	clearPatrolPoints();
	setHomeLocation(driver->getPositionX(), driver->getPositionZ(), driver->getPositionY());
	setOblivious();

	taxiVehicle = driver;

	// The throttle's two paces, both DRIVER speeds now (the companion's own
	// speeds are never touched -- as a RIDER child it doesn't locomote):
	// "saved" = the mimicked vehicle's real pace (throttled-back state),
	// "boosted" = 15% over it (normal state).
	taxiSavedRunSpeed = vehicleRealRunSpeed;
	taxiSavedWalkSpeed = vehicleRealWalkSpeed;
	taxiBoostedRunSpeed = boostedRunSpeed;
	taxiBoostedWalkSpeed = boostedWalkSpeed;
	taxiThrottled = false;

	taxiDestX = destX;
	taxiDestY = destY;
	taxiActive = true;
	taxiHasDestination = hasDestination;
	taxiOwnerWasMounted = owner->isRidingMount();

	// COMPANION_TAXI_CHAIN_2026_08_07 -- snapshot the owner's CURRENT
	// on-planet waypoints (personal datapad + active missions) so
	// updateTaxiTick()'s arrival check can recognize a genuinely NEW one
	// that appears mid-ride (see collectOwnerPlanetWaypoints()'s doc
	// comment) and auto-chain to it instead of stopping. Escort rides
	// (!hasDestination) never reach the chaining check, but the snapshot
	// is cheap and harmless to take either way -- simpler than threading
	// an extra branch through here.
	taxiSeenWaypointIds.removeAll();
	collectOwnerPlanetWaypoints(owner, zone->getZoneCRC(), taxiSeenWaypointIds, nullptr, nullptr);

	// Auto-target the DRIVER the moment the ride starts (2026-07-16: was the
	// companion, but the companion is a RIDER child now -- targeting the
	// top-level moving vehicle is what makes the owner's /follow work on it
	// directly, no macro needed).
	owner->setTargetID(driver->getObjectID());

	// True auto-taxi RETIRED (2026-07-16, live-tested and reverted per user
	// choice): the client ignores server movement updates for the mount its
	// OWN player is sitting on (client-authoritative movement -- the ride
	// happened invisibly server-side and the owner just teleported at
	// arrival). startOwnerAutoDrive()/stopOwnerAutoDrive() are kept intact
	// but no longer invoked from anywhere except stopTaxiRide()'s safe
	// no-op cleanup. The shipped design is: driver auto-targeted (above) +
	// the 5-second departure hold below, so the owner just presses /follow.

	scheduleCompanionTaxiTick(_this.getReferenceUnsafeStaticCast());
	CompanionGearExchangeManager::scheduleGearCheckTick(_this.getReferenceUnsafeStaticCast());

	return true;
}

// True auto-taxi (2026-07-16, per user choice "option 1" -- the server
// literally drives the owner to the waypoint, zero input; see NOTES.md).
// @pre: owner and this locked (called from the deferred kickoff above).
void CompanionObjectImplementation::startOwnerAutoDrive(CreatureObject* owner) {
	if (owner == nullptr || !taxiActive || !taxiHasDestination || taxiOwnerCarriage.get() != nullptr) {
		return;
	}

	ManagedReference<SceneObject*> vehicle = taxiVehicle.get();
	CreatureObject* driverCreo = vehicle != nullptr ? vehicle->asCreatureObject() : nullptr;

	if (driverCreo == nullptr) {
		return;
	}

	Zone* zone = owner->getZone();
	ZoneServer* zoneServer = getZoneServer();

	if (zone == nullptr || zoneServer == nullptr || owner->isDead()) {
		return;
	}

	// If the owner is sitting on their real vehicle, dismount them first
	// (DismountCommand's step order). An owner parented to anything else
	// (a building cell etc.) can't be auto-driven -- ride continues, they
	// travel on their own.
	ManagedReference<SceneObject*> ownerParent = owner->getParent().get();

	if (ownerParent != nullptr) {
		if (!ownerParent->isVehicleObject()) {
			owner->sendSystemMessage("Step outside and your companion will drive you next time -- follow it to your waypoint!");
			return;
		}

		float offX = ownerParent->getPositionX();
		float offZ = ownerParent->getPositionZ();
		float offY = ownerParent->getPositionY();

		owner->clearState(CreatureState::RIDINGMOUNT, true);
		owner->setPosition(offX, offZ, offY);
		zone->transferObject(owner, -1, false);

		CreatureObject* realVehicleCreo = ownerParent->asCreatureObject();

		if (realVehicleCreo != nullptr) {
			Locker realVehicleLocker(realVehicleCreo, owner);
			realVehicleCreo->clearState(CreatureState::MOUNTEDCREATURE, true);
		}

		owner->teleport(offX, offZ, offY, 0);
	}

	// Stow the owner's real vehicle for the duration -- the taxi replaces
	// it. VehicleControlDeviceImplementation's store hook skips destination
	// rides (isTaxiDestinationRide() guard), so this can't cancel our own
	// trip; any escort rides of the owner's OTHER companions end, which is
	// correct (their escort vehicle just vanished).
	ManagedReference<SceneObject*> datapad = owner->getSlottedObject("datapad");

	if (datapad != nullptr) {
		for (int i = 0; i < datapad->getContainerObjectsSize(); ++i) {
			ManagedReference<SceneObject*> obj = datapad->getContainerObject(i);

			if (obj == nullptr || !obj->isVehicleControlDevice()) {
				continue;
			}

			VehicleControlDevice* device = cast<VehicleControlDevice*>(obj.get());
			ManagedReference<SceneObject*> realVehicle = device->getControlledObject();

			if (realVehicle != nullptr && realVehicle->isInQuadTree()) {
				Locker deviceLocker(device, owner);
				device->storeObject(owner, true);
				break;
			}
		}
	}

	// The carriage: same construction as the driver (see startTaxiRide()),
	// wearing the same vehicle model (client CRC copied straight off the
	// driver), following the driver -- with the OWNER mounted on it.
	ManagedReference<SceneObject*> carriageObj = zoneServer->createObject(STRING_HASHCODE("object/mobile/companion_actor.iff"), 0);
	CompanionObject* carriage = carriageObj != nullptr ? carriageObj.castTo<CompanionObject*>().get() : nullptr;

	if (carriage == nullptr) {
		if (carriageObj != nullptr) {
			Locker cleanupLocker(carriageObj, owner);
			carriageObj->destroyObjectFromWorld(true);
		}

		error("CompanionSystem: could not create auto-taxi carriage -- owner rides manually");
		return;
	}

	Locker carriageLocker(carriage, owner);

	carriage->setClientObjectCRC(driverCreo->getClientObjectCRC());
	carriage->setCustomObjectName(owner->getFirstName() + "'s taxi", false);
	carriage->setCreatureLink(owner);
	carriage->setOptionBit(OptionBitmask::VEHICLE, false);
	carriage->setOptionBit(OptionBitmask::INVULNERABLE, false);
	carriage->clearOptionBit(OptionBitmask::CONVERSE, false);

	carriage->setRunSpeed(taxiBoostedRunSpeed > 0.f ? taxiBoostedRunSpeed : COMPANION_TAXI_SPEED, true);
	// genesis port: dropped carriage->setWalkSpeed(taxiBoostedWalkSpeed > 0.f ? taxiBoostedWalkSpeed : COMPANION_TAXI_SPEED, true) -- genesis's
	// CreatureObject.idl exposes walkSpeed READ-ONLY (field :100, getWalkSpeed()
	// :1676); setRunSpeed() (:468) is the only speed setter, and it is already
	// called on the line(s) directly above with the matching run-speed value, so
	// the pace change still takes effect for RUN movement. DEFERRED: walk-mode
	// pacing cannot be tuned on this base.

	carriage->initializePosition(owner->getPositionX(), owner->getPositionZ(), owner->getPositionY());
	carriage->setDirection(owner->getDirectionW(), owner->getDirectionX(), owner->getDirectionY(), owner->getDirectionZ());
	zone->transferObject(carriage, -1, true);

	carriage->setHomeLocation(owner->getPositionX(), owner->getPositionZ(), owner->getPositionY(), nullptr);
	// genesis port: DEFERRED -- setCustomAiMap()/customAiMap does not exist on this
	// base and adding it would require an AiAgent.idl change (out of scope). Dropped
	// the setCustomAiMap(STRING_HASHCODE("companion")) call; the companion falls back
	// to the default AiMap trees selected from creatureBitmask. This is part of the
	// already-known behaviour-tree gap: genesis drives AI from Lua behaviour trees and
	// the native leaf classes the Companion System expects do not exist here, so the
	// "companion" AI map (follow-at-a-run, no leash-home) is not applied.
	// genesis port: setAITemplate() -> setupBehaviorTree() (AiAgent.idl:1178,
	// autogen/.../AiAgent.h:805) -- same no-arg "assemble the default trees" call.
	carriage->setupBehaviorTree();
	carriage->activateRecovery();

	// Convoy behind the taxi.
	carriage->setCompanionState(CompanionObject::FOLLOW);
	carriage->setFollowObject(driverCreo);
	carriage->setFollowState(AiAgent::FOLLOWING); // genesis port: was setMovementState()

	// Mount the OWNER -- same 3-step idiom as the companion's own mount.
	carriage->setState(CreatureState::MOUNTEDCREATURE, true);
	carriage->transferObject(owner, 4 /* PlayerArrangement::RIDER */, true);
	owner->setState(CreatureState::RIDINGMOUNT, true);

	taxiOwnerCarriage = carriage;

	owner->sendSystemMessage("Sit back -- your companion is driving! (Dismount at any time to take over.)");
}

// True auto-taxi (2026-07-16) -- see the idl doc comment.
void CompanionObjectImplementation::stopOwnerAutoDrive(bool notifyOwner) {
	ManagedReference<SceneObject*> carriage = taxiOwnerCarriage.get();
	taxiOwnerCarriage = nullptr;

	if (carriage == nullptr) {
		return;
	}

	Locker carriageLocker(carriage, _this.getReferenceUnsafeStaticCast());

	CreatureObject* owner = getLinkedCreature().get();

	// Dismount the owner BEFORE destroying the carriage -- destroying a
	// container takes its children with it (same order as the companion's
	// own dismount in stopTaxiRide()).
	if (owner != nullptr && owner->getParent().get() == carriage) {
		Locker ownerLocker(owner, _this.getReferenceUnsafeStaticCast());

		float dismountX = carriage->getPositionX();
		float dismountZ = carriage->getPositionZ();
		float dismountY = carriage->getPositionY();

		owner->clearState(CreatureState::RIDINGMOUNT, true);
		owner->setPosition(dismountX, dismountZ, dismountY);

		Zone* zone = carriage->getZone();

		if (zone != nullptr) {
			zone->transferObject(owner, -1, false);
		}

		CreatureObject* carriageCreo = carriage->asCreatureObject();

		if (carriageCreo != nullptr) {
			carriageCreo->clearState(CreatureState::MOUNTEDCREATURE, true);
		}

		if (zone != nullptr) {
			owner->teleport(dismountX, dismountZ, dismountY, 0);
		}

		if (notifyOwner) {
			owner->sendSystemMessage("Your taxi ride ends here.");
		}
	}

	carriage->destroyObjectFromWorld(true);
}

void CompanionObjectImplementation::stopTaxiRide(bool resumeFollow) {
	// True auto-taxi (2026-07-16): retire the owner's carriage on every
	// ride exit -- dismounts the owner right where the ride ended.
	// (Feature retired same day -- safe no-op now, see startTaxiRide().)
	stopOwnerAutoDrive(false);

	taxiActive = false;
	taxiHasDestination = false;
	taxiThrottled = false;
	taxiDepartureTime = 0;
	taxiDestX = 0;
	taxiDestY = 0;

	ManagedReference<SceneObject*> vehicle = taxiVehicle.get();
	taxiVehicle = nullptr;

	if (vehicle != nullptr) {
		Locker vehicleLocker(vehicle, _this.getReferenceUnsafeStaticCast());

		// 2026-07-16 rider-flip redesign: the companion is a real RIDER
		// child of the driver now, so it MUST be dismounted back into the
		// world BEFORE the driver is destroyed -- destroying a container
		// takes its children with it. Same step order as DismountCommand:
		// clear rider state, transfer into the zone, clear the mount's
		// state, then teleport to force a clean client position sync.
		if (getParent().get() == vehicle) {
			clearState(CreatureState::RIDINGMOUNT, true);

			Zone* zone = getZone();

			if (zone == nullptr) {
				zone = vehicle->getZone();
			}

			float dismountX = vehicle->getPositionX();
			float dismountZ = vehicle->getPositionZ();
			float dismountY = vehicle->getPositionY();

			setPosition(dismountX, dismountZ, dismountY);

			if (zone != nullptr) {
				zone->transferObject(_this.getReferenceUnsafeStaticCast(), -1, false);
			}

			CreatureObject* vehicleCreo = vehicle->asCreatureObject();

			if (vehicleCreo != nullptr) {
				vehicleCreo->clearState(CreatureState::MOUNTEDCREATURE, true);
			}

			if (zone != nullptr) {
				teleport(dismountX, dismountZ, dismountY, 0);
			}
		}

		vehicle->destroyObjectFromWorld(true);
	}

	// Rider-flip redesign: the throttle speeds belong to the (now
	// destroyed) driver -- the companion's own speeds were never touched,
	// so there's nothing to restore, only fields to clear.
	taxiSavedRunSpeed = 0;
	taxiSavedWalkSpeed = 0;
	taxiBoostedRunSpeed = 0;
	taxiBoostedWalkSpeed = 0;

	clearPatrolPoints();

	if (resumeFollow) {
		CreatureObject* owner = getLinkedCreature().get();

		if (owner != nullptr) {
			setCompanionState(CompanionObject::FOLLOW);
			setFollowObject(owner);
			setFollowState(AiAgent::FOLLOWING); // genesis port: was setMovementState()
		}
	}
}

// 2026-07-16 rider-flip redesign: no position mirroring left at all -- the
// engine's native RIDER containment keeps companion and vehicle glued, and
// the driver's own AI does all the moving. The tick only supervises.
void CompanionObjectImplementation::updateTaxiTick() {
	if (!taxiActive) {
		return;
	}

	Zone* zone = getZone();

	if (zone == nullptr || isDead()) {
		// Despawned/died mid-ride -- teardown (store/death paths also call
		// stopTaxiRide() directly; this is the belt-and-braces catch).
		stopTaxiRide(false);
		return;
	}

	ManagedReference<SceneObject*> vehicle = taxiVehicle.get();
	CreatureObject* driverCreo = vehicle != nullptr ? vehicle->asCreatureObject() : nullptr;

	if (driverCreo == nullptr || driverCreo->isDead() || driverCreo->getZone() == nullptr) {
		// Driver lost somehow -- dismount (stopTaxiRide handles the child
		// extraction safely even from a half-gone driver) and resume follow.
		stopTaxiRide(true);
		return;
	}

	// Keep the parked companion's home anchored to the moving driver every
	// tick, so the OBLIVIOUS branch's path-toward-home logic never has
	// anywhere to go (see the mount-time comment in startTaxiRide()).
	setHomeLocation(driverCreo->getPositionX(), driverCreo->getPositionZ(), driverCreo->getPositionY());

	CreatureObject* rideOwner = getLinkedCreature().get();

	// True auto-taxi (2026-07-16): if the owner has hopped off the carriage
	// on their own (native dismount button = the opt-out), quietly retire
	// the carriage -- the taxi itself keeps driving and they can follow
	// manually or just meet the companion at the waypoint.
	ManagedReference<SceneObject*> ownerCarriage = taxiOwnerCarriage.get();

	if (ownerCarriage != nullptr && (rideOwner == nullptr || rideOwner->getParent().get() != ownerCarriage)) {
		stopOwnerAutoDrive(false);

		if (rideOwner != nullptr) {
			rideOwner->sendSystemMessage("You take over the driving -- your companion continues to the waypoint.");
		}
	}

	// Combat always takes priority (user spec): if the companion or its
	// owner gets attacked mid-ride, dismount immediately and let the normal
	// threat/combat machinery take over. resumeFollow=true puts the
	// companion back on FOLLOW, which the combat AI overrides while
	// fighting and naturally resumes afterward.
	if (isInCombat() || (rideOwner != nullptr && rideOwner->isInCombat())) {
		if (rideOwner != nullptr) {
			rideOwner->sendSystemMessage("Your companion leaps off its vehicle to fight!");
		}

		stopTaxiRide(true);
		return;
	}

	// Owner gap, world-space (getWorldPosition() resolves through the
	// owner's parent chain -- a mounted rider's own position is
	// cell-relative to their vehicle; the driver is never parented).
	float ownerDistSq = -1.f;
	Vector3 ownerWorld;

	if (rideOwner != nullptr) {
		ownerWorld = rideOwner->getWorldPosition();
		float odx = driverCreo->getPositionX() - ownerWorld.getX();
		float ody = driverCreo->getPositionY() - ownerWorld.getY();
		ownerDistSq = odx * odx + ody * ody;
	}

	if (taxiHasDestination) {
		// Departure hold (2026-07-16, per user request): the driver stands
		// still until BOTH the 5-second click-and-follow window has passed
		// AND the owner is within 35m -- then it flips to PATROL once.
		//
		// GO button (2026-07-30, per user request): taxiDepartureTime is no
		// longer pre-armed at ride start -- the driver holds in STAY with
		// taxiAwaitingGoConfirm set until the owner presses GO on the popup
		// (see startTaxiRide()/CompanionTaxiGoSuiCallback.h). Re-schedule and
		// bail the same way the timer/proximity gate below does until then.
		if (taxiAwaitingGoConfirm) {
			scheduleCompanionTaxiTick(_this.getReferenceUnsafeStaticCast());
			CompanionGearExchangeManager::scheduleGearCheckTick(_this.getReferenceUnsafeStaticCast());
			return;
		}

		if (taxiDepartureTime != 0) {
			bool windowPassed = System::getMiliTime() >= taxiDepartureTime;
			bool ownerClose = ownerDistSq >= 0.f && ownerDistSq <= COMPANION_TAXI_RESUME_DISTANCE_SQ;

			if (!windowPassed || !ownerClose) {
				scheduleCompanionTaxiTick(_this.getReferenceUnsafeStaticCast());
	CompanionGearExchangeManager::scheduleGearCheckTick(_this.getReferenceUnsafeStaticCast());
				return;
			}

			taxiDepartureTime = 0;

			Locker driverLocker(driverCreo, _this.getReferenceUnsafeStaticCast());

			CompanionObject* driverCompanion = driverCreo->isCompanionObject() ? cast<CompanionObject*>(driverCreo) : nullptr;

			if (driverCompanion != nullptr) {
				driverCompanion->setCompanionState(CompanionObject::PATROL);
			}

			AiAgent* departingAgent = driverCreo->asAiAgent();

			if (departingAgent != nullptr) {
				departingAgent->setFollowState(AiAgent::PATROLLING); // genesis port: was setMovementState()
				departingAgent->activateMovementEvent(); // see COMPANION_TAXI_MOVEMENT_EVENT_FIX_2026_08_04
			}

			if (rideOwner != nullptr) {
				rideOwner->sendSystemMessage("Your companion drives off -- follow it!");
			}
		}

		// Pacing rules (2026-07-16 revision -- see the constants block):
		// hard 85m leash with a full stop-and-wait, 35m resume, and a
		// catch-up boost whenever the OWNER has overtaken the taxi.
		if (ownerDistSq >= 0.f) {
			float ddx = driverCreo->getPositionX() - taxiDestX;
			float ddy = driverCreo->getPositionY() - taxiDestY;
			float driverDestSq = ddx * ddx + ddy * ddy;

			float pdx = ownerWorld.getX() - taxiDestX;
			float pdy = ownerWorld.getY() - taxiDestY;
			float ownerDestSq = pdx * pdx + pdy * pdy;

			bool driverAhead = driverDestSq <= ownerDestSq;

			// Companion System (2026-08-10, per Nick: "my companion goes
			// through walls" catching up in a city). Evaluated fresh every
			// 200ms tick off the DRIVER's own position (it's the one doing
			// the navigating) -- see isNearDenseBuildings()'s doc comment.
			// A tighter leash/catch-up pair is swapped in below only while
			// true; open terrain keeps the original 90m/50m/1.2x numbers.
			bool nearCity = isNearDenseBuildings(zone, driverCreo->getPositionX(), driverCreo->getPositionY());
			float leashDistSq = nearCity ? COMPANION_TAXI_CITY_LEASH_DISTANCE_SQ : COMPANION_TAXI_LEASH_DISTANCE_SQ;
			float leashResumeDistSq = nearCity ? COMPANION_TAXI_CITY_RESUME_DISTANCE_SQ : COMPANION_TAXI_RESUME_DISTANCE_SQ;
			float catchupMultiplier = nearCity ? COMPANION_TAXI_CITY_CATCHUP_MULTIPLIER : COMPANION_TAXI_CATCHUP_MULTIPLIER;

			if (taxiThrottled) {
				// Paused at the leash (90m in the open, 30m near buildings) -- wait for the owner.
				if (ownerDistSq <= leashResumeDistSq) {
					// COMPANION_TAXI_PACING_2026_08_04 -- owner back inside the
					// resume distance (50m open terrain, 15m near buildings, see
					// nearCity above), so return to full speed. The driver never
					// stopped and never went OBLIVIOUS, so this only has to undo
					// the crawl.
					taxiThrottled = false;

					Locker driverLocker(driverCreo, _this.getReferenceUnsafeStaticCast());

					if (taxiBoostedRunSpeed > 0.f) {
						driverCreo->setRunSpeed(taxiBoostedRunSpeed, true);
					}

					CompanionObject* driverCompanion = driverCreo->isCompanionObject() ? cast<CompanionObject*>(driverCreo) : nullptr;

					if (driverCompanion != nullptr) {
						driverCompanion->setCompanionState(CompanionObject::PATROL);
					}

					AiAgent* resumingAgent = driverCreo->asAiAgent();

					if (resumingAgent != nullptr) {
						resumingAgent->setFollowState(AiAgent::PATROLLING); // genesis port: was setMovementState()
						resumingAgent->activateMovementEvent(); // see COMPANION_TAXI_MOVEMENT_EVENT_FIX_2026_08_04
					}

					if (rideOwner != nullptr) {
						rideOwner->sendSystemMessage("Your companion drives on.");
					}
				}
			} else if (driverAhead && ownerDistSq > leashDistSq) {
				// COMPANION_TAXI_PACING_2026_08_04 -- leash tripped (90m open
				// terrain, 30m near buildings per nearCity above): SLOW DOWN,
				// do not stop, until the owner is back inside the resume
				// distance.
				//
				// This used to setCompanionState(STAY) + setOblivious(), and
				// MovePetBase:checkConditions refuses to move at all while
				// OBLIVIOUS -- so getting going again depended entirely on the
				// resume branch firing correctly, the same fragile shape behind
				// several bugs found today. A crawl keeps the driver moving and
				// keeps its follow object, so there is nothing to recover from.
				// Patrol points stay queued either way, so the route continues.
				taxiThrottled = true;

				Locker driverLocker(driverCreo, _this.getReferenceUnsafeStaticCast());

				float crawl = taxiBoostedRunSpeed * COMPANION_TAXI_THROTTLED_MULTIPLIER;

				if (crawl > 0.f && driverCreo->getRunSpeed() != crawl) {
					driverCreo->setRunSpeed(crawl, true);
				}

				if (rideOwner != nullptr) {
					rideOwner->sendSystemMessage("Your companion eases off to let you catch up.");
				}
			} else {
				// COMPANION_TAXI_PACING_2026_08_04 -- pace off the OWNER, live.
				//
				// taxiBoostedRunSpeed was derived from the VEHICLE TEMPLATE's own
				// speed row and fixed once at ride start, so a slow template made
				// the taxi crawl regardless of what the player was riding, and it
				// never adapted when the player's speed changed (mount, dismount,
				// buff, or the control panel's speed knobs). Recomputing from the
				// owner every tick keeps the driver just ahead of whatever the
				// player is actually doing.
				//
				// Still 20% extra on top whenever the owner has overtaken it and
				// pulled beyond the resume distance ("always keep up").
				if (rideOwner != nullptr) {
					float ownerRun = rideOwner->getRunSpeed();

					if (ownerRun > 0.f) {
						taxiBoostedRunSpeed = ownerRun * COMPANION_TAXI_SPEED_MULTIPLIER;
						taxiBoostedWalkSpeed = taxiBoostedRunSpeed;
					}
				}

				float targetRun = taxiBoostedRunSpeed;
				float targetWalk = taxiBoostedWalkSpeed;

				if (!driverAhead && ownerDistSq > COMPANION_TAXI_CATCHUP_TRIGGER_DISTANCE_SQ) {
					targetRun *= catchupMultiplier;
					targetWalk *= catchupMultiplier;
				}

				if (targetRun > 0.f && driverCreo->getRunSpeed() != targetRun) {
					Locker driverLocker(driverCreo, _this.getReferenceUnsafeStaticCast());
					driverCreo->setRunSpeed(targetRun, true);
					// genesis port: dropped driverCreo->setWalkSpeed(targetWalk > 0.f ? targetWalk : targetRun, true) -- genesis's
					// CreatureObject.idl exposes walkSpeed READ-ONLY (field :100, getWalkSpeed()
					// :1676); setRunSpeed() (:468) is the only speed setter, and it is already
					// called on the line(s) directly above with the matching run-speed value, so
					// the pace change still takes effect for RUN movement. DEFERRED: walk-mode
					// pacing cannot be tuned on this base.
				}
			}
		}

		// Arrived? (Driver position -- the companion's own coordinates are
		// cell-relative to the driver while mounted.)
		float dx = driverCreo->getPositionX() - taxiDestX;
		float dy = driverCreo->getPositionY() - taxiDestY;

		if ((dx * dx + dy * dy) <= COMPANION_TAXI_ARRIVAL_RADIUS_SQ) {
			// COMPANION_TAXI_CHAIN_2026_08_07 -- per user request: "we need
			// the taxi to be able to goto the mission waypoint, and then the
			// nearest group mission all in one shot... it needs to
			// automatically pick it once it shows up and drive to it
			// without having the user pick the new closest group
			// waypoint." Before settling into the normal arrival-wait
			// below, rescan the owner's waypoints and diff against the
			// snapshot taken at departure (startTaxiRide()). Exactly ONE
			// new waypoint (e.g. a "Closest Group Mission" waypoint that
			// only appears once the owner nears the first stop) means
			// re-target and keep driving; zero or more than one is
			// ambiguous and falls back to the existing stop-and-wait
			// behavior below rather than guessing which one the player
			// wants.
			if (taxiHasDestination && rideOwner != nullptr) {
				uint32 planetCRC = zone->getZoneCRC();
				Vector<uint64> currentWaypointIds;
				Vector<float> currentWaypointX;
				Vector<float> currentWaypointY;

				collectOwnerPlanetWaypoints(rideOwner, planetCRC, currentWaypointIds, &currentWaypointX, &currentWaypointY);

				int newWaypointIndex = -1;
				int newWaypointCount = 0;

				for (int i = 0; i < currentWaypointIds.size(); ++i) {
					if (!taxiSeenWaypointIds.contains(currentWaypointIds.get(i))) {
						if (newWaypointCount == 0) {
							newWaypointIndex = i;
						}

						++newWaypointCount;
					}
				}

				if (newWaypointCount == 1 && addTaxiWaypoint(currentWaypointX.get(newWaypointIndex), currentWaypointY.get(newWaypointIndex))) {
					taxiSeenWaypointIds.add(currentWaypointIds.get(newWaypointIndex));

					rideOwner->sendSystemMessage("A new waypoint has appeared -- your companion is taking you there.");

					scheduleCompanionTaxiTick(_this.getReferenceUnsafeStaticCast());
					CompanionGearExchangeManager::scheduleGearCheckTick(_this.getReferenceUnsafeStaticCast());
					return;
				}
			}

			if (rideOwner != nullptr) {
				rideOwner->sendSystemMessage("Your companion has arrived at the waypoint and is waiting for you.");
			}

			// COMPANION_TAXI_ARRIVAL_WAIT_2026_08_05 -- make the wait authoritative. standingOrder
			// defaults to FOLLOW and nothing here ever touched it, so any of the
			// several background helpers that restore a companion's standing
			// order (flee recovery, training SUI, skill-train walkup/abandon,
			// the post-combat sweep straggler check) would drag a parked,
			// waiting taxi companion straight back to the owner. They already
			// have a STAY branch that does the right thing -- it just was never
			// armed for a taxi arrival until now.
			setStandingOrder(CompanionObject::STAY);

			// 2026-07-16 (user request): WAIT at the destination instead of
			// driving back toward the owner -- resumeFollow=false leaves the
			// companion exactly as the ride parked it (STAY, home anchored
			// to the arrival spot by the per-tick anchor above), so it
			// stands at the waypoint until the owner arrives and orders it
			// to follow again (or its leash logic intervenes).
			stopTaxiRide(false);

			// COMPANION_TAXI_ARRIVAL_WAIT_2026_08_05 -- and actually come back once the owner
			// catches up, instead of waiting forever for a manual /follow.
			scheduleTaxiPickupWatch(_this.getReferenceUnsafeStaticCast());
			return;
		}

		// Keep the route alive if the driver's pathing consumed the point
		// without reaching the arrival radius (repath around obstacles,
		// interrupted movement). Skipped while leash-paused (2026-07-16) --
		// re-adding a point sets PATROLLING and would override the stop.
		if (!taxiThrottled && driverCreo->isAiAgent()) {
			AiAgent* driverAgent = driverCreo->asAiAgent();

			if (driverAgent != nullptr && driverAgent->getPatrolPointSize() == 0) {
				Locker driverLocker(driverAgent, _this.getReferenceUnsafeStaticCast());

				float destZ = zone->getHeight(taxiDestX, taxiDestY);
				PatrolPoint destination(taxiDestX, destZ, taxiDestY);
				// genesis port: setMovementState() -> setFollowState(); genesis
				// setFollowState() calls clearPatrolPoints(), so the state must be
				// set BEFORE the point is queued (see PetPatrolCommand.h).
				driverAgent->setFollowState(AiAgent::PATROLLING);
				driverAgent->addPatrolPoint(destination);
				driverAgent->activateMovementEvent(); // see COMPANION_TAXI_MOVEMENT_EVENT_FIX_2026_08_04
			}
		}
	}

	// Companion System (2026-07-20, "taxi greeting" pass, per user
	// request): escort mode's own 5-second departure hold, the same
	// taxiDepartureTime clock destination mode uses (set in
	// startTaxiRide()'s escort branch) -- kept as its own standalone
	// branch rather than folded into the `if (taxiHasDestination)` block
	// above so none of that block's already-proven waypoint pacing/leash
	// logic is touched.
	//
	// TAXI_GO_SCOPE_FIX_2026_07_30 -- escort mode no longer ever sets
	// taxiAwaitingGoConfirm (see startTaxiRide()), so the awaiting-
	// confirm early-return that used to sit here has been removed
	// entirely; escort-mode rides go straight to the flat-timer check
	// below, exactly like before today's GO-button patch.
	if (!taxiHasDestination && taxiDepartureTime != 0) {
		if (System::getMiliTime() < taxiDepartureTime) {
			scheduleCompanionTaxiTick(_this.getReferenceUnsafeStaticCast());
	CompanionGearExchangeManager::scheduleGearCheckTick(_this.getReferenceUnsafeStaticCast());
			return;
		}

		taxiDepartureTime = 0;

		Locker driverLocker(driverCreo, _this.getReferenceUnsafeStaticCast());

		CompanionObject* driverCompanion = driverCreo->isCompanionObject() ? cast<CompanionObject*>(driverCreo) : nullptr;

		if (driverCompanion != nullptr) {
			driverCompanion->setCompanionState(CompanionObject::FOLLOW);
			driverCompanion->setFollowObject(rideOwner);
		}

		AiAgent* departingAgent = driverCreo->asAiAgent();

		if (departingAgent != nullptr) {
			departingAgent->setFollowState(AiAgent::FOLLOWING); // genesis port: was setMovementState()
			// COMPANION_TAXI_MOVEMENT_EVENT_FIX_2026_08_04 -- the driver had a
			// correct follow object and a correct follow state and NO CLOCK.
			// An AiAgent only moves because an AiMoveEvent keeps re-firing its
			// behaviour tree, and setFollowState/setFollowObject no longer arm
			// one (the calls are commented out in AiAgent.idl:652/664/676/688).
			// There was not a single activateMovementEvent() anywhere in the
			// taxi system, which is exactly why a mounted companion sat still.
			// Idempotent -- it only schedules when nothing is pending.
			departingAgent->activateMovementEvent();
		}

		// COMPANION_TAXI_ESCORT_SILENT_SETOFF_FIX_2026_08_07 -- same bug class as
		// COMPANION_TAXI_ESCORT_SILENT_BARK_FIX_2026_08_05 above (the "Click on me
		// and follow!" bark): this whole code path only runs when
		// !taxiHasDestination (see the enclosing "if (!taxiHasDestination &&
		// taxiDepartureTime != 0)" a few lines up) -- i.e. it is ONLY reachable
		// from escort/mimicry mode, never from a real, player-chosen taxi ride
		// (destination rides go through the GO-button path instead, see above).
		// Escort mode is documented a few lines below as "SILENT cosmetic
		// mimicry" -- it starts automatically the instant the owner calls out
		// their OWN real vehicle (see VehicleControlDeviceImplementation.cpp's
		// startCompanionVehicleMimicry()), so this system message fired on every
		// single vehicle call-out even though the owner never asked for a taxi
		// (2026-08-07, live bug report on a brand-new character: "i never asked
		// for a taxi, the companion is still with me and following me"). No
		// message needed here -- the state transitions above (FOLLOW +
		// activateMovementEvent()) already do the actual work silently.
	}

	// No destination (general escort mode): the driver keeps following the
	// owner until the vehicle-store mimicry hook stops the ride -- OR
	// (2026-07-18, per user request) until the owner DISMOUNTS: then the
	// companion rides in close first and only hops off within 10m, instead
	// of dismounting wherever it happened to be.
	//
	// HOTFIX (2026-07-18, "companion vehicles no longer showing up"): only
	// treat not-mounted as a dismount AFTER the owner has actually ridden
	// during this escort -- the first ticks run before the owner has even
	// climbed onto their freshly-called vehicle, and were ending the ride
	// (and despawning the companion's vehicle) instantly.
	if (!taxiHasDestination && rideOwner != nullptr) {
		if (rideOwner->isRidingMount()) {
			taxiOwnerWasMounted = true;
		} else if (taxiOwnerWasMounted && ownerDistSq >= 0.f && ownerDistSq <= 100.0f) { // 10m, squared
			stopTaxiRide(true);
			return;
		}

		// Not mounted yet, or too far: keep riding -- the driver is
		// already FOLLOWing the owner, so the gap closes on its own and a
		// later tick lands inside the 10m window.
	}

	scheduleCompanionTaxiTick(_this.getReferenceUnsafeStaticCast());
	CompanionGearExchangeManager::scheduleGearCheckTick(_this.getReferenceUnsafeStaticCast());
}

// Companion Taxi multi-stop route (2026-07-16) -- see the idl doc comment.
bool CompanionObjectImplementation::addTaxiWaypoint(float destX, float destY) {
	if (!taxiActive || !taxiHasDestination) {
		return false;
	}

	ManagedReference<SceneObject*> vehicle = taxiVehicle.get();
	CreatureObject* driverCreo = vehicle != nullptr ? vehicle->asCreatureObject() : nullptr;
	AiAgent* driverAgent = driverCreo != nullptr ? driverCreo->asAiAgent() : nullptr;

	if (driverAgent == nullptr || driverAgent->isDead()) {
		return false;
	}

	Zone* zone = driverAgent->getZone();

	if (zone == nullptr) {
		return false;
	}

	Locker driverLocker(driverAgent, _this.getReferenceUnsafeStaticCast());

	float destZ = zone->getHeight(destX, destY);
	PatrolPoint stop(destX, destZ, destY);

	// Don't kick the driver into motion if it's still in the 5-second
	// departure hold -- the point queues up and the tick starts the whole
	// route when the clock passes (2026-07-16).
	//
	// genesis port: setMovementState() -> setFollowState(); genesis
	// setFollowState() calls clearPatrolPoints(), so the state change was
	// moved ABOVE addPatrolPoint() (see PetPatrolCommand.h) -- the point is
	// still queued unconditionally, exactly as before.
	if (taxiDepartureTime == 0) {
		driverAgent->setFollowState(AiAgent::PATROLLING);
	}

	driverAgent->addPatrolPoint(stop);

	// The arrival check (and the tick's route-keepalive repath) always
	// tracks the FINAL stop of the route.
	taxiDestX = destX;
	taxiDestY = destY;

	return true;
}

// ============================================================================
// Post-combat auto-loot & ranger auto-harvest (2026-07-18, per user request --
// see NOTES.md). Armed by CompanionThreatObserver on every fight the owner
// gets into; polls until owner AND companion are fully out of combat; then
// each companion claims the nearby lootable corpses IT is nearest to (every
// companion computes the same nearest-companion answer, so claims are
// naturally disjoint -- no cross-companion coordination needed), walks corpse
// to corpse looting into its own bag (ranger-trained companions also harvest
// the owner's chosen resource), then walks back to the owner and delivers
// everything into the owner's inventory.
// ============================================================================

#include "server/zone/objects/transaction/TransactionLog.h"

namespace {

	constexpr float LOOT_SWEEP_SCAN_RANGE = 64.f;
	constexpr float LOOT_SWEEP_REACH = 6.f;
	constexpr int LOOT_SWEEP_MAX_STEPS = 300; // x 400ms = 2 minute hard cap

	void companionSweepSay(CompanionObject* companion, const String& text) {
		if (companion == nullptr) {
			return;
		}

		ZoneServer* zoneServer = companion->getZoneServer();

		if (zoneServer == nullptr) {
			return;
		}

		ChatManager* chatManager = zoneServer->getChatManager();

		if (chatManager != nullptr) {
			chatManager->broadcastChatMessage(companion, UnicodeString(text), 0, 0, 0, 0, 1);
		}
	}

	bool companionHasRangerTraining(CompanionObject* companion) {
		if (companion == nullptr) {
			return false;
		}

		for (int i = 0; i < companion->getLearnedSkillCount(); ++i) {
			const String& skill = companion->getLearnedSkill(i);

			if (skill.beginsWith("outdoors_ranger_") || skill.beginsWith("outdoors_scout_")) {
				return true;
			}
		}

		return false;
	}

	/** All of the owner's currently summoned, living companions. */
	void resolveOwnersCompanions(CreatureObject* owner, Vector<CompanionObject*>& out) {
		if (owner == nullptr) {
			return;
		}

		ManagedReference<SceneObject*> datapad = owner->getSlottedObject("datapad");

		if (datapad == nullptr) {
			return;
		}

		for (int i = 0; i < datapad->getContainerObjectsSize(); ++i) {
			ManagedReference<SceneObject*> obj = datapad->getContainerObject(i);

			if (obj == nullptr || !obj->isCompanionControlDevice()) {
				continue;
			}

			CompanionControlDevice* device = cast<CompanionControlDevice*>(obj.get());

			if (device == nullptr || device->isCompanionDead()) {
				continue;
			}

			CompanionObject* comp = device->getCompanionObject();

			if (comp == nullptr || comp->getZone() == nullptr || comp->isDead()) {
				continue;
			}

			if (comp->getLinkedCreature().get() != owner) {
				continue;
			}

			out.add(comp);
		}
	}

	/** True when this corpse's loot bag belongs to the owner (or their group). */
	bool corpseLootableBy(AiAgent* corpse, CreatureObject* owner) {
		if (corpse == nullptr || owner == nullptr) {
			return false;
		}

		SceneObject* lootBag = corpse->getSlottedObject("inventory");

		if (lootBag == nullptr) {
			return false;
		}

		uint64 lootOwnerID = lootBag->getContainerPermissions()->getOwnerID();

		return lootOwnerID == owner->getObjectID() || (owner->getGroupID() != 0 && lootOwnerID == owner->getGroupID());
	}

	bool corpseHarvestableBy(AiAgent* corpse, CreatureObject* owner) {
		if (corpse == nullptr || owner == nullptr || !corpse->isCreature()) {
			return false;
		}

		Creature* creature = cast<Creature*>(corpse);

		// The ranger companion is the one doing the harvesting, not the owner -- bypass the
		// stock "player personally holds Scout" gate (requirePlayerSkill=false) and rely on
		// companionHasRangerTraining() instead. Every other precondition is unchanged.
		if (creature == nullptr || !creature->canHarvestMe(owner, false)) {
			return false;
		}

		return creature->getHideMax() > 0 || creature->getMeatMax() > 0 || creature->getBoneMax() > 0;
	}

	/**
	 * CreatureManagerImplementation::harvest(), re-hosted for a RANGER
	 * COMPANION standing at the corpse: identical resource math and rewards
	 * (all credited to the OWNER -- resources, XP, observers), minus the
	 * stock "player within 7m of corpse" gate that the whole feature exists
	 * to avoid (the ranger walked there so the owner doesn't have to). Group
	 * modifier/spam intentionally omitted (owner may be far away).
	 * @pre: corpse locked (cross), owner safe to message.
	 */
	void companionHarvestCorpse(CompanionObject* ranger, CreatureObject* owner, Creature* creature, int selectedID) {
		if (ranger == nullptr || owner == nullptr || creature == nullptr) {
			return;
		}

		Zone* zone = creature->getZone();

		// Same bypass as corpseHarvestableBy() above -- the ranger companion's own training
		// gates this, not the owner's personal Scout skill.
		if (zone == nullptr || !creature->isCreature() || !creature->canHarvestMe(owner, false)) {
			return;
		}

		ZoneServer* zoneServer = zone->getZoneServer();

		if (zoneServer == nullptr) {
			return;
		}

		ManagedReference<ResourceManager*> resourceManager = zoneServer->getResourceManager();

		if (resourceManager == nullptr) {
			return;
		}

		String restype = "";
		float quantity = 0;

		if (selectedID == 234) {
			restype = creature->getMeatType();
			quantity = creature->getMeatMax();
		} else if (selectedID == 235) {
			restype = creature->getHideType();
			quantity = creature->getHideMax();
		} else if (selectedID == 236) {
			restype = creature->getBoneType();
			quantity = creature->getBoneMax();
		}

		if (quantity == 0 || restype.isEmpty()) {
			// This creature simply doesn't have the chosen resource -- skip
			// quietly (the owner chose "hide" and killed something boneless
			// -- not an error worth spamming).
			return;
		}

		int quantityExtracted = int(quantity * float(owner->getSkillMod("creature_harvesting") / 100.0f));
		quantityExtracted = Math::max(quantityExtracted, 3);

		ManagedReference<ResourceSpawn*> resourceSpawn = resourceManager->getCurrentSpawn(restype, zone->getZoneName());

		if (resourceSpawn == nullptr) {
			return;
		}

		// Density sampled at the RANGER's spot (it's the one doing the
		// harvesting), same quality tiers as the stock flow.
		float density = resourceSpawn->getDensityAt(zone->getZoneName(), ranger->getPositionX(), ranger->getPositionY());

		String creatureHealth = "";

		if (density > 0.75f) {
			quantityExtracted = int(quantityExtracted * 1.25f);
			creatureHealth = "creature_quality_fat";
		} else if (density > 0.50f) {
			quantityExtracted = int(quantityExtracted * 1.00f);
			creatureHealth = "creature_quality_medium";
		} else if (density > 0.25f) {
			quantityExtracted = int(quantityExtracted * 0.75f);
			creatureHealth = "creature_quality_scrawny";
		} else {
			quantityExtracted = int(quantityExtracted * 0.50f);
			creatureHealth = "creature_quality_skinny";
		}

		if (creature->getParent().get() != nullptr) {
			quantityExtracted = 1;
		}

		TransactionLog trx(TrxCode::HARVESTED, owner, resourceSpawn);
		resourceManager->harvestResourceToPlayer(trx, owner, resourceSpawn, quantityExtracted);
		trx.commit();

		StringIdChatParameter harvestMessage("skl_use", creatureHealth);
		harvestMessage.setDI(quantityExtracted);
		harvestMessage.setTU(resourceSpawn->getFinalClass());
		owner->sendSystemMessage(harvestMessage);

		ManagedReference<PlayerManager*> playerManager = zoneServer->getPlayerManager();

		if (playerManager != nullptr) {
			int scoutXp = creature->getLevel() * 5 + 19;

			playerManager->awardExperience(owner, "scout", scoutXp, true);

			// COMPANION_HARVEST_XP_PARITY_2026_08_07 -- awardExperience() above
			// only ever credits the OWNER: it looks up player->getPlayerObject()
			// internally and silently returns 0 for a CompanionObject, which has
			// no PlayerObject ghost (same root cause as the earlier
			// COMPANION_XP_PARITY_2026_08_05 combat_general gap). The owner
			// credit above is correct and intentional (see this function's own
			// doc comment -- "credited to the OWNER"), but Scout is also a
			// companion-trainable skill (CompanionSkillTrainer.cpp's
			// outdoors_scout_novice and up), so the ranger companion doing the
			// harvesting needs its OWN "scout" xp toward its own tier progress
			// too. Live report: "my scout companion isnt getting scout xp when
			// harvesting creatures." Same scaleXpForCompanion() + addExperience()
			// pattern as the combat_general fix.
			ranger->addExperience("scout", playerManager->scaleXpForCompanion("scout", scoutXp));
		}

		creature->addAlreadyHarvested(owner);

		owner->notifyObservers(ObserverEventType::HARVESTEDCREATURE, resourceSpawn, quantityExtracted);
	}

	/** Shared sweep bookkeeping carried across the step-task chain. */
	class CompanionSweepState : public Object {
	public:
		Vector<uint64> corpseIDs;
		Vector<uint64> lootedItemIDs;
		int lootedCash = 0;
		int steps = 0;
		int stillInCombatPolls = 0;
		bool saidInventoryFull = false;
	};

	void scheduleSweepStep(Reference<CompanionObject*> companionRef, Reference<CreatureObject*> ownerRef, Reference<CompanionSweepState*> state, int delayMs);

	void runSweepStep(Reference<CompanionObject*> companionRef, Reference<CreatureObject*> ownerRef, Reference<CompanionSweepState*> state) {
		CompanionObject* companion = companionRef.get();
		CreatureObject* owner = ownerRef.get();

		if (companion == nullptr || owner == nullptr || state == nullptr) {
			return;
		}

		Locker clocker(companion);

		auto endSweep = [&](bool resumeFollow) {
			companion->setLootSweepActive(false);

			if (resumeFollow && !companion->isDead()) {
				// Companion System (2026-07-20, "massive battlefield" pass,
				// per user request). Loot and credits above always go
				// straight to the OWNER, unchanged -- only the posture the
				// companion resumes AFTER delivering them changes here,
				// based on standingOrder (the last EXPLICIT order the owner
				// gave -- see CompanionObject.idl's doc comment; unlike
				// companionState, combat interception and this very sweep
				// never overwrite it).
				int standing = companion->getStandingOrder();

				if (standing == CompanionObject::STAY) {
					// Posted at a fixed spot (via /companionstay or
					// /companionguard on an object): homeLocation was never
					// touched by the fight or the sweep, so just resume
					// holding it -- OBLIVIOUS paths back there on its own if
					// looting/harvesting carried the companion off first.
					companion->setCompanionState(CompanionObject::STAY);
					companion->setFollowObject(nullptr);
					companion->setOblivious();
				} else if (standing == CompanionObject::GUARD) {
					CreatureObject* guardTarget = companion->getGuardTarget().get();

					if (guardTarget != nullptr && guardTarget->getZone() != nullptr) {
						companion->setCompanionState(CompanionObject::GUARD);
						companion->setFollowObject(guardTarget);
						companion->setFollowState(AiAgent::FOLLOWING); // genesis port: was setMovementState()
					} else {
						// Guarded creature logged out/died/left the zone --
						// nothing left to guard; fall back to the owner
						// exactly like the FOLLOW branch below.
						companion->setCompanionState(CompanionObject::FOLLOW);
						companion->setFollowObject(owner);
						companion->setFollowState(AiAgent::FOLLOWING); // genesis port: was setMovementState()
					}
				} else {
					// FOLLOW (the default/original behavior) -- 2026-07-20
					// earlier same-day fix: honor a standing
					// /companionfollowother escort order if one is set,
					// otherwise return to the owner. See the escortTarget
					// field doc comment.
					companion->setCompanionState(CompanionObject::FOLLOW);

					CreatureObject* escortTarget = companion->getEscortTarget().get();

					if (escortTarget != nullptr && escortTarget != owner && escortTarget->getZone() != nullptr) {
						companion->setFollowObject(escortTarget);
					} else {
						companion->setFollowObject(owner);
					}

					companion->setFollowState(AiAgent::FOLLOWING); // genesis port: was setMovementState()
				}
			}
		};

		if (companion->getZone() == nullptr || companion->isDead()) {
			endSweep(false);
			return;
		}

		// Combat restarted mid-sweep: drop everything and fight -- the
		// observer will re-arm a fresh sweep for the new fight's corpses.
		//
		// Companion System (2026-07-29 fix, per Nick: "companion kills
		// something and runs off") -- isInCombat() can get stuck true forever
		// here for the exact same reason already documented on
		// combatStuckPollCount in runPostCombatSweepCheck (defenderList-driven
		// bitmask that normal death cleanup never scrubs for a bystander/stale
		// defender entry), and unlike that pre-sweep gate, this mid-sweep
		// check used to bail out via endSweep(false) -- resumeFollow=false --
		// with NO recovery path at all, silently stranding the companion
		// wherever the loot walk had carried it (which can legitimately be
		// 100+ meters from the owner if the kill happened while chasing a
		// fleeing target) the moment this ever fired on a stale flag instead
		// of a genuine new fight, since the "observer will re-arm" assumption
		// only holds if a fresh OWNER-side combat event happens to follow.
		// Give it the same bounded-retry + forcePeace() safety net already
		// proven in runPostCombatSweepCheck instead: tolerate up to the same
		// LOOT_SWEEP_MAX_STEPS (2 minute) window this function already uses
		// elsewhere before assuming something is stuck, then force whichever
		// side is still flagged out of combat and restore standingOrder-based
		// FOLLOW/STAY/GUARD via endSweep(true) instead of stranding it.
		if (companion->isInCombat() || owner->isInCombat()) {
			if (++state->stillInCombatPolls <= LOOT_SWEEP_MAX_STEPS) {
				scheduleSweepStep(companionRef, ownerRef, state, 400);
				return;
			}

			if (companion->isInCombat()) {
				CombatManager::instance()->forcePeace(companion);
			}

			if (owner->isInCombat()) {
				CombatManager::instance()->forcePeace(owner);
			}

			endSweep(true);
			return;
		}

		if (++state->steps > LOOT_SWEEP_MAX_STEPS) {
			endSweep(true);
			return;
		}

		ZoneServer* zoneServer = companion->getZoneServer();

		if (zoneServer == nullptr) {
			endSweep(false);
			return;
		}

		// ---- Corpse phase -------------------------------------------------
		while (state->corpseIDs.size() > 0) {
			ManagedReference<SceneObject*> corpseObj = zoneServer->getObject(state->corpseIDs.get(0));
			AiAgent* corpse = corpseObj != nullptr ? cast<AiAgent*>(corpseObj.get()) : nullptr;

			if (corpse == nullptr || corpse->getZone() == nullptr || !corpse->isDead()) {
				state->corpseIDs.remove(0);
				continue;
			}

			if (companion->getDistanceTo(corpse) > LOOT_SWEEP_REACH) {
				// Walk there (same proven patrol pathing the taxi uses).
				companion->setCompanionState(CompanionObject::PATROL);
				companion->setFollowObject(nullptr);

				if (companion->getPatrolPointSize() == 0) {
					PatrolPoint point(corpse->getPositionX(), corpse->getPositionZ(), corpse->getPositionY());
					// genesis port: setMovementState() -> setFollowState(); genesis
					// setFollowState() calls clearPatrolPoints(), so the state must be
					// set BEFORE the point is queued (see PetPatrolCommand.h).
					companion->setFollowState(AiAgent::PATROLLING);
					companion->addPatrolPoint(point);
				}

				scheduleSweepStep(companionRef, ownerRef, state, 400);
				return;
			}

			// At the corpse: loot everything in its bag into our own bag.
			Locker corpseLocker(corpse, companion);

			SceneObject* lootBag = corpse->getSlottedObject("inventory");
			SceneObject* myBag = companion->getSlottedObject("inventory");
			SceneObject* destination = myBag != nullptr ? myBag : static_cast<SceneObject*>(companion);

			if (lootBag != nullptr && corpseLootableBy(corpse, owner)) {
				// Collect refs first -- transferring while iterating shifts
				// the container indices under us.
				Vector<ManagedReference<SceneObject*> > lootItems;

				for (int i = 0; i < lootBag->getContainerObjectsSize(); ++i) {
					ManagedReference<SceneObject*> item = lootBag->getContainerObject(i);

					if (item != nullptr && item->isTangibleObject()) {
						lootItems.add(item);
					}
				}

				for (int i = 0; i < lootItems.size(); ++i) {
					ManagedReference<SceneObject*> item = lootItems.get(i);

					Locker itemLocker(item, companion);

					if (destination->transferObject(item, -1, true)) {
						state->lootedItemIDs.add(item->getObjectID());
					}
				}

				// Corpse cash (2026-07-18 follow-up, per user request).
				// Deferred to delivery time (2026-08-10 follow-up) -- credits
				// used to land in the owner's account immediately on kill,
				// while items rode home in the companion's bag until it
				// walked back. That mismatch is the bug: cash is now just
				// tallied here and actually paid out in the Delivery phase
				// below, at the exact same moment items are handed over, so
				// both arrive together only once the companion is physically
				// back with the owner.
				int cashCredits = corpse->getCashCredits();

				if (cashCredits > 0) {
					int luck = owner->getSkillMod("force_luck");

					if (luck > 0) {
						cashCredits += (cashCredits * luck) / 20;
					}

					corpse->clearCashCredits();
					state->lootedCash += cashCredits;
				}

				corpse->notifyObservers(ObserverEventType::LOOTCREATURE, owner, 0);

				// COMPANION_CORPSE_DESPAWN_FIX_2026_08_04 -- bring the corpse's
				// despawn forward, exactly as PlayerManagerImplementation::
				// lootAll() does in both of its exit paths (3733 and 3749). The
				// sweep never did this, so a corpse the companion had fully
				// emptied still sat out its whole timer.
				//
				// Deliberately deferred to a scheduled task with NO locks held:
				// shouldRescheduleCorpseDestruction() takes Locker(player, ai)
				// and we are currently holding BOTH the companion and the corpse.
				// A three-way lock is exactly how Core3 deadlocks. Same
				// scheduled-task idiom as CompanionLootResendLambda below. The
				// engine decides whether the corpse actually qualifies --
				// shouldRescheduleCorpseDestruction() still refuses while loot,
				// cash or an unharvested carcass remains.
				{
					ManagedReference<CreatureObject*> corpseRef = corpse;
					ManagedReference<CreatureObject*> looterRef = owner;

					Core::getTaskManager()->scheduleTask([corpseRef, looterRef] () {
						CreatureObject* deadOne = corpseRef.get();
						CreatureObject* looter = looterRef.get();

						if (deadOne == nullptr || looter == nullptr) {
							return;
						}

						ZoneServer* zs = deadOne->getZoneServer();

						if (zs == nullptr) {
							return;
						}

						ManagedReference<PlayerManager*> pm = zs->getPlayerManager();

						if (pm != nullptr) {
							pm->rescheduleCorpseDestruction(looter, deadOne);
						}
					}, "CompanionCorpseDespawnLambda", 250);
				}
			}

			// Ranger harvest (chosen resource; ask once if never chosen).
			if (companionHasRangerTraining(companion) && corpseHarvestableBy(corpse, owner)) {
				int preference = companion->getHarvestPreference();

				if (preference == 0) {
					// Ask the owner ONCE -- this sweep skips harvesting,
					// every later one uses the stored answer (also
					// changeable any time from the radial Harvesting
					// submenu).
					CompanionHarvestChoiceSuiCallback::sendChoiceBox(owner, companion);
				} else {
					Creature* creature = cast<Creature*>(corpse);

					if (creature != nullptr) {
						companionHarvestCorpse(companion, owner, creature, preference);
					}
				}
			}

			state->corpseIDs.remove(0);
			companion->clearPatrolPoints();
		}

		// ---- Delivery phase ----------------------------------------------
		// 2026-08-10: cash no longer skips the walk-back -- it used to be
		// credited on the spot at kill time (see corpse-cash block above),
		// so a cash-only haul short-circuited straight to endSweep() here
		// with just a chat line. Now cash rides along with items and isn't
		// paid out until the companion is actually back in reach, so this
		// early exit only fires when there is truly nothing to deliver.
		if (state->lootedItemIDs.size() == 0 && state->lootedCash == 0) {
			endSweep(true);
			return;
		}

		Vector3 ownerWorld = owner->getWorldPosition();
		float dx = companion->getPositionX() - ownerWorld.getX();
		float dy = companion->getPositionY() - ownerWorld.getY();

		if ((dx * dx + dy * dy) > (LOOT_SWEEP_REACH * LOOT_SWEEP_REACH)) {
			// Run the spoils back to the owner.
			companion->setCompanionState(CompanionObject::FOLLOW);
			companion->setFollowObject(owner);
			companion->setFollowState(AiAgent::FOLLOWING); // genesis port: was setMovementState()

			scheduleSweepStep(companionRef, ownerRef, state, 400);
			return;
		}

		// Hand everything over -- same destroy-first/silent-transfer/deferred
		// re-create idiom as unequipItemToInventory() (see its doc comments;
		// server-initiated cross-creature moves desync the client otherwise).
		ObjectController* objectController = zoneServer->getObjectController();
		SceneObject* ownerInventory = owner->getSlottedObject("inventory");
		int delivered = 0;

		if (objectController != nullptr && ownerInventory != nullptr) {
			Locker inventoryLocker(ownerInventory, companion);

			for (int i = 0; i < state->lootedItemIDs.size(); ++i) {
				ManagedReference<SceneObject*> itemObj = zoneServer->getObject(state->lootedItemIDs.get(i));

				if (itemObj == nullptr || itemObj->getRootParent() != companion) {
					continue;
				}

				String errorDescription;

				if (ownerInventory->canAddObject(itemObj, -1, errorDescription) != 0) {
					if (!state->saidInventoryFull) {
						state->saidInventoryFull = true;
						companionSweepSay(companion, "Your pack's full -- I'll hold the rest in mine.");
					}

					continue;
				}

				Locker itemLocker(itemObj, companion);

				itemObj->broadcastDestroy(itemObj, true);

				if (objectController->transferObject(itemObj, ownerInventory, -1, false)) {
					++delivered;

					ManagedReference<SceneObject*> itemRef = itemObj;
					ManagedReference<CreatureObject*> requesterRef = owner;

					Core::getTaskManager()->scheduleTask([itemRef, requesterRef] () {
						SceneObject* item = itemRef.get();
						CreatureObject* requester = requesterRef.get();

						if (item == nullptr || requester == nullptr) {
							return;
						}

						Locker locker(item);
						item->sendTo(requester, true);
					}, "CompanionLootResendLambda", 400);
				} else {
					companion->broadcastObject(itemObj.castTo<TangibleObject*>(), true);
				}
			}
		}

		// Pay out the tallied cash now -- the companion is confirmed within
		// LOOT_SWEEP_REACH of the owner at this point (see the distance
		// check above), so this is the delivery-time equivalent of the old
		// immediate-credit block that used to run back at the corpse.
		if (state->lootedCash > 0) {
			Locker ownerLocker(owner, companion);

			TransactionLog trx(companion, owner, TrxCode::NPCLOOTCLAIM, state->lootedCash, true);
			owner->addCashCredits(state->lootedCash, true);

			StringIdChatParameter param("base_player", "prose_coin_loot"); // You loot %DI credits from %TT.
			param.setDI(state->lootedCash);
			param.setTT(companion->getObjectID());
			owner->sendSystemMessage(param);
		}

		if (delivered > 0 || state->lootedCash > 0) {
			String haul = "Here you go -- " + String::valueOf(delivered) + (delivered == 1 ? " item" : " items");

			if (state->lootedCash > 0) {
				haul += " and " + String::valueOf(state->lootedCash) + " credits";
			}

			haul += " from the battlefield!";
			companionSweepSay(companion, haul);
		}

		endSweep(true);
	}

	void scheduleSweepStep(Reference<CompanionObject*> companionRef, Reference<CreatureObject*> ownerRef, Reference<CompanionSweepState*> state, int delayMs) {
		Core::getTaskManager()->scheduleTask([companionRef, ownerRef, state] () {
			runSweepStep(companionRef, ownerRef, state);
		}, "CompanionLootSweepStepLambda", delayMs);
	}

	// -------------------------------------------------------------------
	// Companion System (2026-07-29, MEDIC_AUTOHEAL_RESTOCK_FEATURE): Medic
	// auto-heal + auto-restock. Nick's combined request: "a medic
	// companions main roll is to heal the group, we need to have the medic
	// automatically craft stim heals on the fly and heal the group member
	// once their health reaches 20% ... always have 3 stacks on hand ...
	// using the best stims they can make and use" -- scope explicitly
	// confirmed as the FULL real SWG group (/creategroup) union the
	// owner's own companion squad. See docs/companion_system/NOTES.md for
	// the full design writeup.
	// -------------------------------------------------------------------

	constexpr int MEDIC_HEAL_THRESHOLD_PCT = 20;
	constexpr int MEDIC_STOCK_TARGET_CHARGES = 30; // 3 stacks x 10 charges/stim
	constexpr int MEDIC_STIM_CHARGES_PER_ITEM = 10; // matches the confirmed StimPackImplementation useCount=10 baseline (NOTES.md) -- if a live datatable ever differs, restock just self-corrects a tick later since onHand is always recounted from real getUseCount() values, never assumed
	constexpr uint64 MEDIC_HEAL_TARGET_COOLDOWN_MS = 4000;

	// Best-effort per-owner dedupe so only ONE Medic-trained companion
	// attempts a RESTOCK pass per owner at a time (avoids two medics racing
	// to craft the same batch). Same SortedVector<uint64> pattern as
	// CampDeploymentManager's activeCampAmbiance (CampDeploymentManager.h)
	// -- reused, not reinvented, including that precedent's own accepted
	// risk level (no extra Mutex -- a rare true concurrent hit just means
	// two medics occasionally restock in the same 2s window, not a crash).
	// Deliberately NOT used to gate healing -- multiple medics healing
	// DIFFERENT wounded targets in the same tick is desirable, not a race;
	// the per-target cooldown map below is what actually prevents a
	// double-heal on the SAME target.
	SortedVector<uint64> activeMedicRestockOwners;

	bool claimMedicRestockSlot(uint64 ownerID) {
		if (activeMedicRestockOwners.contains(ownerID)) {
			return false;
		}

		activeMedicRestockOwners.put(ownerID);
		return true;
	}

	void releaseMedicRestockSlot(uint64 ownerID) {
		activeMedicRestockOwners.drop(ownerID);
	}

	struct MedicRestockSlotGuard {
		uint64 ownerID;
		MedicRestockSlotGuard(uint64 id) : ownerID(id) {}
		~MedicRestockSlotGuard() { releaseMedicRestockSlot(ownerID); }
	};

	// Per-target heal cooldown (a few seconds) so a heal that hasn't
	// landed/ticked yet doesn't get double-triggered next cycle -- covers
	// the real race window between two different companions' independent
	// 2000ms keep-up ticks (see startKeepUpMonitor()) both reading a
	// target's HP before either's heal has actually applied. VectorMap's
	// put() overwrites in place (ALLOW_OVERWRITE), get() returns 0 (the
	// default null value) for a key that was never set.
	VectorMap<uint64, uint64> medicHealCooldowns;

	uint64 getMedicHealCooldown(uint64 targetID) {
		return medicHealCooldowns.get(targetID);
	}

	void setMedicHealCooldown(uint64 targetID, uint64 untilMs) {
		medicHealCooldowns.put(targetID, untilMs);
	}

	bool medicIsTrained(CompanionObject* companion) {
		if (companion == nullptr) {
			return false;
		}

		for (int i = 0; i < companion->getLearnedSkillCount(); ++i) {
			if (companion->getLearnedSkill(i).beginsWith("science_medic_")) {
				return true;
			}
		}

		return false;
	}

	/**
	 * Full heal scope, per Nick's explicit scope choice: the UNION of (a)
	 * the owner's real SWG group (/creategroup) if grouped, else just the
	 * owner alone, AND (b) the owner's own companion squad (same
	 * datapad-scan shape as CompanionMenuComponent.cpp's
	 * resolveAllActiveCompanionsForBuff() / CompanionGearExchangeManager.h's
	 * resolveActiveCompanions() -- duplicated here per this project's own
	 * per-file-copy convention for small companion-scan helpers).
	 */
	void resolveMedicHealScope(CreatureObject* owner, Vector<ManagedReference<CreatureObject*> >& out) {
		if (owner == nullptr) {
			return;
		}

		GroupObject* group = owner->getGroup();

		if (group != nullptr) {
			for (int i = 0; i < group->getGroupSize(); ++i) {
				CreatureObject* member = group->getGroupMember(i);

				if (member != nullptr && member->getZone() != nullptr) {
					out.add(member);
				}
			}
		} else {
			out.add(owner);
		}

		ManagedReference<SceneObject*> datapad = owner->getSlottedObject("datapad");

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

			if (comp == nullptr || comp->getZone() == nullptr || comp->getLinkedCreature().get() != owner) {
				continue;
			}

			bool alreadyPresent = false;

			for (int j = 0; j < out.size(); ++j) {
				if (out.get(j).get() == static_cast<CreatureObject*>(comp)) {
					alreadyPresent = true;
					break;
				}
			}

			if (!alreadyPresent) {
				out.add(comp);
			}
		}
	}

	// True only for the plain single-target med stimpacks (med_stimpack_a
	// .. med_stimpack_e) -- excludes the area/range/state variants that
	// live in the same chemistry/ folder but are a different item family.
	// Returns the StimPackTemplate::STIM_A..STIM_E rank (1..5), or 0 if not
	// a match.
	int plainStimTierRank(const String& schematicOrItemPath) {
		if (schematicOrItemPath.indexOf("med_stimpack_area_") != -1
				|| schematicOrItemPath.indexOf("med_stimpack_range_") != -1
				|| schematicOrItemPath.indexOf("med_stimpack_state_") != -1) {
			return 0;
		}

		if (schematicOrItemPath.endsWith("_e.iff")) {
			return 5;
		}

		if (schematicOrItemPath.endsWith("_d.iff")) {
			return 4;
		}

		if (schematicOrItemPath.endsWith("_c.iff")) {
			return 3;
		}

		if (schematicOrItemPath.endsWith("_b.iff")) {
			return 2;
		}

		if (schematicOrItemPath.endsWith("_a.iff")) {
			return 1;
		}

		return 0;
	}

	/**
	 * Best plain-stim schematic this medic's OWN learned skills grant --
	 * walks getSchematicsGranted() exactly like CompanionCraftingManager::
	 * findSchematicForComponent() does (same enumeration shape, reused
	 * intentionally instead of hardcoding tier D, so a dual-trained/
	 * future-retiered medic still picks up the real cap). Returns the
	 * schematic's own draft-schematic template path (what craftBatch()
	 * takes as its draftSchematicTemplate argument) and the STIM_A..
	 * STIM_E rank; false if this medic can't make any plain med stimpack
	 * at all.
	 */
	bool medicBestCraftableStimSchematic(CompanionObject* medic, String& outSchematicPath, int& outRank) {
		if (medic == nullptr) {
			return false;
		}

		int bestRank = 0;
		String bestPath;

		for (int s = 0; s < medic->getLearnedSkillCount(); ++s) {
			Skill* skill = SkillManager::instance()->getSkill(medic->getLearnedSkill(s));

			if (skill == nullptr) {
				continue;
			}

			const Vector<String>* groups = skill->getSchematicsGranted();

			if (groups == nullptr) {
				continue;
			}

			for (int g = 0; g < groups->size(); ++g) {
				DraftSchematicGroup* group = SchematicMap::instance()->getGroup(groups->get(g));

				if (group == nullptr) {
					continue;
				}

				for (int d = 0; d < group->size(); ++d) {
					DraftSchematic* schematic = group->get(d).get();

					if (schematic == nullptr || schematic->getObjectTemplate() == nullptr) {
						continue;
					}

					String path = schematic->getObjectTemplate()->getFullTemplateString();

					if (path.indexOf("/chemistry/med_stimpack_") == -1) {
						continue;
					}

					int rank = plainStimTierRank(path);

					if (rank > bestRank) {
						bestRank = rank;
						bestPath = path;
					}
				}
			}
		}

		if (bestRank == 0) {
			return false;
		}

		outSchematicPath = bestPath;
		outRank = bestRank;
		return true;
	}

	/** Best stimpack currently in the medic's own bag, regardless of tier --
	 * "use whatever tier IS on hand" for the reactive heal path (Nick's
	 * explicit edge-case instruction; only skip healing if genuinely zero
	 * stims of any tier exist). Same inventory-scan filter shape as
	 * HealDamageCommand.h's findStimPack(), minus its melee/ranged split
	 * (a background auto-heal always uses the plain melee-style stim). */
	StimPack* medicFindBestStimOnHand(CompanionObject* medic) {
		SceneObject* inventory = medic->getSlottedObject("inventory");

		if (inventory == nullptr) {
			return nullptr;
		}

		StimPack* best = nullptr;

		for (int i = 0; i < inventory->getContainerObjectsSize(); ++i) {
			SceneObject* item = inventory->getContainerObject(i);

			if (item == nullptr || !item->isPharmaceuticalObject()) {
				continue;
			}

			PharmaceuticalObject* pharma = cast<PharmaceuticalObject*>(item);

			if (!pharma->isStimPack() || pharma->isRangedStimPack() || pharma->isPetStimPack() || pharma->isDroidRepairKit()) {
				continue;
			}

			StimPack* stim = cast<StimPack*>(pharma);

			if (stim->getUseCount() <= 0) {
				continue;
			}

			if (best == nullptr || stim->getMedicineClass() > best->getMedicineClass()) {
				best = stim;
			}
		}

		return best;
	}

	// Sum of getUseCount() across every plain stim of the given
	// StimPackTemplate::STIM_A..STIM_E rank currently in the medic's own
	// bag -- "a stack" for this item type is one tangible carrying
	// useCount=10, so 3 stacks = 30 total charges, per NOTES.md.
	int medicCountStimChargesOnHand(CompanionObject* medic, int medicineClassRank) {
		SceneObject* inventory = medic->getSlottedObject("inventory");

		if (inventory == nullptr) {
			return 0;
		}

		int total = 0;

		for (int i = 0; i < inventory->getContainerObjectsSize(); ++i) {
			SceneObject* item = inventory->getContainerObject(i);

			if (item == nullptr || !item->isPharmaceuticalObject()) {
				continue;
			}

			PharmaceuticalObject* pharma = cast<PharmaceuticalObject*>(item);

			if (!pharma->isStimPack() || pharma->isRangedStimPack() || pharma->isPetStimPack() || pharma->isDroidRepairKit()) {
				continue;
			}

			StimPack* stim = cast<StimPack*>(pharma);

			if (stim->getMedicineClass() != medicineClassRank) {
				continue;
			}

			total += stim->getUseCount();
		}

		return total;
	}

	/**
	 * Core heal-application, mirroring HealDamageCommand.h::doQueueCommand()
	 * 's real heal-application shape (calculatePower() -> healDamage() per
	 * attribute -> decreaseUseCount()) -- reused rather than reinvented,
	 * per the design brief. Deliberately skips that command's player-
	 * issued-command-only ergonomics (mind-cost self-inflict, TEF checks,
	 * distance/LOS gating, XP award, canPerformSkill()'s skill-mod gate)
	 * since this is an NPC background action, not a player-typed
	 * /healdamage -- those pieces exist to keep a PLAYER's manual heal
	 * spam in check, not to gate whether an autonomous companion medic may
	 * act.
	 */
	void medicApplyHeal(CompanionObject* medic, CreatureObject* target, StimPack* stimPack) {
		uint32 stimPower = stimPack->calculatePower(medic, target);

		Vector<byte> atts = stimPack->getAttributes();
		bool notifyObservers = true;

		if (atts.contains(CreatureAttribute::HEALTH)) {
			target->healDamage(medic, CreatureAttribute::HEALTH, stimPower, true, notifyObservers);
			notifyObservers = false;
		}

		if (atts.contains(CreatureAttribute::ACTION)) {
			target->healDamage(medic, CreatureAttribute::ACTION, stimPower, true, notifyObservers);
			notifyObservers = false;
		}

		if (atts.contains(CreatureAttribute::MIND)) {
			target->healDamage(medic, CreatureAttribute::MIND, stimPower, true, notifyObservers);
			notifyObservers = false;
		}

		Locker stimLocker(stimPack, medic);
		stimPack->decreaseUseCount();
		stimLocker.release();

		target->playEffect("clienteffect/healing_healdamage.cef", "");
		medic->doAnimation(medic == target ? "heal_self" : "heal_other");
		medic->notifyObservers(ObserverEventType::MEDPACKUSED);
	}

	/**
	 * After a successful craftBatch() for a plain med stimpack schematic,
	 * the finished factory crate lands in the OWNER's inventory
	 * (craftBatch()'s own unchanged behavior) rather than the medic's own
	 * bag -- the same "owner-vs-companion inventory" gotcha the Doctor
	 * Buff Radial craft-then-buff fix (CompanionMenuComponent.cpp's
	 * moveFreshEnhancePackToDoctor()) already had to work around, though
	 * that exact helper doesn't apply here since it moves a single ready-
	 * to-use craftItem() result, not a craftBatch() factory crate. Finds
	 * the crate this medic just produced (identified by matching tier +
	 * craftersID, which craftBatch() stamps onto the prototype) and drains
	 * every unit of it straight into the medic's own inventory via the
	 * same FactoryCrate::extractObjectToInventory() the real client-facing
	 * "Extract Object" radial uses (ExtractObjectCommand.h) -- not
	 * pre-locked here, matching that command's own calling convention
	 * exactly (the method locks the crate itself internally).
	 */
	void medicDrainFreshStimCrate(CreatureObject* owner, CompanionObject* medic, int medicineClassRank) {
		ManagedReference<SceneObject*> ownerInventory = owner->getSlottedObject("inventory");

		if (ownerInventory == nullptr) {
			return;
		}

		for (int i = 0; i < ownerInventory->getContainerObjectsSize(); ++i) {
			ManagedReference<SceneObject*> obj = ownerInventory->getContainerObject(i);

			if (obj == nullptr || !obj->isFactoryCrate()) {
				continue;
			}

			FactoryCrate* crate = cast<FactoryCrate*>(obj.get());
			Reference<TangibleObject*> proto = crate->getPrototype();

			if (proto == nullptr || !proto->isPharmaceuticalObject()) {
				continue;
			}

			PharmaceuticalObject* protoPharma = cast<PharmaceuticalObject*>(proto.get());

			if (!protoPharma->isStimPack() || protoPharma->isRangedStimPack() || protoPharma->isPetStimPack() || protoPharma->isDroidRepairKit()) {
				continue;
			}

			StimPack* protoStim = cast<StimPack*>(protoPharma);

			// genesis port: was proto->getCraftersID() != medic->getObjectID() -- genesis's
			// TangibleObject stores the crafter by NAME (craftersName, TangibleObject.idl:67 /
			// getCraftersName() :695); there is no craftersID field. Compared against
			// getDisplayedName() rather than getFirstName() because getDisplayedName() is
			// exactly what CompanionCraftingManager stamps into setCraftersName() on the
			// prototype, so this stays an exact round-trip match.
			if (protoStim->getMedicineClass() != medicineClassRank || proto->getCraftersName() != medic->getDisplayedName()) {
				continue;
			}

			while (crate->getUseCount() > 0) {
				if (!crate->extractObjectToInventory(medic)) {
					break;
				}
			}

			if (crate->getUseCount() <= 0) {
				crate->destroyObjectFromDatabase(true);
			}

			return;
		}
	}

	/**
	 * Companion System (2026-07-29, Medic auto-heal + auto-restock): called
	 * once per keep-up tick (every 2000ms, see startKeepUpMonitor()) for
	 * EVERY companion, gated internally on being Medic-trained
	 * (science_medic_* -- same skill prefix CompanionDialogMenuSuiCallback
	 * / CompanionMenuComponent already use for Doctor Buff Radial). Two
	 * independent jobs:
	 *  1. Reactive heal-at-20%: ALWAYS runs, regardless of combat state --
	 *     that's exactly when it's needed. Heals the weakest member of the
	 *     full heal scope (owner's real group union owner's own companion
	 *     squad, per Nick's explicit scope choice -- resolveMedicHealScope())
	 *     using the best stim currently on hand.
	 *  2. Proactive restock-to-3-stacks: ONLY when neither the medic nor
	 *     the owner is in combat (crafting mid-fight is impractical) --
	 *     tops the medic's own best-craftable-tier stim count back up to
	 *     30 charges (3 stacks of 10) via ONE CompanionCraftingManager::
	 *     craftBatch() call, gated by a per-owner dedupe so only one of the
	 *     owner's medics restocks at a time.
	 * `medic` is already locked by the caller (runKeepUpTick(), same
	 * convention every other keep-up-tick-adjacent helper in this file
	 * uses); cross-locks below follow that same "primary already held,
	 * secondary via Locker(x, primary)" shape (see e.g. runSweepStep()'s
	 * `Locker ownerLocker(owner, companion)`).
	 */
	void runMedicAutoCareTick(CompanionObject* medic, CreatureObject* owner) {
		if (medic == nullptr || owner == nullptr || !medicIsTrained(medic)) {
			return;
		}

		// ---- 1. Reactive heal-at-20% (always, regardless of combat state) ----

		Vector<ManagedReference<CreatureObject*> > scope;
		resolveMedicHealScope(owner, scope);

		CreatureObject* weakest = nullptr;
		int weakestPct = 101;
		uint64 now = System::getMiliTime();

		for (int i = 0; i < scope.size(); ++i) {
			CreatureObject* member = scope.get(i).get();

			if (member == nullptr || member->getZone() == nullptr || member->isDead()) {
				continue;
			}

			int maxHealth = member->getMaxHAM(CreatureAttribute::HEALTH);

			if (maxHealth <= 0) {
				continue;
			}

			int pct = (member->getHAM(CreatureAttribute::HEALTH) * 100) / maxHealth;

			if (pct > MEDIC_HEAL_THRESHOLD_PCT) {
				continue;
			}

			if (getMedicHealCooldown(member->getObjectID()) > now) {
				continue;
			}

			if (pct < weakestPct) {
				weakestPct = pct;
				weakest = member;
			}
		}

		if (weakest != nullptr) {
			StimPack* stimPack = medicFindBestStimOnHand(medic);

			if (stimPack != nullptr) {
				Locker targetLocker(weakest, medic);

				// Re-check under lock -- state may have changed (healed by
				// someone else, died, logged off/left range) between the
				// scan above and now.
				int maxHealth = (weakest->getZone() != nullptr && !weakest->isDead()) ? weakest->getMaxHAM(CreatureAttribute::HEALTH) : 0;

				if (maxHealth > 0 && (weakest->getHAM(CreatureAttribute::HEALTH) * 100) / maxHealth <= MEDIC_HEAL_THRESHOLD_PCT) {
					medicApplyHeal(medic, weakest, stimPack);
					setMedicHealCooldown(weakest->getObjectID(), now + MEDIC_HEAL_TARGET_COOLDOWN_MS);
				}
			} else {
				// Companion Reactions (2026-07-30 patch, spec part 8) --
				// someone needs healing and this medic has genuinely zero
				// stims of any tier on hand. announceReaction() is
				// cooldown-gated internally, so this is safe to call every
				// time this branch is hit.
				CompanionChatter::announceReaction(medic, owner, "outofstims");
			}
		}

		// ---- 2. Proactive restock-to-3-stacks (combat-free only) ----

		if (medic->isInCombat() || owner->isInCombat()) {
			return;
		}

		uint64 ownerID = owner->getObjectID();

		if (!claimMedicRestockSlot(ownerID)) {
			return; // another of this owner's medics is already handling it
		}

		MedicRestockSlotGuard restockGuard(ownerID);

		String schematicPath;
		int rank = 0;

		if (!medicBestCraftableStimSchematic(medic, schematicPath, rank)) {
			return; // this medic can't make any plain stim at all
		}

		int onHand = medicCountStimChargesOnHand(medic, rank);

		if (onHand >= MEDIC_STOCK_TARGET_CHARGES) {
			return;
		}

		int quantity = (MEDIC_STOCK_TARGET_CHARGES - onHand + MEDIC_STIM_CHARGES_PER_ITEM - 1) / MEDIC_STIM_CHARGES_PER_ITEM;

		if (quantity < 1) {
			return;
		}

		String errorMessage;
		Locker ownerLocker(owner, medic);
		bool ok = CompanionCraftingManager::instance()->craftBatch(owner, medic, schematicPath, quantity, errorMessage);
		ownerLocker.release();

		if (ok) {
			medicDrainFreshStimCrate(owner, medic, rank);
		}
	}

}

namespace {

	// Companion Personality/Flee/Self-buff/Idle-emote patch (2026-07-30).
	// All free functions taking CompanionObject*/CreatureObject* params,
	// same shape as runMedicAutoCareTick() above -- NOT
	// CompanionObjectImplementation:: members. A brand-new member method
	// would need a matching "native" declaration added to
	// CompanionObject.idl (confirmed: CompanionObjectImplementation.h is
	// fully autogenerated from that .idl -- no hand-maintained header
	// exists), which is out of scope for this single .cpp-file patch.
	// These are called from the real, already-idl-declared
	// runKeepUpTick()/startKeepUpMonitor() native methods below instead.

	/**
	 * Shared standing-order restore -- mirrors runSweepStep()'s endSweep()
	 * lambda and recoverFromAbortedIntercept() above EXACTLY (same
	 * standingOrder-based STAY / GUARD / FOLLOW+escortTarget branches).
	 * Duplicated rather than reused because recoverFromAbortedIntercept()
	 * is gated on companionState == ATTACK (an aborted-intercept
	 * precondition that doesn't hold for a flee recovery), and endSweep()
	 * is a capture-local lambda inside runSweepStep() -- neither is
	 * reachable from here without changing their existing gating.
	 */
	void restoreStandingPosture(CompanionObject* companion, CreatureObject* owner) {
		if (companion == nullptr || owner == nullptr || companion->isDead()) {
			return;
		}

		int standing = companion->getStandingOrder();

		if (standing == CompanionObject::STAY) {
			companion->setCompanionState(CompanionObject::STAY);
			companion->setFollowObject(nullptr);
			companion->setOblivious();
		} else if (standing == CompanionObject::GUARD) {
			CreatureObject* guardTarget = companion->getGuardTarget().get();

			if (guardTarget != nullptr && guardTarget->getZone() != nullptr) {
				companion->setCompanionState(CompanionObject::GUARD);
				companion->setFollowObject(guardTarget);
				companion->setFollowState(AiAgent::FOLLOWING); // genesis port: was setMovementState()
			} else {
				companion->setCompanionState(CompanionObject::FOLLOW);
				companion->setFollowObject(owner);
				companion->setFollowState(AiAgent::FOLLOWING); // genesis port: was setMovementState()
			}
		} else {
			companion->setCompanionState(CompanionObject::FOLLOW);

			CreatureObject* escortTarget = companion->getEscortTarget().get();

			if (escortTarget != nullptr && escortTarget != owner && escortTarget->getZone() != nullptr) {
				companion->setFollowObject(escortTarget);
			} else {
				companion->setFollowObject(owner);
			}

			companion->setFollowState(AiAgent::FOLLOWING); // genesis port: was setMovementState()
		}
	}

	/**
	 * Flee check + recovery (spec part 3). Runs every ~2000ms keep-up
	 * tick. fleeingUntil == 0 means "not currently fleeing" (see
	 * CompanionObject.idl's doc comment, landing via a parallel patch).
	 */
	void runFleeCheckTick(CompanionObject* companion, CreatureObject* owner) {
		if (companion == nullptr || owner == nullptr) {
			return;
		}

		if (companion->getZone() == nullptr || companion->isDead()) {
			return;
		}

		int maxHealth = companion->getMaxHAM(CreatureAttribute::HEALTH);
		int healthPct = maxHealth > 0 ? (companion->getHAM(CreatureAttribute::HEALTH) * 100) / maxHealth : 100;

		int threshold = getFleeHealthThresholdPercent(companion->getPersonalityType());
		uint64 fleeingUntil = companion->getFleeingUntil();

		if (companion->isInCombat() && fleeingUntil == 0 && healthPct <= threshold) {
			CombatManager::instance()->forcePeace(companion);
			companion->setFollowObject(owner);
			companion->setFollowState(AiAgent::FLEEING); // genesis port: was setMovementState()
			companion->setFleeingUntil(System::getMiliTime() + 15000);
			CompanionChatter::announceReaction(companion, owner, "flee");
			return;
		}

		if (fleeingUntil != 0 && (System::getMiliTime() >= fleeingUntil || healthPct >= (threshold + 15))) {
			companion->setFleeingUntil(0);
			restoreStandingPosture(companion, owner);
		}
	}

	/**
	 * Heavy-damage bark (spec part 7). A true
	 * CompanionObjectImplementation::inflictDamage() override was
	 * requested, but both AiAgentImplementation::inflictDamage() overloads
	 * can only be overridden by a class whose own .idl declares a matching
	 * "native" method -- CompanionObjectImplementation.h is entirely
	 * autogenerated from CompanionObject.idl (confirmed -- every
	 * CompanionObjectImplementation:: member defined anywhere in this file
	 * has a corresponding .idl entry, and no hand-maintained header exists
	 * for this class). Adding inflictDamage() here without a matching .idl
	 * declaration would not compile, and adding that declaration is an
	 * .idl edit outside this single-.cpp-file patch's scope. Substituted
	 * with an edge-triggered check in the same tick that already computes
	 * health% for the flee logic above: fires the first tick health% is
	 * observed at/below 30% after having been above it. Poll-granularity
	 * (up to ~2s), not a true per-hit hook, but independent of and
	 * additive to the flee decision above exactly as the spec intended.
	 */
	void runHeavyDamageBarkTick(CompanionObject* companion, CreatureObject* owner) {
		if (companion == nullptr || owner == nullptr) {
			return;
		}

		if (companion->getZone() == nullptr || companion->isDead()) {
			return;
		}

		int maxHealth = companion->getMaxHAM(CreatureAttribute::HEALTH);

		if (maxHealth <= 0) {
			return;
		}

		int healthPct = (companion->getHAM(CreatureAttribute::HEALTH) * 100) / maxHealth;

		static VectorMap<uint64, uint64> lastHealthPctByCompanion; // objectID -> last observed health% (101 = unknown/full)

		uint64 companionID = companion->getObjectID();
		uint64 lastPct = lastHealthPctByCompanion.contains(companionID) ? lastHealthPctByCompanion.get(companionID) : 101;

		if ((uint64) healthPct <= 30 && lastPct > 30) {
			CompanionChatter::announceReaction(companion, owner, "heavydamage");
		}

		lastHealthPctByCompanion.put(companionID, (uint64) healthPct);
	}

	/**
	 * Wipe bark (spec part 8, second bullet). Runs every ~2000ms keep-up
	 * tick, once per companion -- announceReaction() is cooldown-gated
	 * internally, so redundant per-companion calls are expected/safe (no
	 * dedup built here, per spec). Same datapad/CompanionControlDevice
	 * enumeration idiom as resolveMedicHealScope() above.
	 */
	void runWipeCheckTick(CompanionObject* companion, CreatureObject* owner) {
		if (companion == nullptr || owner == nullptr) {
			return;
		}

		if (!(owner->isDead() || owner->isIncapacitated())) {
			return;
		}

		SceneObject* datapad = owner->getSlottedObject("datapad");

		if (datapad == nullptr) {
			return;
		}

		int totalCompanions = 0;
		int downCompanions = 0;

		for (int i = 0; i < datapad->getContainerObjectsSize(); ++i) {
			SceneObject* obj = datapad->getContainerObject(i);

			if (obj == nullptr || !obj->isCompanionControlDevice()) {
				continue;
			}

			CompanionControlDevice* device = cast<CompanionControlDevice*>(obj);
			CompanionObject* comp = device->getCompanionObject();

			if (comp == nullptr || comp->getLinkedCreature().get() != owner) {
				continue;
			}

			++totalCompanions;

			if (device->isCompanionDead() || comp->getZone() == nullptr || comp->isDead() || comp->isIncapacitated()) {
				++downCompanions;
			}
		}

		if (totalCompanions == 0 || downCompanions < totalCompanions) {
			return;
		}

		CompanionChatter::announceReaction(companion, owner, "wipe");
	}

	/**
	 * Cross-feature busy check for the self-buff tick below (spec part 5).
	 * CompanionCraftTheater::activeCraftersByOwner() is the only
	 * cross-feature "is this companion mid-craft" signal actually reachable
	 * from this file -- the Doctor Buff / Heal Wounds / Stim Heal radial
	 * busy flags (doctorBuffCraftBusy()/isWoundHealCraftBusy()/
	 * isStimHealCraftBusy()) live in an ANONYMOUS namespace inside
	 * CompanionMenuComponent.cpp (internal linkage, a different
	 * translation unit) and are not visible here; cross-checking them
	 * would require editing that file, out of scope for this
	 * single-.cpp-file patch. Also confirmed those radial features route
	 * through CompanionFieldStation::begin(), not CompanionCraftTheater,
	 * so this check does not cover them either -- documented scope
	 * limitation, not silently dropped.
	 */
	bool isCraftTheaterBusy(CreatureObject* owner, CompanionObject* companion) {
		if (owner == nullptr || companion == nullptr) {
			return false;
		}

		auto& table = CompanionCraftTheater::activeCraftersByOwner();
		uint64 ownerID = owner->getObjectID();

		if (!table.contains(ownerID)) {
			return false;
		}

		return table.get(ownerID).find(companion->getObjectID()) != -1;
	}

	/**
	 * Self-buff tick (spec part 5). Internal ~30s cooldown (every ~15 of
	 * the 2000ms keep-up ticks) via a companion-objectID-keyed VectorMap,
	 * same cooldown-map idiom as medicHealCooldowns above. Consumable has
	 * no public getter exposing its own buffCRC, so there is no way to
	 * pre-check "does the companion already have THIS item's buff" from
	 * outside ConsumableImplementation -- instead this relies on
	 * consumeByCreature()'s own internal `consumer->hasBuff(buffCRC)`
	 * guard (confirmed in ConsumableImplementation.cpp), which already
	 * no-ops (returns false) if the companion is already under an
	 * EFFECT_ATTRIBUTE/EFFECT_SKILL buff from that exact item --
	 * functionally equivalent to the pre-check the spec described.
	 * Deliberately does NOT trigger a new craft when nothing is on hand
	 * (documented scope cut -- see delivery report).
	 */
	void runSelfBuffTick(CompanionObject* companion, CreatureObject* owner) {
		if (companion == nullptr || owner == nullptr) {
			return;
		}

		if (companion->getZone() == nullptr || companion->isDead() || companion->isIncapacitated()) {
			return;
		}

		if (companion->isInCombat()) {
			return;
		}

		static VectorMap<uint64, uint64> selfBuffNextCheckMs; // objectID -> next-allowed System::getMiliTime()

		uint64 companionID = companion->getObjectID();
		uint64 now = System::getMiliTime();

		if (selfBuffNextCheckMs.contains(companionID) && selfBuffNextCheckMs.get(companionID) > now) {
			return;
		}

		selfBuffNextCheckMs.put(companionID, now + 30000);

		if (isCraftTheaterBusy(owner, companion)) {
			return;
		}

		SceneObject* inventory = companion->getSlottedObject("inventory");

		if (inventory == nullptr) {
			return;
		}

		for (int i = 0; i < inventory->getContainerObjectsSize(); ++i) {
			SceneObject* item = inventory->getContainerObject(i);

			if (item == nullptr || !item->isTangibleObject()) {
				continue;
			}

			TangibleObject* tangible = cast<TangibleObject*>(item);

			if (tangible == nullptr || !tangible->isConsumable()) {
				continue;
			}

			Consumable* consumable = cast<Consumable*>(tangible);

			if (consumable == nullptr) {
				continue;
			}

			if (consumable->consumeByCreature(companion, owner)) {
				return; // fed once this pass -- don't binge the whole bag
			}
		}
	}

	// Auto Skill-Training Walkup (AUTO_SKILL_TRAIN_WALKUP_2026_07_30). Walk-over time budget --
	// how long a pending trainingReadyUntil is allowed to stay set
	// before this tick gives up and restores normal posture. This is a
	// BUDGET TO ARRIVE, not "how long the SUI stays open" -- the flag
	// clears the instant the SUI is actually sent (see
	// scheduleTrainingSuiSend() below), well before this could ever be
	// reached in the common case.
	//
	// Build fix (2026-07-30, build-fix-2): TRAINING_WALKUP_TIMEOUT_MS is
	// no longer (re)defined here -- it moved to the EARLIER forward-
	// declare namespace block right before addExperience(), because this
	// block (opened near runMedicAutoCareTick()) only becomes visible
	// well after addExperience()'s use of the constant. See that earlier
	// declaration (marker: TRAINING_WALKUP_TIMEOUT_MS_MOVED_2026_07_30_BUILD_FIX_2).

	/**
	 * Busy check for the walk-to-train feature (spec: not in combat,
	 * not crafting, not dancing/performing, not fleeing). Reuses the
	 * exact same signals the other 2026-07-30 ticks above already use:
	 * isCraftTheaterBusy() (self-buff tick), companion->isEntertaining()
	 * (native CreatureObject method -- isDancing()||isPlayingMusic(),
	 * covers "dancing/performing"), and getFleeingUntil() != 0 (the
	 * authoritative flee-state flag runFleeCheckTick() itself uses,
	 * rather than re-deriving it from movementState).
	 */
	bool isCompanionBusyForTraining(CompanionObject* companion) {
		if (companion == nullptr) {
			return true;
		}

		if (companion->getZone() == nullptr || companion->isDead() || companion->isIncapacitated()) {
			return true;
		}

		CreatureObject* busyOwner = companion->getLinkedCreature().get();

		if (companion->isInCombat() || companion->isEntertaining() || companion->getFleeingUntil() != 0) {
			return true;
		}

		if (busyOwner != nullptr && isCraftTheaterBusy(busyOwner, companion)) {
			return true;
		}

		return false;
	}

	/**
	 * First untrained, non-auto-grantable, real-cost skill at 100%+ of
	 * its xpCost in the companion's CURRENT tree -- deliberately reuses
	 * only CompanionSkillTrainer::sendSkillTree()'s SECOND source (see
	 * that function's 2026-07-29 doc comment): children of skills the
	 * companion has ALREADY learned, gated by Skill::getSkillsRequired().
	 * Deliberately NOT the owner's-personally-held-skills source --
	 * that would fire this feature for a profession the companion has
	 * never actually started, which isn't "its current tree". Same
	 * percent formula as sendSkillTree()'s costSuffix() lambda
	 * (cappedBalance/xpCost*100); auto-grantable skills are always
	 * free/instant so they're skipped (isAutoGrantable() is public on
	 * CompanionSkillTrainer, same call CompanionMenuComponent.cpp uses).
	 * Read-only -- no lock beyond whatever the caller already holds on
	 * companion.
	 */
	bool findReadyUntrainedSkill(CompanionObject* companion, CreatureObject* owner, String& outSkillName) {
		if (companion == nullptr || owner == nullptr) {
			return false;
		}

		for (int i = 0; i < companion->getLearnedSkillCount(); ++i) {
			const String& learnedName = companion->getLearnedSkill(i);

			int rootFirstUnderscore = learnedName.indexOf("_");
			int rootSecondUnderscore = rootFirstUnderscore >= 0 ? learnedName.indexOf("_", rootFirstUnderscore + 1) : -1;
			String professionRootName = rootSecondUnderscore > 0 ? learnedName.subString(0, rootSecondUnderscore) : learnedName;

			Skill* professionRoot = SkillManager::instance()->getSkill(professionRootName);

			if (professionRoot == nullptr) {
				continue;
			}

			for (int c = 0; c < professionRoot->getTotalChildren(); ++c) {
				const Skill* child = professionRoot->getChildNode(c);

				if (child == nullptr) {
					continue;
				}

				const String& name = child->getSkillName();

				if (companion->hasLearnedSkill(name)) {
					continue;
				}

				if (!name.beginsWith("combat_") && !name.beginsWith("crafting_") && !name.beginsWith("science_")
						&& !name.beginsWith("outdoors_") && !name.beginsWith("social_")) {
					continue;
				}

				const Vector<String>* required = child->getSkillsRequired();
				bool eligible = true;

				if (required != nullptr) {
					for (int r = 0; r < required->size(); ++r) {
						const String& prereq = required->get(r);

						if (!prereq.isEmpty() && !companion->hasLearnedSkill(prereq)) {
							eligible = false;
							break;
						}
					}
				}

				if (!eligible || CompanionSkillTrainer::instance()->isAutoGrantable(name)) {
					continue;
				}

				// Companion System (2026-08-07, per user report of the walk-up
				// nagging loop -- see canOwnerTeachSkill()'s doc comment in
				// CompanionSkillTrainer.h): never advertise a skill as "ready" if
				// trainSkill() would just reject it anyway once selected.
				if (!CompanionSkillTrainer::instance()->canOwnerTeachSkill(owner, name)) {
					continue;
				}

				Skill* costSkill = SkillManager::instance()->getSkill(name);

				if (costSkill == nullptr || costSkill->getXpType().isEmpty() || costSkill->getXpCost() <= 0) {
					continue;
				}

				int xpCost = costSkill->getXpCost();
				int balance = companion->getExperience(costSkill->getXpType());
				int cappedBalance = balance > xpCost ? xpCost : balance;
				int percent = (int) (((float) cappedBalance / (float) xpCost) * 100.0f);

				if (percent >= 100) {
					outSkillName = name;
					return true;
				}
			}
		}

		return false;
	}

	/**
	 * Multi-companion dogpile guard (spec part 4) -- CONFIRMED real
	 * risk, not hypothetical: FOLLOW-state companions live within the
	 * keep-up monitor's ~10m/25m envelope of the owner almost all the
	 * time, so two companions becoming training-ready in the same XP
	 * event (e.g. one crafting batch, or splash combat XP from one
	 * kill) can both detect "arrived" on the very same 2s tick.
	 * sendSkillTree() does NOT queue -- it unconditionally
	 * closeSuiWindowType(COMPANION_SKILL_TREE) then opens a new one of
	 * that SAME window type, so a same-tick second call silently
	 * replaces the first companion's freshly-opened window rather than
	 * stacking or erroring. Staggered here per-owner, same fixed-
	 * increment idiom CompanionChatter::announceOrder() already uses
	 * for its reply spacing (there: 600 + i*500 + jitter; here: a
	 * reserved-slot queue since arrivals aren't one synchronous loop
	 * like announceOrder()'s companions Vector is).
	 */
	VectorMap<uint64, uint64>& nextTrainSuiSlotMs() {
		static VectorMap<uint64, uint64> map; // ownerID -> next reserved send-slot mili time
		return map;
	}

	/** Per-companion "already scheduled, waiting on its stagger slot"
	 * guard -- without this, every 2s tick the companion spends already
	 * arrived-but-not-yet-its-turn would schedule ANOTHER duplicate
	 * send task. Function-local static, same header/file-scope-shared-
	 * static idiom every other cooldown map in this patch batch uses. */
	VectorMap<uint64, bool>& trainSuiSendScheduled() {
		static VectorMap<uint64, bool> map; // companion objectID -> true while a send task is in flight
		return map;
	}

	/** 2026-08-07 -- companion objectID -> mili time the training SUI was
	 * last actually shown to the owner. tryInitiateSkillTrainWalkup()
	 * refuses to re-arm within TRAIN_SUI_RESHOW_COOLDOWN_MS of this, so a
	 * skill that's still "ready" (player hasn't trained it yet) doesn't
	 * cause the box to be resent -- and thus visually replaced -- every
	 * single keep-up tick. */
	VectorMap<uint64, uint64>& trainSuiLastShownMs() {
		static VectorMap<uint64, uint64> map;
		return map;
	}

	/** 2026-08-10, per Nick: "the companion never asked me to train him" --
	 * companion objectID -> consecutive ~2s keep-up ticks the training
	 * walkup was blocked SPECIFICALLY by isInCombat() while a skill was
	 * genuinely 100%+ ready to offer. isInCombat() is the same
	 * defenderList-driven bitmask that runPostCombatSweepCheck() already
	 * documents getting stuck true forever for a bystander/stale-defender
	 * companion (2026-07-29 fix, combatStuckPollCount) -- but that recovery
	 * lives entirely inside the post-combat loot sweep, a SEPARATE poll
	 * loop this walkup tick shares no state with. A companion stuck this
	 * way had no recovery at all here: tryInitiateSkillTrainWalkup() just
	 * bailed at isCompanionBusyForTraining() every single tick, forever,
	 * with the ready skill sitting at 100% and never offered -- confirmed
	 * live (Carbines II sat ready at have=10000/need=5000 and was never
	 * offered after an earlier fight). Same ~40-poll threshold convention
	 * as combatStuckPollCount (this tick is also ~2s, so ~80s) before
	 * force-clearing via CombatManager::forcePeace() and proceeding. */
	VectorMap<uint64, int>& trainingStuckCombatPolls() {
		static VectorMap<uint64, int> map;
		return map;
	}

	/**
	 * Fires once, at (or after) this companion's reserved stagger slot.
	 * Re-verifies EVERYTHING under lock at fire time (zone/dead, still
	 * pending, still near, still not busy, still actually has a ready
	 * skill) rather than trusting the state from when it was scheduled
	 * a few seconds earlier -- covers design point 3 (busy mid-flight)
	 * and "companion trained the skill / owner already worked with it
	 * manually in the meantime" equally. Locking: companion first
	 * (already-established primary-then-cross-lock convention this
	 * file uses throughout, e.g. runMedicAutoCareTick()'s
	 * `Locker ownerLocker(owner, medic)`), THEN owner via
	 * `Locker ownerLocker(owner, companion)` specifically because
	 * sendSkillTree() mutates the owner's ghost (closeSuiWindowType()/
	 * addSuiBox()) -- unlike the read-mostly ambient ticks elsewhere in
	 * this file that only ever read owner via a raw pointer. Always
	 * clears trainingReadyUntil and calls restoreStandingPosture()
	 * before returning, whichever branch it takes -- single cleanup
	 * path, matches flee-recovery's own unconditional-cleanup shape.
	 */
	void fireTrainingSuiSend(Reference<CompanionObject*> companionRef, Reference<CreatureObject*> ownerRef) {
		CompanionObject* companion = companionRef.get();
		CreatureObject* owner = ownerRef.get();

		if (companion == nullptr || owner == nullptr) {
			return;
		}

		Locker locker(companion);

		trainSuiSendScheduled().drop(companion->getObjectID());

		if (companion->getZone() == nullptr || companion->isDead() || companion->getTrainingReadyUntil() == 0) {
			return;
		}

		if (owner->getZone() != companion->getZone() || isCompanionBusyForTraining(companion)) {
			companion->setTrainingReadyUntil(0);
			restoreStandingPosture(companion, owner);
			return;
		}

		Vector3 ownerWorld = owner->getWorldPosition();
		float dx = companion->getPositionX() - ownerWorld.getX();
		float dy = companion->getPositionY() - ownerWorld.getY();
		float distSq = dx * dx + dy * dy;

		if (distSq <= 100.0f) { // still within the same ~10m arrival envelope
			String readySkill;

			if (findReadyUntrainedSkill(companion, owner, readySkill)) {
				Locker ownerLocker(owner, companion);
				CompanionSkillTrainer::instance()->sendSkillTree(owner, companion);
				ownerLocker.release();

				// 2026-08-07 -- mark it shown so tryInitiateSkillTrainWalkup()
				// won't re-arm and resend this for TRAIN_SUI_RESHOW_COOLDOWN_MS,
				// even though the skill itself is still "ready" until the
				// player actually acts on the box.
				auto& lastShown = trainSuiLastShownMs();
				lastShown.drop(companion->getObjectID());
				lastShown.put(companion->getObjectID(), System::getMiliTime());
			}
		}

		companion->setTrainingReadyUntil(0);
		restoreStandingPosture(companion, owner);
	}

	void scheduleTrainingSuiSend(Reference<CompanionObject*> companionRef, Reference<CreatureObject*> ownerRef) {
		CompanionObject* companion = companionRef.get();
		CreatureObject* owner = ownerRef.get();

		if (companion == nullptr || owner == nullptr) {
			return;
		}

		uint64 companionID = companion->getObjectID();

		auto& scheduled = trainSuiSendScheduled();

		if (scheduled.contains(companionID) && scheduled.get(companionID)) {
			return; // already queued, waiting on its stagger slot
		}

		scheduled.drop(companionID);
		scheduled.put(companionID, true);

		uint64 ownerID = owner->getObjectID();
		uint64 now = System::getMiliTime();
		auto& slots = nextTrainSuiSlotMs();
		uint64 slot = slots.contains(ownerID) ? slots.get(ownerID) : now;

		if (slot < now) {
			slot = now;
		}

		slots.drop(ownerID);
		slots.put(ownerID, slot + 1500); // reserve the next companion's slot ~1.5s later

		uint64 delay = slot - now;

		Core::getTaskManager()->scheduleTask([companionRef, ownerRef] () {
			fireTrainingSuiSend(companionRef, ownerRef);
		}, "CompanionTrainingSuiSendLambda", delay);
	}

	/**
	 * Runs every ~2000ms keep-up tick (finer-grained than the 20s
	 * idle-emote tick, needed so a busy-abandon mid-walk -- e.g. pulled
	 * into combat -- is noticed promptly, same cadence runFleeCheckTick()
	 * already uses for the same reason). Only MANAGES an
	 * already-triggered walk-over (trainingReadyUntil != 0); the
	 * trigger itself lives in addExperience(), event-driven. Arrival
	 * check reuses the EXACT same squared-distance formula/threshold
	 * (dx*dx+dy*dy <= 100.0f, i.e. 10m) as runIdleEmoteTick()'s arrival
	 * greet -- not a shared function call (that tick is oriented
	 * around emotes, not a walk state machine), but the identical
	 * idiom, deliberately not reinvented.
	 *
	 * 2026-08-10: closed off this doc comment, which had never actually
	 * been terminated before the next function's own "/**" opened right
	 * inside it (harmless -Wcomment warning, pre-existing, unrelated to
	 * this session's changes -- just cleaning it up while in the area).
	 */

	/**
	 * Companion System (2026-08-07, per user request "as soon as the user
	 * and companions are out of battle, they walk up to the owner and
	 * request training and a pop up right away"): the ORIGINAL Auto
	 * Skill-Training Walkup design (2026-07-30) only checked for a newly-
	 * ready skill at the exact moment addExperience() granted XP -- if the
	 * companion was still busy (almost always true, since combat XP is the
	 * common trigger) the walk-up was abandoned outright and nothing
	 * re-checked it until the NEXT XP grant, which may never arrive once
	 * the fight that made the skill ready is already over. Polled from the
	 * keep-up tick (see the call site next to runSkillTrainWalkupTick())
	 * in addition to addExperience(), so this now also catches "became
	 * eligible while busy" within one 2s tick of combat actually ending.
	 * No-op (returns false) if a walk-over is already pending, the
	 * companion is busy, or nothing is ready -- runSkillTrainWalkupTick()
	 * owns everything past initiation.
	 */
	bool tryInitiateSkillTrainWalkup(CompanionObject* companion, CreatureObject* owner) {
		if (companion == nullptr || owner == nullptr) {
			return false;
		}

		if (companion->getTrainingReadyUntil() != 0) {
			return false; // already pending -- runSkillTrainWalkupTick() owns it
		}

		// 2026-08-07 -- don't re-show the training SUI for a while after it
		// was last actually displayed (see TRAIN_SUI_RESHOW_COOLDOWN_MS's
		// doc comment above): otherwise a skill that's still ready re-arms
		// the walkup every ~2s tick and the box keeps getting replaced
		// before the player can click it.
		auto& lastShown = trainSuiLastShownMs();
		uint64 companionID = companion->getObjectID();

		if (lastShown.contains(companionID)) {
			uint64 sinceShown = System::getMiliTime() - lastShown.get(companionID);

			if (sinceShown < TRAIN_SUI_RESHOW_COOLDOWN_MS) {
				return false;
			}
		}

		// Stuck-combat recovery -- see trainingStuckCombatPolls()'s doc
		// comment just above findReadyUntrainedSkill() for the full
		// rationale. Only probe/intervene when isInCombat() is the SOLE
		// reason isCompanionBusyForTraining() would say busy (not dead,
		// not entertaining, not fleeing, not craft-theater-busy) -- a
		// companion genuinely busy for any other reason is left alone.
		auto& stuckPolls = trainingStuckCombatPolls();

		if (companion->getZone() != nullptr && !companion->isDead() && !companion->isIncapacitated()
				&& companion->isInCombat() && !companion->isEntertaining() && companion->getFleeingUntil() == 0) {
			String probeSkill;

			if (findReadyUntrainedSkill(companion, owner, probeSkill)) {
				int polls = stuckPolls.contains(companionID) ? stuckPolls.get(companionID) : 0;
				++polls;

				if (polls > 40) {
					CombatManager::instance()->forcePeace(companion);
					stuckPolls.drop(companionID);
				} else {
					stuckPolls.drop(companionID);
					stuckPolls.put(companionID, polls);
					return false;
				}
			}
		}

		if (isCompanionBusyForTraining(companion)) {
			return false;
		}

		stuckPolls.drop(companionID);

		String readySkill;

		if (!findReadyUntrainedSkill(companion, owner, readySkill)) {
			return false;
		}

		if (owner->getZone() != companion->getZone()) {
			return false;
		}

		// Same movement mechanism /companionfollow uses --
		// setFollowObject()+setMovementState(FOLLOWING) only, deliberately NOT
		// touching companionState/standingOrder (mirrors runFleeCheckTick()'s
		// flee-entry: temporary movement override now, restoreStandingPosture()
		// puts the real standing order back once this resolves).
		companion->setFollowObject(owner);
		companion->setFollowState(AiAgent::FOLLOWING); // genesis port: was setMovementState()
		companion->setTrainingReadyUntil(System::getMiliTime() + TRAINING_WALKUP_TIMEOUT_MS);
		CompanionChatter::announceReaction(companion, owner, "readytotrain");
		return true;
	}

	/**
	 * Runs every ~2000ms keep-up tick (finer-grained than the 20s
	 * idle-emote tick, needed so a busy-abandon mid-walk -- e.g. pulled
	 * into combat -- is noticed promptly, same cadence runFleeCheckTick()
	 * already uses for the same reason). Only MANAGES an
	 * already-triggered walk-over (trainingReadyUntil != 0); the trigger
	 * itself lives in tryInitiateSkillTrainWalkup() just below, called both
	 * from addExperience() (event-driven) and from the keep-up tick
	 * (state-driven -- see its own doc comment for why). Arrival check
	 * reuses the EXACT same squared-distance formula/threshold
	 * (dx*dx+dy*dy <= 100.0f, i.e. 10m) as runIdleEmoteTick()'s arrival
	 * greet -- not a shared function call (that tick is oriented
	 * around emotes, not a walk state machine), but the identical
	 * idiom, deliberately not reinvented.
	 */
	void runSkillTrainWalkupTick(CompanionObject* companion, CreatureObject* owner) {
		if (companion == nullptr || owner == nullptr) {
			return;
		}

		if (companion->getZone() == nullptr || companion->isDead()) {
			return;
		}

		uint64 pendingUntil = companion->getTrainingReadyUntil();

		if (pendingUntil == 0) {
			return; // nothing pending -- trigger lives in addExperience()
		}

		if (isCompanionBusyForTraining(companion)) {
			// Design point 3 -- abandon cleanly, don't fight the AI combat/
			// craft/dance system for movement control. Will retry next time
			// addExperience() fires while eligible again.
			companion->setTrainingReadyUntil(0);
			restoreStandingPosture(companion, owner);
			return;
		}

		if (System::getMiliTime() >= pendingUntil) {
			// Design point 2 -- owner never showed up / walked away.
			companion->setTrainingReadyUntil(0);
			restoreStandingPosture(companion, owner);
			return;
		}

		Vector3 ownerWorld = owner->getWorldPosition();
		float dx = companion->getPositionX() - ownerWorld.getX();
		float dy = companion->getPositionY() - ownerWorld.getY();
		float distSq = dx * dx + dy * dy;

		if (distSq <= 100.0f) { // 10m, squared -- same idiom as the idle-emote arrival greet
			scheduleTrainingSuiSend(companion, owner);
		}
	}

	/**
	 * Idle-emote tick (spec part 6). Runs from a SEPARATE, lower-frequency
	 * (~20s) self-rescheduling task started alongside the keep-up monitor
	 * (see startKeepUpMonitor() below), not from the 2000ms keep-up tick.
	 */

	/**
	 * Companion System (2026-08-10, per Nick: "my companion goes through
	 * walls" when catching up at a boosted speed after taking a taxi into
	 * a city). Cheap proxy for "is this point on a city street, not open
	 * terrain" -- 2+ real BuildingObjects within DENSE_BUILDING_SCAN_RANGE.
	 * Deliberately does NOT use CityManager/CityRegion: that system tracks
	 * player-FOUNDED cities via a mayor's CityRegion and would miss every
	 * NPC-designed city (Coronet, Theed, Mos Eisley, ...) entirely. Reuses
	 * the same Zone::getInRangeObjects() scan idiom as findNearbySeat()/
	 * the loot sweep, just counting buildings instead of furniture/corpses.
	 * Used by both the taxi driver's pacing tick and the plain-FOLLOW
	 * keep-up boost below to cap catch-up speed (and, for keep-up, crawl
	 * instead of rush) near dense geometry -- open-terrain pacing is
	 * untouched, since that's not where the clipping was reported.
	 */
	bool isNearDenseBuildings(Zone* zone, float x, float y) {
		if (zone == nullptr) {
			return false;
		}

		constexpr float DENSE_BUILDING_SCAN_RANGE = 25.f;
		constexpr int DENSE_BUILDING_THRESHOLD = 2;

		SortedVector<ManagedReference<QuadTreeEntry*>> nearbyObjects;
		zone->getInRangeObjects(x, y, DENSE_BUILDING_SCAN_RANGE, &nearbyObjects, true, true);

		int buildingCount = 0;

		for (int i = 0; i < nearbyObjects.size(); ++i) {
			SceneObject* scno = cast<SceneObject*>(nearbyObjects.get(i).get());

			if (scno != nullptr && scno->isBuildingObject()) {
				++buildingCount;

				if (buildingCount >= DENSE_BUILDING_THRESHOLD) {
					return true;
				}
			}
		}

		return false;
	}

	/**
	 * Companion System (2026-08-10, per Nick: "have them find a seat if
	 * there is one within 15 meters ... search for a seat instead of
	 * sitting on the floor"). There is no dedicated "this is a chair"
	 * flag on the shared object template -- confirmed against this
	 * deployment's own furniture (Small/Large Cantina Seat, Lawn Chair
	 * x3: all SceneObjectType::FURNITURE, all with "chair" or "seat" in
	 * their template basename: shared_frn_tatt_chair_cantina_seat[_2].iff,
	 * shared_camp_chair_s1/s2/s3.iff). Same "no clean data flag, match
	 * the template naming convention instead" idiom
	 * CompanionSkillTrainer::isAutoGrantable() already uses for an
	 * analogous problem. Deliberately excludes non-seat furniture (tables,
	 * lamps, shelves) that also carry SceneObjectType::FURNITURE but never
	 * this naming pattern.
	 */
	bool isSeatFurniture(SceneObject* scno) {
		if (scno == nullptr || scno->getGameObjectType() != SceneObjectType::FURNITURE) {
			return false;
		}

		SharedObjectTemplate* objTemplate = scno->getObjectTemplate();

		if (objTemplate == nullptr) {
			return false;
		}

		String path = objTemplate->getFullTemplateString().toLowerCase();

		static const char* const seatKeywords[] = { "chair", "seat", "couch", "bench", "stool", "sofa", "settee" };

		for (const char* keyword : seatKeywords) {
			if (path.indexOf(keyword) >= 0) {
				return true;
			}
		}

		return false;
	}

	/** Nearest qualifying seat within 15m, if any -- same
	 * Zone::getInRangeObjects() scan idiom the loot sweep already uses
	 * for corpses (LOOT_SWEEP_SCAN_RANGE), just a tighter radius and a
	 * furniture filter instead of a corpse filter. */
	bool findNearbySeat(CompanionObject* companion, uint64& outObjectID) {
		if (companion == nullptr) {
			return false;
		}

		Zone* zone = companion->getZone();

		if (zone == nullptr) {
			return false;
		}

		constexpr float SEAT_SEARCH_RANGE = 15.f;

		// 2026-08-11 FIX (Nick: "still arent sitting down, or finding a
		// chair to sit on" -- the batch-54 eager cantina seat search never
		// worked). getPositionX()/getPositionY() are CELL-LOCAL coordinates
		// whenever the object's root parent is a BuildingObject (see
		// SceneObjectImplementation::getWorldPositionX/Y() -- they only
		// equal getPositionX/Y() outside a building), but a cantina
		// companion is always standing inside the building's interior
		// cell. Zone::getInRangeObjects() operates in ZONE/WORLD space
		// (confirmed by runEntertainerWatchTick()'s own correct use of
		// getWorldPositionX/Y() a few functions up), so this was querying
		// the quadtree around the companion's small cell-relative
		// coordinates as if they were real zone coordinates -- almost
		// always the wrong location in the zone entirely, silently
		// returning zero furniture. Worse, since runCantinaAmbianceTick()
		// only ever makes ONE eager attempt (gated on cantinaAttireActive()
		// being freshly set), that single doomed attempt never got a
		// second try. Switched the search anchor AND the per-candidate
		// distance check to getWorldPositionX/Y() throughout, matching
		// runEntertainerWatchTick()'s already-correct idiom.
		SortedVector<ManagedReference<QuadTreeEntry*>> nearbyObjects;
		zone->getInRangeObjects(companion->getWorldPositionX(), companion->getWorldPositionY(), SEAT_SEARCH_RANGE, &nearbyObjects, true, true);

		SceneObject* nearestSeat = nullptr;
		float nearestDistSq = 0.f;

		for (int i = 0; i < nearbyObjects.size(); ++i) {
			SceneObject* scno = cast<SceneObject*>(nearbyObjects.get(i).get());

			if (!isSeatFurniture(scno)) {
				continue;
			}

			float dx = scno->getWorldPositionX() - companion->getWorldPositionX();
			float dy = scno->getWorldPositionY() - companion->getWorldPositionY();
			float distSq = dx * dx + dy * dy;

			if (nearestSeat == nullptr || distSq < nearestDistSq) {
				nearestSeat = scno;
				nearestDistSq = distSq;
			}
		}

		if (nearestSeat == nullptr) {
			return false;
		}

		outObjectID = nearestSeat->getObjectID();
		return true;
	}

	/** Companion objectID -> chosen seat's objectID while walking over to
	 * it (0/absent == not seat-walking). Set by runIdleEmoteTick()'s sit
	 * roll, managed and cleared by runSeatWalkTick() every ~2s keep-up
	 * tick -- same two-part initiate/manage split as
	 * tryInitiateSkillTrainWalkup()/runSkillTrainWalkupTick(), and the
	 * same reason: the ~20s idle-emote tick is too coarse to notice
	 * arrival or a busy-abandon promptly. */
	VectorMap<uint64, uint64>& seatWalkTargetID() {
		static VectorMap<uint64, uint64> map;
		return map;
	}

	/** Companion objectID -> 1 iff WE (the idle-emote/seat-walk logic)
	 * are the reason this companion is currently sitting -- hoisted out
	 * of runIdleEmoteTick() into a shared class-level accessor (same
	 * pattern as seatWalkTargetID() just above) so BOTH runIdleEmoteTick()
	 * (immediate floor-sit path, and the "stand back up when busy" check)
	 * AND runSeatWalkTick() (walked-to-a-seat arrival path) mark/read the
	 * exact same underlying state. Before this, runSeatWalkTick() had no
	 * way to tell runIdleEmoteTick() "I just sat this companion down",
	 * so a chair-seated companion was invisible to the stand-up-when-busy
	 * logic. */
	VectorMap<uint64, uint64>& idleSittingCompanionsMap() {
		static VectorMap<uint64, uint64> map;
		return map;
	}

	/**
	 * Runs every ~2000ms keep-up tick. Only manages an already-chosen
	 * seat walk (seatWalkTargetID() has an entry for this companion); the
	 * choice itself is made in runIdleEmoteTick()'s sit roll. Abandons
	 * cleanly (same idiom as runSkillTrainWalkupTick()'s busy-abandon) if
	 * the companion becomes busy, the seat vanishes, or the walk drags on
	 * too long -- never fights the AI combat/craft/dance system for
	 * movement control.
	 */
	void runSeatWalkTick(CompanionObject* companion, CreatureObject* owner) {
		if (companion == nullptr || owner == nullptr) {
			return;
		}

		auto& targets = seatWalkTargetID();
		uint64 companionID = companion->getObjectID();

		if (!targets.contains(companionID)) {
			return; // nothing pending
		}

		uint64 seatID = targets.get(companionID);

		if (companion->getZone() == nullptr || companion->isDead() || companion->isIncapacitated()
				|| companion->isInCombat() || companion->isEntertaining() || companion->isTaxiActive()
				|| companion->isLootSweepActive() || isCraftTheaterBusy(owner, companion)) {
			targets.drop(companionID);
			companion->clearPatrolPoints();
			restoreStandingPosture(companion, owner);
			return;
		}

		ManagedReference<SceneObject*> seatObj = companion->getZoneServer()->getObject(seatID);
		SceneObject* seat = seatObj.get();

		if (seat == nullptr || seat->getZone() == nullptr) {
			targets.drop(companionID);
			companion->clearPatrolPoints();
			restoreStandingPosture(companion, owner);
			return;
		}

		constexpr float SEAT_ARRIVAL_REACH = 2.f;

		if (companion->getDistanceTo(seat) > SEAT_ARRIVAL_REACH) {
			companion->setCompanionState(CompanionObject::PATROL);
			companion->setFollowObject(nullptr);

			if (companion->getPatrolPointSize() == 0) {
				// 2026-08-11 FIX (same report as findNearbySeat()'s
				// coordinate fix above): PatrolPoint's 4th arg (CellObject*
				// cell) defaults to nullptr when omitted, which
				// WorldCoordinates then treats as "these X/Y are OUTDOOR
				// zone coordinates" -- but seat->getPositionX/Y() are
				// CELL-LOCAL (the seat is furniture inside the cantina's
				// interior cell). Without the seat's own parent cell
				// attached, the pathfinder aimed the companion at whatever
                                // tiny coordinates the chair happens to have relative to its
				// cell, interpreted as an outdoor location -- nowhere near
				// the actual chair. Passing the seat's parent CellObject
				// (same idiom as FormationManager.cpp's
				// setNextPosition(..., parent.castTo<CellObject*>())) fixes
				// this the same way homeLocation.setCell(cell) does for
				// AiAgent's own patrol points.
				ManagedReference<SceneObject*> seatParent = seat->getParent().get();
				PatrolPoint point(seat->getPositionX(), seat->getPositionZ(), seat->getPositionY(), seatParent.castTo<CellObject*>());
				// setFollowState() calls clearPatrolPoints(), so the state
				// must be set BEFORE the point is queued -- same ordering
				// as the loot sweep's own corpse-walk (runSweepStep()).
				companion->setFollowState(AiAgent::PATROLLING);
				companion->addPatrolPoint(point);
			}

			return; // still walking -- picked back up next tick
		}

		// Arrived, close enough -- sit. Approximate (no per-seat mesh slot
		// data is available server-side the way the client resolves one
		// when a player actually clicks a chair), but close enough to read
		// as "found a seat" rather than sitting on open floor.
		targets.drop(companionID);
		companion->clearPatrolPoints();
		companion->setPosture(CreaturePosture::SITTING, true);

		// Shared with runIdleEmoteTick() via idleSittingCompanionsMap() (see
		// that accessor's own doc comment) so its stand-up-when-busy check
		// recognizes a companion WE just walked over to a seat, exactly as
		// it already recognizes an immediate floor-sit.
		idleSittingCompanionsMap().put(companionID, (uint64) 1);
	}

	/**
	 * Companion System (2026-08-11, per Nick: "the companion is not
	 * sitting or taking off armor or weapon when inside a cantina", and
	 * earlier "if the companion is in a cantina, they should do the same
	 * type unequip like we do in a camp, equip the clothes they have and
	 * unequip the item they are holding"). Cheap "is the owner inside a
	 * cantina" proxy -- walks to the owner's root parent BuildingObject
	 * and checks its gameObjectType against
	 * SceneObjectType::RECREATIONBUILDING (the real designer-data tag SWG
	 * cantina buildings carry), with a template-name "cantina" fallback --
	 * same "no clean flag, match the convention instead" idiom
	 * isSeatFurniture() already uses for chairs. Checked off the OWNER,
	 * not the companion, since a companion is always within a few meters
	 * of its owner while FOLLOWing/idling and "the owner walked into the
	 * cantina" is the actually-meaningful signal.
	 */
	bool isOwnerInCantina(CreatureObject* owner) {
		if (owner == nullptr) {
			return false;
		}

		ManagedReference<SceneObject*> root = owner->getRootParent(); // matches every other getRootParent() call site in this codebase

		if (root == nullptr || !root->isBuildingObject()) {
			return false;
		}

		if (root->getGameObjectType() == SceneObjectType::RECREATIONBUILDING) {
			return true;
		}

		SharedObjectTemplate* objTemplate = root->getObjectTemplate();

		if (objTemplate != nullptr && objTemplate->getFullTemplateString().toLowerCase().indexOf("cantina") >= 0) {
			return true;
		}

		return false;
	}

	/** Companion objectID -> 1 iff THIS tick is the one that stripped the
	 * companion's armor for cantina clothes. Deliberately separate from
	 * CampDeploymentManager's own campAttireRemovedArmor map (which
	 * changeIntoCampClothes()/restoreArmorFromCamp() already guard
	 * themselves against double-triggering) so this tick only ever
	 * restores armor IT removed -- an active camp's own attire state,
	 * removed by the SAME underlying methods for a different reason, is
	 * never touched from here. */
	VectorMap<uint64, uint64>& cantinaAttireActive() {
		static VectorMap<uint64, uint64> map;
		return map;
	}

	/** Companion objectID -> the weapon's objectID THIS tick unequipped for
	 * cantina ambiance (0 == checked, had none equipped). Mirrors
	 * cantinaAttireActive() above but for the weapon slot -- armor removal
	 * reuses CampDeploymentManager's own tracked state via
	 * changeIntoCampClothes()/restoreArmorFromCamp(), but weapon un/re-equip
	 * has no cantina-specific home there (its only existing weapon-unequip
	 * caller is the Entertainer Dance/Watch feature, a different flow), so
	 * it's tracked locally here instead. Added 2026-08-11, per Nick: "they
	 * never took off their weapon" while cantina attire was otherwise
	 * working. */
	VectorMap<uint64, uint64>& cantinaWeaponRemoved() {
		static VectorMap<uint64, uint64> map;
		return map;
	}

	/**
	 * Runs from the ~20s idle-emote tick (see the call site next to
	 * runIdleEmoteTick()). While the owner is inside a cantina and the
	 * companion isn't busy, swaps it into carried civilian clothes via
	 * CampDeploymentManager's own proven changeIntoCampClothes() -- the
	 * exact same swap the camp-ambiance feature already uses, just gated
	 * on "in a cantina" instead of "camp deployed nearby". Restores real
	 * armor the moment the owner leaves the cantina or the companion goes
	 * busy, but only if THIS tick removed it (cantinaAttireActive() above).
	 * Also unequips/re-equips the companion's weapon (cantinaWeaponRemoved()
	 * above) and, the moment cantina clothes go on, makes one immediate
	 * attempt to find and walk to a real seat -- 2026-08-11, per Nick: the
	 * general idle-emote sit roll (runIdleEmoteTick(), ~15% per ~20s tick x
	 * 1-in-4) was too slow to read as real cantina ambiance ("they never sat
	 * down at all and never found a chair to sit on").
	 */
	void runCantinaAmbianceTick(CompanionObject* companion, CreatureObject* owner) {
		if (companion == nullptr || owner == nullptr) {
			return;
		}

		uint64 companionID = companion->getObjectID();
		auto& active = cantinaAttireActive();
		auto& weaponRemoved = cantinaWeaponRemoved();

		bool busy = companion->getZone() == nullptr || companion->isDead() || companion->isIncapacitated()
				|| companion->isInCombat() || companion->isTaxiActive() || companion->isLootSweepActive()
				|| isCraftTheaterBusy(owner, companion);

		bool wantCantinaClothes = !busy && isOwnerInCantina(owner);

		if (wantCantinaClothes) {
			if (!active.contains(companionID)) {
				active.put(companionID, (uint64) 1);
				CampDeploymentManager::instance()->changeIntoCampClothes(companion, owner);

				bool alreadySeated = idleSittingCompanionsMap().contains(companionID) && idleSittingCompanionsMap().get(companionID) != 0;
				bool alreadyWalkingToSeat = seatWalkTargetID().contains(companionID);

				if (!companion->isInCombat() && !alreadySeated && !alreadyWalkingToSeat) {
					uint64 seatObjectID = 0;

					if (findNearbySeat(companion, seatObjectID)) {
						companion->setCompanionState(CompanionObject::PATROL);
						companion->setFollowObject(nullptr);
						seatWalkTargetID().put(companionID, seatObjectID);
					}
				}
			}

			if (!weaponRemoved.contains(companionID)) {
				ManagedReference<WeaponObject*> weapon = companion->getWeapon();
				uint64 weaponID = 0;

				if (weapon != nullptr) {
					weaponID = weapon->getObjectID();
					companion->unequipItemToInventory(weapon, owner);
				}

				weaponRemoved.put(companionID, weaponID);
			}
		} else {
			if (active.contains(companionID)) {
				active.drop(companionID);
				CampDeploymentManager::instance()->restoreArmorFromCamp(companion, owner);
			}

			if (weaponRemoved.contains(companionID)) {
				uint64 weaponID = weaponRemoved.get(companionID);
				weaponRemoved.drop(companionID);

				if (weaponID != 0) {
					Zone* zone = companion->getZone();
					ZoneServer* zoneServer = zone != nullptr ? zone->getZoneServer() : nullptr;
					ManagedReference<SceneObject*> weaponObj = zoneServer != nullptr ? zoneServer->getObject(weaponID) : nullptr;
					TangibleObject* weaponTano = weaponObj != nullptr ? weaponObj->asTangibleObject() : nullptr;

					if (weaponTano != nullptr && weaponTano->getRootParent() == companion) {
						companion->equipItemFromInventory(weaponTano, owner);
					}
				}
			}
		}
	}

	/** Companion objectID -> the real entertainer's objectID this companion
	 * is currently registered as watching/listening to via the STOCK
	 * PlayerManager startWatch()/startListen() machinery (absent == not
	 * currently watching anyone). Companion System (2026-08-11, per Nick:
	 * "they never watched the entertainer that was dancing to heal their
	 * wounds and get a buff") -- deliberately reuses the REAL player
	 * /watch and /listen code paths directly rather than re-implementing
	 * wound-heal/buff math in parallel (the way the companion-PERFORMER
	 * Dance/Watch feature in CampDeploymentManager.cpp had to, since ITS
	 * "entertainer" is a CompanionObject the stock system has never heard
	 * of). Confirmed safe to call with a companion as the WATCHING side:
	 * startWatch()/stopWatch()/startListen()/stopListen() and
	 * EntertainingSessionImplementation's addWatcher()/healWounds()/
	 * activateEntertainerBuff() never call getPlayerObject() on the
	 * watcher/listener/patron parameter, only on the ENTERTAINER (always a
	 * real player here) -- and stopWatch()'s one real player-only
	 * messaging block is already guarded behind `creature->isPlayerCreature()`.
	 * This means wound healing comes for free from the entertainer's own
	 * periodic doEntertainerPatronEffects() task (runs for every
	 * registered watcher/listener regardless of who registered them, and
	 * already self-evicts anyone who wanders past 10m of the entertainer),
	 * and the one-time attribute buff grants automatically when
	 * stopWatch()/stopListen() runs -- exactly like a real patron, with no
	 * new buff-math code needed here at all. */
	VectorMap<uint64, uint64>& entertainerWatchTarget() {
		static VectorMap<uint64, uint64> map;
		return map;
	}

	/**
	 * Runs from the ~20s idle-emote tick (see the call site next to
	 * runIdleEmoteTick()). While idle, looks for the nearest real (player)
	 * entertainer dancing or playing music within 15m (matches
	 * findNearbySeat()'s own "would a companion realistically notice this"
	 * radius) and registers the companion as a watcher/listener via
	 * PlayerManager's real startWatch()/startListen() -- see
	 * entertainerWatchTarget()'s doc comment above for why this is safe
	 * and sufficient with no extra buff/heal code. Stops watching the
	 * moment the companion goes busy; the stock system handles every other
	 * stop condition (entertainer stops performing, range, session end) on
	 * its own.
	 */
	void runEntertainerWatchTick(CompanionObject* companion, CreatureObject* owner) {
		if (companion == nullptr || owner == nullptr) {
			return;
		}

		Zone* zone = companion->getZone();

		if (zone == nullptr) {
			return;
		}

		ZoneServer* zoneServer = zone->getZoneServer();

		if (zoneServer == nullptr) {
			return;
		}

		ManagedReference<PlayerManager*> playerManager = zoneServer->getPlayerManager();

		if (playerManager == nullptr) {
			return;
		}

		uint64 companionID = companion->getObjectID();
		auto& watching = entertainerWatchTarget();

		bool busy = companion->isDead() || companion->isIncapacitated() || companion->isInCombat()
				|| companion->isTaxiActive() || companion->isLootSweepActive()
				|| isCraftTheaterBusy(owner, companion);

		bool currentlyTracked = watching.contains(companionID);

		if (busy) {
			if (currentlyTracked) {
				uint64 entertainerID = watching.get(companionID);

				if (companion->isWatching()) {
					playerManager->stopWatch(companion, entertainerID, true, false, false, false);
				} else if (companion->isListening()) {
					playerManager->stopListen(companion, entertainerID, true, false, false, false);
				}

				watching.drop(companionID);
			}

			return;
		}

		if (currentlyTracked) {
			// Ongoing healing/eviction is entirely the entertainer's own
			// doEntertainerPatronEffects() task's job -- just notice if it
			// already tore this down on its own (entertainer stopped
			// performing, session ended, range self-eviction) and drop our
			// stale bookkeeping to match.
			if (!companion->isWatching() && !companion->isListening()) {
				watching.drop(companionID);
			}

			return;
		}

		CloseObjectsVector* vec = (CloseObjectsVector*) companion->getCloseObjects();
		SortedVector<QuadTreeEntry*> closeObjects;

		if (vec != nullptr) {
			closeObjects.removeAll(vec->size(), 10);
			vec->safeCopyReceiversTo(closeObjects, CloseObjectsVector::PLAYERTYPE);
		} else {
			zone->getInRangeObjects(companion->getWorldPositionX(), companion->getWorldPositionY(), 15.0f, &closeObjects, true);
		}

		CreatureObject* bestEntertainer = nullptr;
		float bestDistSq = 225.0f; // 15m squared

		for (int i = 0; i < closeObjects.size(); ++i) {
			SceneObject* object = static_cast<SceneObject*>(closeObjects.get(i));

			if (object == nullptr || !object->isPlayerCreature() || object == companion) {
				continue;
			}

			CreatureObject* candidate = static_cast<CreatureObject*>(object);

			if (!candidate->isDancing() && !candidate->isPlayingMusic()) {
				continue;
			}

			float dx = companion->getPositionX() - candidate->getPositionX();
			float dy = companion->getPositionY() - candidate->getPositionY();
			float distSq = dx * dx + dy * dy;

			if (distSq <= bestDistSq) {
				bestDistSq = distSq;
				bestEntertainer = candidate;
			}
		}

		if (bestEntertainer == nullptr) {
			return;
		}

		uint64 entertainerID = bestEntertainer->getObjectID();

		if (bestEntertainer->isDancing()) {
			playerManager->startWatch(companion, entertainerID);
		} else {
			playerManager->startListen(companion, entertainerID);
		}

		// If the target had nothing valid to offer (session already gone,
		// etc.) startWatch()/startListen() simply return early without
		// setting watchToID/listenToID -- the next tick's currentlyTracked
		// branch above notices isWatching()/isListening() are both still
		// false and drops this stale entry on its own, so no extra
		// verification is needed here.
		watching.put(companionID, entertainerID);
	}

	void runIdleEmoteTick(CompanionObject* companion, CreatureObject* owner) {
		if (companion == nullptr || owner == nullptr) {
			return;
		}

		if (companion->getZone() == nullptr || companion->isDead()) {
			return;
		}

		uint64 companionID = companion->getObjectID();
		// genesis port: was companion->getMovementState() -- genesis has no movement-state
		// machine. AiAgent::getFollowState() (AiAgent.idl:724) is the equivalent and holds
		// the very OBLIVIOUS/WATCHING/STALKING/FOLLOWING/PATROLLING/FLEEING constants
		// (AiAgent.idl:156-161) this tick compares against below.
		unsigned int movementState = companion->getFollowState();

		bool coreBusy = companion->isInCombat()
				|| companion->isIncapacitated()
				|| isCraftTheaterBusy(owner, companion)
				|| companion->isTaxiActive()
				|| companion->isLootSweepActive()
				|| movementState == AiAgent::FLEEING
				|| movementState == AiAgent::PATROLLING;

		// Shared class-level map (idleSittingCompanionsMap(), see its doc
		// comment near seatWalkTargetID()) -- present+nonzero == WE sat this
		// companion down, whether by an immediate floor-sit below or via a
		// runSeatWalkTick() seat arrival.
		auto& idleSittingCompanions = idleSittingCompanionsMap();

		bool weSatItDown = idleSittingCompanions.contains(companionID) && idleSittingCompanions.get(companionID) != 0;

		// Stand back up the moment this tick finds the companion no longer
		// idle (spec 6: "...stand back up the next time this tick finds
		// the companion no longer idle").
		if (weSatItDown && coreBusy) {
			companion->setPosture(CreaturePosture::UPRIGHT, true);
			companion->setFollowState(AiAgent::FOLLOWING); // genesis port: was setMovementState()
			idleSittingCompanions.drop(companionID);
			weSatItDown = false;
		}

		// Arrival greet -- cheap squared-distance crossing check (no
		// sqrt, same idiom runKeepUpTick() already uses for its own
		// 25m/10m distance checks), independent of idle state.
		static VectorMap<uint64, uint64> idleWasNearOwner; // 1 == was <=10m last tick

		Vector3 ownerWorld = owner->getWorldPosition();
		float dx = companion->getPositionX() - ownerWorld.getX();
		float dy = companion->getPositionY() - ownerWorld.getY();
		float distSq = dx * dx + dy * dy;
		bool nowNear = distSq <= 100.0f; // 10m, squared
		bool wasNear = idleWasNearOwner.contains(companionID) && idleWasNearOwner.get(companionID) != 0;

		if (nowNear && !wasNear) {
			companion->doAnimation("happy");
		}

		idleWasNearOwner.put(companionID, nowNear ? (uint64) 1 : (uint64) 0);

		// Genuinely idle, per spec: not busy per coreBusy above, AND not
		// already resting/sitting for some OTHER reason (e.g. camp
		// ambiance) -- but a rest WE caused (weSatItDown) should not block
		// itself here, it's handled by the stand-up branch above.
		// genesis port: isResting() has no equivalent on this base; the term was
		// dropped. coreBusy/weSatItDown still cover the cases we set up ourselves.
		if (coreBusy || weSatItDown) {
			return;
		}

		if (System::random(99) >= 15) { // ~15% roll per ~20s tick
			return;
		}

		if (System::random(3) == 0) { // 1-in-4 of successful rolls: sit instead of a standing emote
			// Companion System (2026-08-10, per Nick: "search for a seat
			// instead of sitting on the floor... if there is a seat, or
			// couch or any other mountable object in range of 15 meters").
			// Prefer walking to a real chair/couch/bench within 15m over
			// sitting in place -- initiate the walk here (the CHOICE is
			// made on this ~20s tick) and let runSeatWalkTick(), running
			// every ~2s from the keep-up tick, manage arrival/abandon. Only
			// fall back to the original immediate floor-sit if nothing
			// qualifying is nearby.
			uint64 seatObjectID = 0;

			if (findNearbySeat(companion, seatObjectID)) {
				companion->setCompanionState(CompanionObject::PATROL);
				companion->setFollowObject(nullptr);
				seatWalkTargetID().put(companionID, seatObjectID);
				return; // runSeatWalkTick() picks up the walk next keep-up tick
			}

			companion->setPosture(CreaturePosture::SITTING, true);
			// DEFERRED (genesis port): no equivalent for AiAgent::RESTING -- companion->setMovementState(AiAgent::RESTING);
			idleSittingCompanions.put(companionID, (uint64) 1);
			return;
		}

		switch (System::random(2)) {
		case 0:
			companion->doAnimation("happy");
			break;
		case 1:
			companion->doAnimation("alert");
			break;
		default:
			companion->doAnimation("confused");
			break;
		}
	}

	// Self-rescheduling ~20s idle-emote task -- started once from
	// startKeepUpMonitor() below, stops rescheduling itself once the
	// companion is despawned/dead (mirrors runKeepUpTick()'s own
	// zone/dead stop condition; no public keepUpMonitorActive getter
	// exists to share that exact flag from outside the class).
	void scheduleCompanionIdleEmoteTick(Reference<CompanionObject*> companionRef) {
		Core::getTaskManager()->scheduleTask([companionRef] () {
			CompanionObject* companion = companionRef.get();

			if (companion == nullptr) {
				return;
			}

			Locker locker(companion);

			if (companion->getZone() == nullptr || companion->isDead()) {
				return;
			}

			CreatureObject* owner = companion->getLinkedCreature().get();

			runIdleEmoteTick(companion, owner);
			runCantinaAmbianceTick(companion, owner);
			runEntertainerWatchTick(companion, owner);

			scheduleCompanionIdleEmoteTick(companionRef);
		}, "CompanionIdleEmoteTickLambda", 20000);
	}

}

// Keep-up monitor (2026-07-18, "companions shouldn't ever be further than
// 25 meters behind the user" -- see the idl doc comment).
void CompanionObjectImplementation::startKeepUpMonitor() {
	if (keepUpMonitorActive) {
		return;
	}

	keepUpMonitorActive = true;

	ManagedReference<CompanionObject*> companionRef = _this.getReferenceUnsafeStaticCast();

	Core::getTaskManager()->scheduleTask([companionRef] () {
		CompanionObject* companion = companionRef.get();

		if (companion == nullptr) {
			return;
		}

		Locker locker(companion);
		companion->runKeepUpTick();
	}, "CompanionKeepUpTickLambda", 2000);

	// Idle-emote watchdog (2026-07-30 patch, spec part 6) -- a SEPARATE,
	// lower-frequency (~20s) companion-flavor tick (ambient emotes,
	// sit/stand-idle, arrival greet), started alongside the keep-up
	// monitor above.
	scheduleCompanionIdleEmoteTick(companionRef);
}

void CompanionObjectImplementation::runKeepUpTick() {
	// Despawned/dead: stop the monitor -- the next summon restarts it
	// (spawnObject also resets speeds, so a mid-boost despawn is harmless).
	if (getZone() == nullptr || isDead()) {
		keepUpMonitorActive = false;
		keepUpBoosted = false;
		return;
	}

	ManagedReference<CompanionObject*> companionRef = _this.getReferenceUnsafeStaticCast();

	auto reschedule = [&companionRef] () {
		Core::getTaskManager()->scheduleTask([companionRef] () {
			CompanionObject* companion = companionRef.get();

			if (companion == nullptr) {
				return;
			}

			Locker locker(companion);
			companion->runKeepUpTick();
		}, "CompanionKeepUpTickLambda", 2000);
	};

	CreatureObject* owner = getLinkedCreature().get();

	// Theater Mode foundation (2026-07-30) -- a scripted show (birthday
	// show / muster call / battle theater / "The Landing" / future
	// shows) is running for this companion. Bail out of EVERY background
	// keep-up sub-tick below (medic auto-care, flee/heavy-damage-bark/
	// wipe/self-buff/skill-train-walkup checks, the chase movement-speed
	// throttle, the FOLLOW-only keep-up speed boost -- whichever of
	// these are present in this build; any NEW sub-tick added later
	// should stay AFTER this check too) so none of them fight the show
	// director's choreography. Still self-reschedules via the SAME
	// `reschedule` lambda every other exit path in this function already
	// uses, so the tick keeps firing every ~2000ms and this resumes
	// normal behavior automatically the moment companionState changes
	// back off THEATER -- do NOT stop the monitor here, THEATER is a
	// temporary show state, not a despawn/death.
	if (getCompanionState() == CompanionObject::THEATER) {
		reschedule();
		return;
	}

	// MEDIC_AUTOHEAL_RESTOCK_FEATURE: runs every tick regardless of
	// FOLLOW/combat state -- healing and restocking must not depend on the
	// keep-up speed-boost's own FOLLOW-only applicability gate below.
	// COMPANION_MOVEMENT_EVENT_SELFHEAL_2026_08_04 -- defensive re-arm.
	//
	// An AiAgent stops acting the moment its AiMoveEvent goes away, and
	// AiAgentImplementation::activateMovementEvent() destroys that event
	// whenever it is entered with no follow object and no retreat in progress.
	// spawnObject() now arms it correctly at summon time, but any later engine
	// path that transiently clears the follow object can tear it down again and
	// nothing in the AI code re-arms it (the calls are commented out in
	// AiAgent.idl:652/664/676/688).
	//
	// activateMovementEvent() only schedules when the event is not already
	// scheduled, so calling it once per 2000ms tick costs a mutex and a null
	// check and makes a dead move event self-heal within one tick instead of
	// stranding the companion until the owner loots or fights something.
	if (owner != nullptr && getFollowObject().get() != nullptr) {
		activateMovementEvent();
	}

	// COMPANION_SEATED_FOLLOW_FIX_2026_08_04 -- a companion that has sat down
	// cannot move again on its own. MovePetBase:checkConditions gates on
	// posture == UPRIGHT, so a seated companion fails it on every tick forever;
	// pressing follow works only because CompanionFollowCommand now stands it up
	// first (COMPANION_SIT_FOLLOW_FIX). Nothing did that automatically, so
	// walking away from a seated companion just left it behind, silently.
	//
	// Only fires when it is SUPPOSED to be coming along -- FOLLOW state, a live
	// follow object, not mid-loot-sweep, and the owner genuinely out of reach. A
	// companion told to STAY, or seated in a theater show, is left alone (this
	// tick already returns early for THEATER further up).
	{
		CompanionObject* self = companionRef.get();

		if (self != nullptr && owner != nullptr
				&& self->getCompanionState() == CompanionObject::FOLLOW
				&& !self->isLootSweepActive()
				&& self->getPosture() != CreaturePosture::UPRIGHT
				&& self->getFollowObject().get() != nullptr
				&& self->getDistanceTo(owner) > 8.f) {
			self->setPosture(CreaturePosture::UPRIGHT, true, true);
			self->activateMovementEvent();
		}
	}

	runMedicAutoCareTick(companionRef.get(), owner);

	// CRAFTING_RANGE_INDICATOR_FEATURE (2026-08-01 re-anchor -- the original 2026-07-29
	// anchor point stopped being contiguous once the 2026-07-30
	// Personality/Flee/Wipe/Self-buff and Auto Skill-Training Walkup
	// patches inserted their own calls between runMedicAutoCareTick() and
	// the FOLLOW-only speed-boost gate below) -- same "runs every tick
	// regardless of state" shape as the medic call just above -- see
	// CompanionCraftingRangeIndicator.h for the full design writeup.
	CompanionCraftingRangeIndicator::tick(companionRef.get(), owner);

	// Companion Personality/Flee/Wipe/Self-buff patch (2026-07-30, spec
	// parts 3/5/7/8) -- same "runs every tick regardless of FOLLOW/combat
	// state" rationale as the medic auto-care call above.
	runFleeCheckTick(companionRef.get(), owner);
	runHeavyDamageBarkTick(companionRef.get(), owner);
	runWipeCheckTick(companionRef.get(), owner);
	runSelfBuffTick(companionRef.get(), owner);

	// Auto Skill-Training Walkup (AUTO_SKILL_TRAIN_WALKUP_2026_07_30) -- same "runs every tick
	// regardless of FOLLOW/combat state" rationale as the calls above
	// (it needs to notice combat/busy transitions promptly to abandon a
	// walk-over cleanly, and needs to keep checking arrival distance
	// while genuinely eligible). tryInitiateSkillTrainWalkup() added here
	// too (2026-08-07) so a skill that becomes ready WHILE busy (e.g.
	// mid-combat, the common case since combat XP is what usually crosses
	// the threshold) gets caught within one tick of no longer being busy,
	// instead of waiting for a fresh addExperience() event that may never
	// come once the fight that made it ready is already over.
	tryInitiateSkillTrainWalkup(companionRef.get(), owner);
	runSkillTrainWalkupTick(companionRef.get(), owner);

	// Seat-search (2026-08-10, per Nick) -- manages an already-chosen seat
	// walk (the CHOICE itself is made in runIdleEmoteTick()'s ~20s sit
	// roll via seatWalkTargetID()). Same "runs every 2s keep-up tick so it
	// can notice arrival/busy-abandon promptly" rationale as the training
	// walkup calls just above; a no-op most ticks (returns immediately
	// when no seat walk is pending for this companion).
	runSeatWalkTick(companionRef.get(), owner);

	// Chase movement-speed throttle (2026-07-30, per Nick: companions
	// chasing an attack target look like they "teleport 60 meters in 2
	// seconds" -- use the owner's own WALK speed as the companion's chase
	// speed). Movement speed only -- does not touch combat attack/weapon
	// speed in any way. Companions have no distinct walk pace of their own
	// at all -- walkSpeed was intentionally set equal to runSpeed back on
	// 2026-07-14 (CompanionControlDeviceImplementation.cpp's spawnObject())
	// specifically to fix companions lagging behind the owner while
	// FOLLOWing/PATROLling, and that fix must keep working -- it is not
	// touched here. Instead, combat approach now borrows the OWNER's own
	// real walk speed as the chase pace, throttled only while
	// companionState == ATTACK (the companion's existing "actively moving
	// on a target" behavior-state name -- not a combat/weapon speed stat),
	// restored automatically once combat ends -- via this same tick that
	// already handles the FOLLOW keep-up boost below, runs every tick
	// regardless of state (same idiom as the medic tick above), and checks
	// its own applicability internally. NOTE: because this piggybacks on
	// the existing ~2000ms tick rather than hooking every ATTACK-entry/exit
	// call site directly (there are several of each -- interceptThreatToOwner()/
	// CompanionAttackCommand.h entering ATTACK; recoverFromAbortedIntercept(),
	// runPostCombatSweepCheck()'s endSweep lambda, etc. leaving it), there
	// can be up to a ~2 second delay before the throttle kicks in at the
	// very start of a chase (and, symmetrically, before it lets go once
	// combat ends). This is a known, deliberate scope tradeoff -- it
	// mirrors this project's own established idiom of periodic-tick-based
	// reactive behavior for flee/self-buff/idle-emotes rather than
	// instantaneous hooks.
	if (owner == nullptr || getZone() == nullptr || isDead()) {
		if (chaseWalkSpeedActive) {
			if (preChaseRunSpeed > 0.f) {
				setRunSpeed(preChaseRunSpeed, true);
				// genesis port: dropped setWalkSpeed(preChaseWalkSpeed > 0.f ? preChaseWalkSpeed : preChaseRunSpeed, true) -- genesis's
				// CreatureObject.idl exposes walkSpeed READ-ONLY (field :100, getWalkSpeed()
				// :1676); setRunSpeed() (:468) is the only speed setter, and it is already
				// called on the line(s) directly above with the matching run-speed value, so
				// the pace change still takes effect for RUN movement. DEFERRED: walk-mode
				// pacing cannot be tuned on this base.
			}

			chaseWalkSpeedActive = false;
		}
	} else if (getCompanionState() == CompanionObject::ATTACK) {
		if (!chaseWalkSpeedActive) {
			float ownerWalk = owner->getWalkSpeed();

			// Guard against a zero/bogus owner walk speed -- never stomp this
			// companion's own speed down to 0; just skip the throttle this tick
			// and try again on the next one.
			if (ownerWalk > 0.f) {
				preChaseRunSpeed = getRunSpeed();
				preChaseWalkSpeed = getWalkSpeed();

				setRunSpeed(ownerWalk, true);
				// genesis port: dropped setWalkSpeed(ownerWalk, true) -- genesis's
				// CreatureObject.idl exposes walkSpeed READ-ONLY (field :100, getWalkSpeed()
				// :1676); setRunSpeed() (:468) is the only speed setter, and it is already
				// called on the line(s) directly above with the matching run-speed value, so
				// the pace change still takes effect for RUN movement. DEFERRED: walk-mode
				// pacing cannot be tuned on this base.
				chaseWalkSpeedActive = true;
			}
		}
	} else if (chaseWalkSpeedActive) {
		if (preChaseRunSpeed > 0.f) {
			setRunSpeed(preChaseRunSpeed, true);
			// genesis port: dropped setWalkSpeed(preChaseWalkSpeed > 0.f ? preChaseWalkSpeed : preChaseRunSpeed, true) -- genesis's
			// CreatureObject.idl exposes walkSpeed READ-ONLY (field :100, getWalkSpeed()
			// :1676); setRunSpeed() (:468) is the only speed setter, and it is already
			// called on the line(s) directly above with the matching run-speed value, so
			// the pace change still takes effect for RUN movement. DEFERRED: walk-mode
			// pacing cannot be tuned on this base.
		}

		chaseWalkSpeedActive = false;
	}

	// Only meaningful while plainly FOLLOWING the owner outside of combat
	// and outside of taxi rides (the ride manages its own pace). In any
	// other state, drop a lingering boost and just keep watching.
	bool applicable = owner != nullptr && owner->getZone() == getZone()
			&& getCompanionState() == CompanionObject::FOLLOW
			&& !taxiActive && !isInCombat();

	if (!applicable) {
		if (keepUpBoosted || keepUpCityCrawling) {
			keepUpBoosted = false;
			keepUpCityCrawling = false;

			if (keepUpBaseRunSpeed > 0.f) {
				setRunSpeed(keepUpBaseRunSpeed, true);
				// genesis port: dropped setWalkSpeed(keepUpBaseWalkSpeed > 0.f ? keepUpBaseWalkSpeed : keepUpBaseRunSpeed, true) -- genesis's
				// CreatureObject.idl exposes walkSpeed READ-ONLY (field :100, getWalkSpeed()
				// :1676); setRunSpeed() (:468) is the only speed setter, and it is already
				// called on the line(s) directly above with the matching run-speed value, so
				// the pace change still takes effect for RUN movement. DEFERRED: walk-mode
				// pacing cannot be tuned on this base.
			}
		}

		reschedule();
		return;
	}

	Vector3 ownerWorld = owner->getWorldPosition();
	float dx = getPositionX() - ownerWorld.getX();
	float dy = getPositionY() - ownerWorld.getY();
	float distSq = dx * dx + dy * dy;

	// Companion System (2026-08-10, per Nick: "my companion goes through
	// walls" catching up at speed in a city, then "lets do [the tighter
	// city leash] as well"). isNearDenseBuildings() gates a capped boost
	// (1.15x instead of the flat 1.8x) AND, once badly behind (30m+) near
	// buildings, a genuine CRAWL -- the companion deliberately does NOT
	// try to rush a corner-cutting line through city geometry; it waits
	// for the owner to close the gap instead, matching the taxi driver's
	// own established "crawl, never a hard OBLIVIOUS stop" pattern just
	// above. Open terrain (isNearDenseBuildings() false) keeps the exact
	// original 1.8x/25m/10m behavior untouched -- that's not where the
	// clipping was reported.
	bool nearCity = isNearDenseBuildings(getZone(), getPositionX(), getPositionY());
	bool shouldCrawl = nearCity && distSq > COMPANION_KEEPUP_CITY_LEASH_DISTANCE_SQ;

	if (keepUpCityCrawling && !shouldCrawl && distSq > COMPANION_KEEPUP_CITY_RESUME_DISTANCE_SQ) {
		// Building-density scan flickered false (edge of the cluster) or
		// the owner is still >30m but no longer past the trigger -- keep
		// crawling until the tighter 15m resume distance, rather than
		// snapping straight back to a rushing boost mid-recovery.
		shouldCrawl = true;
	}

	if (shouldCrawl) {
		if (!keepUpCityCrawling) {
			keepUpCityCrawling = true;
			keepUpBoosted = false;
			keepUpBaseRunSpeed = getRunSpeed();
			keepUpBaseWalkSpeed = getWalkSpeed();
		}

		setRunSpeed(keepUpBaseRunSpeed * COMPANION_KEEPUP_CITY_CRAWL_MULTIPLIER, true);
	} else if (keepUpCityCrawling) {
		// Back within the crawl's own 15m resume distance -- drop the
		// crawl and fall through to the normal boosted/resumed logic
		// below, using this tick's current distance.
		keepUpCityCrawling = false;

		if (keepUpBaseRunSpeed > 0.f) {
			setRunSpeed(keepUpBaseRunSpeed, true);
		}
	}

	if (!keepUpCityCrawling) {
		if (!keepUpBoosted && distSq > 625.0f) { // fell >25m behind
			keepUpBoosted = true;
			keepUpBaseRunSpeed = getRunSpeed();
			keepUpBaseWalkSpeed = getWalkSpeed();

			float boostMultiplier = nearCity ? COMPANION_KEEPUP_CITY_BOOST_MULTIPLIER : 1.8f;
			setRunSpeed(keepUpBaseRunSpeed * boostMultiplier, true);
			// genesis port: dropped setWalkSpeed(...) -- see the identical
			// dropped-call comment on the crawl/resume branches above;
			// walkSpeed is read-only on this base, setRunSpeed() is the
			// only setter and already carries the pace change for RUN
			// movement.
		} else if (keepUpBoosted && distSq <= 100.0f) { // caught back up to 10m
			keepUpBoosted = false;

			if (keepUpBaseRunSpeed > 0.f) {
				setRunSpeed(keepUpBaseRunSpeed, true);
			}
		}
	}

	reschedule();
}

// Medic Stim Heal Radial (2026-07-29 night #3, per Nick: "give the
// medics radial a way to heal with stims for me, and for the whole
// group"). Thin public wrappers around the exact same anonymous-
// namespace helpers the background medic auto-heal tick already uses
// (medicIsTrained()/medicBestCraftableStimSchematic()/
// medicFindBestStimOnHand()/medicApplyHeal(), all defined earlier in
// this file) so CompanionMenuComponent.cpp's new radial doesn't have
// to re-derive or duplicate any of that already-verified logic.
String CompanionObjectImplementation::getBestCraftableStimSchematicPath() {
	CompanionObject* self = _this.getReferenceUnsafeStaticCast();

	if (!medicIsTrained(self)) {
		return "";
	}

	String path;
	int rank;

	if (!medicBestCraftableStimSchematic(self, path, rank)) {
		return "";
	}

	return path;
}

bool CompanionObjectImplementation::applyStimHealTo(CreatureObject* target) {
	CompanionObject* self = _this.getReferenceUnsafeStaticCast();

	if (target == nullptr || !medicIsTrained(self)) {
		return false;
	}

	StimPack* stimPack = medicFindBestStimOnHand(self);

	if (stimPack == nullptr) {
		return false;
	}

	medicApplyHeal(self, target, stimPack);
	return true;
}

void CompanionObjectImplementation::deferredStartPostCombatSweep() {
	// Fired for EVERY combat event the observer sees -- the armed check
	// inside the locked task keeps it to exactly one poll chain at a time.
	ManagedReference<CompanionObject*> companionRef = _this.getReferenceUnsafeStaticCast();

	Core::getTaskManager()->executeTask([companionRef] () {
		CompanionObject* companion = companionRef.get();

		if (companion == nullptr) {
			return;
		}

		Locker locker(companion);

		if (companion->isLootSweepActive()) {
			return; // already armed or sweeping
		}

		companion->setLootSweepActive(true);
		companion->setCombatStuckPollCount(0);

		Core::getTaskManager()->scheduleTask([companionRef] () {
			CompanionObject* companion = companionRef.get();

			if (companion == nullptr) {
				return;
			}

			Locker locker(companion);
			companion->runPostCombatSweepCheck();
		}, "CompanionSweepPollLambda", 3000);
	}, "CompanionSweepArmLambda");
}

void CompanionObjectImplementation::runPostCombatSweepCheck() {
	if (!lootSweepActive) {
		return; // disarmed while the tick was in flight
	}

	if (getZone() == nullptr || isDead()) {
		lootSweepActive = false;
		return;
	}

	CreatureObject* owner = getLinkedCreature().get();

	if (owner == nullptr) {
		lootSweepActive = false;
		return;
	}

	ManagedReference<CompanionObject*> companionRef = _this.getReferenceUnsafeStaticCast();

	// Still fighting -- keep polling.
	if (isInCombat() || owner->isInCombat()) {
		// Companion System (2026-07-29 fix, per Nick: "sometimes they run
		// off and never return unless i press follow"). isInCombat() is a
		// defenderList-driven bitmask that can get stuck true forever for a
		// THIRD PARTY who never gets the kill credit (normal cleanup only
		// scrubs the dying object's own list and the credited killer's
		// reference -- see the combatStuckPollCount doc comment in
		// CompanionObject.idl) -- so this poll used to have no upper bound
		// at all and could loop past the fight's real end indefinitely,
		// leaving companionState/followObject stuck exactly like they were
		// mid-fight. After ~2 minutes of continuous "still fighting" polls
		// (matching LOOT_SWEEP_MAX_STEPS's own 2-minute convention just
		// below), force-break combat on whichever side is stuck via the
		// same CombatManager::forcePeace() this codebase already uses
		// elsewhere for exactly this class of stuck-combat-state bug (see
		// CompanionControlDeviceImplementation.cpp's handleCompanionDeath()
		// comment), then fall through to the normal end-of-fight handling
		// below instead of rescheduling again.
		if (++combatStuckPollCount <= 40) {
			Core::getTaskManager()->scheduleTask([companionRef] () {
				CompanionObject* companion = companionRef.get();

				if (companion == nullptr) {
					return;
				}

				Locker locker(companion);
				companion->runPostCombatSweepCheck();
			}, "CompanionSweepPollLambda", 3000);
			return;
		}

		if (isInCombat()) {
			CombatManager::instance()->forcePeace(companionRef.get());
		}

		if (owner->isInCombat()) {
			// FIX_2026_08_06: forcePeace() asserts "attacker must be locked"
			// (CombatManager::forcePeace -> fatal(attacker->isLockedByCurrentThread()...)).
			// This scope only holds a lock on companion (via the caller's
			// Locker locker(companion) in the scheduleTask lambda above), never
			// on owner -- calling forcePeace(owner) unlocked hit that fatal
			// assertion and SIGABRT'd the whole server. Same cross-lock idiom
			// already used elsewhere in this file for this exact companion/owner
			// pair (see the corpse-cash Locker ownerLocker(owner, companion) a
			// few hundred lines up).
			Locker ownerLocker(owner, companionRef.get());
			CombatManager::instance()->forcePeace(owner);
		}
	}

	// Fight's over: claim the corpses WE are nearest to. Every companion
	// runs this same computation over the same inputs, so the claims are
	// disjoint without any cross-companion locking.
	Zone* zone = getZone();
	ZoneServer* zoneServer = getZoneServer();

	if (zone == nullptr || zoneServer == nullptr) {
		lootSweepActive = false;
		return;
	}

	CompanionObject* self = _this.getReferenceUnsafeStaticCast();

	Vector<CompanionObject*> siblings;
	resolveOwnersCompanions(owner, siblings);

	// genesis port: QuadTreeEntry (genesis predates the QuadTreeEntry -> TreeEntry
	// rename) and the 6-arg 2D Zone::getInRangeObjects(x, y, range, objects,
	// readLockZone, includeBuildingObjects) -- the newer base's 3D overload took
	// (x, z, y, range, ...). Dropped the z argument. LOST: the query is now a
	// cylinder around (x, y) instead of a sphere around (x, z, y), so objects far
	// above/below the caller that the 3D form excluded can now match.
	SortedVector<ManagedReference<QuadTreeEntry*>> nearbyObjects;
	zone->getInRangeObjects(getPositionX(), getPositionY(), LOOT_SWEEP_SCAN_RANGE, &nearbyObjects, true, true);

	Reference<CompanionSweepState*> state = new CompanionSweepState();
	bool anyEligibleCorpse = false;

	for (int i = 0; i < nearbyObjects.size(); ++i) {
		SceneObject* scno = cast<SceneObject*>(nearbyObjects.get(i).get());

		if (scno == nullptr || !scno->isCreatureObject() || scno == self) {
			continue;
		}

		AiAgent* corpse = cast<AiAgent*>(scno);

		if (corpse == nullptr || !corpse->isDead() || corpse->isCompanionObject()) {
			continue;
		}

		// COMPANION_CREDIT_LOOT_FIX_2026_08_04 -- a corpse carrying nothing but
		// credits has an EMPTY bag, so the old "getContainerObjectsSize() > 0"
		// test excluded it here and the companion never walked to it. The
		// at-the-corpse code has looted cash correctly since 2026-07-18
		// (force_luck, NPCLOOTCLAIM, prose_coin_loot, credited straight to the
		// owner) -- it was simply never reached. Uncollected credits also keep
		// shouldRescheduleCorpseDestruction() returning false, which is why
		// those bodies sat out their full despawn timer.
		SceneObject* corpseBag = corpse->getSlottedObject("inventory");
		bool corpseHasItems = corpseBag != nullptr && corpseBag->getContainerObjectsSize() > 0;
		bool corpseHasCash = corpse->getCashCredits() > 0;
		bool lootable = corpseLootableBy(corpse, owner) && (corpseHasItems || corpseHasCash);
		bool harvestable = companionHasRangerTraining(self) && corpseHarvestableBy(corpse, owner);

		if (!lootable && !harvestable) {
			continue;
		}

		anyEligibleCorpse = true;

		// Nearest-companion arbitration.
		CompanionObject* nearest = nullptr;
		float nearestDist = 0.f;

		for (int s = 0; s < siblings.size(); ++s) {
			CompanionObject* sibling = siblings.get(s);

			if (sibling == nullptr || sibling->getZone() != zone) {
				continue;
			}

			// Only harvest-capable siblings compete for harvest-only corpses.
			if (!lootable && !companionHasRangerTraining(sibling)) {
				continue;
			}

			float dist = sibling->getDistanceTo(corpse);

			if (nearest == nullptr || dist < nearestDist) {
				nearest = sibling;
				nearestDist = dist;
			}
		}

		if (nearest == self) {
			state->corpseIDs.add(corpse->getObjectID());
		}
	}

	if (state->corpseIDs.size() == 0) {
		// Empty-sweep report (2026-07-18 follow-up, per user request): when
		// the whole fight produced NOTHING collectible, say so -- but only
		// from the companion nearest the owner, so multiple companions
		// don't chorus it. (If corpses existed but a SIBLING claimed them,
		// stay silent -- that sibling is doing the talking.)
		if (!anyEligibleCorpse) {
			CompanionObject* closestToOwner = nullptr;
			float closestDist = 0.f;

			for (int s = 0; s < siblings.size(); ++s) {
				CompanionObject* sibling = siblings.get(s);

				if (sibling == nullptr || sibling->getZone() != zone) {
					continue;
				}

				float dist = sibling->getDistanceTo(owner);

				if (closestToOwner == nullptr || dist < closestDist) {
					closestToOwner = sibling;
					closestDist = dist;
				}
			}

			if (closestToOwner == self) {
				companionSweepSay(self, "Nothing worth taking out there.");
			}
		}

		// Squad-attack straggler fix (2026-07-30, per Nick: companions left
		// standing ~80m away after a group /companionattack order once the
		// target died). This companion's OWN combat has genuinely ended (we
		// only reach this point once the "still fighting" branch above lets
		// go), but it has no corpse to claim -- either nothing eligible ever
		// existed, or a sibling was nearer -- so it was falling straight
		// through to "lootSweepActive = false; return;" below with NO
		// restore call at all. endSweep(true) (runSweepStep(), further
		// below in this file) is the only thing that ever resumes
		// FOLLOW/STAY/GUARD, and it's only reachable via
		// scheduleSweepStep(), which is only called when THIS companion
		// claimed at least one corpse. Every companion that took part in a
		// group attack without landing/claiming the kill was permanently
		// stranded at companionState == ATTACK / followObject == nullptr.
		// Reuses the exact same standingOrder-based restore helper
		// endSweep(true) and the flee-recovery tick already call, rather
		// than a fourth ad-hoc copy of the STAY/GUARD/FOLLOW branching.
		restoreStandingPosture(self, owner);

		lootSweepActive = false;
		return;
	}

	companionSweepSay(self, "I'll gather up the spoils!");

	// lootSweepActive stays true for the whole sweep; runSweepStep()'s
	// endSweep clears it.
	scheduleSweepStep(companionRef, getLinkedCreature().get(), state, 200);
}

// Companion System (2026-07-15, "companion teleports/vanishes when the
// owner outranges it" fix -- see the CompanionObject.idl doc comment).
//
// Companion System (2026-07-29 fix, per Nick: "the stay command is not
// working, they dont stay in their spot... it still follows"). Root
// cause: this override used to re-anchor homeLocation to the OWNER's
// live position on EVERY call, regardless of companionState -- but
// idleCompanion's STAY branch (companion.lua) calls the "Leash" node on
// every idle tick while STAYing, so a STAY companion's homeLocation kept
// getting silently overwritten to wherever the owner currently stood,
// then LEASHING paced it there -- i.e. it looked and acted exactly like
// it was still following, even though companionState really was STAY the
// whole time. The re-anchor-to-owner (and the FOLLOW resume right below
// it) now only applies when actually in FOLLOW state, matching the
// resume check that was already here; every other state (STAY, GUARD,
// ATTACK, ...) falls through to the base wild-mobile leash() below, which
// paces the agent back to whatever homeLocation ALREADY is (untouched
// here) instead of hijacking it to the owner's position.
void CompanionObjectImplementation::leash(bool forcePeace) {
	CreatureObject* owner = getLinkedCreature().get();

	if (owner != nullptr && !isDead() && owner->getZone() == getZone() && getCompanionState() == CompanionObject::FOLLOW) {
		// Re-anchor "home" to wherever the owner is right now, so the
		// out-of-sight-range check that fired this leash stops firing.
		setHomeLocation(owner->getPositionX(), owner->getPositionZ(), owner->getPositionY(),
				owner->getParent().get().castTo<CellObject*>());

		setFollowObject(owner);
		setFollowState(AiAgent::FOLLOWING); // genesis port: was setMovementState()

		return;
	}

	// genesis port: genesis's AiAgentImplementation::leash() takes NO parameters
	// (the newer base added leash(bool forcePeace)). Dropped the argument.
	// LOST: the forcePeace == false path -- genesis's leash() unconditionally calls
	// CombatManager::instance()->forcePeace(asAiAgent()), so a leash requested with
	// forcePeace = false now still drops the companion out of combat.
	// NOTE (not fixable here, needs an .idl decision): CompanionObject.idl declares
	// leash(boolean forcePeace = true), which HIDES rather than overrides genesis's
	// 0-arg AiAgent::leash() -- engine-internal leash() calls will therefore run the
	// stock wild-mobile leash, not this override.
	AiAgentImplementation::leash();
}

// Companion System (2026-07-15, "Converse should act like Talk to
// Companion" -- see the CompanionObject.idl doc comment).
bool CompanionObjectImplementation::sendConversationStartTo(SceneObject* player) {
	if (player == nullptr || !player->isPlayerCreature()) {
		return false;
	}

	CreatureObject* playerCreature = player->asCreatureObject();

	if (playerCreature == nullptr) {
		return false;
	}

	if (isAuthorizedActor(playerCreature)) {
		CompanionSkillTrainer::instance()->sendDialogMenu(playerCreature, _this.getReferenceUnsafeStaticCast());
	} else {
		CompanionSkillTrainer::instance()->sendInspectionSheet(playerCreature, _this.getReferenceUnsafeStaticCast());
	}

	return true;
}

bool CompanionObjectImplementation::isAuthorizedActor(CreatureObject* requester) {
	if (requester == nullptr) {
		return false;
	}

	ManagedReference<PlayerObject*> ghost = requester->getPlayerObject();

	if (ghost != nullptr && ghost->isPrivileged()) {
		return true;
	}

	CreatureObject* owner = getLinkedCreature().get();

	return owner != nullptr && owner->getObjectID() == requester->getObjectID();
}

// Companion System -- real per-sub-attribute baseline HAM applied by
// migrateBaselineStats(). These are not placeholder numbers: they are the
// ACTUAL values a real human_male character gets at creation choosing
// Entertainer (the same "master entertainer" baseline this system has
// always targeted, per the doc comment on CompanionObject.idl -- Entertainer
// has no combat stat_*_boost skill mods anywhere in the base game, so it's
// the correct non-combat reference profession).
//
// Formula (matches PlayerCreationManager::addProfessionStartingItems() +
// addRacialMods(), PlayerCreationManager.cpp:757-762 and :1064-1078):
//     final HAM[i] = profession_mods.iff["social_entertainer"][i]
//                  + racial_mods.iff["human_male"][i]
// Extracted directly from the client's shipped creation datatables
// (datatables/creation/{profession_mods,racial_mods}.iff):
//   social_entertainer = { 500, 300, 300, 1000, 400, 400, 800, 400, 400 }
//   human_male racial mod = +100 flat to all 9
// Order matches templates/params/creature/CreatureAttribute.h indices
// (HEALTH, STRENGTH, CONSTITUTION, ACTION, QUICKNESS, STAMINA, MIND, FOCUS,
// WILLPOWER). Sum = 5400, which matches attribute_limits.iff's "total" field
// for human -- i.e. this companion has exactly the same total HAM point
// budget a real human character does, not an arbitrary flat number.
// See docs/companion_system/NOTES.md.
static const int COMPANION_BASELINE_HAM[9] = {
	600,  // HEALTH       (500 profession + 100 racial)
	400,  // STRENGTH     (300 + 100)
	400,  // CONSTITUTION (300 + 100)
	1100, // ACTION       (1000 + 100)
	500,  // QUICKNESS    (400 + 100)
	500,  // STAMINA      (400 + 100)
	900,  // MIND         (800 + 100)
	500,  // FOCUS        (400 + 100)
	500,  // WILLPOWER    (400 + 100)
};

void CompanionObjectImplementation::migrateBaselineStats() {
	// Companion System: loop bound is a literal 9 (not
	// CreatureAttribute::ARRAYSIZE, which is uint8) to avoid a
	// signed/unsigned loop-bound mismatch; matches
	// PlayerCreationManager::addProfessionStartingItems()'s own
	// "for (int i = 0; i < 9; ++i)" pattern.
	for (int i = 0; i < 9; ++i) {
		setBaseHAM(i, COMPANION_BASELINE_HAM[i], false);
		setHAM(i, COMPANION_BASELINE_HAM[i], false);
		setMaxHAM(i, COMPANION_BASELINE_HAM[i], false);
	}

	// Bug fix (first in-game test): this runs on the very first summon,
	// before any skill (and therefore before recalculateCombatLevel() ever
	// runs) -- without this, the companion would sit at engine level 0 for
	// that entire first session, showing "Combat Difficulty: ... looks like
	// instant death" on Examine. recalculateCombatLevel() (called from
	// grantSkill(), e.g. once the starter profession SUI is answered) will
	// raise this to a real value; this is just a safe floor so it's never 0.
	// See the longer explanation on the setLevel() call in
	// recalculateCombatLevel().
	setLevel(1, false);
}
