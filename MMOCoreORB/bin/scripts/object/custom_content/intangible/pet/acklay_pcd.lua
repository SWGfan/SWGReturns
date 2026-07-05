-- acklay_pcd: custom faction pet
-- Available to: neutral

acklay_pcd = {
	customName = "",
	petTemplate = "object/mobile/acklay.iff",
	controlDeviceTemplate = "object/intangible/pet/acklay_pcd.iff",
	generationLimit = 1,
	petLevel = 75,
	attackAll = true,
	petSpecialAttack1 = "",
	petSpecialAttack2 = "",
}

-- Register the PCD
if (PetManager ~= nil) then
	PetManager:addPetTemplate("acklay_pcd", acklay_pcd)
end
