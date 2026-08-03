/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	BIRTHDAY_SHOW_2026_07_30

	Companion System (2026-07-30, "Birthday Show" -- per Nick: "lets get the
	other plays coded in"; premise confirmed as the PLAYER's birthday: the
	whole squad throws a celebration for the owner). Same fixed-timeline
	shape as CompanionTheLandingCommand.h (a phase counter + a state Object
	holding companion/prop ids, driven by
	Core::getTaskManager()->scheduleTask(...) lambdas that re-invoke the
	next step after a delay, each one re-resolving its
	ManagedReference/Reference targets, null-checking, and Locker-ing
	before touching anything) but much shorter (~1:00 vs 2:00) and simpler
	-- no skirmish, no GO-button confirm ceremony, no director-disguise
	trick. Every cast member resolves generically as AiAgent* for shared
	beat calls (walkTo/setPosture/doAnimation/faceObject), same reasoning
	as CompanionTheLandingCommand.h's own THELANDING_FILLER_CAST_2026_07_30
	pass -- kept consistent here even though this show doesn't (yet) spawn
	filler NPCs of its own, so the two files stay easy to compare/extend
	the same way later if that's ever wanted.

	PROP TEMPLATES: confirmed real, already-existing templates (none
	invented) --
	  - Cake: object/tangible/food/crafted/dessert_wedding_cake.iff (a
	    real crafted food item, confirmed via
	    MMOCoreORB/bin/scripts/object/tangible/food/crafted/serverobjects.lua).
	  - Banner: object/tangible/lair/base/objective_banner_generic_2.iff
	    (the SAME plain marker banner CompanionTheLandingCommand.h already
	    uses -- deliberately NOT one of the object/tangible/event_perk/
	    banner_generic_* templates, which carry an EventPerkDataComponent/
	    EventPerkAttributeListComponent expecting a parent deed object;
	    untested/risky to spawn standalone here).

	OVERLAP GUARD: participates in the SAME shared
	"companion_theater_mode_busy" owner-level cooldown
	CompanionTheLandingCommand.h's OVERLAPPING_THEATER_SHOWS_FIX_2026_07_30
	pass introduced (armed in start(), cleared in finishShow(), checked in
	doQueueCommand() and CompanionTheaterModeSuiCallback.h) -- every Theater
	Mode show MUST arm/check/clear this same flag to avoid the exact
	overlapping-shows deadlock that guard was built to fix; this is not
	optional for a new show.

	STATE-SUSPENSION / RESTORE ON FINALE: identical mechanism to
	CompanionTheLandingCommand.h (companionState -> THEATER for the whole
	show, restored via the same getStandingOrder()-based STAY/GUARD/FOLLOW
	branches at the finale) -- copied rather than shared, matching this
	project's own per-file-copy convention for this exact pattern.

	Header-only (all methods in-class => implicitly inline; no new .cpp).
*/

#ifndef COMPANIONBIRTHDAYCOMMAND_H_
#define COMPANIONBIRTHDAYCOMMAND_H_

#include "server/zone/objects/creature/commands/QueueCommand.h"
#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/objects/creature/ai/AiAgent.h"
#include "server/zone/objects/creature/ai/PatrolPoint.h"
#include "server/chat/ChatManager.h"
#include "server/zone/Zone.h"
#include "server/zone/ZoneServer.h"
#include "templates/params/creature/CreaturePosture.h"
#include "server/zone/managers/skill/Performance.h" // PerformanceType::DANCE, confirmed real (Performance.h:13)

// ---- Prop templates -- confirmed real existing tangibles, see file
// header for exactly where each was confirmed in the live source. ----
#define BIRTHDAY_CAKE_TEMPLATE   "object/tangible/food/crafted/dessert_wedding_cake.iff"
#define BIRTHDAY_BANNER_TEMPLATE "object/tangible/lair/base/objective_banner_generic_2.iff"

