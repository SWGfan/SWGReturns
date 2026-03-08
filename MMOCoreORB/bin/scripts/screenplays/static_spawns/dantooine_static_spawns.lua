--This is to be used for static spawns that are NOT part of caves, cities, dungeons, poi's, or other large screenplays.
DantooineStaticSpawnsScreenPlay = ScreenPlay:new
{
	numberOfActs = 1,

	screenplayName = "DantooineStaticSpawnsScreenPlay",
}

registerScreenPlay("DantooineStaticSpawnsScreenPlay", true)


-- Safety wrapper to prevent nil/empty template spawns from causing CRC 0x0 spam
local function _trim(s)
	if type(s) ~= "string" then return s end
	return s:match("^%s*(.-)%s*$")
end

local function safeSpawnMobile(zone, template, respawn, x, y, z, heading, cell)
	template = _trim(template)
	if template == nil or template == "" then
		Logger:error("[safeSpawnMobile] invalid template zone=" .. tostring(zone) .. " template=" .. tostring(template))
		return nil
	end
	local ok, result = pcall(spawnMobile, zone, template, respawn, x, y, z, heading, cell)
	if not ok then
		Logger:error("[safeSpawnMobile] spawnMobile threw zone=" .. tostring(zone) .. " template=" .. tostring(template) .. " err=" .. tostring(result))
		return nil
	end
	if result == nil then
		Logger:error("[safeSpawnMobile] spawnMobile failed zone=" .. tostring(zone) .. " template=" .. tostring(template))
	end
	return result
end

function DantooineStaticSpawnsScreenPlay:start()
	if (isZoneEnabled("dantooine")) then
		self:spawnMobiles()
	end
end

function DantooineStaticSpawnsScreenPlay:spawnMobiles()

	--ancient abandoned Force Shrine (900, 1400)
	local pNpc = safeSpawnMobile("dantooine", "force_crystal_hunter", 7200,902.4,19.8,1395.4,-114,0)
	self:setMoodString(pNpc, "angry")
	pNpc = safeSpawnMobile("dantooine", "force_crystal_hunter", 7200,892.3,21.2,1390.4,-176,0)
	self:setMoodString(pNpc, "angry")

	--old tower with green fire
	pNpc = safeSpawnMobile("dantooine", "force_sensitive_renegade", 7200,-924.1,7.5,6917.9,72,0)
	self:setMoodString(pNpc, "angry")
	pNpc = safeSpawnMobile("dantooine", "force_sensitive_renegade", 7200,-914.8,7.8,6904.4,155,0)
	self:setMoodString(pNpc, "angry")

	-- Vexed Voritor Lizard Spawn (-5500 -1800)
	safeSpawnMobile("dantooine", "vexed_voritor_lizard", 300, getRandomNumber(5) + -5486, 15,getRandomNumber(5) + -1770, getRandomNumber(360), 0)
	safeSpawnMobile("dantooine", "vexed_voritor_lizard", 300, getRandomNumber(5) + -5486, 15,getRandomNumber(5) + -1770, getRandomNumber(360), 0)
	safeSpawnMobile("dantooine", "vexed_voritor_lizard", 300, getRandomNumber(5) + -5486, 15,getRandomNumber(5) + -1770, getRandomNumber(360), 0)
  --safeSpawnMobile("dantooine", "vvl", getRandomNumber(10) * 60000, getRandomNumber(16400) + -8200, 0, getRandomNumber(16400) + -8200, getRandomNumber(360), 0)


	safeSpawnMobile("dantooine", "dark_jedi_master", 3600, -738.2, 1.7, 2103.9, 55, 0)

	--Need to add the rest of static spawns (Incomplete).
end