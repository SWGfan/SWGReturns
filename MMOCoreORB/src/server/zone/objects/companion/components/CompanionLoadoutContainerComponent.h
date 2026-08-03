/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-14, "player-side loadout backpack" redesign --
	see NOTES.md). This is the container component for the new "Companion
	Loadout" backpack, a real, ordinary VOLUME container that lives inside
	the PLAYER'S OWN inventory (created once, at companion-recruitment time,
	in SkillManager.cpp's companion_master_novice grant block) -- not a
	companion-side object at all. Dropping a weapon/wearable into this
	backpack auto-equips it onto the player's currently active companion;
	if that companion already has something equipped in the matching slot,
	the old item is displaced into the player's own main inventory (not
	back into this backpack, to avoid an insert/re-equip loop) rather than
	silently left unequipped -- a deliberate policy difference from
	CompanionContainerComponent's attemptAutoEquip(), which never
	force-swaps, since a drop into this dedicated backpack is an explicit
	"equip this" action by the player, not an incidental loose-item landing.

	Bug fix (2026-07-14, live in-game repro -- repeated log spam
	"cannot add objectToTransfer to destinationObject: errorNumber: 2"
	(PLAYERUSEMASKERROR) with this backpack's own object ID as the
	destination): base class was originally PlayerContainerComponent,
	on the mistaken assumption that it was safe/generic for any nested
	bag inside a player's inventory, matching CompanionContainerComponent's
	own use of it. That assumption was wrong --
	PlayerContainerComponent::canAddObject() (PlayerContainerComponent.cpp:
	18-22) unconditionally requires `dynamic_cast<CreatureObject*>(sceneObject)`
	to succeed on the DESTINATION container itself, i.e. it's designed to be
	attached directly to a CreatureObject's own top-level SLOTTED equip
	container (which is exactly what CompanionContainerComponent governs --
	the companion IS a CreatureObject) -- not a plain nested bag object like
	this backpack, which is a TangibleObject, not a CreatureObject, so that
	cast always fails and every single insert was unconditionally rejected.
	Confirmed the real player's own "inventory" bag (character_inventory.lua)
	doesn't set a containerComponent override at all -- it just uses the
	generic, unmodified ContainerComponent every ordinary nested bag uses,
	with no player/wear-specific gating at the bag level at all (that
	gating correctly happens later, at the actual equip-onto-companion step,
	via CompanionContainerComponent on the companion itself, which really is
	a CreatureObject). Fixed by rebasing on plain ContainerComponent instead
	-- notifyObjectInserted() was already written calling
	ContainerComponent::notifyObjectInserted() as its own base case (not
	PlayerContainerComponent's), so no other code needed to change.
*/

#ifndef COMPANIONLOADOUTCONTAINERCOMPONENT_H_
#define COMPANIONLOADOUTCONTAINERCOMPONENT_H_

#include "server/zone/objects/scene/components/ContainerComponent.h"

class CompanionLoadoutContainerComponent : public ContainerComponent {
public:

	int notifyObjectInserted(SceneObject* sceneObject, SceneObject* object) const;

};

#endif /* COMPANIONLOADOUTCONTAINERCOMPONENT_H_ */