// BIRTHDAY_FILLER_CAST_2026_07_30 -- same real, already-proven filler template
// CompanionTheLandingCommand.h's own filler-cast pass uses.
#define BIRTHDAY_FILLER_TEMPLATE_NAME "rebel_recruiter"
#define BIRTHDAY_TARGET_CAST_SIZE 4

/** Per-companion pre-show snapshot, restored at the finale. */
class CompanionBirthdayCastMember : public Object {
public:
	uint64 companionID = 0;
	int preShowPosture = CreaturePosture::UPRIGHT;

	// BIRTHDAY_FILLER_CAST_2026_07_30 -- true for a temporary spawned filler NPC (see
	// spawnFillerCast()), false for a real companion. A filler has no
	// pre-show state to restore.
	bool isFiller = false;
};

class CompanionBirthdayState : public Object {
public:
	uint64 ownerID = 0;
	uint64 directorID = 0;
	Vector<Reference<CompanionBirthdayCastMember*> > castMembers;

	uint64 cakePropID = 0;
	uint64 bannerPropID = 0;

	// Circle center -- computed once at show start from the owner's
	// position at that moment.
	float centerX = 0, centerY = 0;
};

class CompanionBirthdayShow {
public:

	/** Copied from CompanionTheLandingCommand.h's identical helper. */
	static void say(CompanionObject* companion, const String& text) {
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

	/** Copied from CompanionTheLandingCommand.h's identical helper (already
	 * generalized to AiAgent* there per THELANDING_FILLER_CAST_2026_07_30).
	 * @pre { member locked } */
	static void walkTo(AiAgent* member, Zone* zone, float x, float y) {
		if (member == nullptr || zone == nullptr) {
			return;
		}

		member->setFollowObject(nullptr);
		member->clearPatrolPoints();
		PatrolPoint point(x, zone->getHeight(x, y), y);
		member->addPatrolPoint(point);
		member->setMovementState(AiAgent::PATROLLING);
	}

	/** Copied verbatim from CompanionTheLandingCommand.h's identical
	 * helper. @pre { companion locked } */
	static void restoreStanding(CompanionObject* companion, CreatureObject* owner) {
		if (companion == nullptr || companion->isDead() || companion->getZone() == nullptr) {
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
				companion->setMovementState(AiAgent::FOLLOWING);
			} else {
				companion->setCompanionState(CompanionObject::FOLLOW);
				companion->setFollowObject(owner);
				companion->setMovementState(AiAgent::FOLLOWING);
			}
		} else {
			companion->setCompanionState(CompanionObject::FOLLOW);

			CreatureObject* escortTarget = companion->getEscortTarget().get();

			if (escortTarget != nullptr && escortTarget != owner && escortTarget->getZone() != nullptr) {
				companion->setFollowObject(escortTarget);
			} else {
				companion->setFollowObject(owner);
			}

			companion->setMovementState(AiAgent::FOLLOWING);
		}
	}

	/** BIRTHDAY_FILLER_CAST_2026_07_30 -- spawns `fillerCount` temporary, invulnerable
	 * filler NPCs (same confirmed-real "rebel_recruiter" template +
	 * spawnCreatureWithAi() mechanism CompanionTheLandingCommand.h's own
	 * filler-cast pass already uses), adding each as a full castMembers
	 * entry with isFiller = true. Placed with a little jitter near the
	 * show's center -- beatGather() walks every cast member (real +
	 * filler alike) into the circle formation a moment later anyway, so
	 * the exact spawn point doesn't matter much. Not persisted;
	 * despawned unconditionally in finishShow(). @pre { nothing locked
	 * -- locks each spawned NPC itself } */
	static void spawnFillerCast(ZoneServer* zoneServer, Zone* zone, Reference<CompanionBirthdayState*> state, int fillerCount) {
		if (zoneServer == nullptr || zone == nullptr || state == nullptr || fillerCount <= 0) {
			return;
		}

		CreatureManager* creatureManager = zone->getCreatureManager();

		if (creatureManager == nullptr) {
			return;
		}

		uint32 templateCRC = STRING_HASHCODE(BIRTHDAY_FILLER_TEMPLATE_NAME);

		for (int i = 0; i < fillerCount; ++i) {
			float jitterAngle = ((float) i / (float) fillerCount) * 2.0f * (float) M_PI;
			float fx = state->centerX + sin(jitterAngle) * 3.0f;
			float fy = state->centerY + cos(jitterAngle) * 3.0f;
			float fz = zone->getHeight(fx, fy);

			ManagedReference<CreatureObject*> fillerCreo = creatureManager->spawnCreatureWithAi(templateCRC, fx, fz, fy, 0, false);

			if (fillerCreo == nullptr) {
				continue;
			}

			ManagedReference<AiAgent*> filler = fillerCreo.castTo<AiAgent*>();

			if (filler == nullptr) {
				continue;
			}

			Locker fLocker(filler);

			Reference<CompanionBirthdayCastMember*> member = new CompanionBirthdayCastMember();
			member->companionID = filler->getObjectID();
			member->isFiller = true;
			state->castMembers.add(member);
		}
	}

