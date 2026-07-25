/*
 * ATATWalkerManager.h
 * AT-AT Walker Deployment System for GCW
 */

#ifndef ATATWALKERMANAGER_H_
#define ATATWALKERMANAGER_H_

#include "engine/engine.h"
#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/creature/ai/AiAgent.h"
#include "server/zone/objects/building/BuildingObject.h"

namespace server {
namespace zone {
namespace managers {
namespace gcw {

using namespace server::zone::objects::creature;
using namespace server::zone::objects::building;

class ATATWalkerManager : public Object {
public:
	enum WalkerState {
		STATE_DEPLOYING = 0,
		STATE_ADVANCING = 1,
		STATE_ASSAULTING = 2,
		STATE_RETREATING = 3,
		STATE_DESTROYED = 4
	};

	enum WalkerType {
		TYPE_AT_AT = 1,
		TYPE_AT_ST = 2,
		TYPE_AT_PT = 3
	};

	struct WalkerDeployment {
		uint64 walkerID;
		uint64 targetBaseID;
		uint64 deployerID;
		WalkerType type;
		Vector3 deployLocation;
		WalkerState state;
		Time deployTime;
		Time expirationTime;
	};

	Singleton<ATATWalkerManager> _instance;

public:
	static ATATWalkerManager* instance() {
		if (_instance.get() == nullptr) {
			_instance.set(new ATATWalkerManager());
		}
		return _instance.get();
	}

	ATATWalkerManager() : Logger("ATATWalkerManager") {}

	// Deployment
	bool deployATAT(CreatureObject* deployer, BuildingObject* targetBase, const Vector3& location);
	bool canDeployATAT(CreatureObject* deployer, BuildingObject* targetBase) const;
	CreatureObject* spawnATAT(const Vector3& location, WalkerType type);

	// Walker Control
	void updateWalker(CreatureObject* walker);
	void setWalkerState(CreatureObject* walker, WalkerState state);
	WalkerState getWalkerState(CreatureObject* walker) const;

	// Targeting
	void setWalkerTarget(CreatureObject* walker, uint64 targetBaseID);
	uint64 getWalkerTarget(CreatureObject* walker) const;

	// Combat
	void walkerAttackBase(CreatureObject* walker, BuildingObject* base);
	bool canWalkerBeStopped(CreatureObject* walker, CreatureObject* stopper) const;
	void handleWalkerStopAttempt(CreatureObject* walker, CreatureObject* stopper);

	// Expiration
	void checkWalkerExpiration();
	void expireWalker(CreatureObject* walker);

	// AT-AT Abilities
	void fireMainCannons(CreatureObject* walker);
	void fireConcussionMissiles(CreatureObject* walker);
	void spawnATATTroops(CreatureObject* walker);

	// Defense
	void walkerTakeDamage(CreatureObject* walker, CreatureObject* attacker, int damage);

	// Configuration
	void loadConfiguration();
};

}
}
}
}

#endif /* ATATWALKERMANAGER_H_ */