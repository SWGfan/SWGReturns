/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-14, second half of the PLAYERUSEMASKERROR bug
	-- see CompanionLoadoutContainerComponent.h and NOTES.md). This is the
	container component for the companion's own separate "inventory" bag
	child object (object/tangible/inventory/companion_inventory.iff,
	created at spawn time in CompanionControlDeviceImplementation.cpp, or
	migrated in-place onto pre-existing bags via setContainerComponent()).

	Bug fix: this bag was assigned CompanionContainerComponent earlier
	today (2026-07-14, replacing the original LootContainerComponent to fix
	the "You can not loot that" corpse-only-container bug). That swap
	traded one failure mode for another: CompanionContainerComponent
	extends PlayerContainerComponent, and its own canAddObject() override
	(CompanionContainerComponent.cpp:344-368) only special-cases the
	companion object itself (sceneObject->isCompanionObject()) -- for any
	OTHER sceneObject (like this bag, a plain TangibleObject hanging off
	the companion), it falls straight through to the inherited
	PlayerContainerComponent::canAddObject(), which unconditionally
	requires dynamic_cast<CreatureObject*>(sceneObject) to succeed on the
	destination. A bag is never a CreatureObject, so every insert into this
	bag was silently rejected with PLAYERUSEMASKERROR (errorNumber: 2) --
	the exact same bug class as the loadout backpack, just one level
	deeper in the container hierarchy and easy to miss on the first pass.

	Fixed by giving the bag its own dedicated component instead: plain
	ContainerComponent for canAddObject()/notifyObjectInserted/Removed
	(ordinary VOLUME-container behavior, no CreatureObject requirement),
	but with checkContainerPermission() still overridden to require real
	companion ownership -- copied from CompanionContainerComponent's own
	resolveCompanion()-based owner check, which already worked correctly
	for a nested child object (it walks UP the parent chain looking for
	the owning CompanionObject, so it never needed sceneObject itself to
	BE the companion). Without this override, this bag would fall back to
	ContainerComponent's default checkContainerPermission() (open/permissive
	for any actor), letting any player loot another player's companion's
	bag -- a real regression from the ownership gating spec 2B requires.
*/

#ifndef COMPANIONBAGCONTAINERCOMPONENT_H_
#define COMPANIONBAGCONTAINERCOMPONENT_H_

#include "server/zone/objects/scene/components/ContainerComponent.h"

class CompanionBagContainerComponent : public ContainerComponent {
public:

	bool checkContainerPermission(SceneObject* sceneObject, CreatureObject* creature, uint16 permission) const;

};

#endif /* COMPANIONBAGCONTAINERCOMPONENT_H_ */
