/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.*/

#include "AccountDatabase.h"

#include "conf/ConfigManager.h"
#include "MySqlDatabase.h"

Vector<Database*>* AccountDatabase::databases = nullptr;
AtomicInteger AccountDatabase::currentDB;

AccountDatabase::AccountDatabase(ConfigManager* configManager) {
	const String& dbHost = configManager->getAccountDBHost();
	const String& dbUser = configManager->getAccountDBUser();
	const String& dbPass = configManager->getAccountDBPass();
	const String& dbName = configManager->getAccountDBName();
	const int     dbPort = configManager->getAccountDBPort();

	setLoggingName("AccountDatabase " + dbHost + ":" + String::valueOf(dbPort));

	databases = new Vector<Database*>();

	const static int DEFAULT_ACCOUNTDATABASE_INSTANCES = configManager->getInt("Core3.DBInstances", 1);

	for (int i = 0; i < DEFAULT_ACCOUNTDATABASE_INSTANCES; ++i) {
		Database* db = new server::db::mysql::MySqlDatabase(String("AccountMySqlDatabase" + String::valueOf(i)), dbHost);
		db->connect(dbName, dbUser, dbPass, dbPort);

		databases->add(db);
	}

	info(true) << "AccountDatabase connected to " << dbHost << ":" << dbPort << "/" << dbName;
}

AccountDatabase::~AccountDatabase() {
	for (auto db : *databases) {
		delete db;
	}

	delete databases;
	databases = nullptr;
}
