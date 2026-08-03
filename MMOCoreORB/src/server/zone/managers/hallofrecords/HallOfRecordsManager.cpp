/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.*/

#include "HallOfRecordsManager.h"
#include <fstream>
#include <cstdlib>

const char* HallOfRecordsManager::DATA_FILE = "hall_of_records.dat";

HallOfRecordsManager::HallOfRecordsManager() {
	setLoggingName("HallOfRecordsManager");
	setLogLevel(Logger::INFO);

	topCreatureKillerName = "";
	topCreatureKillerCount = 0;

	topPvpKillerName = "";
	topPvpKillerCount = 0;

	loadData();
}

// Small line-based format, one category per line: CATEGORY|name|count.
// Read once at server boot (constructor) so a restart doesn't reset the
// leaderboard back to empty.
void HallOfRecordsManager::loadData() {
	Locker locker(&hallOfRecordsMutex);

	std::ifstream file(DATA_FILE);

	if (!file.is_open()) {
		info("No existing hall_of_records.dat found -- starting with an empty leaderboard.", true);
		return;
	}

	std::string line;

	while (std::getline(file, line)) {
		if (line.length() == 0)
			continue;

		size_t firstPipe = line.find('|');

		if (firstPipe == std::string::npos)
			continue;

		size_t secondPipe = line.find('|', firstPipe + 1);

		if (secondPipe == std::string::npos)
			continue;

		std::string category = line.substr(0, firstPipe);
		std::string name = line.substr(firstPipe + 1, secondPipe - firstPipe - 1);
		std::string countStr = line.substr(secondPipe + 1);

		int count = atoi(countStr.c_str());

		if (category == "CREATURE") {
			topCreatureKillerName = String(name.c_str());
			topCreatureKillerCount = count;
		} else if (category == "PVP") {
			topPvpKillerName = String(name.c_str());
			topPvpKillerCount = count;
		}
	}

	file.close();

	info("Loaded Hall of Records: top creature killer = " + topCreatureKillerName + " (" + String::valueOf(topCreatureKillerCount)
		+ "), top pvp killer = " + topPvpKillerName + " (" + String::valueOf(topPvpKillerCount) + ")", true);
}

// Caller must already hold hallOfRecordsMutex.
void HallOfRecordsManager::saveData() {
	std::ofstream file(DATA_FILE, std::ios::trunc);

	if (!file.is_open()) {
		error("Could not open hall_of_records.dat for writing -- Hall of Records data will not persist across restarts until this is fixed.");
		return;
	}

	file << "CREATURE|" << topCreatureKillerName.toCharArray() << "|" << topCreatureKillerCount << "\n";
	file << "PVP|" << topPvpKillerName.toCharArray() << "|" << topPvpKillerCount << "\n";

	file.close();
}

void HallOfRecordsManager::reportCreatureKill(const String& playerName, int newCount) {
	Locker locker(&hallOfRecordsMutex);

	if (newCount <= topCreatureKillerCount)
		return;

	topCreatureKillerName = playerName;
	topCreatureKillerCount = newCount;

	saveData();
}

void HallOfRecordsManager::reportPvpKill(const String& playerName, int newCount) {
	Locker locker(&hallOfRecordsMutex);

	if (newCount <= topPvpKillerCount)
		return;

	topPvpKillerName = playerName;
	topPvpKillerCount = newCount;

	saveData();
}

String HallOfRecordsManager::getCreatureKillLine() {
	Locker locker(&hallOfRecordsMutex);

	if (topCreatureKillerCount == 0)
		return "Most Creature Kills: (no record yet)";

	return "Most Creature Kills: " + topCreatureKillerName + " (" + String::valueOf(topCreatureKillerCount) + ")";
}

String HallOfRecordsManager::getPvpKillLine() {
	Locker locker(&hallOfRecordsMutex);

	if (topPvpKillerCount == 0)
		return "Most PvP Kills: (no record yet)";

	return "Most PvP Kills: " + topPvpKillerName + " (" + String::valueOf(topPvpKillerCount) + ")";
}
