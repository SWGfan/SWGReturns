-- GCW Stealth Configuration
-- Detection levels, KOS rules, and stealth mechanics

gcwStealthConfig = {
	-- Detection System
	detection = {
		visionConeAngle = 120, -- degrees
		baseDetectionRange = 64, -- meters
		rangePerLevel = 32, -- meters per visibility/sound level
		
		-- Visibility Levels (V-0 to V-3)
		visibilityLevels = {
			[0] = { name = "Still", description = "Not moving, maximum stealth" },
			[1] = { name = "Prone", description = "Lying flat, nearly invisible" },
			[2] = { name = "Kneeling", description = "Crouched, reduced visibility" },
			[3] = { name = "Standing", description = "Fully visible" }
		},
		
		-- Sound Levels (S-0 to S-3)
		soundLevels = {
			[0] = { name = "Still", description = "Silent" },
			[1] = { name = "Crawling", description = "Almost silent movement" },
			[2] = { name = "Walking", description = "Moderate noise" },
			[3] = { name = "Running", description = "Loud, easily heard" }
		},
		
		-- AI Alert States and Detection Thresholds
		aiAlertStates = {
			normal = {
				name = "Normal",
				visThreshold = 1, -- Detects V-1 and above
				sndThreshold = 1, -- Detects S-1 and above
				rangeMultiplier = 1.0,
				behavior = "patrol"
			},
			cautious = {
				name = "Cautious",
				visThreshold = 2, -- Detects V-2 and above
				sndThreshold = 2, -- Detects S-2 and above
				rangeMultiplier = 1.5,
				behavior = "investigate"
			},
			alerted = {
				name = "Alerted",
				visThreshold = 3, -- Detects everything
				sndThreshold = 3,
				rangeMultiplier = 2.0,
				behavior = "combat"
			}
		},
		
		-- Environmental Modifiers
		environmentModifiers = {
			night = {
				enabled = true,
				visionPenalty = 0.5, -- 50% harder to see at night
				soundPenalty = 0.8, -- 20% harder to hear
				nightHours = { 20, 5 } -- 8 PM to 5 AM
			},
			weather = {
				clear = { vision = 1.0, sound = 1.0 },
				rain = { vision = 0.8, sound = 1.2 },
				sandstorm = { vision = 0.3, sound = 0.5 },
				fog = { vision = 0.4, sound = 1.1 }
			},
			terrain = {
				forest = { vision = 1.2, sound = 0.9 },
				urban = { vision = 0.9, sound = 1.1 },
				desert = { vision = 1.1, sound = 1.0 },
				swamp = { vision = 1.1, sound = 0.8 }
			}
		},
		
		-- Disguise System
		disguise = {
			detectionReduction = 0.9, -- 90% harder to detect when disguised
			onlyInGCWZones = true,
			breaksOnCombat = true,
			breaksOnZoneExit = true,
			validSources = {
				imperial = { "imperial_officer", "stormtrooper", "imperial_private" },
				rebel = { "rebel_trooper", "rebel_officer", "rebel_specforce" }
			}
		},
		
		-- First Strike
		firstStrike = {
			enabled = true,
			requiresAbility = "first_strike",
			validTargets = {
				"stormtrooper", "imperial_officer", "stormtrooper_sniper",
				"rebel_trooper", "rebel_officer", "rebel_specforce"
			},
			noXP = true,
			disguiseChance = 0.5,
			intelChance = 0.3,
			onlyInMilitaryZones = true
		}
	},
	
	-- KOS System
	kos = {
		imperial = {
			startsKOS = true, -- Imperials start as KOS
			description = "Imperial players are automatically marked as KOS (Kill on Sight)",
			agentDuty = {
				enabled = true,
				factionPointsCost = 500,
				duration = 1800, -- 30 minutes
				cooldown = 3600, -- 1 hour
				removesKOS = true,
				removesTEF = true,
				minFactionPoints = 500
			}
		},
		
		rebel = {
			startsStealth = true, -- Rebels start with stealth
			description = "Rebel players start with stealth active",
			losesStealthAtNegativeRep = true,
			stealthLossThreshold = 0, -- Below 0 faction standing
			regainStealthThreshold = 100, -- Need +100 to regain
			stealthCompromisedEffects = {
				visibleToAllImperials = true,
				appearsOnTerminals = true,
				losingStealthMessage = "Your stealth has been compromised! Imperial forces can now detect you."
			}
		}
	},
	
	-- Faction Point Requirements
	factionPoints = {
		joinFaction = 200,
		factionGuild = 400, -- Double the requirement
		rankPurchase = {
			private = 0,
			corporal = 500,
			sergeant = 1500,
			lieutenant = 3000,
			captain = 6000,
			major = 12000,
			colonel = 25000
		}
	},
	
	-- Military Zones
	militaryZones = {
		enabled = true,
		zones = {
			-- Imperial strongholds
			{ planet = "corellia", region = "imperial_outpost", radius = 500 },
			{ planet = "tatooine", region = "bestine", radius = 500 },
			{ planet = "naboo", region = "kadaara", radius = 500 },
			-- Rebel strongholds
			{ planet = "corellia", region = "vreni_island", radius = 500 },
			{ planet = "tatooine", region = "anchorhead", radius = 500 },
			{ planet = "naboo", region = "moenia", radius = 500 }
		},
		firstStrikeOnlyInZones = true,
		disguiseOnlyInZones = true
	},
	
	-- Messages
	messages = {
		imperialKOS = "You are marked as Kill on Sight by Rebel forces.",
		imperialAgentDutyStart = "Agent duty activated. KOS status suspended for 30 minutes.",
		imperialAgentDutyEnd = "Agent duty expired. KOS status restored.",
		rebelStealthActive = "Stealth active. Imperial forces cannot easily detect you.",
		rebelStealthCompromised = "Your stealth has been compromised! Imperial forces can now detect you.",
		rebelStealthRestored = "Your stealth has been restored.",
		firstStrikeSuccess = "First strike successful! Target eliminated silently.",
		firstStrikeFail = "First strike failed. Target detected you.",
		disguiseApplied = "You have donned a disguise.",
		disguiseRemoved = "Your disguise has worn off.",
		disguiseFailed = "Cannot apply disguise. Requirements not met."
	}
}