/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System -- spec 2B ("Inventory, Public Inspection, & Container
	Security"). Modeled on server/zone/objects/creature/components/PetMenuComponent.h,
	the equivalent hook used by the (isolated) Creature Handler pet system.

	Radial IDs: rather than adding brand-new entries to RadialOptions.h (which
	is explicitly commented "Do not modify this list, it matches
	datatables/player/radial_menu.iff" -- i.e. it must stay byte-identical to a
	compiled client datatable), this component reuses the four
	RadialOptions::SERVER_MENU1-4 slots that Core3 reserves specifically for
	custom, server-defined actions with fully custom STF labels. See
	docs/companion_system/NOTES.md for details.
*/

#ifndef COMPANIONMENUCOMPONENT_H_
#define COMPANIONMENUCOMPONENT_H_

#include "server/zone/objects/tangible/components/TangibleObjectMenuComponent.h"

class CompanionMenuComponent : public TangibleObjectMenuComponent {
public:

	virtual void fillObjectMenuResponse(SceneObject* sceneObject, ObjectMenuResponse* menuResponse, CreatureObject* player) const;

	virtual int handleObjectMenuSelect(SceneObject* sceneObject, CreatureObject* player, byte selectedID) const;

};

#endif /* COMPANIONMENUCOMPONENT_H_ */
