-- GCW Base Configuration
-- Based on the 2005 GCW redesign document

gcwBaseConfig = {
	-- Base Sizes and Types
	baseSizes = {
		s01 = { name = "Forward Outpost", points = 2, maxPerPlanet = 10 },
		s02 = { name = "Field Hospital", points = 6, maxPerPlanet = 6 },
		s03 = { name = "Tactical Center", points = 8, maxPerPlanet = 4 },
		s04 = { name = "Detachment HQ", points = 20, maxPerPlanet = 2 },
		s05 = { name = "Regional HQ", points = 50, maxPerPlanet = 1 }
	},

	-- Imperial Base Perks
	imperialPerks = {
		reinforcements = {
			name = "Call Reinforcements",
			description = "Call in waves of Imperial reinforcements to defend the base",
			factionPointsCost = 500,
			cooldown = 1800, -- 30 minutes
			maxWaves = 3,
			waveInterval = 300, -- 5 minutes between waves
			reinforcementTypes = {
				{ template = "stormtrooper", count = 8, level = 90 },
				{ template = "stormtrooper_sniper", count = 2, level = 90 },
				{ template = "stormtrooper_medic", count = 1, level = 90 },
				{ template = "imperial_officer", count = 1, level = 92 }
			}
		},
		at_at_deploy = {
			name = "Deploy AT-AT Walker",
			description = "Deploy an AT-AT walker to assault a discovered Rebel base",
			factionPointsCost = 2000,
			cooldown = 3600, -- 1 hour
			requiresDiscoveredBase = true,
			at_at_template = "at_at_walker",
			deployRange = 128, -- meters from base
			duration = 1800, -- 30 minutes
			health = 500000,
			armor = 200,
			weapons = {
				{ name = "heavy_blaster_cannon", damage = 2000, range = 200 },
				{ name = "concussion_missile", damage = 5000, range = 300 }
			}
		}
	},

	-- Rebel Base Perks
	rebelPerks = {
		relocation = {
			name = "Relocate Base",
			description = "Move the base to a new location (outside vulnerability window)",
			factionPointsCost = 1000,
			cooldown = 86400, -- 24 hours
			relocationTime = 10800, -- 3 hours to deconstruct/reconstruct
			cannotRelocateDuringVulnerability = true
		},
		evacuation = {
			name = "Emergency Evacuation",
			description = "Evacuate base under attack, destroying it but allowing rebuild at reduced cost",
			factionPointsCost = 500,
			cooldown = 0, -- No cooldown, but only during attack
			evacuationTimer = 1800, -- 30 minutes to survive
			rebuildDiscount = 0.5, -- 50% discount on new base
			requiresPvEConnection = true -- Must be connected to Rebel Cell
		},
		silent_strike = {
			name = "Silent Strike Bonus",
			description = "Connected Rebel Cells provide stealth bonuses to nearby operatives",
			factionPointsCost = 0, -- Passive perk
			stealthBonus = 25, -- +25% stealth effectiveness
			range = 500 -- meters from connected PvP base
		}
	},

	-- PvE Base Types
	pveBases = {
		imperial_relay = {
			name = "Imperial Relay Station",
			description = "Communications relay providing reinforcement calls to connected PvP base",
			template = "object/building/general/imperial_relay_station.iff",
			perk = "reinforcements",
			perkDisabledDuration = 3600, -- 1 hour when infiltrated silently
			perkDisabledDurationLoud = 7200, -- 2 hours when destroyed/alerted
			connections = {
				maxDistance = 5000,
				minDistance = 1000,
				maxPerPvPBase = 2
			},
			infiltration = {
				silentRewards = { "intel_data", "uniform_imperial_officer" },
				loudPenalty = "alert_all_bases",
				guardCount = 12,
				guardLevel = 90
			}
		},
		rebel_cell = {
			name = "Rebel Resistance Cell",
			description = "Hidden cell providing evacuation and relocation perks to connected PvP base",
			template = "object/building/general/rebel_resistance_cell.iff",
			perk = "evacuation",
			perkDisabledDuration = 1800, -- 30 minutes when infiltrated
			connections = {
				maxDistance = 5000,
				minDistance = 1000,
				maxPerPvPBase = 2
			},
			infiltration = {
				silentRewards = { "base_location_data", "relocation_plans" },
				loudPenalty = "trigger_relocation",
				guardCount = 8,
				guardLevel = 85
			}
		}
	},

	-- Vulnerability System
	vulnerability = {
		duration = 10800, -- 3 hours
		frequency = 172800, -- 48 hours
		initialDelay = 0, -- Delay after placement
		pvpBasesOnly = true,
		announceToFaction = true,
		announceToEnemy = false
	},

	-- AT-AT Walker
	at_at = {
		health = 500000,
		armor = 200,
		shields = 100000,
		weapons = {
			main = { damage = 2000, range = 200, cooldown = 3 },
			secondary = { damage = 5000, range = 300, cooldown = 10 }
		},
		movementSpeed = 5, -- m/s
		deployRange = 128,
		duration = 1800, -- 30 minutes
		requiresDiscoveredBase = true,
		canBeStopped = true,
		stopDifficulty = 0.8, -- 80% chance to fail stopping
		selfDefense = true,
		targetPriority = "base_structure"
	},

	-- Reinforcement Waves
	reinforcements = {
		waveInterval = 300,
		maxWaves = 3,
		waveScaling = { 1.0, 1.5, 2.0 }, -- Each wave stronger
		types = {
			{ template = "stormtrooper", count = 8, level = 90, weight = 1.0 },
			{ template = "stormtrooper_sniper", count = 2, level = 90, weight = 1.2 },
			{ template = "stormtrooper_medic", count = 1, level = 90, weight = 1.5 },
			{ template = "imperial_officer", count = 1, level = 92, weight = 1.8 },
			{ template = "dark_trooper", count = 2, level = 95, weight = 2.0 }
		}
	},

	-- Faction Point Costs
	factionPointCosts = {
		-- Base purchase
		s01_pvp = 20000, s01_pve = 10000,
		s02_pvp = 40000, s02_pve = 20000,
		s03_pvp = 45000, s03_pve = 22500,
		s04_pvp = 60000, s04_pve = 30000,
		s05_pvp = 80000,
		
		-- Perks
		reinforcements = 500,
		at_at_deploy = 2000,
		relocation = 1000,
		evacuation = 500,
		
		-- Guild ranks
		rank_purchase = {
			private = 0,
			corporal = 500,
			sergeant = 1500,
			lieutenant = 3000,
			captain = 6000,
			major = 12000,
			colonel = 25000
		}
	},

	-- Planet Control
	planetControl = {
		winnerXPBonus = 50,
		loserXPPenalty = -30,
		strongholdCities = {
			imperial = { "bela_vistal", "deeja_peak", "bestine" },
			rebel = { "vreni_island", "moenia", "anchorhead" }
		}
	}
}

