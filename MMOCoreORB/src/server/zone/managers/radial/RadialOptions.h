/*
 * RadialOptions.h
 *
 *  Created on: 10/04/2010
 *      Author: victor
 */

#ifndef RADIALOPTIONS_H_
#define RADIALOPTIONS_H_

class RadialOptions {
public:
	// Values below are indices into datatables/player/radial_menu.iff on the
	// client. NEVER renumber an existing constant -- only append new ones in
	// free slots.
	const static int VEHICLE_GENERATE = 60;
	const static int VEHICLE_STORE = 61;

	// --- Stock radial_menu.iff entries used by the ported Companion System ---
	// This (older) genesis base never declared these, but the imported Companion
	// sources reference them by name. Values are the canonical radial_menu.iff
	// indices derived from the Companion fork's sequential enum; that enum's
	// numbering was cross-checked against the two genesis constants above
	// (VEHICLE_GENERATE/VEHICLE_STORE land on 60/61 on both sides), so the two
	// numbering schemes are identical. Purely additive -- nothing above moved.
	const static int ITEM_ACTIVATE = 18;
	const static int PET_CALL = 44;
	const static int PET_STORE = 59;
	const static int SERVER_MENU1 = 68;
	const static int SERVER_MENU2 = 69;
	const static int SERVER_MENU3 = 70;
	const static int SERVER_MENU4 = 71;
	const static int SERVER_MENU5 = 72;
	const static int SERVER_MENU6 = 73;
	const static int SERVER_MENU7 = 74;
	const static int SERVER_MENU8 = 75;
	const static int SERVER_MENU9 = 76;
	const static int SERVER_MENU10 = 77;

	// --- Companion System additions (custom, not in stock radial_menu.iff) ---
	// EQUIP_DROID_ON_SHIP (233) is the last stock entry, so everything from 234
	// up is free on the client and cannot collide with a datatable row. These
	// keep the exact values the Companion fork's enum produced (234-247) so the
	// ported behaviour matches the fork it came from. APPEND ONLY -- inserting
	// mid-list would renumber later entries and silently break unrelated systems.
	// 2026-07-23: sub-option under Craft; opens CompanionArmorTypeSuiCallback's "Full Suit" picker (CompanionMenuComponent.cpp).
	const static int COMPANION_REQUEST_ARMOR = 234;
	// 2026-07-24: "Get Test Resources..." (CompanionCraftingManager::giveTestResourceBag()).
	const static int COMPANION_TEST_RESOURCES = 235;
	// 2026-07-27: Doctor Buff Radial -- "Medical: Buff Me".
	const static int COMPANION_DOCTOR_BUFF_ME = 236;
	// 2026-07-27: Doctor Buff Radial -- "Medical: Buff The Squad".
	const static int COMPANION_DOCTOR_BUFF_SQUAD = 237;
	// 2026-07-27: "Craft: Factory Run...".
	const static int COMPANION_CRAFT_BATCH = 238;
	// 2026-07-29: Entertainer Dance/Watch -- "Dance".
	const static int COMPANION_DANCE = 239;
	// 2026-07-29: Entertainer Dance/Watch -- "Stop Dance".
	const static int COMPANION_STOP_DANCE = 240;
	// 2026-07-29: Medic Heal Wounds -- "Medical: Heal Wounds".
	const static int COMPANION_HEAL_WOUNDS_ME = 241;
	// 2026-07-29: Medic Heal Wounds -- "Medical: Heal The Squad's Wounds".
	const static int COMPANION_HEAL_WOUNDS_SQUAD = 242;
	// 2026-07-29: Musician Play/Watch -- "Play Music".
	const static int COMPANION_PLAY_MUSIC = 243;
	// 2026-07-29: Musician Play/Watch -- "Stop Music".
	const static int COMPANION_STOP_MUSIC = 244;
	// 2026-07-29 night #3: Medic Stim Heal -- "Medical: Heal Me (Stims)".
	const static int COMPANION_STIM_HEAL_ME = 245;
	// 2026-07-29 night #3: Medic Stim Heal -- "Medical: Heal The Squad (Stims)".
	const static int COMPANION_STIM_HEAL_SQUAD = 246;
	// 2026-07-29: Jenkin's Cloner placed-prop radial, "Bind as My Personal Cloner" (JenkinsClonerMenuComponent.h).
	const static int JENKINS_CLONER_BIND = 247;
};

#endif /* RADIALOPTIONS_H_ */
