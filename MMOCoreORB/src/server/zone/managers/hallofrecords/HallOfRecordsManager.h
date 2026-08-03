/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.*/

#ifndef HALLOFRECORDSMANAGER_H_
#define HALLOFRECORDSMANAGER_H_

#include "engine/engine.h"

namespace server {
namespace zone {
namespace managers {
namespace hallofrecords {

// Hall of Records (2026-07-16): backs the public, no-click auto-cycling
// "scrolling screen" plaque. Holds just the current leaderboard-topper
// name+count for each category -- deliberately NOT a live query (Berkeley
// DB, the primary player-data store, has no secondary-index/max-of-field
// query of any kind, and nothing else in this engine scans the whole
// offline player population). Instead this is updated incrementally at the
// exact two call sites that already increment the underlying counters
// (CreatureManagerImplementation's KILLEDCREATURE handling, and
// PlayerManagerImplementation's pvpRating update), comparing the new count
// against whatever's currently stored and overwriting only if it's higher --
// the same pattern this engine already uses for pvpRating itself.
//
// Persisted to a small flat file (not the object database) since this is
// two scalars with no relationship to player-object transactional
// integrity -- simplest reliable option, avoids inventing new
// ObjectManager-backed ManagedObject machinery for two numbers.
class HallOfRecordsManager : public Singleton<HallOfRecordsManager>, public Object, public Logger {
	String topCreatureKillerName;
	int topCreatureKillerCount;

	String topPvpKillerName;
	int topPvpKillerCount;

	Mutex hallOfRecordsMutex;

	static const char* DATA_FILE;

public:
	HallOfRecordsManager();

	void loadData();
	void saveData();

	void reportCreatureKill(const String& playerName, int newCount);
	void reportPvpKill(const String& playerName, int newCount);

	// Pre-formatted lines ready to hand straight to setCustomObjectName().
	String getCreatureKillLine();
	String getPvpKillLine();

	friend class Singleton<HallOfRecordsManager>;
};

} // namespace hallofrecords
} // namespace managers
} // namespace zone
} // namespace server

using namespace server::zone::managers::hallofrecords;

#endif // HALLOFRECORDSMANAGER_H_
