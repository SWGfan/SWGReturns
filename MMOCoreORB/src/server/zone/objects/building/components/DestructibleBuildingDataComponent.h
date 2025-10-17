/*
 * DestructibleBuildingDataComponent.h
 *
 *  Created on: Oct 22, 2012
 *      Author: pete
 */

#ifndef DESTRUCTIBLEBUILDINGDATACOMPONENT_H_
#define DESTRUCTIBLEBUILDINGDATACOMPONENT_H_

#include "engine/engine.h"
#include "server/zone/objects/building/components/BuildingDataComponent.h"
#include "system/util/Vector.h"
#include "server/zone/objects/scene/SceneObject.h"

class DestructibleBuildingDataComponent : public BuildingDataComponent, public Logger {
private:
	Vector<String> dnaStrand;
	Vector<int> dnaLocks;
	String currentDnaChain;

	Vector<int> powerSwitchRules;
	Vector<bool> powerSwitchStates;

	int intCurrentState;       // serialized
	bool terminalDamaged;      // serialized
	bool inRepair;             // serialized

	Vector<uint64> turretSlots;
	Vector<uint64> minefieldSlots;
	Vector<uint64> scannerSlots;

	Time lastVulnerableTime;    // serialized
	Time nextVulnerableTime;    // serialized
	Time vulnerabilityEndTime;  // serialized
	Time placementTime;         // serialized
	Time lastResetTime;         // serialized
	int uplinkBand;             // secret code used to jam the uplink
	bool activeDefenses;
	bool defenseAddedThisVuln;
	bool terminalsSpawned;
	Vector<ManagedReference<SceneObject*> > baseTerminals;
	Vector<uint64> hackBaseAlarms;
	Vector<uint64> destructBaseAlarms;

public:
	enum State {
		VULNERABLE = 1,
		BAND = 2,
		JAMMED = 3,
		SLICED = 4,
		DNA = 5,
		OVERLOADED = 6,
		SHUTDOWNSEQUENCE = 7,
		REBOOTSEQUENCE = 8
	};

	DestructibleBuildingDataComponent() 
		: intCurrentState(0)  // start as invulnerable
		, terminalDamaged(false)
		, inRepair(false)
		, uplinkBand(0)
		, activeDefenses(true)
		, defenseAddedThisVuln(false)
		, terminalsSpawned(false)
	{
		setLoggingName("DESTOBJ");
		currentDnaChain.clear();
	}

	virtual ~DestructibleBuildingDataComponent() = default;

	void writeJSON(nlohmann::json& j) const;

	bool toBinaryStream(ObjectOutputStream* stream);
	bool parseFromBinaryStream(ObjectInputStream* stream);

	bool isVulnerable() const {
		return intCurrentState >= VULNERABLE;
	}

	bool isDestructibleBuildingData() const {
		return true;
	}

	int getState() const {
		return intCurrentState;
	}

	Time getLastVulnerableTime() const { return lastVulnerableTime; }
	Time getNextVulnerableTime() const { return nextVulnerableTime; }
	Time getVulnerabilityEndTime() const { return vulnerabilityEndTime; }
	Time getPlacementTime() const { return placementTime; }
	Time getLastResetTime() const { return lastResetTime; }
	int getUplinkBand() const { return uplinkBand; }
	bool isTerminalBeingRepaired() const { return inRepair; }
	bool isTerminalDamaged() const { return terminalDamaged; }

	void setState(int state) { intCurrentState = state; }
	void setLastVulnerableTime(const Time& time) { lastVulnerableTime = time; }
	void setNextVulnerableTime(const Time& time) { nextVulnerableTime = time; }
	void setVulnerabilityEndTime(const Time& time) { vulnerabilityEndTime = time; }
	void setPlacementTime(const Time& time) { placementTime = time; }
	void setLastResetTime(const Time& time) { lastResetTime = time; }
	void setUplinkBand(int band) { uplinkBand = band; }
	void setTerminalBeingRepaired(bool val) { inRepair = val; }
	void setTerminalDamaged(bool val) { terminalDamaged = val; }

	void setActiveTurret(int indx, uint64 turretOID) { turretSlots.get(indx) = turretOID; }
	void setActiveMinefield(int indx, uint64 minefieldOID) { minefieldSlots.get(indx) = minefieldOID; }
	void setTurretID(int indx, uint64 turretOID) { turretSlots.elementAt(indx) = turretOID; }
	void setMinefieldID(int indx, uint64 minefieldOID) { minefieldSlots.elementAt(indx) = minefieldOID; }
	void setScannerID(int indx, uint64 scannerOID) { scannerSlots.elementAt(indx) = scannerOID; }