	/** Copied verbatim from CompanionTheLandingCommand.h's identical
	 * helper. */
	static uint64 spawnProp(ZoneServer* zoneServer, Zone* zone, const char* templatePath, float x, float y) {
		if (zoneServer == nullptr || zone == nullptr) {
			return 0;
		}

		ManagedReference<SceneObject*> prop = zoneServer->createObject(String(templatePath).hashCode(), 0);

		if (prop == nullptr) {
			return 0;
		}

		Locker propLocker(prop);

		float z = zone->getHeight(x, y);
		prop->initializePosition(x, z, y);
		zone->transferObject(prop, -1, true);

		return prop->getObjectID();
	}

	static void despawnProp(ZoneServer* zoneServer, uint64 objectID) {
		if (zoneServer == nullptr || objectID == 0) {
			return;
		}

		ManagedReference<SceneObject*> obj = zoneServer->getObject(objectID);

		if (obj == nullptr) {
			return;
		}

		Locker locker(obj);
		obj->destroyObjectFromWorld(true);
	}

	static void scheduleStep(int phase, ZoneServer* zoneServer, Reference<CreatureObject*> ownerRef, Reference<CompanionBirthdayState*> state, int delayMs) {
		Core::getTaskManager()->scheduleTask([phase, zoneServer, ownerRef, state] () {
			runPhase(phase, zoneServer, ownerRef, state);
		}, "CompanionBirthdayStepLambda", delayMs);
	}

	/** @pre { player and every resolved companion crosslocked to the
	 * player at call time (matches CompanionTheLandingCommand.h's own
	 * documented precondition) } */
	static void start(CreatureObject* owner, Vector<ManagedReference<CompanionObject*> >& companions) {
		if (owner == nullptr || owner->getZone() == nullptr || companions.size() == 0) {
			return;
		}

		// Shared Theater Mode overlap guard -- see file header. Every show
		// must arm/check/clear this same flag.
		owner->updateCooldownTimer("companion_theater_mode_busy", 600000);

		Zone* zone = owner->getZone();
		ZoneServer* zoneServer = owner->getZoneServer();

		if (zoneServer == nullptr) {
			return;
		}

		Reference<CompanionBirthdayState*> state = new CompanionBirthdayState();
		state->ownerID = owner->getObjectID();
		state->directorID = companions.get(0)->getObjectID();
		state->centerX = owner->getPositionX();
		state->centerY = owner->getPositionY();

		for (int i = 0; i < companions.size(); ++i) {
			CompanionObject* companion = companions.get(i);

			if (companion == nullptr || companion->isDead() || companion->getZone() != zone) {
				continue;
			}

			Locker clocker(companion, owner);

			Reference<CompanionBirthdayCastMember*> member = new CompanionBirthdayCastMember();
			member->companionID = companion->getObjectID();
			member->preShowPosture = companion->getPosture();
			state->castMembers.add(member);

			if (companion->isTaxiActive()) {
				companion->stopTaxiRide(false);
			}

			companion->setFollowObject(nullptr);
			companion->clearPatrolPoints();
			companion->setCompanionState(CompanionObject::THEATER);
		}

		// BIRTHDAY_FILLER_CAST_2026_07_30 -- top up to a minimum cast size with temporary
		// filler NPCs, same reasoning as CompanionTheLandingCommand.h's own
		// filler-cast pass: a player with too few real companions still
		// gets the full show.
		int fillerNeeded = BIRTHDAY_TARGET_CAST_SIZE - state->castMembers.size();

		if (fillerNeeded > 0) {
			spawnFillerCast(zoneServer, zone, state, fillerNeeded);
		}

		if (state->castMembers.size() == 0) {
			owner->updateCooldownTimer("companion_theater_mode_busy", 0);
			return;
		}

		ManagedReference<SceneObject*> directorObj = zoneServer->getObject(state->directorID);
		CompanionObject* director = directorObj != nullptr ? directorObj.castTo<CompanionObject*>().get() : nullptr;

		say(director, "Hold on a second -- we've got something for you.");

		Reference<CreatureObject*> ownerRef = owner;
		scheduleStep(1, zoneServer, ownerRef, state, 500);
	}

