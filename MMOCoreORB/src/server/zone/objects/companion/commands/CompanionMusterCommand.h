/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-29, "Muster Call" -- ship-and-summon spine,
	per Nick's verbatim intent: "a ship flies down and lands, after landing
	summons everyone on the datapad, groups them, forms up in escort around
	me, ship leaves ~10s after landing"). ONE macro command, /companionmuster.

	Research confirmed against live source before writing this file:
	  1. Mass-summon + group + escort formup -- REUSED, not rebuilt.
	     - The summon-everyone-in-the-datapad loop is the same shape already
	       shipped in CompanionJenkinsCommand.h (registered as the standalone
	       "/jenkins" command, 2026-07-24/25 -- summon-only + escort formup,
	       no explicit grouping step). Duplicated here rather than shared,
	       matching this project's own established convention that every
	       Companion*Command.h keeps its own self-contained copy of
	       resolveActiveCompanions()/the datapad scan (see
	       CompanionFollowCommand.h's header comment for the rationale).
	     - Grouping is a genuinely NEW step this file adds on top of what
	       /jenkins does -- CompanionJenkinsCommand.h never calls
	       GroupManager. Modeled directly on CompanionGroupCommand.h's own
	       join-only half (GroupManager::instance()->inviteToGroup(owner,
	       companion), skipping anyone already grouped).
	     - Escort formup is FormationManager::instance()->formUp(owner,
	       "escort") -- confirmed the real, current, snap-teleport entry
	       point (FormationManager.h/.cpp): "escort" is one of the 7 real
	       FORMATION_NAMES (VIP-protection diamond, computeOffset()'s
	       "escort" branch). applyFormationOffsets() is a DIFFERENT, no-
	       snap re-arm-only entry point used by plain /companionfollow --
	       NOT what we want here, since the whole point is a hard teleport
	       into formation the instant the ship lands.
	  2. Ship cinematic -- confirmed against
	     server/zone/managers/gcw/tasks/LambdaShuttleWithReinforcementsTask.h.
	     Real state machine: SPAWNSHUTTLE -> UPRIGHT -> ZONEIN -> LAND ->
	     SPAWNTROOPS -> TAKEOFF -> CLOSINGIN -> DELAY -> PICKUP* -> DESPAWN ->
	     FINISHED. Confirmed "landing" is NOT a real flight/camera cutscene:
	     lambdaShuttleLanding()/Upright()/Takeoff() are pure
	     CreatureObject::setPosture(PRONE|UPRIGHT, true, true) calls; the
	     actual position placement is Zone::transferObject() (a snap, not a
	     flown path). Real template: "object/creature/npc/theme_park/
	     lambda_shuttle.iff" (it's a CreatureObject so setPosture works on
	     it). CompanionMusterShuttleTask below is a TRIMMED copy of that real
	     skeleton -- same SPAWNSHUTTLE/UPRIGHT/ZONEIN/LAND/TAKEOFF/DESPAWN/
	     FINISHED shape, with SPAWNTROOPS retargeted to SUMMON (calls the
	     summon+group+escort logic above instead of spawning troops) and all
	     the CLOSINGIN/DELAY/PICKUP, contraband-scan/faction/troop states
	     (MUSTER_CALL_HEADER_COMMENT_FIX_2026_08_01 -- the original prose here contained an
	     asterisk immediately followed by a slash, which is the C++
	     block-comment CLOSE token and silently ended this comment
	     early, breaking the build; fixed to a comma with identical
	     meaning. NOTE FOR FUTURE EDITS: do not write that same two-
	     character sequence -- star then slash -- anywhere inside this
	     comment block, including in THIS note.)
	     removed (out of scope -- this isn't a GCW reinforcement drop).
	     EXACT TIMING (documented per instructions -- these are the real
	     scheduled delays below, not approximations of some other number):
	       landing posture (LAND state) fires, then exactly 1000ms later
	       SUMMON runs (Nick's "after landing + 1s"), then exactly 9000ms
	       after THAT, TAKEOFF fires -- i.e. the ship departs exactly 10000ms
	       (10s) after the LAND state, matching Nick's "~10 seconds after
	       landing" spec precisely. DESPAWN follows 1000ms after TAKEOFF
	       (mirrors the stock task's own TAKEOFF->next-state TASKDELAY gap).
	  3. Sound -- CreatureObjectImplementation::playMusicMessage(const
	     String& file) confirmed real (CreatureObjectImplementation.cpp:551,
	     declared `public native void playMusicMessage(final string file);`
	     in CreatureObject.idl:344). Real call-site precedent:
	     MissionManagerImplementation.cpp:566, `player->playMusicMessage(
	     "sound/music_themequest_fail_criminal.snd")`. Grepped this
	     codebase's C++ for any ship-adjacent .snd already in use: found
	     exactly ONE real hit, "sound/ship_hyperspace_countdown.snd"
	     (HyperspaceToLocationTask.h:89, ShipObjectImplementation's
	     sendShipMembersMusicMessage()) -- but that's a hyperspace countdown
	     cue, not a landing/engine/takeoff sound, so reusing it here would be
	     a thematic guess dressed up as a "confirmed" one. Per instructions,
	     NOT wiring in sound tonight rather than guess -- SOUND IS DEFERRED,
	     see the NO-OP comment at the SPAWNSHUTTLE/TAKEOFF states below for
	     exactly where a real call would go once Nick (or a future research
	     pass) confirms a genuinely fitting stock ship-engine/landing .snd
	     exists in the client TRE.

	DEFERRED (explicitly out of scope tonight, do not build/guess at any of
	these -- separate, independently-scoped follow-ups):
	  - Ranger tent auto-deploy
	  - Doctor field-buffing ("call a droid" / buff-in-the-wild)
	  - Entertainer dance-loop integration (exotic4/flourish auto-buff loop)
	  - Seat/chair object targeting ("seat"-named camp props)
	  - Randomized greeting emotes (nod/grin/angry/etc mill-around behavior)
	  - Any new .snd sound authoring (see point 3 above -- reuse-only, and
	    even reuse was skipped this round for lack of a fitting real file)
*/

#ifndef COMPANIONMUSTERCOMMAND_H_
#define COMPANIONMUSTERCOMMAND_H_

#include <cmath>

#include "server/zone/objects/creature/commands/QueueCommand.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/objects/intangible/PetControlDevice.h"
#include "server/zone/objects/group/GroupObject.h"
#include "server/zone/managers/group/GroupManager.h"
#include "server/zone/managers/companion/FormationManager.h"
#include "server/zone/managers/companion/CompanionChatter.h"
#include "server/zone/managers/collision/CollisionManager.h"

/**
 * Deferred ship state machine -- see file header for the full research
 * trail. Trimmed copy of LambdaShuttleWithReinforcementsTask.h's real
 * skeleton (SPAWNSHUTTLE/UPRIGHT/ZONEIN/LAND/TAKEOFF/DESPAWN/FINISHED),
 * retargeting its SPAWNTROOPS step to a datapad mass-summon + group +
 * escort-formup instead of spawning GCW troops. ManagedWeakReference to the
 * owner, re-locked inside run(), matching every other deferred-task pattern
 * in this codebase (e.g. CompanionJenkinsFormUpTask in
 * CompanionJenkinsCommand.h, PetControlDeviceStoreTask.h).
 */
class CompanionMusterShuttleTask : public Task {
	ManagedWeakReference<CreatureObject*> ownerRef;
	ManagedReference<SceneObject*> shuttle;

	Vector3 spawnPosition;
	Quaternion spawnDirection;

	enum MusterShuttleState {
		SPAWNSHUTTLE,
		UPRIGHT,
		ZONEIN,
		LAND,
		SUMMON,
		TAKEOFF,
		DESPAWN,
		FINISHED
	};

	MusterShuttleState state;

	static const int TASKDELAY = 1000; // 1s beats, matches stock task's own TASKDELAY
	static const int DESCENDDELAY = 2000; // cosmetic pause between "zoned in" and "posture snaps to landed" -- there is no real flight path to time against, see file header point 2
	static const int LANDEDBEFORESUMMON = 1000; // Nick's "after landing + 1s"
	static const int GROUNDTIMEAFTERSUMMON = 9000; // 1000 + 9000 = 10000ms dwell-on-ground total, i.e. departs ~10s after landing per Nick's spec

	const String SHUTTLETEMPLATE = "object/creature/npc/theme_park/lambda_shuttle.iff";

	/** See CompanionFollowCommand.h's identical helper for the rationale
	 * (every Companion*Command.h keeps its own self-contained copy rather
	 * than sharing one). */
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

	/**
	 * The retargeted "SPAWNTROOPS" step (see file header point 2). Summon
	 * logic is the same shape as CompanionJenkinsCommand.h's doQueueCommand()
	 * loop (duplicated per this project's own header-only-command
	 * convention, not shared). Grouping is new here -- CompanionJenkinsCommand
	 * never groups. Escort formup reuses FormationManager::formUp(), the
	 * real, confirmed snap-teleport entry point.
	 */
	void summonGroupAndFormUp(CreatureObject* owner) const {
		ManagedReference<SceneObject*> datapad = owner->getSlottedObject("datapad");

		if (datapad == nullptr) {
			return;
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

				// Same revive-on-summon behavior as /jenkins (2026-07-29 fix
				// -- see CompanionJenkinsCommand.h's own comment): don't skip
				// dead companions, spawnObject() itself auto-revives them.
				CompanionObject* companion = device->getCompanionObject();

				if (companion != nullptr && companion->getZone() != nullptr) {
					continue; // already summoned, nothing to do
				}

				Locker clocker(device, owner);
				device->spawnObject(owner);
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

				Locker plocker(device, owner);
				device->spawnObject(owner);
				summonedPets++;
			}
		}

		Vector<ManagedReference<CompanionObject*>> companions;
		resolveActiveCompanions(owner, companions);

		// NEW step vs. /jenkins: group every summoned companion that isn't
		// already grouped. Same call shape as CompanionGroupCommand.h's
		// join-only half.
		for (int i = 0; i < companions.size(); ++i) {
			CompanionObject* companion = companions.get(i);

			Locker clocker(companion, owner);

			if (companion->getGroup() == nullptr) {
				GroupManager::instance()->inviteToGroup(owner, companion);
			}
		}

		// Escort formup -- real, confirmed snap-teleport entry point.
		FormationManager::instance()->formUp(owner, "escort");

		if (summonedCompanions > 0 || summonedPets > 0 || companions.size() > 0) {
			CompanionChatter::announceOrder(owner, "Squad -- muster up, escort formation on me!", "formup", companions);
		}
	}

	void setShuttlePosture(SceneObject* shuttleObj, int posture) {
		CreatureObject* shuttleCreature = shuttleObj->asCreatureObject();

		if (shuttleCreature == nullptr) {
			return;
		}

		shuttleCreature->setPosture(posture, true, true);
	}

public:
	CompanionMusterShuttleTask(CreatureObject* owner, const Vector3& position, const Quaternion& direction) {
		ownerRef = owner;
		spawnPosition = position;
		spawnDirection = direction;
		state = SPAWNSHUTTLE;
	}

	void run() {
		ManagedReference<CreatureObject*> owner = ownerRef.get();

		if (owner == nullptr) {
			return;
		}

		Locker locker(owner);

		// Create the shuttle (if this is the first tick) BEFORE locking it --
		// same ordering CampDeploymentManager.cpp's placeCamp() flow uses for
		// its own freshly-created CampSiteActiveArea (create, THEN lock,
		// THEN touch/position/transfer -- never touch an object before it
		// has its own lock held).
		if (state == SPAWNSHUTTLE && shuttle == nullptr) {
			ZoneServer* zoneServer = owner->getZoneServer();

			if (zoneServer == nullptr) {
				return;
			}

			shuttle = zoneServer->createObject(SHUTTLETEMPLATE.hashCode(), 0);
		}

		ManagedReference<SceneObject*> localShuttle = shuttle;

		if (localShuttle == nullptr) {
			// Either creation failed just now, or we already reached
			// DESPAWN/FINISHED on a prior tick and cleared the reference.
			return;
		}

		Locker sLock(localShuttle, owner);

		try {
			switch (state) {
			case SPAWNSHUTTLE: {
				localShuttle->initializePosition(spawnPosition.getX(), spawnPosition.getZ(), spawnPosition.getY());
				localShuttle->setDirection(spawnDirection);

				// MUSTER_CALL_SHUTTLE_VISIBILITY_REORDER_2026_08_01 -- transferObject() moved here (was
				// previously in a later ZONEIN state, ~2s after this tick) so the
				// shuttle actually enters the world/becomes visible on this very
				// first tick instead of sitting created-but-unzoned (and
				// therefore invisible) for two full ticks. Also fixes a real
				// ordering bug this exposed: the old UPRIGHT state called
				// setPosture() on localShuttle BEFORE it had ever been
				// transferred into a zone -- a posture set on an unzoned object
				// has no zone to broadcast through, so that call was a silent
				// no-op.
				Zone* zone = owner->getZone();

				if (zone == nullptr || !zone->transferObject(localShuttle, -1, true)) {
					state = FINISHED;
					break;
				}

				localShuttle->createChildObjects();
				localShuttle->_setUpdated(true);

				// SOUND (deferred -- see file header point 3): a confirmed-
				// real ship .snd would be played here via
				// owner->playMusicMessage("sound/<file>.snd") the instant the
				// shuttle appears. No fitting stock file confirmed tonight --
				// left as a no-op, not a guess.
				state = UPRIGHT;
				reschedule(TASKDELAY);
				break;
			}
			case UPRIGHT:
				setShuttlePosture(localShuttle, CreaturePosture::UPRIGHT);
				state = ZONEIN;
				reschedule(TASKDELAY);
				break;
			case ZONEIN:
				// MUSTER_CALL_SHUTTLE_VISIBILITY_REORDER_2026_08_01 -- the real zone insertion now happens up
				// in SPAWNSHUTTLE (see above); this state is kept as a plain
				// passthrough tick purely so the total elapsed-time budget
				// before LAND (and therefore the documented "lands, +1s summon,
				// +9s takeoff" timing) is unchanged.
				state = LAND;
				reschedule(DESCENDDELAY);
				break;
			case LAND:
				setShuttlePosture(localShuttle, CreaturePosture::PRONE);
				state = SUMMON;
				reschedule(LANDEDBEFORESUMMON);
				break;
			case SUMMON:
				summonGroupAndFormUp(owner);
				state = TAKEOFF;
				reschedule(GROUNDTIMEAFTERSUMMON);
				break;
			case TAKEOFF:
				setShuttlePosture(localShuttle, CreaturePosture::UPRIGHT);

				// SOUND (deferred -- see file header point 3, same no-op as
				// SPAWNSHUTTLE above, this time for a takeoff/engine cue).
				state = DESPAWN;
				reschedule(TASKDELAY);
				break;
			case DESPAWN:
				localShuttle->destroyObjectFromWorld(true);
				shuttle = nullptr;
				state = FINISHED;
				break;
			default:
				break;
			}
		} catch (Exception& e) {
			owner->error() << "exception caught in CompanionMusterShuttleTask " << e.what();
			e.printStackTrace();
		}
	}
};

/**
 * /companionmuster -- the one macro command. Spawns the shuttle a short
 * distance in front of the owner (forward/right vector derived from the
 * owner's heading, same sin/cos convention FormationManager::
 * arrangeFollowers() uses and documents as verified-consistent with the
 * engine's own formationOffset blackboard rotation), then hands off to
 * CompanionMusterShuttleTask for the whole land -> summon/group/escort ->
 * takeoff -> despawn sequence.
 */
class CompanionMusterCommand : public QueueCommand {
public:

	CompanionMusterCommand(const String& name, ZoneProcessServer* server)
		: QueueCommand(name, server) {

	}

	int doQueueCommand(CreatureObject* creature, const uint64& target, const UnicodeString& arguments) const {
		if (!checkStateMask(creature)) {
			return INVALIDSTATE;
		}

		if (!checkInvalidLocomotions(creature)) {
			return INVALIDLOCOMOTION;
		}

		Zone* zone = creature->getZone();

		if (zone == nullptr) {
			return GENERALERROR;
		}

		ManagedReference<SceneObject*> datapad = creature->getSlottedObject("datapad");

		if (datapad == nullptr) {
			creature->sendSystemMessage("You have no datapad.");
			return GENERALERROR;
		}

		static const float SHUTTLE_DISTANCE = 15.f;

		float headingAngle = creature->getDirectionAngle();

		// Forward unit vector from heading -- same sin/cos convention as
		// FormationManager::arrangeFollowers() (verified there against the
		// engine's own formationOffset blackboard rotation).
		float forwardX = std::sin(headingAngle);
		float forwardY = std::cos(headingAngle);

		float shuttleX = creature->getPositionX() + forwardX * SHUTTLE_DISTANCE;
		float shuttleY = creature->getPositionY() + forwardY * SHUTTLE_DISTANCE;
		float shuttleZ = CollisionManager::getWorldFloorCollision(shuttleX, shuttleY, zone, true);

		// Vector3's (x, y, z) fields map directly to world (X, Y, Z) with Z
		// = height -- confirmed via Coordinate.h's real
		// initializePosition(float x, float z, float y) signature and its
		// real call site in LambdaShuttleWithReinforcementsTask.h
		// (lambdaShuttleSpawn() passes spawnPosition.getX()/getZ()/getY()
		// into that x/z/y parameter order), so construction order here must
		// be (planarX, planarY, height) -- NOT (planarX, height, planarY).
		Vector3 spawnPosition(shuttleX, shuttleY, shuttleZ);

		creature->sendSystemMessage("A shuttle roars in overhead...");

		Reference<CompanionMusterShuttleTask*> shuttleTask = new CompanionMusterShuttleTask(creature, spawnPosition, *creature->getDirection()); // MUSTER_CALL_DIRECTION_DEREF_FIX_2026_08_01 -- getDirection() returns a pointer-like reference, not a plain Quaternion
		shuttleTask->schedule(100);

		return SUCCESS;
	}
};

#endif // COMPANIONMUSTERCOMMAND_H_
