/*
 * ArmorSetResistSuiCallback.h
 *
 * Staff dev tool — sets a resistance value on an armor piece via SUI.
 * Step 1: SuiListBox  — player picks resistance type
 * Step 2: SuiInputBox — player enters float value (0.0 - 80.0)
 *
 * Radial IDs used:
 *   140 = "Staff: Set Resistance" (parent menu item, type=1)
 *   141 = triggered by step 1 list selection  (internally via SuiWindowType)
 *
 * Install path:
 *   server/zone/objects/player/sui/callbacks/ArmorSetResistSuiCallback.h
 */

#ifndef ARMORSETRESISTSUICALLBACK_H_
#define ARMORSETRESISTSUICALLBACK_H_

#include "server/zone/objects/player/sui/SuiCallback.h"
#include "server/zone/objects/player/sui/listbox/SuiListBox.h"
#include "server/zone/objects/player/sui/inputbox/SuiInputBox.h"
#include "server/zone/objects/tangible/wearables/ArmorObject.h"
#include "templates/tangible/SharedWeaponObjectTemplate.h"

// Step 2: receives the value string after type was chosen
class ArmorSetResistValueSuiCallback : public SuiCallback {
	int resistType;

public:
	ArmorSetResistValueSuiCallback(ZoneServer* serv, int type)
		: SuiCallback(serv), resistType(type) {}

	void run(CreatureObject* creature, SuiBox* sui, uint32 eventIndex, Vector<UnicodeString>* args) {
		bool cancelPressed = (eventIndex == 1);
		if (cancelPressed || !sui->isInputBox())
			return;

		if (args == nullptr || args->size() == 0)
			return;

		ManagedReference<ArmorObject*> armor = cast<ArmorObject*>(sui->getUsingObject().get().get());
		if (armor == nullptr)
			return;

		String inputStr = args->get(0).toString();
		float value = Float::valueOf(inputStr);

		if (value < 0.f)   value = 0.f;
		if (value > 80.f)  value = 80.f;

		// Set the chosen resistance type and mark it special so it shows in the
		// Special Protection section of the examine window
		armor->setProtectionValue(resistType, value);
		armor->specialResists |= resistType;

		StringBuffer msg;
		msg << "[Staff] Resistance set to " << value << "% on " << armor->getDisplayedName();
		creature->sendSystemMessage(msg.toString());
	}
};

// Step 1: player picks which resistance type to set
class ArmorSetResistSuiCallback : public SuiCallback {
public:
	ArmorSetResistSuiCallback(ZoneServer* serv) : SuiCallback(serv) {}

	void run(CreatureObject* creature, SuiBox* sui, uint32 eventIndex, Vector<UnicodeString>* args) {
		bool cancelPressed = (eventIndex == 1);
		if (cancelPressed || !sui->isListBox())
			return;

		if (args == nullptr || args->size() == 0)
			return;

		ManagedReference<ArmorObject*> armor = cast<ArmorObject*>(sui->getUsingObject().get().get());
		if (armor == nullptr)
			return;

		// Map list index -> resistance type constant
		static const int resistTypes[] = {
			SharedWeaponObjectTemplate::KINETIC,
			SharedWeaponObjectTemplate::ENERGY,
			SharedWeaponObjectTemplate::ELECTRICITY,
			SharedWeaponObjectTemplate::STUN,
			SharedWeaponObjectTemplate::BLAST,
			SharedWeaponObjectTemplate::HEAT,
			SharedWeaponObjectTemplate::COLD,
			SharedWeaponObjectTemplate::ACID,
			SharedWeaponObjectTemplate::LIGHTSABER
		};

		int selectedIndex = Integer::valueOf(args->get(0).toString());
		if (selectedIndex < 0 || selectedIndex > 8)
			return;

		int chosenType = resistTypes[selectedIndex];

		// Open value input box
		ManagedReference<SuiInputBox*> inputBox = new SuiInputBox(creature, SuiWindowType::OBJECT_NAME);
		inputBox->setPromptTitle("[Staff] Set Resistance Value");
		inputBox->setPromptText("Enter resistance value (0.0 - 80.0):");
		inputBox->setDefaultInput("50.0");
		inputBox->setUsingObject(armor);
		inputBox->setCallback(new ArmorSetResistValueSuiCallback(server, chosenType));

		creature->getPlayerObject()->addSuiBox(inputBox);
		creature->sendMessage(inputBox->generateMessage());
	}
};

#endif /* ARMORSETRESISTSUICALLBACK_H_ */