	/** Single dispatcher for every scheduled beat -- same shape as
	 * CompanionTheLandingCommand.h's runPhase(). */
	static void runPhase(int phase, ZoneServer* zoneServer, Reference<CreatureObject*> ownerRef, Reference<CompanionBirthdayState*> state) {
		CreatureObject* owner = ownerRef.get();

		if (owner == nullptr || state == nullptr || zoneServer == nullptr) {
			return;
		}

		Zone* zone = owner->getZone();

		if (zone == nullptr || owner->isInCombat()) {
			finishShow(zoneServer, owner, state);
			return;
		}

		// If every cast member has died/despawned/gone into combat since
		// the last beat, cut the show short rather than talking to an
		// empty field. Generic CreatureObject* check (not
		// CompanionObject*-specific), same reasoning as
		// CompanionTheLandingCommand.h's own alive-checks.
		int aliveCount = 0;

		for (int i = 0; i < state->castMembers.size(); ++i) {
			ManagedReference<SceneObject*> obj = zoneServer->getObject(state->castMembers.get(i).get()->companionID);
			CreatureObject* member = obj != nullptr ? obj.castTo<CreatureObject*>().get() : nullptr;

			if (member != nullptr && member->getZone() == zone && !member->isDead() && !member->isInCombat()) {
				++aliveCount;
			}
		}

		if (aliveCount == 0) {
			finishShow(zoneServer, owner, state);
			return;
		}

		switch (phase) {
		case 1:
			beatGather(zoneServer, owner, state);
			break;
		case 2:
			beatCakeReveal(zoneServer, owner, state);
			break;
		case 3:
			beatCelebrationPerform(zoneServer, owner, state);
			break;
		case 4:
			beatBannerRaise(zoneServer, owner, state);
			break;
		case 5:
			beatFinale(zoneServer, owner, state);
			break;
		case 6:
			finishShow(zoneServer, owner, state);
			break;
		default:
			finishShow(zoneServer, owner, state);
			break;
		}
	}

	// ---- 0:00-0:10 -- converge into a circle around the owner ------------
	static void beatGather(ZoneServer* zoneServer, CreatureObject* owner, Reference<CompanionBirthdayState*> state) {
		Zone* zone = owner->getZone();
		int total = state->castMembers.size();
		const float radius = 4.0f;

		for (int i = 0; i < total; ++i) {
			ManagedReference<SceneObject*> obj = zoneServer->getObject(state->castMembers.get(i).get()->companionID);
			AiAgent* member = obj != nullptr ? obj.castTo<AiAgent*>().get() : nullptr;

			if (member == nullptr || member->getZone() != zone || member->isDead()) {
				continue;
			}

			Locker clocker(member, owner);

			float memberAngle = (2.0f * (float) M_PI * i) / (float) Math::max(1, total);
			float tx = state->centerX + sin(memberAngle) * radius;
			float ty = state->centerY + cos(memberAngle) * radius;

			walkTo(member, zone, tx, ty);
		}

		Reference<CreatureObject*> ownerRef = owner;
		scheduleStep(2, zoneServer, ownerRef, state, 10000); // give 10s to converge
	}

