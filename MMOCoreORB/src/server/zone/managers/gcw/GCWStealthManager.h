/*
 * GCWStealthManager.h
 * Implements the stealth/detection system for GCW gameplay
 * Based on the 2005 GCW redesign document
 */

#ifndef GCWSTEALTHMANAGER_H_
#define GCWSTEALTHMANAGER_H_

#include "engine/engine.h"
#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/managers/player/PlayerManager.h"

namespace server {
namespace zone {
namespace managers {
namespace gcw {

using namespace server::zone::objects::creature;
using namespace server::zone::objects::player;

class GCWStealthManager : public Object {
	// Detection levels
	enum VisibilityLevel {
		VISIBILITY_STANDING = 3,   // V-3: Very visible
		VISIBILITY_KNEELING = 2,   // V-2: Less visible
		VISIBILITY_PRONE = 1       // V-1: Almost invisible
	};

	enum SoundLevel {
		SOUND_RUNNING = 3,         // S-3: Loud
		SOUND_WALKING = 2,         // S-2: Moderate
		SOUND_CRAWLING = 1,        // S-1: Quiet
		SOUND_STILL = 0            // S-0: Silent
	};

	enum AIAlertState {
		AI_NORMAL = 0,
		AI_CAUTIOUS = 1,
		AI_ALERTED = 2
	};

	// Vision cone angle (degrees)
	static constexpr float VISION_CONE_ANGLE = 120.0f;
	// Detection range base (meters)
	static constexpr float BASE_DETECTION_RANGE = 64.0f;
	// Range falloff per level
	static constexpr float RANGE_PER_LEVEL = 32.0f;

	Singleton<GCWStealthManager> _instance;

public:
	static GCWStealthManager* instance() {
		if (_instance.get() == nullptr) {
			_instance.set(new GCWStealthManager());
		}
		return _instance.get();
	}

	GCWStealthManager() : Logger("GCWStealthManager") {}

	// Player visibility/sound level getters
	VisibilityLevel getPlayerVisibilityLevel(CreatureObject* player) const;
	SoundLevel getPlayerSoundLevel(CreatureObject* player) const;

	// AI Detection calculations
	bool canAIDetectPlayer(AiAgent* ai, CreatureObject* player) const;
	float calculateDetectionLevel(AiAgent* ai, CreatureObject* player) const;
	bool isPlayerInVisionCone(AiAgent* ai, CreatureObject* player) const;
	float getDistanceFalloff(float distance) const;

	// AI Alert state management
	void setAIAlertState(AiAgent* ai, AIAlertState state);
	AIAlertState getAIAlertState(AiAgent* ai) const;
	void updateAIAlertState(AiAgent* ai);

	// First strike check
	bool canFirstStrike(CreatureObject* player, AiAgent* target) const;
	void performFirstStrike(CreatureObject* player, AiAgent* target);

	// Disguise system
	bool canDisguise(CreatureObject* player, AiAgent* npc) const;
	void applyDisguise(CreatureObject* player, AiAgent* npcTemplate);
	void removeDisguise(CreatureObject* player);
	bool isDisguised(CreatureObject* player) const;
	String getDisguiseTemplate(CreatureObject* player) const;

	// KOS/Reputation checks
	bool isImperialKOS(CreatureObject* player) const;
	bool isRebelStealthCompromised(CreatureObject* player) const;
	void applyAgentDutyBonus(CreatureObject* player);
	void removeAgentDutyBonus(CreatureObject* player);

	// Load configuration
	void loadConfiguration();
};

}
}
}
}

#endif /* GCWSTEALTHMANAGER_H_ */