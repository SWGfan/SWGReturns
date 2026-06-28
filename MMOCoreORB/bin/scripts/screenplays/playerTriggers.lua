PlayerTriggers = { }

local jediLightDarkConflictGroups = {
	{
		light = {
			"jedi_light_side_journeyman_master",
			"jedi_light_side_journeyman_saber_04",
			"jedi_light_side_journeyman_saber_03",
			"jedi_light_side_journeyman_saber_02",
			"jedi_light_side_journeyman_saber_01",
			"jedi_light_side_journeyman_healing_04",
			"jedi_light_side_journeyman_healing_03",
			"jedi_light_side_journeyman_healing_02",
			"jedi_light_side_journeyman_healing_01",
			"jedi_light_side_journeyman_force_power_04",
			"jedi_light_side_journeyman_force_power_03",
			"jedi_light_side_journeyman_force_power_02",
			"jedi_light_side_journeyman_force_power_01",
			"jedi_light_side_journeyman_force_manipulation_04",
			"jedi_light_side_journeyman_force_manipulation_03",
			"jedi_light_side_journeyman_force_manipulation_02",
			"jedi_light_side_journeyman_force_manipulation_01",
			"jedi_light_side_journeyman_novice"
		},
		dark = {
			"jedi_dark_side_journeyman_master",
			"jedi_dark_side_journeyman_saber_04",
			"jedi_dark_side_journeyman_saber_03",
			"jedi_dark_side_journeyman_saber_02",
			"jedi_dark_side_journeyman_saber_01",
			"jedi_dark_side_journeyman_healing_04",
			"jedi_dark_side_journeyman_healing_03",
			"jedi_dark_side_journeyman_healing_02",
			"jedi_dark_side_journeyman_healing_01",
			"jedi_dark_side_journeyman_force_power_04",
			"jedi_dark_side_journeyman_force_power_03",
			"jedi_dark_side_journeyman_force_power_02",
			"jedi_dark_side_journeyman_force_power_01",
			"jedi_dark_side_journeyman_force_manipulation_04",
			"jedi_dark_side_journeyman_force_manipulation_03",
			"jedi_dark_side_journeyman_force_manipulation_02",
			"jedi_dark_side_journeyman_force_manipulation_01",
			"jedi_dark_side_journeyman_novice"
		}
	},
	{
		light = {
			"jedi_light_side_master_master",
			"jedi_light_side_master_saber_04",
			"jedi_light_side_master_saber_03",
			"jedi_light_side_master_saber_02",
			"jedi_light_side_master_saber_01",
			"jedi_light_side_master_healing_04",
			"jedi_light_side_master_healing_03",
			"jedi_light_side_master_healing_02",
			"jedi_light_side_master_healing_01",
			"jedi_light_side_master_force_power_04",
			"jedi_light_side_master_force_power_03",
			"jedi_light_side_master_force_power_02",
			"jedi_light_side_master_force_power_01",
			"jedi_light_side_master_force_manipulation_04",
			"jedi_light_side_master_force_manipulation_03",
			"jedi_light_side_master_force_manipulation_02",
			"jedi_light_side_master_force_manipulation_01",
			"jedi_light_side_master_novice"
		},
		dark = {
			"jedi_dark_side_master_master",
			"jedi_dark_side_master_saber_04",
			"jedi_dark_side_master_saber_03",
			"jedi_dark_side_master_saber_02",
			"jedi_dark_side_master_saber_01",
			"jedi_dark_side_master_healing_04",
			"jedi_dark_side_master_healing_03",
			"jedi_dark_side_master_healing_02",
			"jedi_dark_side_master_healing_01",
			"jedi_dark_side_master_force_power_04",
			"jedi_dark_side_master_force_power_03",
			"jedi_dark_side_master_force_power_02",
			"jedi_dark_side_master_force_power_01",
			"jedi_dark_side_master_force_manipulation_04",
			"jedi_dark_side_master_force_manipulation_03",
			"jedi_dark_side_master_force_manipulation_02",
			"jedi_dark_side_master_force_manipulation_01",
			"jedi_dark_side_master_novice"
		}
	},
	{
		light = {
			"returns_jedi_elder_light_master",
			"returns_jedi_elder_light_lightsaber_04",
			"returns_jedi_elder_light_lightsaber_03",
			"returns_jedi_elder_light_lightsaber_02",
			"returns_jedi_elder_light_lightsaber_01",
			"returns_jedi_elder_light_healing_04",
			"returns_jedi_elder_light_healing_03",
			"returns_jedi_elder_light_healing_02",
			"returns_jedi_elder_light_healing_01",
			"returns_jedi_elder_light_enhancer_04",
			"returns_jedi_elder_light_enhancer_03",
			"returns_jedi_elder_light_enhancer_02",
			"returns_jedi_elder_light_enhancer_01",
			"returns_jedi_elder_light_powers_04",
			"returns_jedi_elder_light_powers_03",
			"returns_jedi_elder_light_powers_02",
			"returns_jedi_elder_light_powers_01",
			"returns_jedi_elder_light_novice",
			"returns_jedi_elder_light"
		},
		dark = {
			"returns_jedi_elder_dark_master",
			"returns_jedi_elder_dark_lightsaber_04",
			"returns_jedi_elder_dark_lightsaber_03",
			"returns_jedi_elder_dark_lightsaber_02",
			"returns_jedi_elder_dark_lightsaber_01",
			"returns_jedi_elder_dark_healing_04",
			"returns_jedi_elder_dark_healing_03",
			"returns_jedi_elder_dark_healing_02",
			"returns_jedi_elder_dark_healing_01",
			"returns_jedi_elder_dark_enhancer_04",
			"returns_jedi_elder_dark_enhancer_03",
			"returns_jedi_elder_dark_enhancer_02",
			"returns_jedi_elder_dark_enhancer_01",
			"returns_jedi_elder_dark_powers_04",
			"returns_jedi_elder_dark_powers_03",
			"returns_jedi_elder_dark_powers_02",
			"returns_jedi_elder_dark_powers_01",
			"returns_jedi_elder_dark_novice",
			"returns_jedi_elder_dark"
		}
	}
}

local function getTrainedSkillCount(player, skills)
	local count = 0

	for i = 1, #skills, 1 do
		if (player:hasSkill(skills[i])) then
			count = count + 1
		end
	end

	return count
end

local function surrenderSkills(pPlayer, skills)
	for i = 1, #skills, 1 do
		surrenderSkill(pPlayer, skills[i])
	end
end

function PlayerTriggers:cleanupJediLightDarkPreclusions(pPlayer)
	if (readScreenPlayData(pPlayer, "JediPreclusionCleanup", "lightDarkCleanupV1") == "1") then
		return
	end

	local player = CreatureObject(pPlayer)

	for i = 1, #jediLightDarkConflictGroups, 1 do
		local group = jediLightDarkConflictGroups[i]
		local lightCount = getTrainedSkillCount(player, group.light)
		local darkCount = getTrainedSkillCount(player, group.dark)

		if (lightCount > 0 and darkCount > 0) then
			if (darkCount > lightCount) then
				surrenderSkills(pPlayer, group.light)
			else
				surrenderSkills(pPlayer, group.dark)
			end
		end
	end

	writeScreenPlayData(pPlayer, "JediPreclusionCleanup", "lightDarkCleanupV1", 1)
end

function PlayerTriggers:playerLoggedIn(pPlayer)
	if (pPlayer == nil) then
		return
	end


	self:cleanupJediLightDarkPreclusions(pPlayer)

	ServerEventAutomation:playerLoggedIn(pPlayer)
	BestineElection:playerLoggedIn(pPlayer)
end
