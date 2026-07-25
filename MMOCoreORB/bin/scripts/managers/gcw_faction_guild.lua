-- GCW Faction Guild Configuration
-- Ranks above Colonel, Alliances, Custom Uniforms
-- NOTE: Extended guild features (alliances, custom uniforms) require IDL changes
-- to GuildObject. This config provides data + stubs for future implementation.

gcwFactionGuildConfig = {
	requirements = {
		minFactionPoints = 400,
		minMembers = 5,
		factionAlignmentRequired = true,
		canOnlyJoinOwnFaction = true
	},

	ranks = {
		[0] = { name = "Private", abbreviation = "PVT", factionPoints = 0, icon = "rank_0" },
		[1] = { name = "Corporal", abbreviation = "CPL", factionPoints = 500, icon = "rank_1" },
		[2] = { name = "Sergeant", abbreviation = "SGT", factionPoints = 1500, icon = "rank_2" },
		[3] = { name = "Lieutenant", abbreviation = "LT", factionPoints = 3000, icon = "rank_3" },
		[4] = { name = "Captain", abbreviation = "CPT", factionPoints = 6000, icon = "rank_4" },
		[5] = { name = "Major", abbreviation = "MAJ", factionPoints = 12000, icon = "rank_5" },
		[6] = { name = "Colonel", abbreviation = "COL", factionPoints = 25000, icon = "rank_6" },
		[7] = { name = "Guild Officer", abbreviation = "GOF", factionPoints = 0, requiresGuild = true, icon = "rank_7" },
		[8] = { name = "Guild Leader", abbreviation = "GLD", factionPoints = 0, requiresGuild = true, icon = "rank_8" },
		[9] = { name = "Alliance Leader", abbreviation = "ALD", factionPoints = 0, requiresAlliance = true, icon = "rank_9" }
	},

	rankBonuses = {
		[0] = { combat = 0.0, social = 0.0, economic = 0.0 },
		[1] = { combat = 0.02, social = 0.05, economic = 0.02 },
		[2] = { combat = 0.05, social = 0.10, economic = 0.05 },
		[3] = { combat = 0.10, social = 0.15, economic = 0.10 },
		[4] = { combat = 0.15, social = 0.20, economic = 0.15 },
		[5] = { combat = 0.20, social = 0.25, economic = 0.20 },
		[6] = { combat = 0.25, social = 0.30, economic = 0.25 },
		[7] = { combat = 0.30, social = 0.35, economic = 0.30 },
		[8] = { combat = 0.35, social = 0.40, economic = 0.35 },
		[9] = { combat = 0.40, social = 0.50, economic = 0.40 }
	},

	rankOffsets = {
		guild_officer = 1,
		guild_leader = 2,
		alliance_leader = 3
	},

	alliances = {
		minGuilds = 3,
		maxGuilds = 10,
		leaderRank = 9,
		officerRank = 8,
		formationCost = 10000,
		maintenanceCost = 500,
		disbandPenalty = 5000
	},

	uniformCustomization = {
		imperial = {
			shoulderPadColors = {
				"red", "blue", "yellow", "green", "black", "white", "orange", "purple",
				"gold", "silver", "dark_red", "dark_blue"
			},
			stripePatterns = { "none", "single", "double", "triple", "chevron", "diamond" },
			helmetMarkings = { "none", "stripe", "checker", "skull", "imperial_cog", "lightning" },
			pauldronStyles = { "standard", "officer", "command", "special_forces" }
		},
		rebel = {
			shoulderPadColors = {
				"red", "blue", "yellow", "green", "black", "white", "orange", "purple",
				"gold", "silver", "olive", "sand"
			},
			vestColors = { "olive", "brown", "gray", "black", "blue", "tan", "camouflage" },
			headgearOptions = { "none", "bandana", "beret", "helmet", "cap", "hood" },
			insigniaStyles = { "none", "rebel_symbol", "unit_patch", "rank_chevron", "custom" }
		}
	},

	management = {
		promoteCooldown = 300,
		demoteCooldown = 300,
		transferLeadershipCooldown = 86400,
		kickMemberCooldown = 600,
		guildDisbandProtection = 604800
	},

	guildRankCosts = {
		guild_officer = 5000,
		guild_leader = 10000,
		alliance_leader = 20000
	}
}

