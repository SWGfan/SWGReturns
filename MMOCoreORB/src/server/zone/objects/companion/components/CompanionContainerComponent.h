/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System -- spec 2B ("Container Security Hook"). Modeled on
	server/zone/objects/tangible/components/droid/DroidDatapadContainerComponent.h,
	the closest existing analogue (an owner-restricted equipment/inventory
	container hanging off a control device).

	2026-07-12 -- Auto-Equip: base class changed from ContainerComponent to
	PlayerContainerComponent so canAddObject()/notifyObjectInserted() reuse
	the exact same validated "Wear" logic (race*, faction, armor
	encumbrance, wearable skill certification, jedi lightsaber ownership,
	skill-mod application, wearables-vector tracking) a real player gets --
	instead of a hand-rolled duplicate. (*race/species checking is a no-op
	for companions specifically -- see the comment on
	PlayerContainerComponent::canAddObject(). See docs/companion_system/
	NOTES.md, "Companion Auto-Equip", for the full writeup, including why
	canAddObject/notifyObjectInserted needed their own overrides here on top
	of the inherited logic (the companion has no separate "inventory" bag
	child object the way a player does -- its own containerObjects list
	*is* its loose inventory, which the inherited SLOTTED containerType
	would otherwise reject outright).
*/

#ifndef COMPANIONCONTAINERCOMPONENT_H_
#define COMPANIONCONTAINERCOMPONENT_H_

#include "server/zone/objects/player/components/PlayerContainerComponent.h"

class CompanionContainerComponent : public PlayerContainerComponent {
public:

	bool checkContainerPermission(SceneObject* sceneObject, CreatureObject* creature, uint16 permission) const;

	int canAddObject(SceneObject* sceneObject, SceneObject* object, int containmentType, String& errorDescription) const;

	int notifyObjectInserted(SceneObject* sceneObject, SceneObject* object) const;

	int notifyObjectRemoved(SceneObject* sceneObject, SceneObject* object, SceneObject* destination) const;

};

#endif /* COMPANIONCONTAINERCOMPONENT_H_ */