	void addTurret(int indx, uint64 turretOID) { turretSlots.add(indx, turretOID); }
	void addMinefield(int indx, uint64 minefieldOID) { minefieldSlots.add(indx, minefieldOID); }
	void addScanner(int indx, uint64 scannerOID) { scannerSlots.add(indx, scannerOID); }

	int getTotalTurretCount() const { return turretSlots.size(); }
	int getTotalMinefieldCount() const { return minefieldSlots.size(); }
	int getTotalScannerCount() const { return scannerSlots.size(); }

	bool isTurretSlotOccupied(int indx) const { return turretSlots.get(indx) > 0; }
	bool isMinefieldSlotOccupied(int indx) const { return minefieldSlots.get(indx) > 0; }
	bool isScannerSlotOccupied(int idx) const { return scannerSlots.get(idx) > 0; }

	uint64 getTurretID(int indx) const { return turretSlots.elementAt(indx); }
	uint64 getMinefieldID(int indx) const { return minefieldSlots.elementAt(indx); }
	uint64 getScannerID(int indx) const { return scannerSlots.elementAt(indx); }

	bool hasTurret(uint64 turretID) const { return turretSlots.contains(turretID); }
	bool hasMinefield(uint64 minefieldOID) const { return minefieldSlots.contains(minefieldOID); }
	bool hasScanner(uint64 scannerOID) const { return scannerSlots.contains(scannerOID); }
	bool hasDefense(uint64 defenseOID) const { return hasTurret(defenseOID) || hasMinefield(defenseOID) || hasScanner(defenseOID); }

	int getIndexOfTurret(uint64 turretID) const {
		for (int i = 0; i < turretSlots.size(); ++i)
			if (turretSlots.elementAt(i) == turretID)
				return i;
		return -1;
	}

	int getIndexOfMinefield(uint64 minefieldOID) const {
		for (int i = 0; i < minefieldSlots.size(); ++i)
			if (minefieldSlots.elementAt(i) == minefieldOID)
				return i;
		return -1;
	}

	int getIndexOfScanner(uint64 scannerOID) const {
		for (int i = 0; i < scannerSlots.size(); ++i)
			if (scannerSlots.elementAt(i) == scannerOID)
				return i;
		return -1;
	}

	void initializeTransientMembers();

	bool isGCWBaseData() const { return true; }
	bool hasDefense() const { return activeDefenses; }
	void setDefense(bool value) { activeDefenses = value; }
	bool wasDefenseAddedThisVuln() const { return defenseAddedThisVuln; }
	void setDefenseAddedThisVuln(bool added) { defenseAddedThisVuln = added; }

	void clearDnaStrand() { dnaStrand.removeAll(); }
	void setDnaStrand(const Vector<String>& strand) { dnaStrand = strand; }
	const Vector<String>& getDnaStrand() const { return dnaStrand; }

	void clearDnaLocks() { dnaLocks.removeAll(); }
	void setDnaLocks(const Vector<int>& locks) { dnaLocks = locks; }
	const Vector<int>& getDnaLocks() const { return dnaLocks; }
	Vector<int>& getDnaLocks() { return dnaLocks; }

	const String& getCurrentDnaChain() const { return currentDnaChain; }
	void setCurrentDnaChain(const String& chain) { currentDnaChain = chain; }

	const Vector<int>& getPowerSwitchRules() const { return powerSwitchRules; }
	void setPowerSwitchRules(const Vector<int>& rules) { powerSwitchRules = rules; }

	const Vector<bool>& getPowerSwitchStates() const { return powerSwitchStates; }
	void setPowerSwitchStates(const Vector<bool>& states) { powerSwitchStates = states; }
	bool getPowerPosition(int indx) const { return powerSwitchStates.get(indx); }

	int getBaseTerminalCount() const { return baseTerminals.size(); }
	SceneObject* getBaseTerminal(int idx) const { return baseTerminals.get(idx); }
	void addBaseTerminal(SceneObject* term) { baseTerminals.add(term); }
	void clearBaseTerminals() { baseTerminals.removeAll(); }
	bool areTerminalsSpawned() const { return terminalsSpawned; }
	void setTerminalsSpawned(bool val) { terminalsSpawned = val; }

	const Vector<uint64>& getHackAlarms() const { return hackBaseAlarms; }
	const Vector<uint64>& getDestructAlarms() const { return destructBaseAlarms; }
	void addHackBaseAlarm(uint64 alarmID) { hackBaseAlarms.add(alarmID); }
	void addDestructBaseAlarm(uint64 alarmID) { destructBaseAlarms.add(alarmID); }

private:
	int writeObjectMembers(ObjectOutputStream* stream);
	bool readObjectMember(ObjectInputStream* stream, const String& name);
};

#endif /* DESTRUCTIBLEBUILDINGDATACOMPONENT_H_ */
