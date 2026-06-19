/*
 * WeaponDotControlSuiCallback.h
 *
 * Staff dev tool — adds or removes DOT effects on a weapon via SUI.
 *
 * Flow for Add DOT:
 *   Step 1: SuiListBox  — pick DOT type (Poison/Disease/Fire/Bleeding)
 *   Step 2: SuiListBox  — pick HAM pool (Health/Action/Mind)
 *   Step 3: SuiInputBox — enter strength,duration,potency,uses  (comma separated)
 *
 * Flow for Remove DOTs:
 *   Immediate — calls weapon->clearDots() with confirmation message
 *
 * Radial IDs used (on WeaponObject):
 *   150 = "Staff: DOT Controls"    (parent, type=1)
 *   151 = "Add DOT"                (child of 150, type=3)
 *   152 = "Remove All DOTs"        (child of 150, type=3)
 *
 * Install path:
 *   server/zone/objects/player/sui/callbacks/WeaponDotControlSuiCallback.h
 *
 * Also used from LightsaberCrystalComponentImplementation for crystal radial.
 */

#ifndef WEAPONDOTCONTROLSUICALLBACK_H_
#define WEAPONDOTCONTROLSUICALLBACK_H_

#include "server/zone/objects/player/sui/SuiCallback.h"
#include "server/zone/objects/player/sui/listbox/SuiListBox.h"
#include "server/zone/objects/player/sui/inputbox/SuiInputBox.h"
#include "server/zone/objects/tangible/weapon/WeaponObject.h"

// Step 3: receives "strength,duration,potency,uses" and applies DOT
class WeaponDotApplySuiCallback : public SuiCallback {
	int dotType;
	int hamPool; // 0=Health, 3=Action, 6=Mind

public:
	WeaponDotApplySuiCallback(ZoneServer* serv, int type, int pool)
		: SuiCallback(serv), dotType(type), hamPool(pool) {}

	void run(CreatureObject* creature, SuiBox* sui, uint32 eventIndex, Vector<UnicodeString>* args) {
		bool cancelPressed = (eventIndex == 1);
		if (cancelPressed || !sui->isInputBox())
			return;

		if (args == nullptr || args->size() == 0)
			return;

		ManagedReference<WeaponObject*> weapon = cast<WeaponObject*>(sui->getUsingObject().get().get());
		if (weapon == nullptr)
			return;

		// Parse "strength,duration,potency,uses"
		StringTokenizer tokenizer(args->get(0).toString());
		tokenizer.setDelimeter(",");

		int strength = 1500, duration = 800, potency = 60, uses = 50000;
		if (tokenizer.hasMoreTokens()) strength  = tokenizer.getIntToken();
		if (tokenizer.hasMoreTokens()) duration  = tokenizer.getIntToken();
		if (tokenizer.hasMoreTokens()) potency   = tokenizer.getIntToken();
		if (tokenizer.hasMoreTokens()) uses      = tokenizer.getIntToken();

		// Clamp values to sane ranges
		if (strength < 1)    strength = 1;
		if (duration < 1)    duration = 1;
		if (potency < 1)     potency  = 1;
		if (potency > 100)   potency  = 100;
		if (uses < 1)        uses     = 1;

		weapon->clearDots();
		weapon->addDotType(dotType);
		weapon->addDotAttribute(hamPool);
		weapon->addDotStrength(strength);
		weapon->addDotDuration(duration);
		weapon->addDotPotency(potency);
		weapon->addDotUses(uses);

		static const char* dotNames[] = {"", "Poison", "Disease", "Fire", "Bleeding"};
		static const char* hamNames[] = {"Health", "", "", "Action", "", "", "Mind"};

		StringBuffer msg;
		msg << "[Staff] DOT applied: " << dotNames[dotType]
		    << " | Pool: " << hamNames[hamPool]
		    << " | Str: " << strength
		    << " | Dur: " << duration
		    << "s | Pot: " << potency
		    << "% | Uses: " << uses;
		creature->sendSystemMessage(msg.toString());
	}
};

// Step 2: pick HAM pool, then open value input
class WeaponDotPickPoolSuiCallback : public SuiCallback {
	int dotType;

public:
	WeaponDotPickPoolSuiCallback(ZoneServer* serv, int type)
		: SuiCallback(serv), dotType(type) {}

	void run(CreatureObject* creature, SuiBox* sui, uint32 eventIndex, Vector<UnicodeString>* args) {
		bool cancelPressed = (eventIndex == 1);
		if (cancelPressed || !sui->isListBox())
			return;

		if (args == nullptr || args->size() == 0)
			return;

		ManagedReference<WeaponObject*> weapon = cast<WeaponObject*>(sui->getUsingObject().get().get());
		if (weapon == nullptr)
			return;

		static const int hamPools[] = {0, 3, 6}; // Health, Action, Mind
		int selectedIndex = Integer::valueOf(args->get(0).toString());
		if (selectedIndex < 0 || selectedIndex > 2)
			return;

		int chosenPool = hamPools[selectedIndex];

		// Open stats input box
		ManagedReference<SuiInputBox*> inputBox = new SuiInputBox(creature, SuiWindowType::OBJECT_NAME);
		inputBox->setPromptTitle("[Staff] DOT Stats");
		inputBox->setPromptText("Enter: strength,duration,potency,uses\nExample: 2000,1200,75,50000");
		inputBox->setDefaultInput("2000,1200,75,50000");
		inputBox->setUsingObject(weapon);
		inputBox->setCallback(new WeaponDotApplySuiCallback(server, dotType, chosenPool));

		creature->getPlayerObject()->addSuiBox(inputBox);
		creature->sendMessage(inputBox->generateMessage());
	}
};

// Step 1: pick DOT type
class WeaponDotControlSuiCallback : public SuiCallback {
public:
	WeaponDotControlSuiCallback(ZoneServer* serv) : SuiCallback(serv) {}

	void run(CreatureObject* creature, SuiBox* sui, uint32 eventIndex, Vector<UnicodeString>* args) {
		bool cancelPressed = (eventIndex == 1);
		if (cancelPressed || !sui->isListBox())
			return;

		if (args == nullptr || args->size() == 0)
			return;

		ManagedReference<WeaponObject*> weapon = cast<WeaponObject*>(sui->getUsingObject().get().get());
		if (weapon == nullptr)
			return;

		// DOT types: 1=Poison, 2=Disease, 3=Fire, 4=Bleeding
		static const int dotTypes[] = {1, 2, 3, 4};
		int selectedIndex = Integer::valueOf(args->get(0).toString());
		if (selectedIndex < 0 || selectedIndex > 3)
			return;

		int chosenType = dotTypes[selectedIndex];

		// Open HAM pool picker
		ManagedReference<SuiListBox*> listBox = new SuiListBox(creature, SuiWindowType::OBJECT_NAME);
		listBox->setPromptTitle("[Staff] DOT - Pick HAM Pool");
		listBox->setPromptText("Which HAM pool does this DOT drain?");
		listBox->addMenuItem("Health");
		listBox->addMenuItem("Action");
		listBox->addMenuItem("Mind");
		listBox->setUsingObject(weapon);
		listBox->setCallback(new WeaponDotPickPoolSuiCallback(server, chosenType));

		creature->getPlayerObject()->addSuiBox(listBox);
		creature->sendMessage(listBox->generateMessage());
	}
};

#endif /* WEAPONDOTCONTROLSUICALLBACK_H_ */
