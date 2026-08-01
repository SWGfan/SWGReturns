/*
 * DetectorZoneComponent.h
 *
 *  Created on: Dec 17, 2012
 *      Author: root
 */

#ifndef DETECTORZONECOMPONENT_H_
#define DETECTORZONECOMPONENT_H_

#include "engine/engine.h"
#include "server/zone/objects/scene/SceneObject.h"
#include "server/zone/objects/scene/components/GroundZoneComponent.h"
#include "server/zone/TreeEntry.h"


class DetectorZoneComponent : public GroundZoneComponent {

public:
	void notifyPositionUpdate(SceneObject* sceneObject, TreeEntry* entry) const;

};

#endif /* DETECTORZONECOMPONENT_H_ */
