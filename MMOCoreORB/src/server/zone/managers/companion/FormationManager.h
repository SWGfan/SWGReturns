/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System -- spec 4C ("Multi-Entity Coordination Mechanics / Form
	Up Formations"). Aggregates every player-controlled follower across all
	three systems (Creature Handler pets + Droids via PlayerObject's active
	pet list, and Companions via a datapad scan) and repositions them into a
	formation relative to the owner's heading.

	2026-07-17 ("militant formations" pass, per user request) -- formations
	are now PERSISTENT, not a one-shot teleport: formUp() additionally writes
	each follower's slot offset into the engine's own existing
	"formationOffset" AI blackboard hook (AiAgentImplementation::
	setDestination()'s FOLLOWING branch rotates that Vector3 by the follow
	target's live heading every movement tick -- the exact mechanism the
	stock game's own herd/squad/GCW-reinforcement NPCs use to march in
	formation). Followers therefore HOLD their slots continuously while the
	owner moves, turn when the owner turns, and never collapse into a stack
	on the owner's position the way plain setFollowObject() follow does.

	Also new in the same pass: three additional formation shapes (column,
	vanguard, escort) beyond the original line/wedge/box, and an in-memory
	per-owner "last chosen formation" so plain /companionfollow can re-arm
	the same formation instead of resetting everyone to a stacked follow.
*/

#ifndef FORMATIONMANAGER_H_
#define FORMATIONMANAGER_H_

#include "engine/engine.h"

namespace server {
namespace zone {
namespace objects {
namespace creature {
	class CreatureObject;
}
}
}
}

using namespace server::zone::objects::creature;

namespace server {
namespace zone {
namespace managers {
namespace companion {

class FormationManager : public Singleton<FormationManager>, public Logger, public Object {

public:
	FormationManager();

	/**
	 * Spec 4C entry point. formationType is one of "line", "wedge", "box",
	 * "column", "vanguard", "escort". Gathers every active follower
	 * (Creature Handler pets, Droids, Companions) belonging to owner,
	 * snap-teleports each into its slot, and writes the persistent
	 * per-follower "formationOffset" blackboard entry so the slot is HELD
	 * while the owner moves (see file header). Remembers formationType as
	 * the owner's active formation.
	 */
	void formUp(CreatureObject* owner, const String& formationType);

	/**
	 * Re-arms the owner's last-chosen formation's blackboard offsets on
	 * every follower WITHOUT the snap-teleport -- called by
	 * /companionfollow so a plain "follow me" order keeps the militant
	 * spacing instead of collapsing everyone back onto the owner's
	 * position. Defaults to "wedge" if the owner never picked one.
	 */
	void applyFormationOffsets(CreatureObject* owner);

	/** True if formationType names a known formation shape. */
	bool isValidFormation(const String& formationType) const;

	/** The owner's last-chosen formation ("wedge" if never chosen). */
	String getFormationForOwner(CreatureObject* owner);

	/** All valid formation names, for SUI pickers / error messages. */
	static const int FORMATION_COUNT = 7;
	static const char* const FORMATION_NAMES[FORMATION_COUNT];

private:
	/** Fallback per-slot spacing used when a follower's bounding radius can't
	 * be resolved. See NOTES.md, "Formation spacing precision". */
	static const int DEFAULT_SPACING = 3;

	/** Owner objectID -> last formation chosen via formUp(). In-memory only
	 * (resets to the "wedge" default on server restart, deliberately --
	 * matches how pet formations are transient in the stock game too). */
	HashTable<uint64, String> lastFormationByOwner;
	Mutex formationTableMutex;

	void computeOffset(const String& formationType, int slotIndex, int totalFollowers, float& outForward, float& outRight) const;

	/** Shared core of formUp()/applyFormationOffsets(). */
	void arrangeFollowers(CreatureObject* owner, const String& formationType, bool snapTeleport);
};

}
}
}
}

using namespace server::zone::managers::companion;

#endif // FORMATIONMANAGER_H_
