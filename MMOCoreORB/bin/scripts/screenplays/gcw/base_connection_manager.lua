--[[
	GCWBaseConnectionManager.lua
	PvE <-> PvP Base Connection System
	Relay Stations (Imperial) / Rebel Cells (Rebel) grant perks to linked PvP bases

	Base discovery is event-driven: when a base is placed via faction deed,
	the placer calls registerPvPBase() or registerPvEBase() to register it.
	Object IDs are tracked via shared memory.
]]

gcwBaseConnectionManager = {
	SCREENPLAY_NAME = "gcwBaseConnectionManager",

	IMPERIAL_RELAY = "imperial_relay",
	REBEL_CELL = "rebel_cell",

	PERK_REINFORCEMENTS = "reinforcements",
	PERK_ARTILLERY = "artillery",
	PERK_RADAR_JAMMER = "radar_jammer",
	PERK_SUPPLY_DROP = "supply_drop",
	PERK_EVACUATION = "evacuation",
	PERK_RELOCATION = "relocation",

	STATE_CONNECTED = 1,
	STATE_DISCOVERED = 2,
	STATE_COMPROMISED = 3,
	STATE_DESTROYED = 4,

	COMPROMISED_DURATION = 1800,
	RELAY_DISABLE_DURATION = 900,
	EVACUATION_TIMER = 600,
	RELOCATION_TIMER = 10800,
	DISCOVERY_DURATION = 3600,

	connections = {},
	pveBases = {},
	pvpBases = {},
	activeCompromises = {},

	init = function(self)
		self:loadRegisteredBases()
		createEvent(30000, self.SCREENPLAY_NAME, "checkConnections", "", "")
		createEvent(60000, self.SCREENPLAY_NAME, "updateCompromises", "", "")
	end,

	loadRegisteredBases = function(self)
		local count = readData(self.SCREENPLAY_NAME .. ":pvpCount") or 0
		for i = 1, count do
			local baseID = readData(self.SCREENPLAY_NAME .. ":pvp_" .. i)
			if baseID and baseID > 0 then
				local pObj = getSceneObject(baseID)
				if pObj ~= nil then
					self:registerPvPBase(pObj)
				end
			end
		end

		local pveCount = readData(self.SCREENPLAY_NAME .. ":pveCount") or 0
		for i = 1, pveCount do
			local baseID = readData(self.SCREENPLAY_NAME .. ":pve_" .. i)
			if baseID and baseID > 0 then
				local pObj = getSceneObject(baseID)
				if pObj ~= nil then
					self:registerPvEBase(pObj)
				end
			end
		end
	end,

	registerPvPBase = function(self, baseObject)
		local baseID = SceneObject(baseObject):getObjectID()
		local faction = self:getBaseFaction(baseObject)

		self.pvpBases[baseID] = {
			id = baseID,
			faction = faction,
			connectedPvE = nil,
			perks = {},
			state = self.STATE_CONNECTED,
			location = {
				x = SceneObject(baseObject):getPositionX(),
				y = SceneObject(baseObject):getPositionY(),
				z = SceneObject(baseObject):getPositionZ()
			}
		}
	end,

	registerPvEBase = function(self, baseObject)
		local baseID = SceneObject(baseObject):getObjectID()
		local faction = self:getBaseFaction(baseObject)
		local baseType = (faction == 1) and self.IMPERIAL_RELAY or self.REBEL_CELL

		self.pveBases[baseID] = {
			id = baseID,
			faction = faction,
			type = baseType,
			connectedPvP = nil,
			state = self.STATE_CONNECTED,
			location = {
				x = SceneObject(baseObject):getPositionX(),
				y = SceneObject(baseObject):getPositionY(),
				z = SceneObject(baseObject):getPositionZ()
			},
			lastCompromised = 0
		}
	end,

	storePvPBase = function(self, baseObject)
		local baseID = SceneObject(baseObject):getObjectID()
		local count = readData(self.SCREENPLAY_NAME .. ":pvpCount") or 0
		count = count + 1
		writeData(self.SCREENPLAY_NAME .. ":pvpCount", count)
		writeData(self.SCREENPLAY_NAME .. ":pvp_" .. count, baseID)
		self:registerPvPBase(baseObject)
	end,

	storePvEBase = function(self, baseObject)
		local baseID = SceneObject(baseObject):getObjectID()
		local count = readData(self.SCREENPLAY_NAME .. ":pveCount") or 0
		count = count + 1
		writeData(self.SCREENPLAY_NAME .. ":pveCount", count)
		writeData(self.SCREENPLAY_NAME .. ":pve_" .. count, baseID)
		self:registerPvEBase(baseObject)
	end,

	connectBases = function(self, pveBaseID, pvpBaseID)
		local pveBase = self.pveBases[pveBaseID]
		local pvpBase = self.pvpBases[pvpBaseID]

		if pveBase == nil or pvpBase == nil then
			return false
		end

		if pveBase.faction ~= pvpBase.faction then
			return false
		end

		if pveBase.connectedPvP ~= nil then
			return false
		end

		if pvpBase.connectedPvE ~= nil then
			return false
		end

		local dist = self:getDistance(pveBase.location, pvpBase.location)
		if dist < 500 then
			return false
		end

		pveBase.connectedPvP = pvpBaseID
		pvpBase.connectedPvE = pveBaseID
		pveBase.state = self.STATE_CONNECTED
		pvpBase.state = self.STATE_CONNECTED

		self:grantPerks(pvpBaseID, pveBase.type)

		self.connections[pveBaseID] = {
			pveID = pveBaseID,
			pvpID = pvpBaseID,
			faction = pveBase.faction,
			created = getTimestamp()
		}

		return true
	end,

	grantPerks = function(self, pvpBaseID, pveType)
		local pvpBase = self.pvpBases[pvpBaseID]
		if pvpBase == nil then return end

		local perks = {}

		if pveType == self.IMPERIAL_RELAY then
			perks[self.PERK_REINFORCEMENTS] = { active = true, level = 1 }
			perks[self.PERK_ARTILLERY] = { active = true, level = 1 }
			perks[self.PERK_RADAR_JAMMER] = { active = false, level = 0 }
		elseif pveType == self.REBEL_CELL then
			perks[self.PERK_EVACUATION] = { active = true, level = 1 }
			perks[self.PERK_RELOCATION] = { active = true, level = 1 }
			perks[self.PERK_SUPPLY_DROP] = { active = true, level = 1 }
		end

		pvpBase.perks = perks
	end,

	compromisePvEBase = function(self, pveBaseID, compromiserID)
		local pveBase = self.pveBases[pveBaseID]
		if pveBase == nil then return false end

		if pveBase.state == self.STATE_COMPROMISED or pveBase.state == self.STATE_DESTROYED then
			return false
		end

		pveBase.state = self.STATE_COMPROMISED
		pveBase.lastCompromised = getTimestamp()
		pveBase.compromisedBy = compromiserID

		if pveBase.connectedPvP ~= nil then
			self:disablePerks(pveBase.connectedPvP)
		end

		createEvent(self.COMPROMISED_DURATION * 1000, self.SCREENPLAY_NAME, "recoverPvEBase", "", tostring(pveBaseID))

		return true
	end,

	recoverPvEBase = function(self, pveBaseID)
		local id = tonumber(pveBaseID)
		if id == nil then return end

		local pveBase = self.pveBases[id]
		if pveBase == nil or pveBase.state ~= self.STATE_COMPROMISED then return end

		pveBase.state = self.STATE_CONNECTED
		pveBase.compromisedBy = nil

		if pveBase.connectedPvP ~= nil then
			self:grantPerks(pveBase.connectedPvP, pveBase.type)
		end
	end,

	disablePerks = function(self, pvpBaseID)
		local pvpBase = self.pvpBases[pvpBaseID]
		if pvpBase == nil then return end

		for perkName, perkData in pairs(pvpBase.perks) do
			perkData.active = false
		end
	end,

	destroyPvEBase = function(self, pveBaseID, destroyerID)
		local pveBase = self.pveBases[pveBaseID]
		if pveBase == nil then return false end

		pveBase.state = self.STATE_DESTROYED

		if pveBase.connectedPvP ~= nil then
			local pvpBase = self.pvpBases[pveBase.connectedPvP]
			if pvpBase ~= nil then
				pvpBase.connectedPvE = nil
			end
		end

		self.pveBases[pveBaseID] = nil
		self.connections[pveBaseID] = nil
		return true
	end,

	discoverRebelBase = function(self, rebelPvPBaseID, discovererID)
		local pvpBase = self.pvpBases[rebelPvPBaseID]
		if pvpBase == nil or pvpBase.faction ~= 2 then return false end

		if pvpBase.state == self.STATE_DISCOVERED then return false end

		pvpBase.state = self.STATE_DISCOVERED
		pvpBase.discoveredBy = discovererID
		pvpBase.discoveryTime = getTimestamp()

		createEvent(self.DISCOVERY_DURATION * 1000, self.SCREENPLAY_NAME, "expireDiscovery", "", tostring(rebelPvPBaseID))

		return true
	end,

	expireDiscovery = function(self, pvpBaseID)
		local id = tonumber(pvpBaseID)
		if id == nil then return end

		local pvpBase = self.pvpBases[id]
		if pvpBase == nil then return end

		pvpBase.state = self.STATE_CONNECTED
		pvpBase.discoveredBy = nil
		pvpBase.discoveryTime = nil
	end,

	getBaseFaction = function(self, baseObject)
		local templateName = SceneObject(baseObject):getTemplateObjectPath()
		if templateName == nil then return 0 end

		if string.find(templateName, "imp") or string.find(templateName, "imperial") then
			return 1
		elseif string.find(templateName, "rebel") then
			return 2
		end
		return 0
	end,

	getDistance = function(self, loc1, loc2)
		local dx = loc1.x - loc2.x
		local dz = loc1.z - loc2.z
		return math.sqrt(dx * dx + dz * dz)
	end,

	checkConnections = function(self)
		for pveID, pveBase in pairs(self.pveBases) do
			if pveBase.state == self.STATE_COMPROMISED then
				local elapsed = getTimestamp() - pveBase.lastCompromised
				if elapsed > self.COMPROMISED_DURATION then
					self:recoverPvEBase(pveID)
				end
			end
		end

		for pvpID, pvpBase in pairs(self.pvpBases) do
			if pvpBase.state == self.STATE_DISCOVERED then
				local elapsed = getTimestamp() - (pvpBase.discoveryTime or 0)
				if elapsed > self.DISCOVERY_DURATION then
					self:expireDiscovery(pvpID)
				end
			end
		end
	end,

	updateCompromises = function(self)
	end
}

gcwBaseConnectionManager:init()