-- Faction Guild Configuration
factionGuildConfig = {
	-- Requirements
	requirements = {
		minFactionPoints = 400, -- Double the normal 200
		minMembers = 5,
		factionAlignmentRequired = true
	},

	-- Rank Structure (extends beyond Colonel)
	ranks = {
		[0] = { name = "Private", abbreviation = "PVT", factionPoints = 0 },
		[1] = { name = "Corporal", abbreviation = "CPL", factionPoints = 500 },
		[2] = { name = "Sergeant", abbreviation = "SGT", factionPoints = 1500 },
		[3] = { name = "Lieutenant", abbreviation = "LT", factionPoints = 3000 },
		[4] = { name = "Captain", abbreviation = "CPT", factionPoints = 6000 },
		[5] = { name = "Major", abbreviation = "MAJ", factionPoints = 12000 },
		[6] = { name = "Colonel", abbreviation = "COL", factionPoints = 25000 },
		-- Faction Guild exclusive
		[7] = { name = "Guild Officer", abbreviation = "GOF", factionPoints = 0, requiresGuild = true },
		[8] = { name = "Guild Leader", abbreviation = "GLD", factionPoints = 0, requiresGuild = true },
		[9] = { name = "Alliance Leader", abbreviation = "ALD", factionPoints = 0, requiresAlliance = true }
	},

	-- Guild Rank Offsets
	rankOffsets = {
		guild_officer = 1,      -- +1 above bought rank
		guild_leader = 2,       -- +2 above bought rank
		alliance_leader = 3     -- +3 above bought rank
	},

	-- Alliance System
	alliances = {
		minGuilds = 3,
		maxGuilds = 10,
		leaderRank = 9,
		officerRank = 8
	},

	-- Custom Uniforms
	uniformCustomization = {
		imperial = {
			shoulderPadColors = { "red", "blue", "yellow", "green", "black", "white", "orange", "purple" },
			stripePatterns = { "single", "double", "triple", "chevron" },
			helmetMarkings = { "none", "stripe", "checker", "skull" }
		},
		rebel = {
			shoulderPadColors = { "red", "blue", "yellow", "green", "black", "white", "orange", "purple" },
			vestColors = { "olive", "brown", "gray", "black", "blue" },
			headgearOptions = { "none", "bandana", "beret", "helmet" }
		}
	}
}

-- KOS/Stealth Configuration
stealthConfig = {
	-- Imperial KOS
	imperial = {
		startsKOS = true,
		agentDuty = {
			available = true,
			duration = 1800, -- 30 minutes
			cooldown = 3600, -- 1 hour
			removesKOS = true,
			removesTEF = true,
			requiresFactionPoints = 500
		}
	},

	-- Rebel Stealth
	rebel = {
		startsStealth = true,
		losesStealthAtNegativeRep = true,
		stealthLossThreshold = 0, -- Below 0 faction standing
		regainStealthThreshold = 100 -- Need +100 to regain
	},

	-- Detection System
	detection = {
		visionConeAngle = 120,
		baseDetectionRange = 64,
		rangePerLevel = 32,
		visibilityLevels = {
			[0] = { name = "Still", sound = 0, visibility = 0 },
			[1] = { name = "Prone", sound = 1, visibility = 1 },
			[2] = { name = "Crawling", sound = 1, visibility = 1 },
			[3] = { name = "Kneeling", sound = 2, visibility = 2 },
			[4] = { name = "Walking", sound = 2, visibility = 3 },
			[5] = { name = "Running", sound = 3, visibility = 3 }
		},
		aiAlertStates = {
			normal = { visThreshold = 1, sndThreshold = 1 },
			cautious = { visThreshold = 2, sndThreshold = 2 },
			alerted = { visThreshold = 3, sndThreshold = 3 }
		}
	},

	-- First Strike
	firstStrike = {
		enabled = true,
		grantsXP = false,
		instaKill = true,
		requiresStealth = true,
		requiresBehindTarget = true,
		requiresAbility = "first_strike",
		onlyInMilitaryZones = true,
		grantsDisguise = true
	},

	-- Disguise
	disguise = {
		duration = 0, -- Until zone exit or combat
		overwritesAppearance = true,
		restoresOnZoneExit = true,
		restoresOnCombat = true,
		restoresOnDetected = true
	}
}