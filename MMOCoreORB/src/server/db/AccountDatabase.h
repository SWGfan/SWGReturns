/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.*/

#ifndef ACCOUNTDATABASE_H_
#define ACCOUNTDATABASE_H_

#include "engine/engine.h"

namespace conf {
	class ConfigManager;
}

// Connection to the shared account/session/character/galaxy database.
// Defaults to the same target as ServerDatabase when Core3.AccountDB* isn't
// explicitly configured, so single-server deployments are unaffected.
class AccountDatabase : public Logger {
	static Vector<Database*>* databases;
	static AtomicInteger currentDB;

public:
	AccountDatabase(conf::ConfigManager* configManager);
	~AccountDatabase();

	inline static Database* instance() {
		if (databases == nullptr)
			throw DatabaseException("No Account Database initiated");

		int i = currentDB.postIncrement() % databases->size();

		return databases->get(i);
	}
};

#endif /*ACCOUNTDATABASE_H_*/
