-- AT-AT Walker Deployment System
-- Configuration and management for AT-AT/AT-ST walkers in GCW

atatWalkerManager = {
	WALKER_AT_AT = 1,
	WALKER_AT_ST = 2,

	STATE_DEPLOYING = 0,
	STATE_ADVANCING = 1,
	STATE_ASSAULTING = 2,
	STATE_RETREATING = 3,
	STATE_DESTROYED = 4,

	config = {
		at_at = {
			template = "at_at_walker",
			health = 500000,
			armor = 200,
			deployRange = 128,
			duration = 1800,
			factionPointsCost = 2000,
			cooldown = 3600,
			weapons = {
				main = { damage = 2000, range = 200, cooldown = 3, aoe = 5 },
				secondary = { damage = 5000, range = 300, cooldown = 10, aoe = 10 },
				anti_infantry = { damage = 500, range = 100, cooldown = 1, aoe = 3 }
			},
			troopCapacity = 40,
			deployTroops = {
				{ template = "stormtrooper", count = 8 },
				{ template = "stormtrooper_sniper", count = 2 },
				{ template = "stormtrooper_medic", count = 1 },
				{ template = "imperial_officer", count = 1 },
				{ template = "dark_trooper", count = 2 }
			},
			stopDifficulty = 0.8
		},
		at_st = {
			template = "at_st_walker",
			health = 150000,
			armor = 100,
			deployRange = 200,
			duration = 1200,
			factionPointsCost = 500,
			cooldown = 1800,
			weapons = {
				main = { damage = 800, range = 100, cooldown = 2 },
				secondary = { damage = 2000, range = 150, cooldown = 5 }
			},
			troopCapacity = 12,
			deployTroops = {
				{ template = "stormtrooper", count = 4 },
				{ template = "scout_trooper", count = 2 }
			}
		}
	},

	activeWalkers = {},

	deployATAT = function(self, deployerID, targetBaseID, deployX, deployZ, deployY)
		local deployer = getCreatureObject(deployerID)
		if deployer == nil then return false end

		local deployerFaction = CreatureObject(deployer):getFaction()
		if deployerFaction ~= 1 then return false end

		local fp = CreatureObject(deployer):getFactionStanding("imperial")
		if fp < self.config.at_at.factionPointsCost then return false end

		local walker = spawnMobile("generic", self.config.at_at.template, 1, deployX, deployZ, deployY, 0, 0)
		if walker == nil then return false end

		CreatureObject(deployer):deductFactionPoints("imperial", self.config.at_at.factionPointsCost)

		local walkerID = SceneObject(walker):getObjectID()

		self.activeWalkers[walkerID] = {
			walkerID = walkerID,
			targetBaseID = targetBaseID,
			deployerID = deployerID,
			type = self.WALKER_AT_AT,
			state = self.STATE_DEPLOYING,
			deployTime = getTimestamp(),
			expireTime = getTimestamp() + self.config.at_at.duration
		}

		writeData(self:getWalkerKey(walkerID) .. ":targetBase", targetBaseID)
		writeData(self:getWalkerKey(walkerID) .. ":deployer", deployerID)

		CreatureObject(deployer):sendSystemMessage("AT-AT Walker deployed!")

		createEvent(10000, "atatWalkerManager", "walkerDeployComplete", walker, "")

		return true
	end,

	walkerDeployComplete = function(self, walker, args)
		if walker == nil then return end
		local walkerID = SceneObject(walker):getObjectID()
		if self.activeWalkers[walkerID] then
			self.activeWalkers[walkerID].state = self.STATE_ADVANCING
		end
	end,

	attemptStopWalker = function(self, stopperID, walkerID)
		local stopper = getCreatureObject(stopperID)
		local walker = getCreatureObject(walkerID)

		if stopper == nil or walker == nil then return false end

		local config = self.config.at_at
		if math.random() < config.stopDifficulty then
			CreatureObject(stopper):sendSystemMessage("Failed to stop the AT-AT!")
			return false
		end

		if self.activeWalkers[walkerID] then
			self.activeWalkers[walkerID].state = self.STATE_RETREATING
		end

		CreatureObject(stopper):sendSystemMessage("Successfully stopped the AT-AT!")
		return true
	end,

	deployTroops = function(self, walkerID)
		local walker = getCreatureObject(walkerID)
		if walker == nil then return end

		local config = self.config.at_at
		local wx = SceneObject(walker):getPositionX()
		local wz = SceneObject(walker):getPositionZ()
		local wy = SceneObject(walker):getPositionY()

		for _, troopGroup in ipairs(config.deployTroops) do
			for i = 1, troopGroup.count do
				local x = wx + math.random(-5, 5)
				local z = wz + math.random(-5, 5)
				spawnMobile("imperial", troopGroup.template, 1, x, z, wy, 0, 0)
			end
		end
	end,

	expireWalker = function(self, walkerID)
		local walker = getSceneObject(walkerID)
		if walker ~= nil then
			SceneObject(walker):destroyObjectFromWorld()
		end
		self.activeWalkers[walkerID] = nil
	end,

	getWalkerKey = function(self, walkerID)
		return "atatWalker:" .. walkerID
	end,

	update = function(self)
		local now = getTimestamp()
		for walkerID, data in pairs(self.activeWalkers) do
			if now > data.expireTime then
				self:expireWalker(walkerID)
			end
		end
	end
}