	// ---- 0:10 -- cake reveal ----------------------------------------------
	static void beatCakeReveal(ZoneServer* zoneServer, CreatureObject* owner, Reference<CompanionBirthdayState*> state) {
		Zone* zone = owner->getZone();

		state->cakePropID = spawnProp(zoneServer, zone, BIRTHDAY_CAKE_TEMPLATE, state->centerX, state->centerY);

		ManagedReference<SceneObject*> directorObj = zoneServer->getObject(state->directorID);
		CompanionObject* director = directorObj != nullptr ? directorObj.castTo<CompanionObject*>().get() : nullptr;

		say(director, "Happy birthday!");

		Reference<CreatureObject*> ownerRef = owner;
		scheduleStep(3, zoneServer, ownerRef, state, 5000);
	}

	// ---- 0:15-0:35ish -- group celebration performance --------------------
	static void beatCelebrationPerform(ZoneServer* zoneServer, CreatureObject* owner, Reference<CompanionBirthdayState*> state) {
		Zone* zone = owner->getZone();
		int total = state->castMembers.size();

		for (int i = 0; i < total; ++i) {
			ManagedReference<SceneObject*> obj = zoneServer->getObject(state->castMembers.get(i).get()->companionID);
			AiAgent* member = obj != nullptr ? obj.castTo<AiAgent*>().get() : nullptr;

			if (member == nullptr || member->getZone() != zone || member->isDead()) {
				continue;
			}

			Locker clocker(member, owner);

			if (member->getPosture() != CreaturePosture::UPRIGHT) {
				member->setPosture(CreaturePosture::UPRIGHT, true, true);
			}

			member->setPerformanceType(PerformanceType::DANCE, true);
			member->setPerformanceAnimation("exotic4", true);
			member->doAnimation("skill_action_1");
		}

		Reference<CreatureObject*> ownerRef = owner;
		scheduleStep(4, zoneServer, ownerRef, state, 20000);
	}

	// ---- 0:40 -- banner raise ----------------------------------------------
	static void beatBannerRaise(ZoneServer* zoneServer, CreatureObject* owner, Reference<CompanionBirthdayState*> state) {
		Zone* zone = owner->getZone();

		state->bannerPropID = spawnProp(zoneServer, zone, BIRTHDAY_BANNER_TEMPLATE, state->centerX, state->centerY);

		int total = state->castMembers.size();

		for (int i = 0; i < total; ++i) {
			ManagedReference<SceneObject*> obj = zoneServer->getObject(state->castMembers.get(i).get()->companionID);
			AiAgent* member = obj != nullptr ? obj.castTo<AiAgent*>().get() : nullptr;

			if (member == nullptr || member->getZone() != zone || member->isDead()) {
				continue;
			}

			Locker clocker(member, owner);

			// End the dance performance mode before facing the owner.
			member->setPerformanceType(0, true);
			member->setPerformanceAnimation("", true);
			member->faceObject(owner, true);
		}

		Reference<CreatureObject*> ownerRef = owner;
		scheduleStep(5, zoneServer, ownerRef, state, 10000);
	}

	// ---- 0:50-1:05 -- finale: closing bow + director line ------------------
	static void beatFinale(ZoneServer* zoneServer, CreatureObject* owner, Reference<CompanionBirthdayState*> state) {
		Zone* zone = owner->getZone();
		int total = state->castMembers.size();

		for (int i = 0; i < total; ++i) {
			ManagedReference<SceneObject*> obj = zoneServer->getObject(state->castMembers.get(i).get()->companionID);
			AiAgent* member = obj != nullptr ? obj.castTo<AiAgent*>().get() : nullptr;

			if (member == nullptr || member->getZone() != zone || member->isDead()) {
				continue;
			}

			Locker clocker(member, owner);
			member->doAnimation("bow");
		}

		ManagedReference<SceneObject*> directorObj = zoneServer->getObject(state->directorID);
		CompanionObject* director = directorObj != nullptr ? directorObj.castTo<CompanionObject*>().get() : nullptr;

		say(director, "Many happy returns.");

		Reference<CreatureObject*> ownerRef = owner;
		scheduleStep(6, zoneServer, ownerRef, state, 15000);
	}

