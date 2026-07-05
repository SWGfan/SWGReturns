-- nightsister_elder_pcd: custom faction pet
-- Available to: neutral

nightsister_elder_pcd = {
	customName = "",
	petTemplate = "object/mobile/nightsister_elder.iff",
	controlDeviceTemplate = "object/intangible/pet/nightsister_elder_pcd.iff",
	generationLimit = 1,
	petLevel = 75,
	attackAll = true,
	petSpecialAttack1 = "",
	petSpecialAttack2 = "",
}

-- Register the PCD
if (PetManager ~= nil) then
	PetManager:addPetTemplate("nightsister_elder_pcd", nightsister_elder_pcd)
end
