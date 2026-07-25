-- GCW Stealth/Detection Configuration and Functions
-- Detection levels, KOS rules, stealth mechanics

gcwStealthConfig = {
	detection = {
		visionConeAngle = 120,
		baseDetectionRange = 64,
		rangePerLevel = 32,
		visionConeEnabled = true,
		nightVisionPenalty = 0.5,
		visibilityLevels = {
			[0] = { name = "Still", description = "Not moving, maximum stealth" },
			[1] = { name = "Prone", description = "Lying flat, nearly invisible" },
			[2] = { name = "Kneeling", description = "Crouched, reduced visibility" },
			[3] = { name = "Standing", description = "Fully visible" }
		},
		soundLevels = {
			[0] = { name = "Still", description = "Silent" },
			[1] = { name = "Crawling", description = "Almost silent movement" },
			[2] = { name = "Walking", description = "Moderate noise" },
			[3] = { name = "Running", description = "Loud, easily heard" }
		},
		aiAlertStates = {
			normal = {
				name = "Normal",
				visThreshold = 1,
				sndThreshold = 1,
				rangeMultiplier = 1.0,
				behavior = "patrol"
			},
			cautious = {
				name = "Cautious",
				visThreshold = 2,
				sndThreshold = 2,
				rangeMultiplier = 1.5,
				behavior = "investigate"
			},
			alerted = {
				name = "Alerted",
				visThreshold = 3,
				sndThreshold = 3,
				rangeMultiplier = 2.0,
				behavior = "combat"
			}
		},
		environmentModifiers = {
			night = {
				enabled = true,
				visionPenalty = 0.5,
				soundPenalty = 0.8,
				nightHours = { 20, 5 }
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
		disguise = {
			detectionReduction = 0.9,
			onlyInGCWZones = true,
			breaksOnCombat = true,
			breaksOnZoneExit = true,
			validSources = {
				imperial = { "imperial_officer", "stormtrooper", "imperial_private" },
				rebel = { "rebel_trooper", "rebel_officer", "rebel_specforce" }
			}
		},
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
	kos = {
		imperial = {
			startsKOS = true,
			description = "Imperial players are automatically marked as KOS",
			agentDuty = {
				enabled = true,
				factionPointsCost = 500,
				duration = 1800,
				cooldown = 3600,
				removesKOS = true,
				removesTEF = true,
				minFactionPoints = 500
			}
		},
		rebel = {
			startsStealth = true,
			description = "Rebel players start with stealth active",
			losesStealthAtNegativeRep = true,
			stealthLossThreshold = 0,
			regainStealthThreshold = 100,
			stealthCompromisedEffects = {
				visibleToAllImperials = true,
				appearsOnTerminals = true,
				losingStealthMessage = "Your stealth has been compromised! Imperial forces can now detect you."
			}
		}
	},
	factionPoints = {
		joinFaction = 200,
		factionGuild = 400,
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
	militaryZones = {
		enabled = true,
		zones = {
			{ planet = "corellia", region = "imperial_outpost", radius = 500, x = 0, z = 0 },
			{ planet = "tatooine", region = "bestine", radius = 500, x = 0, z = 0 },
			{ planet = "naboo", region = "kadaara", radius = 500, x = 0, z = 0 },
			{ planet = "corellia", region = "vreni_island", radius = 500, x = 0, z = 0 },
			{ planet = "tatooine", region = "anchorhead", radius = 500, x = 0, z = 0 },
			{ planet = "naboo", region = "moenia", radius = 500, x = 0, z = 0 }
		},
		firstStrikeOnlyInZones = true,
		disguiseOnlyInZones = true
	},
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

-- Helper: get player posture-based visibility level (0-3)
function getPlayerVisibilityLevel(player)
	if player == nil then return 3 end
	local p = PlayerObject(player:getPlayerObjectID())
	if p == nil then return 3 end
	local posture = p:getPostureState()
	if posture == 5 then return 1
	elseif posture == 4 then return 2
	else return 3 end
end

-- Helper: get player movement-based sound level (0-3)
function getPlayerSoundLevel(player)
	if player == nil then return 3 end
	local movementCounter = player:getMovementCounter()
	if movementCounter > 3 then return 3
	elseif movementCounter > 1 then return 2
	elseif movementCounter > 0 then return 1
	else return 0 end
end

-- Helper: calculate distance between two points
local function getDistanceBetween(pos1, pos2)
	local dx = pos1.x - pos2.x
	local dz = pos1.z - pos2.z
	return math.sqrt(dx * dx + dz * dz)
end

-- Helper: check if point is in vision cone
function isInVisionCone(ai, player)
	if ai == nil or player == nil then return true end
	local aiYaw = ai:getDirectionAngle()
	local dx = SceneObject(player):getPositionX() - SceneObject(ai):getPositionX()
	local dz = SceneObject(player):getPositionZ() - SceneObject(ai):getPositionZ()
	local angleToPlayer = math.atan2(dx, dz) * 180 / math.pi

	while angleToPlayer < 0 do angleToPlayer = angleToPlayer + 360 end
	while angleToPlayer >= 360 do angleToPlayer = angleToPlayer - 360 end
	while aiYaw < 0 do aiYaw = aiYaw + 360 end
	while aiYaw >= 360 do aiYaw = aiYaw - 360 end

	local diff = math.abs(angleToPlayer - aiYaw)
	if diff > 180 then diff = 360 - diff end

	return diff <= (gcwStealthConfig.detection.visionConeAngle / 2)
end

-- Helper: get environment modifier (night, weather, terrain)
function getEnvironmentModifier(position)
	if position == nil then return 1.0 end
	local hour = tonumber(os.date("%H"))
	local isNight = hour >= 20 or hour <= 5
	local mod = 1.0
	if isNight then mod = mod * gcwStealthConfig.detection.nightVisionPenalty end
	return mod
end

-- Helper: check if player is in a GCW military zone
function isInGCWZone(player)
	if player == nil then return false end
	local zone = SceneObject(player):getZoneName()
	local px = SceneObject(player):getPositionX()
	local pz = SceneObject(player):getPositionZ()

	for _, mz in ipairs(gcwStealthConfig.militaryZones.zones) do
		if zone == mz.planet then
			local dist = getDistanceBetween(
				{ x = px, z = pz },
				{ x = mz.x, z = mz.z }
			)
			if dist <= mz.radius then
				return true
			end
		end
	end
	return false
end

-- AI Detection: calculate detection score (0.0 = undetected, 1.0 = fully detected)
function calculateAIDetection(ai, player)
	if ai == nil or player == nil then return 0 end

	local visLevel = getPlayerVisibilityLevel(player)
	local sndLevel = getPlayerSoundLevel(player)

	local aiPos = { x = SceneObject(ai):getPositionX(), z = SceneObject(ai):getPositionZ() }
	local plPos = { x = SceneObject(player):getPositionX(), z = SceneObject(player):getPositionZ() }
	local dist = getDistanceBetween(aiPos, plPos)

	if dist > gcwStealthConfig.detection.baseDetectionRange then
		return 0
	end

	if gcwStealthConfig.detection.visionConeEnabled then
		if not isInVisionCone(ai, player) then
			if sndLevel < 2 then return 0 end
		end
	end

	local falloff = math.max(0, 1 - (dist / gcwStealthConfig.detection.rangePerLevel))
	if falloff <= 0 then return 0 end

	local envMod = getEnvironmentModifier(aiPos)
	falloff = falloff * envMod

	local effVis = math.max(0, visLevel - math.floor(visLevel * falloff))
	local effSnd = math.max(0, math.floor(sndLevel * falloff))

	local stateName = ai:getCustomVariable("gcw_alert_state")
	if stateName == nil then stateName = "normal" end
	local stateConfig = gcwStealthConfig.detection.aiAlertStates[stateName]
	if stateConfig == nil then stateConfig = gcwStealthConfig.detection.aiAlertStates.normal end

	if effVis >= stateConfig.visThreshold or effSnd >= stateConfig.sndThreshold then
		local detection = (effVis * 0.6 + effSnd * 0.4) * falloff
		return math.min(1.0, detection * stateConfig.rangeMultiplier)
	end

	return 0
end

-- Imperial Agent Duty System
imperialAgentDuty = {
	startDuty = function(self, player)
		if player == nil then return false end
		if CreatureObject(player):getFaction() ~= 1 then return false end

		local fp = CreatureObject(player):getFactionStanding("imperial")
		if fp < 500 then return false end

		local lastDuty = readScreenPlayData(player, "gcwStealth", "agentDutyLast")
		if lastDuty ~= nil and lastDuty ~= "" then
			local lastTime = tonumber(lastDuty)
			if lastTime and (getTimestamp() - lastTime) < 3600 then
				return false
			end
		end

		CreatureObject(player):deductFactionPoints("imperial", 500)
		writeScreenPlayData(player, "gcwStealth", "agentDutyActive", "1")
		writeScreenPlayData(player, "gcwStealth", "agentDutyEnd", tostring(getTimestamp() + 1800))
		writeScreenPlayData(player, "gcwStealth", "agentDutyLast", tostring(getTimestamp()))
		writeScreenPlayData(player, "gcwStealth", "kosSuspended", "1")

		CreatureObject(player):sendSystemMessage(gcwStealthConfig.messages.imperialAgentDutyStart)

		createEvent(1800000, "gcwStealth", "endDuty", player, "")

		return true
	end,

	endDuty = function(self, player, args)
		if player == nil then return end

		writeScreenPlayData(player, "gcwStealth", "agentDutyActive", "0")
		deleteScreenPlayData(player, "gcwStealth", "agentDutyEnd")
		deleteScreenPlayData(player, "gcwStealth", "kosSuspended")

		CreatureObject(player):sendSystemMessage(gcwStealthConfig.messages.imperialAgentDutyEnd)
	end,

	isDutyActive = function(self, player)
		if player == nil then return false end
		local val = readScreenPlayData(player, "gcwStealth", "agentDutyActive")
		return val == "1"
	end
}

-- Rebel Stealth Check
rebelStealth = {
	isStealthActive = function(self, player)
		if player == nil then return false end
		if CreatureObject(player):getFaction() ~= 2 then return false end

		local standing = CreatureObject(player):getFactionStanding("rebel")
		if standing < 0 then return false end

		local compromised = readScreenPlayData(player, "gcwStealth", "stealthCompromised")
		if compromised == "1" then return false end

		return true
	end,

	compromiseStealth = function(self, player, reason)
		if player == nil then return end
		writeScreenPlayData(player, "gcwStealth", "stealthCompromised", "1")
		writeScreenPlayData(player, "gcwStealth", "stealthReason", reason or "unknown")
		CreatureObject(player):sendSystemMessage("Your stealth has been compromised: " .. (reason or "unknown"))
	end,

	restoreStealth = function(self, player)
		if player == nil then return end
		if CreatureObject(player):getFactionStanding("rebel") >= 100 then
			deleteScreenPlayData(player, "gcwStealth", "stealthCompromised")
			deleteScreenPlayData(player, "gcwStealth", "stealthReason")
			CreatureObject(player):sendSystemMessage(gcwStealthConfig.messages.rebelStealthRestored)
		end
	end
}

-- Disguise System
disguiseSystem = {
	isDisguised = function(self, player)
		if player == nil then return false end
		local val = readScreenPlayData(player, "gcwStealth", "disguised")
		return val == "1"
	end,

	applyDisguise = function(self, player, npcTemplate)
		if player == nil or npcTemplate == nil then return false end
		if self:isDisguised(player) then return false end
		if isInGCWZone(player) == false then return false end

		writeScreenPlayData(player, "gcwStealth", "originalTemplate", SceneObject(player):getTemplateObjectPath())
		writeScreenPlayData(player, "gcwStealth", "disguiseTemplate", npcTemplate)
		writeScreenPlayData(player, "gcwStealth", "disguised", "1")

		CreatureObject(player):sendSystemMessage(gcwStealthConfig.messages.disguiseApplied)
		return true
	end,

	removeDisguise = function(self, player)
		if player == nil then return end
		if not self:isDisguised(player) then return end

		deleteScreenPlayData(player, "gcwStealth", "originalTemplate")
		deleteScreenPlayData(player, "gcwStealth", "disguiseTemplate")
		deleteScreenPlayData(player, "gcwStealth", "disguised")

		CreatureObject(player):sendSystemMessage(gcwStealthConfig.messages.disguiseRemoved)
	end,

	checkDisguiseDetection = function(self, disguisedPlayer, observer)
		if disguisedPlayer == nil or observer == nil then return 0 end
		local baseDetection = calculateAIDetection(observer, disguisedPlayer)
		return baseDetection * 0.1
	end
}
