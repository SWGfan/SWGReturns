-- child_of_the_watch_pcd: custom faction pet
-- Available to: both

child_of_the_watch_pcd = {
	customName = "",
	petTemplate = "object/mobile/death_watch_soldier.iff",
	controlDeviceTemplate = "object/intangible/pet/child_of_the_watch_pcd.iff",
	generationLimit = 1,
	petLevel = 75,
	attackAll = true,
	petSpecialAttack1 = "",
	petSpecialAttack2 = "",
}

-- Register the PCD
if (PetManager ~= nil) then
	PetManager:addPetTemplate("child_of_the_watch_pcd", child_of_the_watch_pcd)
end
