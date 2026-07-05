/*
 * WearableObjectMenuComponent.cpp
 *
 *  Created on: 10/30/2011
 *      Author: kyle
 */

#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/objects/player/sui/colorbox/SuiColorBox.h"
#include "server/zone/objects/player/sui/callbacks/ColorArmorSuiCallback.h"
#include "server/zone/ZoneServer.h"
#include "WearableObjectMenuComponent.h"
#include "server/zone/packets/object/ObjectMenuResponse.h"
#include "templates/customization/AssetCustomizationManagerTemplate.h"

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

	menuResponse->addRadialMenuItem(71, 3, "Color Options"); // Slice


	menuResponse->addRadialMenuItemToRadialID(71, 72, 3, "Color 1 (Primary)");
	menuResponse->addRadialMenuItemToRadialID(71, 73, 3, "Color 2 (Secondary)");
}

int WearableObjectMenuComponent::handleObjectMenuSelect(SceneObject* sceneObject, CreatureObject* player, byte selectedID) const {
	if (!sceneObject->isASubChildOf(player))
		return 0;

	if (selectedID == 70) {
		if(!sceneObject->isTangibleObject())
			return 0;

		TangibleObject* tano = cast<TangibleObject*>(sceneObject);
		if(tano == nullptr)
			return 0;

		tano->repair(player);

		return 1;
	}
	else if (selectedID == 72 || selectedID == 73)
	{
		// The color index.
		String appearanceFilename = sceneObject->getObjectTemplate()->getAppearanceFilename();
		VectorMap<String, Reference<CustomizationVariable*> > variables;
		AssetCustomizationManagerTemplate::instance()->getCustomizationVariables(appearanceFilename.hashCode(), variables, false);

		ZoneServer* server = player->getZoneServer();

		// The Sui Box.
		ManagedReference<SuiColorBox*> cbox = new SuiColorBox(player, SuiWindowType::COLOR_ARMOR);
		cbox->setCallback(new ColorArmorSuiCallback(server));

		int colorIndex = 0;
		if(selectedID == 73)
		{
			if(variables.size() - 1 > 1)
			{
				colorIndex = 1;
			}
			else
			{
				return 0;
			}
		}
		else if (selectedID == 74)
		{
			if(variables.size() - 1 > 2)
			{
				colorIndex = 2;
			}
			else
			{
				return 0;
			}
		}

		cbox->setColorPalette(variables.elementAt(colorIndex).getKey()); // First one seems to be the frame of it? Skip to 2nd.
		cbox->setUsingObject(sceneObject);

		// Add to player.
		ManagedReference<PlayerObject*> ghost = player->getPlayerObject();
		ghost->addSuiBox(cbox);
		player->sendMessage(cbox->generateMessage());

	}

	return TangibleObjectMenuComponent::handleObjectMenuSelect(sceneObject, player, selectedID);
}
