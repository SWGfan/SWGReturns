/*
 * GCWManager.h
 * Main GCW Manager integrating all subsystems
 */

#ifndef GCWMANAGER_H_
#define GCWMANAGER_H_

#include "engine/engine.h"
#include "server/zone/managers/gcw/GCWStealthManager.h"
#include "server/zone/managers/gcw/GCWFactionGuildManager.h"
#include "server/zone/managers/gcw/GCWBaseManager.h"
#include "server/zone/managers/gcw/ATATWalkerManager.h"

namespace server {
namespace zone {
namespace managers {
namespace gcw {

class GCWManager : public Object {
	Singleton<GCWManager> _instance;

public:
	static GCWManager* instance() {
		if (_instance.get() == nullptr) {
			_instance.set(new GCWManager());
		}
		return _instance.get();
	}

	GCWManager() : Logger("GCWManager") {}

	void initialize() {
		info("Initializing GCW Manager...");
		
		// Initialize all subsystems
		GCWStealthManager::instance()->loadConfiguration();
		GCWFactionGuildManager::instance()->loadConfiguration();
		GCWBaseManager::instance()->loadConfiguration();
		ATATWalkerManager::instance()->loadConfiguration();
		
		info("GCW Manager initialized successfully");
	}

	// Subsystem accessors
	GCWStealthManager* getStealthManager() { return GCWStealthManager::instance(); }
	GCWFactionGuildManager* getFactionGuildManager() { return GCWFactionGuildManager::instance(); }
	GCWBaseManager* getBaseManager() { return GCWBaseManager::instance(); }
	ATATWalkerManager* getATATWalkerManager() { return ATATWalkerManager::instance(); }
};

}
}
}
}

#endif /* GCWMANAGER_H_ */