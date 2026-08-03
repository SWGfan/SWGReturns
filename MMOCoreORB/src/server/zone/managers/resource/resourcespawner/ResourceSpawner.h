/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.*/

/**
 * \file ResourceSpawner.h
 * \author Kyle Burkhardt
 * \date 5-03-10
 */

#ifndef RESOURCESPAWNER_H_
#define RESOURCESPAWNER_H_

#include "server/zone/ZoneServer.h"
#include "server/zone/ZoneProcessServer.h"
#include "server/zone/managers/name/NameManager.h"
#include "server/zone/managers/object/ObjectManager.h"
#include "server/zone/objects/transaction/TransactionLog.h"
#include "resourcetree/ResourceTree.h"
#include "resourcemap/ResourceMap.h"

#include "resourcepool/MinimumPool.h"
#include "resourcepool/FixedPool.h"
#include "resourcepool/RandomPool.h"
#include "resourcepool/NativePool.h"
#include "resourcepool/ManualPool.h"

namespace server {
namespace zone {
namespace objects {
namespace player {
namespace sui {
namespace listbox {
	class SuiListBox;
}
}
}
}
}
}

using namespace server::zone::objects::player::sui::listbox;

/**
 * The ResourceSpawner class represents all the functions related to ResourceSpawns
 * Including spawning, despawning rules,
 */
class ResourceSpawner : public Logger, public Object {
private:
	ManagedReference<ZoneServer* > server;
	ManagedReference<ZoneProcessServer*> processor;

	NameManager* nameManager;
	ObjectManager* objectManager;

	ResourceTree* resourceTree;
	ObjectDatabaseManager* databaseManager;

	ResourceMap* resourceMap;

	Vector<String> jtlResources;
	Vector<String> activeResourceZones;
	Vector<String>* planets;

	MinimumPool* minimumPool;
	FixedPool* fixedPool;
	RandomPool* randomPool;
	NativePool* nativePool;
	ManualPool* manualPool;

	bool scriptLoading;

	int shiftDuration, lowerGateOverride, maxSpawnAmount, spawnThrottling;

	int samplingMultiplier;

public:
	ResourceSpawner(ManagedReference<ZoneServer* > serv,
			ZoneProcessServer* impl);
	~ResourceSpawner();

	void init();

	void initializeMinimumPool(LuaObject includes, const String& excludes);
	void initializeFixedPool(LuaObject includes, const String& excludes);
	void initializeRandomPool(LuaObject includes, const String& excludes, const int size);
	void initializeNativePool(const String& includes, const String& excludes);

	void addZone(const String& zoneName);
	void removeZone(const String& zoneName);
	void addJtlResource(const String& resourceName);
	void setSpawningParameters(bool loadFromScript, const int dur, const int throt,
			const int override, const int spawnquantity);

	void spawnScriptResources();
	bool writeAllSpawnsToScript();
	bool ghDumpAll();

	void start();
	void shiftResources();

	ResourceSpawn* createResourceSpawn(const String& type, const String& zonerestriction = "");
	ResourceSpawn* createResourceSpawn(const String& type, const Vector<String>& excludes, const String& zonerestriction = "");
	ResourceSpawn* createResourceSpawn(const Vector<String>& includes, const Vector<String>& excludes = 0, const String& zonerestriction = "");

	void despawn(ResourceSpawn* spawn);

	ResourceSpawn* manualCreateResourceSpawn(CreatureObject* player, const UnicodeString& args);

	ResourceSpawn* createRecycledResourceSpawn(const ResourceTreeEntry* entry) const;

	ResourceSpawn* getRecycledVersion(const ResourceSpawn* resource) const;

	bool isRecycledResource(const ResourceSpawn* resource) const;

	int sendResourceRecycleType(const ResourceSpawn* resource) const;

	void sendResourceListForSurvey(CreatureObject* player, const int toolType, const String& surveyType) const;

	void sendSurvey(CreatureObject* player, const String& resname) const;
	void sendSample(CreatureObject* player, const String& resname, const String& sampleAnimation) const;
	void sendSampleResults(TransactionLog& trx, CreatureObject* player, const float density, const String& resname) const;

	/** Master Survey Tool (2026-07-29, Companion -- see NOTES.md "Master
	 * Survey Tool"): every currently in-shift resource name on the given
	 * zone, with NO per-survey-tool-type filter (unlike
	 * sendResourceListForSurvey(), which only lists resources matching one
	 * SurveyTool::type). Copy-adapted from that method's own zoneMap loop. */
	void getActiveResourceNames(const String& zoneName, Vector<String>& names) const;

