/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System -- see FormationManager.h and NOTES.md.
*/

#include "FormationManager.h"
#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/creature/ai/AiAgent.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/objects/scene/SceneObject.h"
#include "server/zone/objects/cell/CellObject.h"
#include "server/zone/Zone.h"

#include <cmath>

const char* const FormationManager::FORMATION_NAMES[FormationManager::FORMATION_COUNT] = {
	"line", "wedge", "box", "column", "vanguard", "escort", "stack"
};

FormationManager::FormationManager() : Logger("FormationManager") {
	setGlobalLogging(true);
	setLogging(false);

	lastFormationByOwner.setNullValue(String(""));
}

bool FormationManager::isValidFormation(const String& formationType) const {
	for (int i = 0; i < FORMATION_COUNT; ++i) {
		if (formationType == FORMATION_NAMES[i]) {
			return true;
		}
	}

	return false;
}

String FormationManager::getFormationForOwner(CreatureObject* owner) {
	if (owner == nullptr) {
		return "wedge";
	}

	Locker locker(&formationTableMutex);

	String last = lastFormationByOwner.get(owner->getObjectID());

	if (last.isEmpty()) {
		return "wedge";
	}

	return last;
}

void FormationManager::computeOffset(const String& formationType, int slotIndex, int totalFollowers, float& outForward, float& outRight) const {
	// slotIndex is 0-based across all followers (pets + droids + companions).
	// Offsets are expressed in meters, forward = ahead of the owner
	// (negative = behind), right = lateral offset from the owner's
	// centerline.
	int row = slotIndex / 3;
	int col = slotIndex % 3;

	if (formationType == "line") {
		// Single rank abreast, owner at the center.
		int centeredCol = slotIndex - (totalFollowers - 1) / 2;
		outForward = -DEFAULT_SPACING;
		outRight = centeredCol * DEFAULT_SPACING;
	} else if (formationType == "wedge") {
		// V shape trailing the owner, alternating left/right, deepening with
		// distance.
		int rank = (slotIndex / 2) + 1;
		bool leftSide = (slotIndex % 2) == 0;

		outForward = -DEFAULT_SPACING * rank;
		outRight = (leftSide ? -1 : 1) * DEFAULT_SPACING * rank;
	} else if (formationType == "column") {
		// 2026-07-17 ("militant formations" pass) -- single file directly
		// behind the owner, marching-order style. Best for narrow streets,
		// cave corridors, and doorways.
		outForward = -DEFAULT_SPACING * (slotIndex + 1);
		outRight = 0.f;
	} else if (formationType == "vanguard") {
		// 2026-07-17 -- full rank IN FRONT of the owner, walking point.
		// First rank of up to 5 abreast at 2x spacing ahead (so the owner
		// isn't clipping through them), additional ranks stack further
		// forward.
		int vRow = slotIndex / 5;
		int vCol = slotIndex % 5;
		int rankSize = totalFollowers - vRow * 5;

		if (rankSize > 5) {
			rankSize = 5;
		}

		int centeredCol = vCol - (rankSize - 1) / 2;

		outForward = DEFAULT_SPACING * 2 + vRow * DEFAULT_SPACING;
		outRight = centeredCol * DEFAULT_SPACING;
	} else if (formationType == "escort") {
		// 2026-07-17 -- VIP-protection diamond: front, rear, right, left,
		// then the four corners; slots past 8 repeat the pattern on a wider
		// ring. Scales cleanly from 1 to 8+ followers.
		int ring = slotIndex / 8;
		int pos = slotIndex % 8;
		float radius = DEFAULT_SPACING * 1.6f * (ring + 1);
		float corner = radius * 0.7071f; // radius / sqrt(2)

		switch (pos) {
		case 0: outForward = radius;   outRight = 0.f;      break; // point
		case 1: outForward = -radius;  outRight = 0.f;      break; // rear
		case 2: outForward = 0.f;      outRight = radius;   break; // right flank
		case 3: outForward = 0.f;      outRight = -radius;  break; // left flank
		case 4: outForward = corner;   outRight = corner;   break; // front-right
		case 5: outForward = corner;   outRight = -corner;  break; // front-left
		case 6: outForward = -corner;  outRight = corner;   break; // rear-right
		default: outForward = -corner; outRight = -corner;  break; // rear-left
		}
	} else if (formationType == "stack") {
		// Companion System (2026-07-29, per Nick: "when they are in follow
		// mode, they stack up on each other and not be in a formation, as
		// this is to keep them close to us"). Zero offset for every slot --
		// every follower's blackboard target is the owner's own position,
		// same as plain (non-formation) follow used to look before the
		// 2026-07-17 "militant formations" pass introduced spaced-out
		// shapes. Natural pathfinding/collision jostling as several
		// followers converge on the same point is expected and is what
		// keeps them from perfectly overlapping.
		outForward = 0.f;
		outRight = 0.f;
	} else { // "box"
		// 3-wide grid trailing the owner.
		int centeredCol = col - 1;

		outForward = -DEFAULT_SPACING * (row + 1);
		outRight = centeredCol * DEFAULT_SPACING;
	}
}

