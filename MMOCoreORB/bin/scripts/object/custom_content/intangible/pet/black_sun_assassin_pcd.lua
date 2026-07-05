-- black_sun_assassin_pcd: custom faction pet
-- Available to: both

black_sun_assassin_pcd = {
	customName = "",
	petTemplate = "object/mobile/black_sun_assassin.iff",
	controlDeviceTemplate = "object/intangible/pet/black_sun_assassin_pcd.iff",
	generationLimit = 1,
	petLevel = 75,
	attackAll = true,
	petSpecialAttack1 = "",
	petSpecialAttack2 = "",
}

-- Register the PCD
if (PetManager ~= nil) then
	PetManager:addPetTemplate("black_sun_assassin_pcd", black_sun_assassin_pcd)
end
