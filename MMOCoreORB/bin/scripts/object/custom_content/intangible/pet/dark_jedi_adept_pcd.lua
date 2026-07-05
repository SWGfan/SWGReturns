-- dark_jedi_adept_pcd: custom faction pet
-- Available to: both

dark_jedi_adept_pcd = {
	customName = "",
	petTemplate = "object/mobile/dark_jedi_adept.iff",
	controlDeviceTemplate = "object/intangible/pet/dark_jedi_adept_pcd.iff",
	generationLimit = 1,
	petLevel = 75,
	attackAll = true,
	petSpecialAttack1 = "",
	petSpecialAttack2 = "",
}

-- Register the PCD
if (PetManager ~= nil) then
	PetManager:addPetTemplate("dark_jedi_adept_pcd", dark_jedi_adept_pcd)
end