void FormationManager::formUp(CreatureObject* owner, const String& formationType) {
	String type = formationType.toLowerCase();

	if (!isValidFormation(type)) {
		type = "wedge";
	}

	if (owner != nullptr) {
		Locker locker(&formationTableMutex);
		lastFormationByOwner.put(owner->getObjectID(), type);
	}

	arrangeFollowers(owner, type, true);
}

void FormationManager::applyFormationOffsets(CreatureObject* owner) {
	// Companion System (2026-07-29, per Nick: "when they are in follow
	// mode, they stack up on each other and not be in a formation... to
	// keep them close to us"). Plain /companionfollow now ALWAYS re-arms
	// a tight stack rather than re-arming whatever formation was last
	// chosen via /companionformup -- an explicit /companionformup call
	// still holds its shape for as long as the player doesn't call plain
	// Follow again. getFormationForOwner()/lastFormationByOwner are
	// intentionally left in place (formUp() still records into them) in
	// case a future "what formation am I in" style command wants them;
	// this is just no longer where that value gets read back out.
	arrangeFollowers(owner, "stack", false);
}

void FormationManager::arrangeFollowers(CreatureObject* owner, const String& formationType, bool snapTeleport) {
	if (owner == nullptr) {
		return;
	}

	Zone* zone = owner->getZone();

	if (zone == nullptr) {
		return;
	}

	ManagedReference<PlayerObject*> ghost = owner->getPlayerObject();

	if (ghost == nullptr) {
		return;
	}

	// Gather every active player-controlled entity across all systems (spec
	// 4C): Creature Handler pets + Droids share PlayerObject's active-pet
	// list (both pet types are enrolled into it identically in
	// PetControlDeviceImplementation::spawnObject), Companions are resolved
	// separately via the datapad since they are intentionally isolated from
	// that list.
	Vector<ManagedReference<AiAgent*> > followers;

	for (int i = 0; i < ghost->getActivePetsSize(); ++i) {
		ManagedReference<AiAgent*> pet = ghost->getActivePet(i);

		if (pet != nullptr && pet->getZone() != nullptr) {
			followers.add(pet);
		}
	}

	ManagedReference<SceneObject*> datapad = owner->getSlottedObject("datapad");

	if (datapad != nullptr) {
		for (int i = 0; i < datapad->getContainerObjectsSize(); ++i) {
			ManagedReference<SceneObject*> obj = datapad->getContainerObject(i);

			if (obj == nullptr || !obj->isCompanionControlDevice()) {
				continue;
			}

			CompanionControlDevice* device = cast<CompanionControlDevice*>(obj.get());
			CompanionObject* companion = device->getCompanionObject();

			if (companion != nullptr && companion->getZone() != nullptr && !device->isCompanionDead()) {
				followers.add(companion);
			}
		}
	}

	if (followers.size() == 0) {
		if (snapTeleport) {
			owner->sendSystemMessage("@companion:formup_no_followers"); // You have no active pets, droids, or companions to form up.
		}

		return;
	}

	float headingAngle = owner->getDirectionAngle();

	// Forward/right unit vectors derived from the owner's current heading.
	// NOTE: sign convention verified consistent with AiAgentImplementation::
	// setDestination()'s own "formationOffset" blackboard rotation (x*cos +
	// y*sin / -x*sin + y*cos with forward = (sin, cos)) -- the snap-teleport
	// below and the persistent per-tick blackboard placement therefore land
	// each follower on the exact same spot.
	float forwardX = std::sin(headingAngle);
	float forwardY = std::cos(headingAngle);
	float rightX = std::cos(headingAngle);
	float rightY = -std::sin(headingAngle);

	float ownerX = owner->getPositionX();
	float ownerY = owner->getPositionY();
	float ownerZ = owner->getPositionZ();

	ManagedReference<SceneObject*> parent = owner->getParent().get();
	unsigned long long parentID = (parent != nullptr) ? parent->getObjectID() : 0;

	for (int i = 0; i < followers.size(); ++i) {
		AiAgent* follower = followers.get(i);

		if (follower == nullptr || follower->isDead() || follower->isIncapacitated()) {
			continue;
		}

		float forwardOffset = 0.f;
		float rightOffset = 0.f;

		computeOffset(formationType, i, followers.size(), forwardOffset, rightOffset);

		Locker flocker(follower, owner);

		// Companion Taxi interrupt fix (2026-07-15) -- see
		// CompanionFollowCommand.h's identical guard for the full
		// explanation. Shared across pets/droids/companions (AiAgent*), so
		// the taxi-specific check has to be gated on isCompanionObject()
		// here rather than living on the generic AiAgent path.
		//
		// CRASH FIX (2026-07-20, live SIGABRT backtrace): stopTaxiRide()
		// does its own dismount cross-locks against the companion (owner/
		// vehicle vs. _this), so the companion MUST already be locked by
		// this thread before it's called -- but this block used to run
		// BEFORE the `Locker flocker(follower, owner)` below, so the
		// companion was unlocked and every Locker(x, companion) inside
		// stopTaxiRide asserted `cross->isLockedByCurrentThread()`. Moved
		// AFTER the lock.
		if (follower->isCompanionObject()) {
			CompanionObject* companionFollower = cast<CompanionObject*>(follower);

			if (companionFollower != nullptr && companionFollower->isTaxiActive()) {
				companionFollower->stopTaxiRide(false);
			}
		}

		// 2026-07-17 ("militant formations" pass) -- the persistent half:
		// AiAgentImplementation::setDestination()'s FOLLOWING branch reads
		// this blackboard Vector3 every movement tick and rotates it by the
		// follow target's LIVE heading (x = right of the owner, y = forward
		// of the owner -- see PetFormationCommand.h / LambdaShuttle
		// reinforcements for the stock precedent), so the follower HOLDS
		// this slot continuously while the owner moves instead of stacking
		// on the owner's own position.
		Vector3 blackboardOffset;
		blackboardOffset.setX(rightOffset);
		blackboardOffset.setY(forwardOffset);
		blackboardOffset.setZ(0);
		follower->writeBlackboard("formationOffset", blackboardOffset);

		if (follower->isCompanionObject()) {
			CompanionObject* companionFollower = cast<CompanionObject*>(follower);

			if (companionFollower != nullptr) {
				companionFollower->setCompanionState(CompanionObject::FOLLOW);

				// Companion System (2026-07-20, "massive battlefield" pass,
				// per user request) -- this function backs BOTH
				// /companionformup and /companionfollow's own formation-
				// refresh tail call, so it's the single place that "called
				// back with follow or form up" (the user's own words) can
				// be enforced for every companion, not just the ones a
				// per-companion command happened to touch. Re-centers the
				// standing order on the owner and drops any STAY/GUARD post
				// or escort-other assignment -- this always brings the
				// whole squad back to you, matching pre-existing behavior
				// just below (follower->setFollowObject(owner) is
				// unconditional regardless of any escortTarget).
				companionFollower->setStandingOrder(CompanionObject::FOLLOW);
				companionFollower->setEscortTarget(nullptr);
				companionFollower->setGuardTarget(nullptr);
			}
		}

		if (snapTeleport) {
			float destX = ownerX + forwardX * forwardOffset + rightX * rightOffset;
			float destY = ownerY + forwardY * forwardOffset + rightY * rightOffset;

			follower->teleport(destX, ownerZ, destY, parentID);
			follower->setNextPosition(destX, ownerZ, destY, parent.castTo<CellObject*>());
		}

		follower->setFollowObject(owner);
		follower->setMovementState(AiAgent::FOLLOWING);
	}

	if (snapTeleport) {
		owner->sendSystemMessage("@companion:formup_complete"); // Your followers have assumed formation.
	}
}
