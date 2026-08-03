/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System -- see CompanionBagContainerComponent.h and NOTES.md.

	resolveCompanion() is copied verbatim from CompanionContainerComponent.cpp's
	own anonymous-namespace helper (this project's established convention is
	per-file duplication of small local helpers rather than sharing across
	component .cpp files -- see e.g. the identical resolveActiveCompanion()
	duplicated across the various companion command files).
*/

#include "CompanionBagContainerComponent.h"
#include "server/zone/objects/scene/SceneObject.h"
#include "server/zone/objects/scene/variables/ContainerPermissions.h"
#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/companion/CompanionObject.h"

namespace {

	// Walks up the container hierarchy to find the CompanionObject that owns
	// this container slot set (works whether sceneObject IS the companion, or
	// is a nested bag/slot container hanging off it -- exactly this bag's
	// case).
	CompanionObject* resolveCompanion(SceneObject* sceneObject) {
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

bool CompanionBagContainerComponent::checkContainerPermission(SceneObject* sceneObject, CreatureObject* creature, uint16 permission) const {
	CompanionObject* companion = resolveCompanion(sceneObject);

	if (companion == nullptr || creature == nullptr) {
		return false;
	}

	if (!companion->isAuthorizedActor(creature)) {
		// Spec 2B: "If the actor performing the transaction is not the owner,
		// immediately drop the network packet and log an exploitation attempt."
		Logger::console.warning("CompanionSystem: rejected bag access attempt on companion "
				+ String::valueOf(companion->getObjectID()) + " by non-owner actor "
				+ String::valueOf(creature->getObjectID()) + " (permission=" + String::valueOf(permission) + ")");

		return false;
	}

	return permission == ContainerPermissions::MOVEIN || permission == ContainerPermissions::MOVEOUT || permission == ContainerPermissions::OPEN;
}
