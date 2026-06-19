/*
* HolocronMenuComponent.h
*
*  Created on: 09/16/2019
*      Author: Toxic
*
*  Modified: Aftermath Server
*      Added: callLuaHolocronFunction() private helper
*/
// Coded by BoosterSteel 19-03-2026


#ifndef HOLOCRONMENUCOMPONENT_H_
#define HOLOCRONMENUCOMPONENT_H_

#include "TangibleObjectMenuComponent.h"

namespace server {
namespace zone {
namespace objects {
namespace scene {
	class SceneObject;
}
}
}
}

using namespace server::zone::objects::scene;

namespace server {
namespace zone {
namespace objects {
namespace creature {
	class CreatureObject;
}
}
}
}

using namespace server::zone::objects::creature;

namespace server {
namespace zone {
namespace objects {
namespace player {
	class PlayerObject;
}
}
}
}

using namespace server::zone::objects::player;

class HolocronMenuComponent : public TangibleObjectMenuComponent {

public:
	virtual int handleObjectMenuSelect(SceneObject* sceneObject, CreatureObject* player, byte selectedID) const;

	virtual void fillObjectMenuResponse(SceneObject* sceneObject, ObjectMenuResponse* menuResponse, CreatureObject* player) const;

private:
	// Grants all FS and Jedi title skills with checkRequirements=false (same as /grantskill).
	// Called from handleObjectMenuSelect after Lua confirms jedi_status == "padawan".
	void grantPadawanSkills(CreatureObject* creature) const;

	// Grants Jedi Knight rank skill and sets FRS council membership.
	// Called from fillObjectMenuResponse when HolocronKnightGrantPending == 1.
	void grantKnightSkills(CreatureObject* creature) const;

	// Grants jedi_grand_master_novice or jedi_dark_lord_novice after Phase 1 Revan kill.
	void grantMasterPhase1Skills(CreatureObject* creature) const;

	// Grants force_rank_light/dark_master after Final Revan kill.
	void grantMasterFinalSkills(CreatureObject* creature) const;

	// Calls a Lua function with (pCreature, pSceneObject) args
	void callLuaHolocronFunction(SceneObject* sceneObject, CreatureObject* creature, const String& functionName) const;
};


#endif /* HOLOCRONMENUCOMPONENT_H_ */
