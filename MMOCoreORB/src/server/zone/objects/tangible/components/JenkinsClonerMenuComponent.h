/*
 * JenkinsClonerMenuComponent.h
 *
 * "Jenkin's Cloner" -- a house-placeable personal clone point (2026-07-29,
 * Nick's feature ask). This is the radial handler for the PLACED PROP: it
 * offers "Bind as My Personal Cloner" and writes this object's ID onto the
 * clicking player's own ghost state via PlayerObject::setPersonalCloner()
 * (see patch_jenkins_cloner_ghost_field.py). The actual death/respawn hook
 * lives in PlayerManagerImplementation.cpp (sendActivateCloneRequest() /
 * sendPlayerToCloner(), see patch_jenkins_cloner_clone_hook.py).
 *
 * Header-only by design (all methods defined inline below) -- registered by
 * ComponentManager.cpp the same way every other MenuComponent is, via
 * components.put("JenkinsClonerMenuComponent", new JenkinsClonerMenuComponent()).
 * No new .cpp file, so no cmake reconfigure is required for this file.
 *
 * Mirrors the real, already-working bind precedent exactly:
 * CloningTerminalMenuComponent.cpp's selectedID==20 handler
 * (ghost->setCloningFacility(buildingObject)) -- same shape, but against the
 * new personalCloner field and a plain TangibleObject prop instead of a
 * BuildingObject.
 */

#ifndef JENKINSCLONERMENUCOMPONENT_H_
#define JENKINSCLONERMENUCOMPONENT_H_

#include "TangibleObjectMenuComponent.h"
#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/objects/scene/SceneObject.h"
#include "server/zone/objects/cell/CellObject.h"
#include "server/zone/packets/object/ObjectMenuResponse.h"
#include "server/zone/managers/radial/RadialOptions.h"

class JenkinsClonerMenuComponent : public TangibleObjectMenuComponent {
public:

	inline void fillObjectMenuResponse(SceneObject* sceneObject, ObjectMenuResponse* menuResponse, CreatureObject* player) const {
		if (!sceneObject->isTangibleObject())
			return;

		menuResponse->addRadialMenuItem(RadialOptions::JENKINS_CLONER_BIND, 3, "Bind as My Personal Cloner");

		TangibleObjectMenuComponent::fillObjectMenuResponse(sceneObject, menuResponse, player);
	}

	inline int handleObjectMenuSelect(SceneObject* sceneObject, CreatureObject* player, byte selectedID) const {
		if (selectedID != RadialOptions::JENKINS_CLONER_BIND)
			return TangibleObjectMenuComponent::handleObjectMenuSelect(sceneObject, player, selectedID);

		if (sceneObject == nullptr || !sceneObject->isTangibleObject() || player == nullptr || !player->isPlayerCreature())
			return 0;

		// Must actually be PLACED as furniture (directly parented to a
		// CellObject) -- refuse binding a copy still sitting in inventory or
		// on the ground in the open world, matching Nick's "place in his
		// house" intent.
		ManagedReference<SceneObject*> parent = sceneObject->getParent().get();

		if (parent == nullptr || !parent->isCellObject()) {
			player->sendSystemMessage("Jenkin's Cloner must be placed inside a building before it can be bound.");
			return 0;
		}

		ManagedReference<PlayerObject*> ghost = player->getPlayerObject();

		if (ghost == nullptr)
			return 0;

		if (ghost->getPersonalCloner() == sceneObject->getObjectID()) {
			player->sendSystemMessage("Your clone data is already stored at this Jenkin's Cloner.");
			return 0;
		}

		ghost->setPersonalCloner(sceneObject);
		player->sendSystemMessage("You have bound your clone data to this Jenkin's Cloner. You may now clone here from any planet.");

		return 0;
	}
};

#endif /* JENKINSCLONERMENUCOMPONENT_H_ */
