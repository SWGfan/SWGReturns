/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.*/

#ifndef HALLOFRECORDSDISPLAYTASK_H_
#define HALLOFRECORDSDISPLAYTASK_H_

#include "server/zone/objects/tangible/TangibleObject.h"
#include "server/zone/managers/hallofrecords/HallOfRecordsManager.h"

namespace server {
namespace zone {
namespace objects {
namespace tangible {

// Hall of Records (2026-07-16): drives the public "scrolling screen" plaque.
// No click/menu component at all -- per user request, the plaque cycles on
// its own, forever, via the same self-rescheduling addPendingTask() pattern
// already used throughout this engine (e.g. DroidMerchantBarkerTask's 60s
// re-arm loop). setCustomObjectName() is a plain field-overwrite-plus-one-
// delta-packet call with no accumulating state, so re-arming this every few
// seconds for a permanent world object is well within what this engine
// already does elsewhere (HAM regen ticks every 1 second is a more
// aggressive existing precedent).
//
// Honest limitation, not fixable from server code: what players see is the
// text instantly swapping to the next line, not a smooth scrolling motion --
// there's no client source in this repo to add real scroll-animation to.
class HallOfRecordsDisplayTask : public Task {
	Reference<TangibleObject*> plaque;
	int cycleIndex;

	static const int CYCLE_INTERVAL_MS = 7000; // 7s per line

public:
	HallOfRecordsDisplayTask(TangibleObject* plaque) : Task() {
		this->plaque = plaque;
		this->cycleIndex = 0;
	}

	void run() {
		if (plaque == nullptr)
			return;

		Locker locker(plaque);

		if (plaque->getZoneUnsafe() == nullptr) {
			// Despawned/zone unloaded -- stop rescheduling rather than
			// leaking a forever-repeating task against a dead object.
			return;
		}

		String nextLine;

		if (cycleIndex % 2 == 0)
			nextLine = HallOfRecordsManager::instance()->getCreatureKillLine();
		else
			nextLine = HallOfRecordsManager::instance()->getPvpKillLine();

		cycleIndex++;

		plaque->setCustomObjectName(UnicodeString(nextLine), true);

		plaque->addPendingTask("hallOfRecordsCycle", this, CYCLE_INTERVAL_MS);
	}
};

} // namespace tangible
} // namespace objects
} // namespace zone
} // namespace server

using namespace server::zone::objects::tangible;

#endif // HALLOFRECORDSDISPLAYTASK_H_