gcwFactionGuildManager = {
	alliances = {},

	init = function(self)
		print("[GCW] Faction Guild Manager initialized")
	end,

	canCreateGuild = function(self, player)
		if player == nil then return false end

		local ghost = PlayerObject(player:getPlayerObjectID())
		if ghost == nil then return false end

		if ghost:getFaction() == 0 then return false end

		local faction = ghost:getFaction()
		local points = 0
		if faction == 1 then
			points = CreatureObject(player):getFactionStanding("imperial")
		elseif faction == 2 then
			points = CreatureObject(player):getFactionStanding("rebel")
		end
		if points < 400 then return false end

		local guildID = ghost:getGuildID()
		if guildID > 0 then
			local guild = getSceneObject(guildID)
			if guild ~= nil then
				return false
			end
		end

		return true
	end,

	getGuildRank = function(self, player)
		if player == nil then return 0 end

		local ghost = PlayerObject(player:getPlayerObjectID())
		if ghost == nil then return 0 end

		local guildID = ghost:getGuildID()
		if guildID <= 0 then return 0 end

		local guild = getSceneObject(guildID)
		if guild == nil then return 0 end

		if guild:isGuildLeader(player) then
			return 8
		end

		return 0
	end,

	getEffectiveRank = function(self, player)
		if player == nil then return 0 end

		local ghost = PlayerObject(player:getPlayerObjectID())
		if ghost == nil then return 0 end

		local boughtRank = ghost:getFactionRank()
		local guildRank = self:getGuildRank(player)

		return math.max(boughtRank, guildRank)
	end,

	hasRequiredFactionPoints = function(self, player)
		if player == nil then return false end

		local ghost = PlayerObject(player:getPlayerObjectID())
		if ghost == nil then return false end

		local faction = ghost:getFaction()
		if faction == 1 then
			return CreatureObject(player):getFactionStanding("imperial") >= 400
		elseif faction == 2 then
			return CreatureObject(player):getFactionStanding("rebel") >= 400
		end

		return false
	end,

	checkFactionPointRequirements = function(self)
	end,

	removeMember = function(self, player)
		if player == nil then return end

		local ghost = PlayerObject(player:getPlayerObjectID())
		if ghost == nil then return end

		local guildID = ghost:getGuildID()
		if guildID <= 0 then return end

		local guild = getSceneObject(guildID)
		if guild == nil then return end

		guild:removeMember(player:getObjectID())
	end,

	transferLeadership = function(self, oldLeader, newLeader)
		if oldLeader == nil or newLeader == nil then return false end

		local ghost = PlayerObject(oldLeader:getPlayerObjectID())
		if ghost == nil then return false end

		local guildID = ghost:getGuildID()
		if guildID <= 0 then return false end

		local guild = getSceneObject(guildID)
		if guild == nil then return false end

		if not guild:isGuildLeader(oldLeader) then return false end

		guild:setGuildLeaderID(newLeader:getObjectID())
		return true
	end,

	getGuildData = function(self, guildID)
		local guild = getSceneObject(guildID)
		if guild == nil then return nil end

		return {
			guildID = guildID,
			name = guild:getGuildName(),
			abbreviation = guild:getGuildAbbrev(),
			leaderID = guild:getGuildLeaderID(),
			totalMembers = guild:getTotalMembers()
		}
	end,

	getRankBonus = function(self, rank)
		return gcwFactionGuildConfig.rankBonuses[rank] or { combat = 0, social = 0, economic = 0 }
	end,

	applyCustomUniform = function(self, player)
	end,

	removeCustomUniform = function(self, player)
	end
}

gcwFactionGuildManager:init()
