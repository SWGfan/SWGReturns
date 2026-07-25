/*
 * GCWFactionGuildManager.h
 * Faction Guild Hierarchy System - Ranks above Colonel, Alliances, Custom Uniforms
 */

#ifndef GCWFACTIONGUILDMANAGER_H_
#define GCWFACTIONGUILDMANAGER_H_

#include "engine/engine.h"
#include "server/zone/objects/guild/GuildObject.h"
#include "server/zone/objects/player/PlayerObject.h"

namespace server {
namespace zone {
namespace managers {
namespace gcw {

using namespace server::zone::objects::guild;
using namespace server::zone::objects::player;

class GCWFactionGuildManager : public Object {
public:
	enum GuildRank {
		RANK_PRIVATE = 0,
		RANK_CORPORAL = 1,
		RANK_SERGEANT = 2,
		RANK_LIEUTENANT = 3,
		RANK_CAPTAIN = 4,
		RANK_MAJOR = 5,
		RANK_COLONEL = 6,
		// Faction Guild exclusive ranks (above Colonel)
		RANK_GUILD_OFFICER = 7,      // +1 above bought rank
		RANK_GUILD_LEADER = 8,       // +2 above bought rank
		RANK_ALLIANCE_LEADER = 9     // +3 above bought rank
	};

	enum FactionGuildType {
		GUILD_IMPERIAL = 1,
		GUILD_REBEL = 2
	};

	struct GuildAlliance {
		String name;
		uint64 leaderID;
		SortedVector<uint64> memberGuilds;
		Time created;
		String customUniformColor; // Hex color for shoulder pads/stripes
	};

	struct FactionGuildData {
		uint64 guildID;
		FactionGuildType faction;
		SortedVector<uint64> members;
		uint64 leaderID;
		SortedVector<uint64> officers;
		uint64 allianceID;
		String customUniformColor;
		Time created;
	};

	Singleton<GCWFactionGuildManager> _instance;

public:
	static GCWFactionGuildManager* instance() {
		if (_instance.get() == nullptr) {
			_instance.set(new GCWFactionGuildManager());
		}
		return _instance.get();
	}

	GCWFactionGuildManager() : Logger("GCWFactionGuildManager") {}

	// Faction Guild Creation
	bool createFactionGuild(CreatureObject* player, const String& name, const String& abbreviation);
	bool canCreateFactionGuild(CreatureObject* player) const;

	// Rank Management
	GuildRank getEffectiveRank(CreatureObject* player) const;
	GuildRank getGuildRank(CreatureObject* player) const;
	GuildRank getBoughtRank(CreatureObject* player) const;
	void promoteMember(CreatureObject* promoter, CreatureObject* target, GuildRank newRank);
	void demoteMember(CreatureObject* demoter, CreatureObject* target, GuildRank newRank);

	// Alliance System
	bool formAlliance(CreatureObject* leader, const String& allianceName, const SortedVector<uint64>& memberGuildIDs);
	void dissolveAlliance(uint64 allianceID);
	GuildAlliance* getAlliance(uint64 allianceID);
	SortedVector<uint64> getAllianceMembers(uint64 allianceID);

	// Custom Uniforms
	bool setCustomUniformColor(CreatureObject* player, const String& colorHex);
	String getCustomUniformColor(CreatureObject* player) const;
	void applyCustomUniform(CreatureObject* player);
	void removeCustomUniform(CreatureObject* player);

	// Faction Point Requirements
	bool hasRequiredFactionPoints(CreatureObject* player) const; // 400 FP minimum
	void checkFactionPointRequirements();

	// Guild Management
	void removeMember(CreatureObject* player);
	void transferLeadership(CreatureObject* oldLeader, CreatureObject* newLeader);
	SortedVector<uint64> getGuildMembers(uint64 guildID);
	FactionGuildData* getGuildData(uint64 guildID);

	// Rank Bonuses (above Colonel)
	float getRankBonus(GuildRank rank) const;

	// Configuration
	void loadConfiguration();
};

}
}
}
}

#endif /* GCWFACTIONGUILDMANAGER_H_ */