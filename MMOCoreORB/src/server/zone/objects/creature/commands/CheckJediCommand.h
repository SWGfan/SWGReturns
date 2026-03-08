#ifndef CHECKJEDICOMMAND_H_
#define CHECKJEDICOMMAND_H_

#include "server/zone/objects/creature/commands/QueueCommand.h"
#include "server/zone/managers/director/DirectorManager.h"
#include "server/zone/managers/player/PlayerManager.h"
#include "server/zone/objects/player/PlayerObject.h"

class CheckJediCommand : public QueueCommand {
public:
    CheckJediCommand(const String& name, ZoneProcessServer* server)
        : QueueCommand(name, server) {
    }

    int doQueueCommand(CreatureObject* creature, const uint64& target, const UnicodeString& arguments) const override {
        if (creature == nullptr || !creature->isPlayerCreature())
            return GENERALERROR;

        ManagedReference<PlayerObject*> ghost = creature->getPlayerObject();
        if (ghost == nullptr)
            return GENERALERROR;

        // Call Lua function for personal status
        Lua* lua = DirectorManager::instance()->getLuaInstance();
        if (lua != nullptr) {
            lua_State* L = lua->getLuaState();
            lua_getglobal(L, "holocron_debug_status");
            
            if (lua_isfunction(L, -1)) {
                lua_pushlightuserdata(L, creature);
                if (lua_pcall(L, 1, 0, 0) != 0) {
                    const char* err = lua_tostring(L, -1);
                    lua_pop(L, 1);
                    creature->sendSystemMessage(String("Error: ") + err);
                    return GENERALERROR;
                }
            } else {
                lua_pop(L, 1);
                creature->sendSystemMessage("Holocron system not loaded. Contact an administrator.");
                return GENERALERROR;
            }
        }

        // Server statistics
        ManagedReference<ZoneServer*> zoneServer = creature->getZoneServer();
        if (zoneServer == nullptr)
            return SUCCESS;

        ManagedReference<PlayerManager*> playerManager = zoneServer->getPlayerManager();
        if (playerManager == nullptr)
            return SUCCESS;

        int forceUnlockedCount = 0;
        int padawanCount = 0;
        int knightCount = 0;
        int frsCount = 0;

        Vector<uint64> onlinePlayers = playerManager->getOnlinePlayerList();

        for (int i = 0; i < onlinePlayers.size(); ++i) {
            ManagedReference<SceneObject*> obj = zoneServer->getObject(onlinePlayers.get(i));
            if (obj == nullptr || !obj->isPlayerCreature())
                continue;

            CreatureObject* player = cast<CreatureObject*>(obj.get());
            if (player == nullptr)
                continue;

            Locker locker(player);

            PlayerObject* playerGhost = player->getPlayerObject();
            if (playerGhost == nullptr)
                continue;

            int jediState = playerGhost->getJediState();

            if (jediState >= 1) forceUnlockedCount++;
            if (jediState >= 2) padawanCount++;
            if (jediState >= 4) knightCount++;
            if (jediState >= 8) frsCount++;
        }

        StringBuffer stats;
        stats << "\\#00ff00===== Server Jedi Statistics =====\\#ffffff\n";
        stats << "Online Players: " << onlinePlayers.size() << "\n";
        stats << "Force Sensitive: " << forceUnlockedCount << "\n";
        stats << "Padawans: " << padawanCount << "\n";
        stats << "Knights: " << knightCount << "\n";
        stats << "FRS Members: " << frsCount << "\n";
        stats << "\\#00ff00===================================\\#ffffff";

        creature->sendSystemMessage(stats.toString());

        return SUCCESS;
    }
};

#endif