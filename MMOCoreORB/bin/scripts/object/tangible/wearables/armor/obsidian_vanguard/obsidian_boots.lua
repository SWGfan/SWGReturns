--Companion System (2026-07-20) -- OBSIDIAN VANGUARD custom armor set,
--phase 1 kitbash (see docs/companion_system/NOTES.md, armor research):
--NEW server template + stats wearing the ris family's stock client
--model (armor_ris_boots) -- zero TRE/client changes. Stats: the set's own "new
--type": heavy rating, broad 60-70% resists, lightsaber-only vulnerability.

object_tangible_wearables_armor_obsidian_vanguard_obsidian_boots = object_tangible_wearables_armor_ris_shared_armor_ris_boots:new {
	templateType = ARMOROBJECT,

	playerRaces = { "object/creature/player/bothan_male.iff",
				"object/creature/player/bothan_female.iff",
				"object/creature/player/human_male.iff",
				"object/creature/player/human_female.iff",
				"object/creature/player/moncal_male.iff",
				"object/creature/player/moncal_female.iff",
				"object/creature/player/rodian_male.iff",
				"object/creature/player/rodian_female.iff",
				"object/creature/player/sullustan_male.iff",
				"object/creature/player/sullustan_female.iff",
				"object/creature/player/trandoshan_male.iff",
				"object/creature/player/trandoshan_female.iff",
				"object/creature/player/twilek_male.iff",
				"object/creature/player/twilek_female.iff",
				"object/creature/player/zabrak_male.iff",
				"object/creature/player/zabrak_female.iff",
				"object/creature/player/wookiee_male.iff",
				"object/creature/player/wookiee_female.iff",
				"object/creature/player/ithorian_male.iff",
				"object/creature/player/ithorian_female.iff" },

	vulnerability = LIGHTSABER,

	healthEncumbrance = 3,
	actionEncumbrance = 3,
	mindEncumbrance = 3,

	rating = HEAVY,

	kinetic = 70,
	energy = 70,
	electricity = 60,
	stun = 60,
	blast = 60,
	heat = 60,
	cold = 60,
	acid = 60,
}
ObjectTemplates:addTemplate(object_tangible_wearables_armor_obsidian_vanguard_obsidian_boots, "object/tangible/wearables/armor/obsidian_vanguard/obsidian_boots.iff")
