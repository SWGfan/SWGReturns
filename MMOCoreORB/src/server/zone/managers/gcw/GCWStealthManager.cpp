/*
 * GCWStealthManager.cpp
 * Implements the stealth/detection system for GCW gameplay
 */

#include "GCWStealthManager.h"
#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/creature/ai/AiAgent.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/managers/player/PlayerManager.h"
#include "server/zone/Zone.h"
#include "server/zone/ZoneServer.h"
#include "server/zone/objects/tangible/wearables/WearableObject.h"
#include "server/zone/packets/scene/PlayClientEffectLocMessage.h"

namespace server {
namespace zone {
namespace managers {
namespace gcw {

using namespace server::zone::objects::creature;
using namespace server::zone::objects::player;
using namespace server::zone::managers::player;

GCWStealthManager::GCWStealthManager() : Logger("GCWStealthManager") {
	loadConfiguration();
}

void GCWStealthManager::loadConfiguration() {
	try {
		Lua* lua = new Lua();
		lua->init();
		lua->runFile("scripts/managers/gcw_stealth.lua");
		
		VISION_CONE_ANGLE = lua->getGlobalFloat("visionConeAngle");
		BASE_DETECTION_RANGE = lua->getGlobalFloat("baseDetectionRange");
		RANGE_PER_LEVEL = lua->getGlobalFloat("rangePerLevel");
		
		delete lua;
	} catch (Exception& e) {
		error("Failed to load GCW stealth configuration: " + String(e.getMessage()));
	}
}

GCWStealthManager::VisibilityLevel GCWStealthManager::getPlayerVisibilityLevel(CreatureObject* player) const {
	if (player == nullptr)
		return VISIBILITY_STANDING;

	if (player->isProne())
		return VISIBILITY_PRONE;
	else if (player->isKneeling())
		return VISIBILITY_KNEELING;
	else
		return VISIBILITY_STANDING;
}

GCWStealthManager::SoundLevel GCWStealthManager::getPlayerSoundLevel(CreatureObject* player) const {
	if (player == nullptr)
		return SOUND_STILL;

	if (player->isRunning())
		return SOUND_RUNNING;
	else if (player->isWalking())
		return SOUND_WALKING;
	else if (player->isCrawling())
		return SOUND_CRAWLING;
	else
		return SOUND_STILL;
}

float GCWStealthManager::getDistanceFalloff(float distance) const {
	int levels = (int)(distance / RANGE_PER_LEVEL);
	return std::max(0.0f, 1.0f - (levels * 0.25f));
}

bool GCWStealthManager::isPlayerInVisionCone(AiAgent* ai, CreatureObject* player) const {
	if (ai == nullptr || player == nullptr)
		return false;

	float aiYaw = ai->getDirectionAngle();
	
	float dx = player->getPositionX() - ai->getPositionX();
	float dz = player->getPositionZ() - ai->getPositionZ();
	float angleToPlayer = atan2(dx, dz) * 180.0f / M_PI;
	
	while (angleToPlayer < 0) angleToPlayer += 360.0f;
	while (angleToPlayer >= 360.0f) angleToPlayer -= 360.0f;
	while (aiYaw < 0) aiYaw += 360.0f;
	while (aiYaw >= 360.0f) aiYaw -= 360.0f;
	
	float diff = std::abs(angleToPlayer - aiYaw);
	if (diff > 180.0f) diff = 360.0f - diff;
	
	return diff <= (VISION_CONE_ANGLE / 2.0f);
}

bool GCWStealthManager::canAIDetectPlayer(AiAgent* ai, CreatureObject* player) const {
	if (ai == nullptr || player == nullptr)
		return false;

	AIAlertState alertState = getAIAlertState(ai);
	
	VisibilityLevel visLevel = getPlayerVisibilityLevel(player);
	SoundLevel sndLevel = getPlayerSoundLevel(player);
	
	float distance = ai->getDistanceTo(player);
	if (distance > BASE_DETECTION_RANGE)
		return false;
	
	if (!isPlayerInVisionCone(ai, player))
		return false;
	
	float falloff = getDistanceFalloff(distance);
	if (falloff <= 0.0f)
		return false;
	
	int effectiveVis = std::max(0, (int)visLevel - (int)(distance / RANGE_PER_LEVEL));
	int effectiveSnd = std::max(0, (int)sndLevel - (int)(distance / RANGE_PER_LEVEL));
	
	int visThreshold, sndThreshold;
	switch (alertState) {
		case AI_NORMAL:
			visThreshold = 1;
			sndThreshold = 1;
			break;
		case AI_CAUTIOUS:
			visThreshold = 2;
			sndThreshold = 2;
			break;
		case AI_ALERTED:
			visThreshold = 3;
			sndThreshold = 3;
			break;
		default:
			visThreshold = 1;
			sndThreshold = 1;
	}
	
	return (effectiveVis >= visThreshold || effectiveSnd >= sndThreshold);
}

float GCWStealthManager::calculateDetectionLevel(AiAgent* ai, CreatureObject* player) const {
	if (ai == nullptr || player == nullptr)
		return 0.0f;

	if (!canAIDetectPlayer(ai, player))
		return 0.0f;

	VisibilityLevel visLevel = getPlayerVisibilityLevel(player);
	SoundLevel sndLevel = getPlayerSoundLevel(player);
	
	float distance = ai->getDistanceTo(player);
	float falloff = getDistanceFalloff(distance);
	
	float detection = (visLevel * 0.6f + sndLevel * 0.4f) * falloff;
	return std::min(1.0f, detection);
}

void GCWStealthManager::setAIAlertState(AiAgent* ai, AIAlertState state) {
	if (ai == nullptr)
		return;
	
	ai->setCustomVariable("gcw_alert_state", (int)state);
}

GCWStealthManager::AIAlertState GCWStealthManager::getAIAlertState(AiAgent* ai) const {
	if (ai == nullptr)
		return AI_NORMAL;
	
	int state = ai->getCustomVariable("gcw_alert_state");
	if (state < 0 || state > 2)
		return AI_NORMAL;
	
	return (AIAlertState)state;
}

void GCWStealthManager::updateAIAlertState(AiAgent* ai) {
	if (ai == nullptr)
		return;
	
	// Check nearby threats, recent combat, etc.
	// This would be called periodically by AI think event
}

bool GCWStealthManager::canFirstStrike(CreatureObject* player, AiAgent* target) const {
	if (player == nullptr || target == nullptr)
		return false;
	
	// Must be combatant
	if (!player->isPlayerCreature() || !target->isNonPlayerCreatureObject())
		return false;
	
	// Target must be in normal or cautious state (not alerted)
	AIAlertState targetState = getAIAlertState(target);
	if (targetState == AI_ALERTED)
		return false;
	
	// Player must be undetected
	if (canAIDetectPlayer(target, player))
		return false;
	
	// Player must be behind target (in vision cone blind spot)
	if (isPlayerInVisionCone(target, player))
		return false;
	
	// Check if player has first strike ability
	ManagedReference<PlayerObject*> ghost = player->getPlayerObject();
	if (ghost == nullptr || !ghost->hasAbility("first_strike"))
		return false;
	
	return true;
}

void GCWStealthManager::performFirstStrike(CreatureObject* player, AiAgent* target) {
	if (player == nullptr || target == nullptr)
		return;
	
	// Insta-kill the target
	target->inflictDamage(player, CreatureAttribute::HEALTH, target->getMaxHAM(CreatureAttribute::HEALTH), true, true, true);
	
	// No XP granted (as per design)
	// target->setExperienceReward(0);
	
	// Send effect
	PlayClientEffectLocMessage* effect = new PlayClientEffectLocMessage("clienteffect/combat_force_explosion.cef", target->getZone()->getZoneName(), target->getPositionX(), target->getPositionY(), target->getPositionZ());
	player->sendMessage(effect);
	
	// Make corpse lootable for disguise
	target->setLootable(true);
	target->setFirstStrikeKill(true);
	
	// Update mission/quest if applicable
}

bool GCWStealthManager::canDisguise(CreatureObject* player, AiAgent* npc) const {
	if (player == nullptr || npc == nullptr)
		return false;
	
	// Must be a first-strike kill
	if (!npc->isFirstStrikeKill())
		return false;
	
	// Must be in GCW military zone
	Zone* zone = player->getZone();
	if (zone == nullptr || !zone->isGCWRegion(player->getPositionX(), player.getPositionZ()))
		return false;
	
	// Player must not already be disguised
	if (isDisguised(player))
		return false;
	
	return true;
}

void GCWStealthManager::applyDisguise(CreatureObject* player, AiAgent* npcTemplate) {
	if (player == nullptr || npcTemplate == nullptr)
		return;
	
	String templateName = npcTemplate->getObjectTemplate()->getFullTemplateString();
	
	// Store original appearance
	ManagedReference<PlayerObject*> ghost = player->getPlayerObject();
	if (ghost != nullptr) {
		ghost->setCustomVariable("original_template", player->getObjectTemplate()->getFullTemplateString());
		ghost->setCustomVariable("disguise_template", templateName);
		ghost->setCustomVariable("disguised", 1);
	}
	
	// Overwrite appearance
	player->setObjectTemplate(SharedObjectTemplateManager::instance()->getTemplate(templateName));
	player->broadcastObjectTemplateUpdate();
	
	// Send system message
	player->sendSystemMessage("@gcw:disguise_applied"); // "You have donned a disguise."
}

void GCWStealthManager::removeDisguise(CreatureObject* player) {
	if (player == nullptr)
		return;
	
	ManagedReference<PlayerObject*> ghost = player->getPlayerObject();
	if (ghost == nullptr)
		return;
	
	String originalTemplate = ghost->getCustomVariable("original_template");
	if (originalTemplate.isEmpty())
		return;
	
	ghost->dropCustomVariable("original_template");
	ghost->dropCustomVariable("disguise_template");
	ghost->dropCustomVariable("disguised");
	
	// Restore appearance
	player->setObjectTemplate(SharedObjectTemplateManager::instance()->getTemplate(originalTemplate));
	player->broadcastObjectTemplateUpdate();
	
	player->sendSystemMessage("@gcw:disguise_removed"); // "Your disguise has worn off."
}

bool GCWStealthManager::isDisguised(CreatureObject* player) const {
	if (player == nullptr)
		return false;
	
	ManagedReference<PlayerObject*> ghost = player->getPlayerObject();
	if (ghost == nullptr)
		return false;
	
	return ghost->getCustomVariable("disguised") == 1;
}

String GCWStealthManager::getDisguiseTemplate(CreatureObject* player) const {
	if (player == nullptr)
		return "";
	
	ManagedReference<PlayerObject*> ghost = player->getPlayerObject();
	if (ghost == nullptr)
		return "";
	
	return ghost->getCustomVariable("disguise_template");
}

bool GCWStealthManager::isImperialKOS(CreatureObject* player) const {
	if (player == nullptr)
		return false;
	
	// Imperials start KOS
	ManagedReference<PlayerObject*> ghost = player->getPlayerObject();
	if (ghost == nullptr)
		return false;
	
	if (ghost->getFaction() != 1) // Imperial
		return false;
	
	// Check if agent duty bonus is active
	if (ghost->getCustomVariable("agent_duty_active") == 1)
		return false;
	
	return true;
}

bool GCWStealthManager::isRebelStealthCompromised(CreatureObject* player) const {
	if (player == nullptr)
		return false;
	
	ManagedReference<PlayerObject*> ghost = player->getPlayerObject();
	if (ghost == nullptr)
		return false;
	
	if (ghost->getFaction() != 2) // Rebel
		return false;
	
	// Negative faction reputation compromises stealth
	int factionStanding = ghost->getFactionStanding("rebel");
	return factionStanding < 0;
}

void GCWStealthManager::applyAgentDutyBonus(CreatureObject* player) {
	if (player == nullptr)
		return;
	
	ManagedReference<PlayerObject*> ghost = player->getPlayerObject();
	if (ghost == nullptr)
		return;
	
	ghost->setCustomVariable("agent_duty_active", 1);
	ghost->setCustomVariable("agent_duty_end", System::currentTimeMillis() + (30 * 60 * 1000)); // 30 minutes
	
	player->sendSystemMessage("@gcw:agent_duty_active"); // "Agent duty activated. KOS status temporarily removed."
}

void GCWStealthManager::removeAgentDutyBonus(CreatureObject* player) {
	if (player == nullptr)
		return;
	
	ManagedReference<PlayerObject*> ghost = player->getPlayerObject();
	if (ghost == nullptr)
		return;
	
	ghost->dropCustomVariable("agent_duty_active");
	ghost->dropCustomVariable("agent_duty_end");
	
	player->sendSystemMessage("@gcw:agent_duty_expired"); // "Agent duty expired. KOS status restored."
}

}
}
}
}