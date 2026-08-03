/*
 * WearableObjectMenuComponent.cpp
 *
 *  Created on: 10/30/2011
 *      Author: kyle
 */

#include "server/zone/objects/creature/CreatureObject.h"
#include "WearableObjectMenuComponent.h"
#include "server/zone/packets/object/ObjectMenuResponse.h"
#include "server/zone/objects/companion/CompanionObject.h"

namespace {
	// Companion System (2026-07-15, "no option to wear armor" fix) -- same
	// walk-up helper as TangibleObjectMenuComponent.cpp's (per-file copy,
	// project convention).
	CompanionObject* resolveWearableCompanionAncestor(SceneObject* sceneObject) {
		SceneObject* current = sceneObject;

		for (int i = 0; i < 8 && current != nullptr; ++i) {
			if (current->isCompanionObject()) {
				return cast<CompanionObject*>(current);
			}

			current = current->getParent().get();
		}

		return nullptr;
	}
}

void WearableObjectMenuComponent::fillObjectMenuResponse(SceneObject* sceneObject, ObjectMenuResponse* menuResponse, CreatureObject* player) const {
	if (!sceneObject->isTangibleObject())
		return;

	TangibleObject* tano = cast<TangibleObject*>(sceneObject);
	if (tano == nullptr)
		return;

	if (tano->getConditionDamage() > 0 && tano->canRepair(player)) {
		menuResponse->addRadialMenuItem(70, 3, "@sui:repair"); // Slice
	}

	TangibleObjectMenuComponent::fillObjectMenuResponse(sceneObject, menuResponse, player);

}

int WearableObjectMenuComponent::handleObjectMenuSelect(SceneObject* sceneObject, CreatureObject* player, byte selectedID) const {
	if (!sceneObject->isASubChildOf(player)) {
		// Companion System (2026-07-15, "no option to wear armor" fix --
		// see NOTES.md): wearables/armor held by the player's OWN companion
		// aren't subchildren of the player, so this gate silently swallowed
		// every radial click on them -- including "Equip on Companion" (82)
		// and "Pick Up" (83), which are handled further down the chain in
		// TangibleObjectMenuComponent. Companion-held items are click-eligible
		// for their owner.
		CompanionObject* companion = resolveWearableCompanionAncestor(sceneObject->getParent().get());

		if (companion == nullptr || !companion->isAuthorizedActor(player)) {
			return 0;
		}
	}

	if (selectedID == 70) {
		if(!sceneObject->isTangibleObject())
			return 0;

		TangibleObject* tano = cast<TangibleObject*>(sceneObject);
		if(tano == nullptr)
			return 0;

		tano->repair(player);

		return 1;
	}

	return TangibleObjectMenuComponent::handleObjectMenuSelect(sceneObject, player, selectedID);
}