	/** Despawns both tracked props and restores every cast member's
	 * pre-show posture + standingOrder-based state. Safe to call from any
	 * phase as an early-abort as well as the normal finish -- same
	 * single-choke-point shape as CompanionTheLandingCommand.h's own
	 * finishShow(). */
	static void finishShow(ZoneServer* zoneServer, CreatureObject* owner, Reference<CompanionBirthdayState*> state) {
		if (owner != nullptr) {
			owner->updateCooldownTimer("companion_theater_mode_busy", 0);
		}

		despawnProp(zoneServer, state->cakePropID);
		despawnProp(zoneServer, state->bannerPropID);

		for (int i = 0; i < state->castMembers.size(); ++i) {
			CompanionBirthdayCastMember* member = state->castMembers.get(i).get();

			// BIRTHDAY_FILLER_CAST_2026_07_30 -- a filler has no pre-show state to restore;
			// despawn it unconditionally instead, same shape as
			// CompanionTheLandingCommand.h's own filler-vs-real finishShow()
			// branch.
			if (member->isFiller) {
				ManagedReference<SceneObject*> fillerObj = zoneServer->getObject(member->companionID);

				if (fillerObj != nullptr) {
					Locker fLocker(fillerObj);
					fillerObj->destroyObjectFromWorld(true);
				}

				continue;
			}

			ManagedReference<SceneObject*> obj = zoneServer->getObject(member->companionID);
			CompanionObject* companion = obj != nullptr ? obj.castTo<CompanionObject*>().get() : nullptr;

			if (companion == nullptr || companion->isDead()) {
				continue;
			}

			Locker clocker(companion, owner);

			// Same unconditional-clear-before-zone-null-check shape as
			// CompanionTheLandingCommand.h's THEATER_STRANDING_FIX_2026_07_30.
			companion->setCompanionState(CompanionObject::FOLLOW);

			if (companion->getZone() == nullptr) {
				continue;
			}

			int posture = member->preShowPosture;

			if (posture < 0 || posture > CreaturePosture::DEAD) {
				posture = CreaturePosture::UPRIGHT;
			}

			companion->setPosture(posture, true, true);
			companion->setPerformanceType(0, true);
			companion->setPerformanceAnimation("", true);
			restoreStanding(companion, owner);
		}
	}

};

class CompanionBirthdayCommand : public QueueCommand {
public:

	CompanionBirthdayCommand(const String& name, ZoneProcessServer* server)
		: QueueCommand(name, server) {

	}

	/** Copied verbatim from CompanionTheLandingCommand.h's identical
	 * helper. */
	void resolveActiveCompanions(CreatureObject* player, Vector<ManagedReference<CompanionObject*> >& companions) const {
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

		if (creature->isInCombat()) {
			creature->sendSystemMessage("Not while there's a fight going on!");
			return GENERALERROR;
		}

		// Shared Theater Mode overlap guard -- see file header.
		if (!creature->checkCooldownRecovery("companion_theater_mode_busy")) {
			creature->sendSystemMessage("A theater show is already in progress -- try again once it's finished.");
			return GENERALERROR;
		}

		Vector<ManagedReference<CompanionObject*> > companions;
		resolveActiveCompanions(creature, companions);

		if (companions.size() == 0) {
			creature->sendSystemMessage("@companion:no_active_companion"); // You have no active companion.
			return GENERALERROR;
		}

		CompanionBirthdayShow::start(creature, companions);

		return SUCCESS;
	}
};

#endif // COMPANIONBIRTHDAYCOMMAND_H_
