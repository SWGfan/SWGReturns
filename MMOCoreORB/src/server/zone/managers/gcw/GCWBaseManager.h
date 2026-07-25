/*
 * GCWBaseManager.h
 * Asymmetric Base System - Imperial vs Rebel bases with different mechanics
 */

#ifndef GCWBASEMANAGER_H_
#define GCWBASEMANAGER_H_

#include "engine/engine.h"
#include "server/zone/objects/building/BuildingObject.h"
#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/managers/structure/StructureManager.h"
#include "server/zone/objects/area/ActiveArea.h"

namespace server {
namespace zone {
namespace managers {
namespace gcw {

using namespace server::zone::objects::building;
using namespace server::zone::objects::creature;
using namespace server::zone::objects::area;

class GCWBaseManager : public Object {
public:
	enum BaseType {
		BASE_IMPERIAL_PVP = 1,
		BASE_IMPERIAL_PVE = 2,
		BASE_REBEL_PVP = 3,
		BASE_REBEL_PVE = 4
	};

	enum BaseSize {
		SIZE_S01 = 1, // Forward Outpost
		SIZE_S02 = 2, // Field Hospital
		SIZE_S03 = 3, // Tactical Center
		SIZE_S04 = 4, // Detachment HQ
		SIZE_S05 = 5  // Regional HQ
	};

	enum BaseState {
		STATE_ACTIVE = 0,
		STATE_VULNERABLE = 1,
		STATE_DESTROYED = 2,
		STATE_RELOCATING = 3,
		STATE_EVACUATING = 4
	};

	enum PvEBaseType {
		PVE_IMPERIAL_RELAY = 1,    // Imperial Relay Station
		PVE_REBEL_CELL = 2         // Rebel Resistance Cell
	};

	enum PerkType {
		PERK_REINFORCEMENTS = 1,      // Imperial: Call reinforcements
		PERK_AT_AT_DEPLOY = 2,        // Imperial: Deploy AT-AT
		PERK_RELOCATION = 3,          // Rebel: Relocate base
		PERK_EVACUATION = 4,          // Rebel: Evacuate base
		PERK_SILENT_STRIKE = 5,       // Rebel: Silent strike bonus
		PERK_INTEL_GATHER = 6         // Both: Intel gathering
	};

	Singleton<GCWBaseManager> _instance;

public:
	static GCWBaseManager* instance() {
		if (_instance.get() == nullptr) {
			_instance.set(new GCWBaseManager());
		}
		return _instance.get();
	}

	GCWBaseManager() : Logger("GCWBaseManager") {}

	// Base creation and management
	bool canPlaceBase(CreatureObject* player, BaseType type, BaseSize size, const Vector3& position);
	BuildingObject* createBase(CreatureObject* player, BaseType type, BaseSize size, const Vector3& position);
	void destroyBase(BuildingObject* base);
	void setBaseState(BuildingObject* base, BaseState state);

	// Imperial specific
	bool callImperialReinforcements(BuildingObject* base, CreatureObject* caller);
	bool deployATAT(BuildingObject* base, CreatureObject* caller, const Vector3& targetLocation);
	void scheduleReinforcementWaves(BuildingObject* base);

	// Rebel specific
	bool relocateRebelBase(BuildingObject* base, CreatureObject* caller, const Vector3& newLocation);
	bool initiateEvacuation(BuildingObject* base, CreatureObject* caller);
	void handleEvacuationSuccess(BuildingObject* base);
	void handleEvacuationFailure(BuildingObject* base);

	// PvE Base Connections
	bool connectPvEBaseToPvP(BuildingObject* pveBase, BuildingObject* pvpBase);
	void disconnectPvEBase(BuildingObject* pveBase);
	BuildingObject* getConnectedPvPBase(BuildingObject* pveBase);
	BuildingObject* getConnectedPvEBase(BuildingObject* pvpBase);

	// PvE Base Infiltration
	bool infiltrateImperialRelay(BuildingObject* relay, CreatureObject* infiltrator);
	bool infiltrateRebelCell(BuildingObject* cell, CreatureObject* infiltrator);
	void handleRelayInfiltration(BuildingObject* relay, CreatureObject* infiltrator, bool silent);
	void handleCellInfiltration(BuildingObject* cell, CreatureObject* infiltrator, bool silent);

	// AT-AT Walker System
	bool canDeployATAT(BuildingObject* base) const;
	void spawnATAT(BuildingObject* base, const Vector3& deployLocation);
	void removeATAT(BuildingObject* base);

	// Base Vulnerability
	void startVulnerabilityWindow(BuildingObject* base);
	void endVulnerabilityWindow(BuildingObject* base);
	bool isBaseVulnerable(BuildingObject* base) const;
	time_t getVulnerabilityEndTime(BuildingObject* base) const;

	// Scoring and Planet Control
	void updatePlanetScores();
	void awardPlanetControlBonus();
	float getPlanetControlPercentage(int faction) const;

	// Configuration
	void loadConfiguration();
};

}
}
}
}

#endif /* GCWBASEMANAGER_H_ */