	/** Master Survey Tool (2026-07-29, Companion -- see NOTES.md "Master
	 * Survey Tool"): scans a gridPoints x gridPoints grid centered on
	 * (centerX, centerY) spanning "range" meters, exactly like
	 * sendSurvey()'s own grid loop, but returns EVERY sampled point's
	 * (x, y, density) via three parallel out-vectors instead of keeping
	 * only the single highest-density point. Caller picks the top N. */
	void scanForHotspots(const String& resname, const String& zoneName, float centerX, float centerY, int range, int gridPoints, Vector<float>& outX, Vector<float>& outY, Vector<float>& outDensity) const;

	Reference<ResourceContainer*> harvestResource(CreatureObject* player, const String& type, const int quantity);
	bool harvestResource(TransactionLog& trx, CreatureObject* player, ResourceSpawn* resourceSpawn, int quantity);
	bool addResourceToPlayerInventory(TransactionLog& trx, CreatureObject* player, ResourceSpawn* resourceSpawn, int unitsExtracted) const;

	ResourceSpawn* getCurrentSpawn(const String& restype, const String& zoneName) const;

	/** Companion System (2026-07-20, resource-deed quality pass -- see
	 * NOTES.md): like getCurrentSpawn(), but scans EVERY matching in-spawn
	 * resource on the zone and returns the highest-quality one (scored by
	 * overall quality, tie-broken by summed attributes) instead of the
	 * first match. Matching accepts either the class-token test
	 * (ResourceSpawn::isType()) or the legacy type-substring test. */
	ResourceSpawn* getBestSpawnOfType(const String& restype, const String& zoneName) const;

	/** Companion System (2026-07-20, "what makes the best resource for THIS
	 * item" -- see NOTES.md): like getBestSpawnOfType() (whole historical
	 * map, any planet), but scored by ONE of the schematic's own
	 * experimental-property weight lines (e.g. a harvester's Extraction
	 * Rate) -- the exact math the crafting system uses. lineIndex < 0
	 * falls back to the generic all-attributes scoring. */
	ResourceSpawn* getBestSpawnOfTypeWeighted(const String& restype, DraftSchematic* schematic, int lineIndex) const;

	ResourceSpawn* getFromRandomPool(const String& type);

	void addNodeToListBox(SuiListBox* sui, const String& nodeName) const;
	void addPlanetsToListBox(SuiListBox* sui) const;

	String addParentNodeToListBox(SuiListBox* sui, const String& currentNode) const;

	inline ResourceMap* getResourceMap() {
		return resourceMap;
	}

	inline const ResourceMap* getResourceMap() const {
		return resourceMap;
	}

	void listResourcesForPlanetOnScreen(CreatureObject* creature, const String& planet) const;

	String healthCheck();

	String dumpResources() {
		if(writeAllSpawnsToScript())
			return "Resources Dumped";

		return "Error Dumping resources";
	}
	String ghDump() {
		if(ghDumpAll())
			return "Galaxy Harvester Output Dumped";
		return "Error Dumping Galaxy Harvester Output";
	}
	String getPlanetByIndex(int index) const;

	/** Companion System (2026-07-28, wrong-slot resource steal fix): depth of a
	 * resource class string in the resource tree (1 = top level like Organic/
	 * Inorganic; bigger = more specific; -1 = unknown class). Used to fill the
	 * most specific ingredient slots first. */
	int getResourceClassDepth(const String& type) const {
		if (resourceTree == nullptr)
			return -1;

		const ResourceTreeEntry* entry = resourceTree->getEntry(type);

		if (entry == nullptr)
			return -1;

		for (int i = 0; i < entry->getStfClassCount(); ++i)
			if (entry->getStfClass(i) == type)
				return i + 1;

		for (int i = 0; i < entry->getClassCount(); ++i)
			if (entry->getClass(i) == type)
				return i + 1;

		return entry->getClassCount(); // exact spawnable leaf type: deepest
	}

private:

	void loadResourceSpawns();
	String makeResourceName(const String& randomNameClass);
	int randomizeValue(int min, int max);
	long getRandomExpirationTime(const ResourceTreeEntry* resourceEntry);
	long getRandomUnixTimestamp(int min, int max) const;

	const Vector<String>& getJtlResources() const;
	const Vector<String>& getActiveResourceZones() const;

	friend class ResourceTree;
	friend class ResourceManager;
	friend class NativePool;
};

#endif /* RESOURCESPAWNER_H_ */
