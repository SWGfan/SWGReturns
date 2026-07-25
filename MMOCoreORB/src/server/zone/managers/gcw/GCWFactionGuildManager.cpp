/*
 * GCWFactionGuildManager.cpp
 * Faction Guild Hierarchy System
 */

#include "GCWFactionGuildManager.h"
#include "server/zone/objects/guild/GuildObject.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/managers/guild/GuildManager.h"
#include "server/zone/managers/player/PlayerManager.h"
#include "server/zone/Zone.h"
#include "server/zone/ZoneServer.h"

namespace server {
namespace zone {
namespace managers {
namespace gcw {

using namespace server::zone::objects::guild;
using namespace server::zone::objects::player;
using namespace server::zone::managers::guild;
using namespace server::zone::managers::player;

GCWFactionGuildManager::GCWFactionGuildManager() : Logger("GCWFactionGuildManager") {
	loadConfiguration();
}

void GCWFactionGuildManager::loadConfiguration() {
	try {
		Lua* lua = new Lua();
		lua->init();
		lua->runFile("scripts/managers/gcw_faction_guild.lua");
		
		minFactionPoints = lua->getGlobalInt("minFactionPoints"); // 400
		rankBonusValues.put(RANK_GUILD_OFFICER, lua->getGlobalFloat("guildOfficerBonus"));
		rankBonusValues.put(RANK_GUILD_LEADER, lua->getGlobalFloat("guildLeaderBonus"));
		rankBonusValues.put(RANK_ALLIANCE_LEADER, lua->getGlobalFloat("allianceLeaderBonus"));
		
		delete lua;
	} catch (Exception& e) {
		error("Failed to load GCW faction guild config: " + String(e.getMessage()));
	}
}

bool GCWFactionGuildManager::canCreateFactionGuild(CreatureObject* player) const {
	if (player == nullptr)
		return false;
	
	ManagedReference<PlayerObject*> ghost = player->getPlayerObject();
	if (ghost == nullptr)
		return false;
	
	// Must be in a faction
	if (ghost->getFaction() == 0)
		return false;
	
	// Must have 400+ faction points (double the 200 to join faction)
	if (ghost->getFactionStanding("imperial") < minFactionPoints && ghost->getFactionStanding("rebel") < minFactionPoints)
		return false;
	
	// Must not already be in a faction guild
	if (ghost->getGuildObject() != nullptr) {
		ManagedReference<GuildObject*> guild = ghost->getGuildObject();
		if (guild != nullptr && guild->isFactionGuild())
			return false;
	}
	
	return true;
}

bool GCWFactionGuildManager::createFactionGuild(CreatureObject* player, const String& name, const String& abbreviation) {
	if (!canCreateFactionGuild(player))
		return false;
	
	ManagedReference<GuildManager*> guildManager = player->getZoneServer()->getGuildManager();
	if (guildManager == nullptr)
		return false;
	
	// Create the guild
	ManagedReference<GuildObject*> guild = guildManager->createGuild(player, name, abbreviation);
	if (guild == nullptr)
		return false;
	
	// Mark as faction guild
	guild->setFactionGuild(true);
	guild->setFactionGuildType(player->getPlayerObject()->getFaction());
	
	// Set custom uniform color (default)
	String defaultColor = (player->getPlayerObject()->getFaction() == 1) ? "#CC0000" : "#0066CC";
	guild->setCustomUniformColor(defaultColor);
	
	info("Created faction guild: " + name + " (" + abbreviation + ") for faction " + String::valueOf(player->getPlayerObject()->getFaction()));
	return true;
}

GCWFactionGuildManager::GuildRank GCWFactionGuildManager::getEffectiveRank(CreatureObject* player) const {
	if (player == nullptr)
		return RANK_PRIVATE;
	
	ManagedReference<PlayerObject*> ghost = player->getPlayerObject();
	if (ghost == nullptr)
		return RANK_PRIVATE;
	
	ManagedReference<GuildObject*> guild = ghost->getGuildObject();
	if (guild == nullptr || !guild->isFactionGuild())
		return ghost->getFactionRank(); // Return bought rank
	
	GuildRank guildRank = getGuildRank(player);
	GuildRank boughtRank = getBoughtRank(player);
	
	// Effective rank is max of guild rank and bought rank
	return (guildRank > boughtRank) ? guildRank : boughtRank;
}

GCWFactionGuildManager::GuildRank GCWFactionGuildManager::getGuildRank(CreatureObject* player) const {
	if (player == nullptr)
		return RANK_PRIVATE;
	
	ManagedReference<PlayerObject*> ghost = player->getPlayerObject();
	if (ghost == nullptr)
		return RANK_PRIVATE;
	
	ManagedReference<GuildObject*> guild = ghost->getGuildObject();
	if (guild == nullptr || !guild->isFactionGuild())
		return RANK_PRIVATE;
	
	// Check guild position
	if (guild->getLeaderID() == player->getObjectID())
		return RANK_GUILD_LEADER;
	
	if (guild->isOfficer(player->getObjectID()))
		return RANK_GUILD_OFFICER;
	
	// Check alliance leadership
	if (guild->getAllianceID() > 0) {
		GuildAlliance* alliance = getAlliance(guild->getAllianceID());
		if (alliance != nullptr && alliance->leaderID == player->getObjectID())
			return RANK_ALLIANCE_LEADER;
	}
	
	return RANK_PRIVATE; // Base member
}

GCWFactionGuildManager::GuildRank GCWFactionGuildManager::getBoughtRank(CreatureObject* player) const {
	if (player == nullptr)
		return RANK_PRIVATE;
	
	ManagedReference<PlayerObject*> ghost = player->getPlayerObject();
	if (ghost == nullptr)
		return RANK_PRIVATE;
	
	return (GuildRank)ghost->getFactionRank();
}

void GCWFactionGuildManager::promoteMember(CreatureObject* promoter, CreatureObject* target, GuildRank newRank) {
	if (promoter == nullptr || target == nullptr)
		return;
	
	ManagedReference<GuildObject*> guild = promoter->getPlayerObject()->getGuildObject();
	if (guild == nullptr || !guild->isFactionGuild())
		return;
	
	// Only guild leader or alliance leader can promote to officer/leader ranks
	GuildRank promoterRank = getEffectiveRank(promoter);
	if (promoterRank < RANK_GUILD_LEADER && newRank >= RANK_GUILD_OFFICER)
		return;
	
	if (promoterRank < RANK_ALLIANCE_LEADER && newRank >= RANK_GUILD_LEADER)
		return;
	
	guild->promoteMember(target->getObjectID(), newRank);
}

void GCWFactionGuildManager::demoteMember(CreatureObject* demoter, CreatureObject* target, GuildRank newRank) {
	if (demoter == nullptr || target == nullptr)
		return;
	
	ManagedReference<GuildObject*> guild = demoter->getPlayerObject()->getGuildObject();
	if (guild == nullptr || !guild->isFactionGuild())
		return;
	
	GuildRank demoterRank = getEffectiveRank(demoter);
	if (demoterRank <= getEffectiveRank(target))
		return; // Cannot demote same or higher rank
	
	guild->demoteMember(target->getObjectID(), newRank);
}

bool GCWFactionGuildManager::formAlliance(CreatureObject* leader, const String& allianceName, const SortedVector<uint64>& memberGuildIDs) {
	if (leader == nullptr)
		return false;
	
	ManagedReference<GuildObject*> leaderGuild = leader->getPlayerObject()->getGuildObject();
	if (leaderGuild == nullptr || !leaderGuild->isFactionGuild())
		return false;
	
	// Must be guild leader or alliance leader
	GuildRank leaderRank = getEffectiveRank(leader);
	if (leaderRank < RANK_GUILD_LEADER)
		return false;
	
	// All member guilds must be same faction
	FactionGuildType faction = (FactionGuildType)leaderGuild->getFactionGuildType();
	for (int i = 0; i < memberGuildIDs.size(); ++i) {
		ManagedReference<GuildObject*> guild = GuildManager::instance()->getGuild(memberGuildIDs.get(i));
		if (guild == nullptr || guild->getFactionGuildType() != faction)
			return false;
	}
	
	// Create alliance
	GuildAlliance* alliance = new GuildAlliance();
	alliance->name = allianceName;
	alliance->leaderID = leader->getObjectID();
	alliance->memberGuilds = memberGuildIDs;
	alliance->created = Time::getCurrentTime();
	alliance->customUniformColor = leaderGuild->getCustomUniformColor();
	
	uint64 allianceID = System::currentTimeMillis(); // Simple ID generation
	alliances.put(allianceID, alliance);
	
	// Assign to all member guilds
	for (int i = 0; i < memberGuildIDs.size(); ++i) {
		ManagedReference<GuildObject*> guild = GuildManager::instance()->getGuild(memberGuildIDs.get(i));
		if (guild != nullptr) {
			guild->setAllianceID(allianceID);
		}
	}
	
	info("Formed alliance: " + allianceName + " with " + String::valueOf(memberGuildIDs.size()) + " guilds");
	return true;
}

void GCWFactionGuildManager::dissolveAlliance(uint64 allianceID) {
	GuildAlliance* alliance = alliances.get(allianceID);
	if (alliance == nullptr)
		return;
	
	// Remove alliance from member guilds
	for (int i = 0; i < alliance->memberGuilds.size(); ++i) {
		ManagedReference<GuildObject*> guild = GuildManager::instance()->getGuild(alliance->memberGuilds.get(i));
		if (guild != nullptr) {
			guild->setAllianceID(0);
		}
	}
	
	alliances.drop(allianceID);
	delete alliance;
}

GCWFactionGuildManager::GuildAlliance* GCWFactionGuildManager::getAlliance(uint64 allianceID) {
	return alliances.get(allianceID);
}

SortedVector<uint64> GCWFactionGuildManager::getAllianceMembers(uint64 allianceID) {
	GuildAlliance* alliance = getAlliance(allianceID);
	if (alliance == nullptr)
		return SortedVector<uint64>();
	
	return alliance->memberGuilds;
}

bool GCWFactionGuildManager::setCustomUniformColor(CreatureObject* player, const String& colorHex) {
	if (player == nullptr)
		return false;
	
	ManagedReference<GuildObject*> guild = player->getPlayerObject()->getGuildObject();
	if (guild == nullptr || !guild->isFactionGuild())
		return false;
	
	GuildRank rank = getEffectiveRank(player);
	if (rank < RANK_GUILD_LEADER)
		return false;
	
	guild->setCustomUniformColor(colorHex);
	
	// Update alliance color if applicable
	if (guild->getAllianceID() > 0) {
		GuildAlliance* alliance = getAlliance(guild->getAllianceID());
		if (alliance != nullptr) {
			alliance->customUniformColor = colorHex;
		}
	}
	
	// Apply to all online members
	SortedVector<uint64> members = guild->getMemberIDs();
	for (int i = 0; i < members.size(); ++i) {
		ManagedReference<CreatureObject*> member = player->getZoneServer()->getObject(members.get(i)).castTo<CreatureObject*>();
		if (member != nullptr && member->isOnline()) {
			applyCustomUniform(member);
		}
	}
	
	return true;
}

String GCWFactionGuildManager::getCustomUniformColor(CreatureObject* player) const {
	if (player == nullptr)
		return "";
	
	ManagedReference<GuildObject*> guild = player->getPlayerObject()->getGuildObject();
	if (guild == nullptr || !guild->isFactionGuild())
		return "";
	
	return guild->getCustomUniformColor();
}

void GCWFactionGuildManager::applyCustomUniform(CreatureObject* player) {
	if (player == nullptr)
		return;
	
	ManagedReference<GuildObject*> guild = player->getPlayerObject()->getGuildObject();
	if (guild == nullptr || !guild->isFactionGuild())
		return;
	
	String color = guild->getCustomUniformColor();
	if (color.isEmpty())
		return;
	
	// Apply colored shoulder pads/stripes to faction armor
	// This would modify the appearance of equipped faction armor
	// Implementation depends on the wearable system
	player->setCustomUniformColor(color);
	player->broadcastObjectTemplateUpdate();
}

void GCWFactionGuildManager::removeCustomUniform(CreatureObject* player) {
	if (player == nullptr)
		return;
	
	player->setCustomUniformColor("");
	player->broadcastObjectTemplateUpdate();
}

bool GCWFactionGuildManager::hasRequiredFactionPoints(CreatureObject* player) const {
	if (player == nullptr)
		return false;
	
	ManagedReference<PlayerObject*> ghost = player->getPlayerObject();
	if (ghost == nullptr)
		return false;
	
	int faction = ghost->getFaction();
	if (faction == 1) // Imperial
		return ghost->getFactionStanding("imperial") >= minFactionPoints;
	else if (faction == 2) // Rebel
		return ghost->getFactionStanding("rebel") >= minFactionPoints;
	
	return false;
}

void GCWFactionGuildManager::checkFactionPointRequirements() {
	// Periodic check - remove players who fall below 400 FP
	// This would be called from a scheduled task
}

void GCWFactionGuildManager::removeMember(CreatureObject* player) {
	if (player == nullptr)
		return;
	
	ManagedReference<GuildObject*> guild = player->getPlayerObject()->getGuildObject();
	if (guild == nullptr || !guild->isFactionGuild())
		return;
	
	guild->removeMember(player->getObjectID());
	removeCustomUniform(player);
}

void GCWFactionGuildManager::transferLeadership(CreatureObject* oldLeader, CreatureObject* newLeader) {
	if (oldLeader == nullptr || newLeader == nullptr)
		return;
	
	ManagedReference<GuildObject*> guild = oldLeader->getPlayerObject()->getGuildObject();
	if (guild == nullptr || !guild->isFactionGuild())
		return;
	
	if (guild->getLeaderID() != oldLeader->getObjectID())
		return;
	
	guild->setLeader(newLeader->getObjectID());
	
	// Update ranks
	guild->promoteMember(newLeader->getObjectID(), RANK_GUILD_LEADER);
	guild->demoteMember(oldLeader->getObjectID(), RANK_PRIVATE);
}

SortedVector<uint64> GCWFactionGuildManager::getGuildMembers(uint64 guildID) {
	ManagedReference<GuildObject*> guild = GuildManager::instance()->getGuild(guildID);
	if (guild == nullptr)
		return SortedVector<uint64>();
	
	return guild->getMemberIDs();
}

GCWFactionGuildManager::FactionGuildData* GCWFactionGuildManager::getGuildData(uint64 guildID) {
	ManagedReference<GuildObject*> guild = GuildManager::instance()->getGuild(guildID);
	if (guild == nullptr || !guild->isFactionGuild())
		return nullptr;
	
	FactionGuildData* data = new FactionGuildData();
	data->guildID = guildID;
	data->faction = (FactionGuildType)guild->getFactionGuildType();
	data->members = guild->getMemberIDs();
	data->leaderID = guild->getLeaderID();
	data->officers = guild->getOfficerIDs();
	data->allianceID = guild->getAllianceID();
	data->customUniformColor = guild->getCustomUniformColor();
	data->created = guild->getCreatedDate();
	
	return data;
}

float GCWFactionGuildManager::getRankBonus(GuildRank rank) const {
	return rankBonusValues.get(rank, 0.0f);
}

}
}
}
}