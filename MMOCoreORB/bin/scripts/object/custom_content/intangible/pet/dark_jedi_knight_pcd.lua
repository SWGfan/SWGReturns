-- dark_jedi_knight_pcd: custom faction pet
-- Available to: both

dark_jedi_knight_pcd = {
	customName = "",
	petTemplate = "object/mobile/dark_jedi_knight.iff",
	controlDeviceTemplate = "object/intangible/pet/dark_jedi_knight_pcd.iff",
	generationLimit = 1,
	petLevel = 75,
	attackAll = true,
	petSpecialAttack1 = "",
	petSpecialAttack2 = "",
}

-- Register the PCD
if (PetManager ~= nil) then
	PetManager:addPetTemplate("dark_jedi_knight_pcd", dark_jedi_knight_pcd)
end
