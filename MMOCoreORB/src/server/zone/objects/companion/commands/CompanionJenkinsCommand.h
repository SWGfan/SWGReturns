/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-24, "Muster Call" pass, per user request "lets
	make a new macro button, that calls all our companions... spawn all the
	companions in the users datapad, along with any pets they might have,
	group all of them and form them up in an escort mode"). Renamed to
	"Jenkins" (2026-07-25, per user request -- functionally identical to the
	original "Muster Call" design, class/file/command name only).

	Phase 1 of the fuller design (ship cinematic, camp behavior, profession
	auto-actions, and auto-gearing are separate later passes -- see
	claude/notes-to-c3rr.md for the open research questions those need).
	This command's whole job: summon every STORED (not-yet-active) companion
	AND pet from the caller's datapad, then hand off to the ALREADY-BUILT
	FormationManager::formUp(owner, "escort") -- it already gathers every
	active pet/droid (via PlayerObject's active-pet list) and every active
	companion (via this same datapad scan) and arranges them in the
	VIP-protection escort diamond (FormationManager.cpp's own
	computeOffset() "escort" branch). The only new work here is making sure
	everyone is actually IN THE WORLD first, since formUp() only ever
	arranges followers that are already spawned.

	1-second delay before forming up (matches the "wait 1 second" cadence
	from Nick's fuller design) via CompanionJenkinsFormUpTask below, mirroring
	the real deferred-task pattern this codebase already uses for control
	devices (PetControlDeviceStoreTask.h) -- ManagedWeakReference + re-locked
	inside run(), not a raw pointer held across the delay.
*/

#ifndef COMPANIONJENKINSCOMMAND_H_
#define COMPANIONJENKINSCOMMAND_H_

#include "server/zone/objects/creature/commands/QueueCommand.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/objects/intangible/PetControlDevice.h"
#include "server/zone/managers/companion/FormationManager.h"
#include "server/zone/managers/companion/CampDeploymentManager.h" // JENKINS_EXTRAS_2026_07_30
#include "server/zone/Zone.h"
#include "server/zone/ZoneServer.h"
#include "templates/params/creature/CreaturePosture.h"

// JENKINS_SHIP_CINEMATIC_2026_07_30 -- per Nick: "lets get the other plays coded in" --
// finishes the "ship cinematic" this file's own header explicitly
// deferred back on 2026-07-24/25. Real, already-used-elsewhere-in-this-
// codebase template (LambdaShuttleWithReinforcementsTask.h's own
// LAMBDATEMPLATE) -- the same shuttle this project's GCW system already
// spawns for its own arrive/land/depart cinematic, reused here instead
// of inventing a new asset.
#define JENKINS_SHUTTLE_TEMPLATE "object/creature/npc/theme_park/lambda_shuttle.iff"

// JENKINS_SHIP_ARRIVAL_HOTFIX_2026_07_30 -- tunables for the flown-in arrival, same
// teleport()-based interpolated-movement technique
// CompanionBattleTheaterFlyoverTask already proved out.
#define JENKINS_ARRIVAL_TICKS 6
#define JENKINS_ARRIVAL_TICK_MS 800
#define JENKINS_ARRIVAL_ALTITUDE 25.f
#define JENKINS_ARRIVAL_DISTANCE 60.f

/** Deferred formation call -- see file header. Runs 1s after CompanionJenkinsCommand
 * fires, once every freshly-summoned companion/pet has actually landed in the zone. */
class CompanionJenkinsFormUpTask : public Task {
	ManagedWeakReference<CreatureObject*> ownerRef;

public:
	CompanionJenkinsFormUpTask(CreatureObject* owner) {
		ownerRef = owner;
	}

	void run() {
		CreatureObject* owner = ownerRef.get();

		if (owner == nullptr) {
			return;
		}

		Locker locker(owner);

		FormationManager::instance()->formUp(owner, "escort");
	}
};

/** JENKINS_SHIP_CINEMATIC_2026_07_30 -- drives the shuttle spawned by
 * spawnMusterShuttle() below through its arrival/hold/departure beats.
 * Mirrors this project's own LambdaShuttleWithReinforcementsTask.h
 * precedent: the shuttle's "flight" is a posture toggle (UPRIGHT =
 * hovering/lifted, PRONE = landed) plus static repositioning, not real
 * movement -- exactly the mechanism that file already uses for the
 * same template. Re-resolves the shuttle by object ID fresh on every
 * stage (never holds a raw pointer across the delay), matching this
 * file's own CompanionJenkinsFormUpTask precedent. Stages: 0 = just
 * spawned hovering -> touch down; 1 = landed, holding while the squad
 * forms up -> lift off; 2 = lifted off -> despawn. */
class CompanionJenkinsShuttleTask : public Task {
	ManagedWeakReference<CreatureObject*> ownerRef;
	uint64 shuttleID;
	int stage;

public:
	CompanionJenkinsShuttleTask(CreatureObject* owner, uint64 shuttleID, int stage) {
		ownerRef = owner;
		this->shuttleID = shuttleID;
		this->stage = stage;
	}

	void run() {
		CreatureObject* owner = ownerRef.get();

		if (owner == nullptr) {
			return;
		}

		ZoneServer* zoneServer = owner->getZoneServer();

		if (zoneServer == nullptr) {
			return;
		}

		ManagedReference<SceneObject*> shuttleObj = zoneServer->getObject(shuttleID);

		if (shuttleObj == nullptr) {
			return;
		}

		Locker slocker(shuttleObj, owner);

		if (stage == 0) {
			// Touch down.
			CreatureObject* shuttle = shuttleObj->asCreatureObject();

			if (shuttle != nullptr) {
				shuttle->setPosture(CreaturePosture::PRONE, true, true);
			}

			Reference<CompanionJenkinsShuttleTask*> next = new CompanionJenkinsShuttleTask(owner, shuttleID, 1);
			next->schedule(8000); // hold on the ground while the squad forms up
		} else if (stage == 1) {
			// Lift off.
			CreatureObject* shuttle = shuttleObj->asCreatureObject();

			if (shuttle != nullptr) {
				shuttle->setPosture(CreaturePosture::UPRIGHT, true, true);
			}

			Reference<CompanionJenkinsShuttleTask*> next = new CompanionJenkinsShuttleTask(owner, shuttleID, 2);
			next->schedule(4000); // let the takeoff pose land before despawning
		} else {
			shuttleObj->destroyObjectFromWorld(true);
		}
	}
};

/** JENKINS_SHIP_ARRIVAL_HOTFIX_2026_07_30 -- flies the muster shuttle in from
 * JENKINS_ARRIVAL_DISTANCE away at JENKINS_ARRIVAL_ALTITUDE down onto
 * the landing spot over JENKINS_ARRIVAL_TICKS ticks, descending as it
 * approaches -- same teleport()-based interpolated movement
 * CompanionBattleTheaterFlyoverTask already proved out. Re-resolves
 * the shuttle by object ID fresh every tick (never holds a raw
 * pointer across the delay), matching every other deferred-task
 * precedent in this project. On the final tick, touches down (PRONE)
 * and hands off to the EXISTING CompanionJenkinsShuttleTask at stage
 * 1 (hold -> lift off -> despawn), completely unchanged -- only the
 * arrival was missing, not the departure. */
class CompanionJenkinsArrivalTask : public Task {
	ManagedWeakReference<CreatureObject*> ownerRef;
	uint64 shuttleID;
	float startX, startY, endX, endY;
	int tick;

public:
	CompanionJenkinsArrivalTask(CreatureObject* owner, uint64 shuttleID, float startX, float startY, float endX, float endY, int tick) {
		ownerRef = owner;
		this->shuttleID = shuttleID;
		this->startX = startX;
		this->startY = startY;
		this->endX = endX;
		this->endY = endY;
		this->tick = tick;
	}

	void run() {
		CreatureObject* owner = ownerRef.get();

		if (owner == nullptr) {
			return;
		}

		ZoneServer* zoneServer = owner->getZoneServer();
		Zone* zone = owner->getZone();

		if (zoneServer == nullptr || zone == nullptr) {
			return;
		}

		ManagedReference<SceneObject*> shuttleObj = zoneServer->getObject(shuttleID);

		if (shuttleObj == nullptr) {
			return;
		}

		Locker slocker(shuttleObj, owner);

		float progress = (float) tick / (float) (JENKINS_ARRIVAL_TICKS - 1);
		float px = startX + (endX - startX) * progress;
		float py = startY + (endY - startY) * progress;

		if (tick >= JENKINS_ARRIVAL_TICKS - 1) {
			// Final tick -- touch down exactly at the landing spot.
			float pz = zone->getHeight(endX, endY);
			shuttleObj->teleport(endX, pz, endY, 0);

			CreatureObject* shuttle = shuttleObj->asCreatureObject();

			if (shuttle != nullptr) {
				shuttle->setPosture(CreaturePosture::PRONE, true, true);
			}

			Reference<CompanionJenkinsShuttleTask*> next = new CompanionJenkinsShuttleTask(owner, shuttleID, 1);
			next->schedule(8000); // hold on the ground while the squad forms up -- unchanged from before
			return;
		}

		float altitude = JENKINS_ARRIVAL_ALTITUDE * (1.f - progress);
		float pz = zone->getHeight(px, py) + altitude;

		shuttleObj->teleport(px, pz, py, 0);

		Reference<CompanionJenkinsArrivalTask*> next = new CompanionJenkinsArrivalTask(owner, shuttleID, startX, startY, endX, endY, tick + 1);
		next->schedule(JENKINS_ARRIVAL_TICK_MS);
	}
};

/** JENKINS_SHIP_CINEMATIC_2026_07_30 -- spawns the muster-call shuttle a short distance
 * in front of `owner`, arriving already hovering (see
 * CompanionJenkinsShuttleTask for the touch-down/hold/departure beats
 * that follow). Same createObject/initializePosition/transferObject
 * sequence CompanionTheLandingCommand.h's spawnProp() already uses.
 * @pre { nothing locked } */
static void spawnMusterShuttle(CreatureObject* owner) {
	if (owner == nullptr) {
		return;
	}

	Zone* zone = owner->getZone();
	ZoneServer* zoneServer = owner->getZoneServer();

	if (zone == nullptr || zoneServer == nullptr) {
		return;
	}

	float angle = owner->getDirectionAngle() * (float) (M_PI / 180.0);
	float fwdX = sin(angle);
	float fwdY = cos(angle);

	// JENKINS_SHIP_ARRIVAL_HOTFIX_2026_07_30 -- lx/ly is the same landing spot as before; the
	// shuttle now spawns JENKINS_ARRIVAL_DISTANCE away at altitude and
	// visibly flies in to it via CompanionJenkinsArrivalTask, instead of
	// instantiating already-hovering at the final spot with no arrival
	// cue (confirmed live: "it just appeared and then started to leave").
	float lx = owner->getPositionX() + fwdX * 15.f;
	float ly = owner->getPositionY() + fwdY * 15.f;
	float sx = lx + fwdX * JENKINS_ARRIVAL_DISTANCE;
	float sy = ly + fwdY * JENKINS_ARRIVAL_DISTANCE;

	ManagedReference<SceneObject*> shuttleObj = zoneServer->createObject(String(JENKINS_SHUTTLE_TEMPLATE).hashCode(), 0);

	if (shuttleObj == nullptr) {
		return;
	}

	uint64 shuttleID;

	{
		Locker shuttleLocker(shuttleObj);

		float sz = zone->getHeight(sx, sy) + JENKINS_ARRIVAL_ALTITUDE;
		shuttleObj->initializePosition(sx, sz, sy);
		zone->transferObject(shuttleObj, -1, true);

		CreatureObject* shuttle = shuttleObj->asCreatureObject();

		if (shuttle != nullptr) {
			shuttle->setPosture(CreaturePosture::UPRIGHT, true, true);
		}

		shuttleID = shuttleObj->getObjectID();
	}

	Reference<CompanionJenkinsArrivalTask*> arrivalTask = new CompanionJenkinsArrivalTask(owner, shuttleID, sx, sy, lx, ly, 1);
	arrivalTask->schedule(JENKINS_ARRIVAL_TICK_MS);
}

/** JENKINS_EXTRAS_2026_07_30 -- ~4s after muster, if any active companion has a
 * learned skill beginning with "social_entertainer_" (same detection
 * CampDeploymentManager.cpp's own camp-ambiance code already uses),
 * automatically starts them performing for the owner via the existing,
 * already-shipped CampDeploymentManager::startEntertainerDanceWatch().
 * No new dance logic -- just a new trigger point for the real one. */
class CompanionJenkinsEntertainerTask : public Task {
	ManagedWeakReference<CreatureObject*> ownerRef;

public:
	CompanionJenkinsEntertainerTask(CreatureObject* owner) {
		ownerRef = owner;
	}

	void run() {
		CreatureObject* owner = ownerRef.get();

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

			if (device->isCompanionDead()) {
				continue;
			}

			CompanionObject* companion = device->getCompanionObject();

			if (companion == nullptr || companion->getZone() == nullptr) {
				continue;
			}

			bool isEntertainer = false;

			for (int s = 0; s < companion->getLearnedSkillCount(); ++s) {
				if (companion->getLearnedSkill(s).beginsWith("social_entertainer_")) {
					isEntertainer = true;
					break;
				}
			}

			if (!isEntertainer) {
				continue;
			}

			Locker locker(owner);

			CampDeploymentManager::instance()->startEntertainerDanceWatch(owner, companion);

			return;
		}
	}
};

// JENKINS_EXTRAS_2026_07_30 -- randomized squad call-out; the original line is
// kept as one of several so existing flavor isn't lost.
static const char* JENKINS_GREETINGS[] = {
	"LEEEroooooyyyyy JEEENNnkkkKKiiInnnsss",
	"Squad up! Move, move, move!",
	"On me! We roll out now!",
	"Everybody in position, on the double!",
	"Saddle up, let's go!"
};
#define JENKINS_GREETINGS_COUNT 5

class CompanionJenkinsCommand : public QueueCommand {
public:

	CompanionJenkinsCommand(const String& name, ZoneProcessServer* server)
		: QueueCommand(name, server) {

	}

	int doQueueCommand(CreatureObject* creature, const uint64& target, const UnicodeString& arguments) const {
		if (!checkStateMask(creature)) {
			return INVALIDSTATE;
		}

		if (!checkInvalidLocomotions(creature)) {
			return INVALIDLOCOMOTION;
		}

		ManagedReference<SceneObject*> datapad = creature->getSlottedObject("datapad");

		if (datapad == nullptr) {
			creature->sendSystemMessage("You have no datapad.");
			return GENERALERROR;
		}

		int summonedCompanions = 0;
		int summonedPets = 0;

		for (int i = 0; i < datapad->getContainerObjectsSize(); ++i) {
			ManagedReference<SceneObject*> obj = datapad->getContainerObject(i);

			if (obj == nullptr) {
				continue;
			}

			if (obj->isCompanionControlDevice()) {
				CompanionControlDevice* device = cast<CompanionControlDevice*>(obj.get());

				if (device == nullptr) {
					continue;
				}

				// Companion System (2026-07-29 fix, per Nick: "when a companion dies, and the
				// jenkins command is used, it should spawn them in as well ... it will not
				// spawn in until i click on the companion in the datapad myself"). This used
				// to also skip here when device->isCompanionDead(), on the assumption that
				// "Revive is a separate, explicit order" -- but that's stale: the single-
				// click datapad radial (CompanionControlDeviceImplementation::
				// handleObjectMenuSelect()) never skips a dead companion either, it just
				// calls spawnObject() unconditionally, and spawnObject() ITSELF auto-revives
				// (isDead -> reviveCompanion() -> full HAM/vitality restore) before falling
				// through into the normal spawn flow -- exactly why clicking the companion
				// directly already worked. Dropping the isCompanionDead() skip here lets a
				// dead companion fall through to the same device->spawnObject(creature) call
				// below, reusing that exact revive-then-spawn logic instead of duplicating
				// it, so Jenkins now matches the working single-click path.
				CompanionObject* companion = device->getCompanionObject();

				if (companion != nullptr && companion->getZone() != nullptr) {
					continue; // already summoned, nothing to do
				}

				Locker clocker(device, creature);
				device->spawnObject(creature);
				summonedCompanions++;
			} else if (obj->isPetControlDevice()) {
				PetControlDevice* device = cast<PetControlDevice*>(obj.get());

				if (device == nullptr) {
					continue;
				}

				ManagedReference<TangibleObject*> controlledObject = device->getControlledObject();

				if (controlledObject != nullptr && controlledObject->isAiAgent()) {
					AiAgent* liveAgent = cast<AiAgent*>(controlledObject.get());

					if (liveAgent != nullptr && liveAgent->getZone() != nullptr) {
						continue; // already summoned
					}
				}

				Locker plocker(device, creature);
				device->spawnObject(creature);
				summonedPets++;
			}
		}

		if (summonedCompanions == 0 && summonedPets == 0) {
			creature->sendSystemMessage("Everyone's already mustered up.");
		} else {
			// JENKINS_SHIP_CINEMATIC_2026_07_30 -- only when something was actually
			// summoned (the all-already-mustered branch above stays quiet).
			spawnMusterShuttle(creature);

			// JENKINS_EXTRAS_2026_07_30 -- randomized greeting, was a single hardcoded line.
			creature->sendSystemMessage(JENKINS_GREETINGS[System::random(JENKINS_GREETINGS_COUNT - 1)]);

			String msg = "Mustering the squad";

			if (summonedCompanions > 0) {
				msg += " -- " + String::valueOf(summonedCompanions) + " companion" + (summonedCompanions == 1 ? "" : "s");
			}

			if (summonedPets > 0) {
				msg += (summonedCompanions > 0 ? " and " : " -- ") + String::valueOf(summonedPets) + " pet" + (summonedPets == 1 ? "" : "s");
			}

			creature->sendSystemMessage(msg + " called in.");
		}

		Reference<CompanionJenkinsFormUpTask*> formUpTask = new CompanionJenkinsFormUpTask(creature);
		formUpTask->schedule(1000);

		// JENKINS_EXTRAS_2026_07_30 -- entertainer dance-loop integration, see class
		// comment above.
		Reference<CompanionJenkinsEntertainerTask*> entertainerTask = new CompanionJenkinsEntertainerTask(creature);
		entertainerTask->schedule(4000);

		return SUCCESS;
	}
};

#endif // COMPANIONJENKINSCOMMAND_H_
