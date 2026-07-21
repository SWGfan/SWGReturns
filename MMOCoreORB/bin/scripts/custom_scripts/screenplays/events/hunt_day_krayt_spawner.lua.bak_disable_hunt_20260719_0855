HuntDayKraytSpawner = ScreenPlay:new {
	numberOfActs = 1,
	enabled = true,
	interval = 10 * 60 * 1000,
	mobileTemplate = "krayt_dragon_ancient",
	respawnTimer = 10 * 60,
	minOffset = 12,
	maxOffset = 22,

	trackedToons = {
		{ name = "Jin", oid = 281474996418332 },
		{ name = "Xix Lightning", oid = 281474998373141 },
		{ name = "Xix Lightning", oid = 281474994232084 },
		{ name = "Jim", oid = 281474994232155 },
	}
}

registerScreenPlay("HuntDayKraytSpawner", true)

function HuntDayKraytSpawner:start()
	if not self.enabled then
		return
	end

	createEvent(60 * 1000, "HuntDayKraytSpawner", "spawnWave", nil, "")
end

function HuntDayKraytSpawner:spawnWave()
	if not self.enabled then
		return
	end

	local pPlayer = self:getFirstOnlineTrackedToon()

	if pPlayer ~= nil then
		self:spawnPairNearPlayer(pPlayer)
	end

	createEvent(self.interval, "HuntDayKraytSpawner", "spawnWave", nil, "")
end

function HuntDayKraytSpawner:getFirstOnlineTrackedToon()
	for i = 1, #self.trackedToons do
		local pPlayer = getCreatureObject(self.trackedToons[i].oid)

		if pPlayer ~= nil then
			local player = LuaCreatureObject(pPlayer)

			if player:getZoneName() ~= "" and not player:isDead() and not player:isIncapacitated() then
				return pPlayer
			end
		end
	end

	return nil
end

function HuntDayKraytSpawner:spawnPairNearPlayer(pPlayer)
	local player = LuaCreatureObject(pPlayer)
	local zoneName = player:getZoneName()

	if zoneName == "" or zoneName == "space_corellia" or zoneName == "space_dantooine" or zoneName == "space_dathomir" or zoneName == "space_endor" or zoneName == "space_lok" or zoneName == "space_naboo" or zoneName == "space_tatooine" or zoneName == "space_yavin4" then
		return
	end

	-- Use world position and parent 0 so the event does not spawn krayts inside buildings.
	local x = player:getWorldPositionX()
	local z = player:getWorldPositionZ()
	local y = player:getWorldPositionY()
	local heading = player:getDirectionAngle()

	for i = 1, 2 do
		local offset = getRandomNumber(self.minOffset, self.maxOffset)
		local side = 1

		if i == 2 then
			side = -1
		end

		local spawnX = x + (offset * side)
		local spawnY = y + getRandomNumber(-6, 6)
		local pKrayt = spawnMobile(zoneName, self.mobileTemplate, self.respawnTimer, spawnX, z, spawnY, heading, 0)

		if pKrayt ~= nil then
			local krayt = LuaCreatureObject(pKrayt)
			krayt:setCustomObjectName("Hunt Day Ancient Krayt")
			krayt:engageCombat(pPlayer)
		end
	end
end
