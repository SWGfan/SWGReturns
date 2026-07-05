-- death_watch_wraith_pcd: custom faction pet
-- Available to: both

death_watch_wraith_pcd = {
	customName = "",
	petTemplate = "object/mobile/death_watch_defender.iff",
	controlDeviceTemplate = "object/intangible/pet/death_watch_wraith_pcd.iff",
	generationLimit = 1,
	petLevel = 75,
	attackAll = true,
	petSpecialAttack1 = "",
	petSpecialAttack2 = "",
}

-- Register the PCD
if (PetManager ~= nil) then
	PetManager:addPetTemplate("death_watch_wraith_pcd", death_watch_wraith_pcd)
end
