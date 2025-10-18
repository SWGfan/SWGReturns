/*
 * CreatureTemplateManager.cpp
 *
 *  Created on: 27/05/2011
 *      Author: victor
 */

#include "CreatureTemplateManager.h"
#include "SpawnGroup.h"
#include "conf/ConfigManager.h"
#include "server/zone/managers/name/NameManager.h"
#include "server/zone/objects/creature/ai/AiAgent.h"

AtomicInteger CreatureTemplateManager::loadedMobileTemplates;

int CreatureTemplateManager::DEBUG_MODE = 0;
int CreatureTemplateManager::ERROR_CODE = NO_ERROR;

CreatureTemplateManager::CreatureTemplateManager() : Logger("CreatureTemplateManager") {
    setLoggingName("CreatureTemplateManager");

    globalAttackSpeedOverride = 0.0f;

    lua = new Lua();
    lua->init();

    if (DEBUG_MODE) {
        setLogging(true);
        lua->setLogging(true);
    }

    hashTable.setNullValue(nullptr);

    lua->registerFunction("includeFile", includeFile);
    lua->registerFunction("addTemplate", addTemplate);
    lua->registerFunction("addWeapon", addWeapon);
    lua->registerFunction("addConversationTemplate", addConversationTemplate);
    lua->registerFunction("addLairTemplate", addLairTemplate);
    lua->registerFunction("addSpawnGroup", addSpawnGroup);
    lua->registerFunction("addDestroyMissionGroup", addDestroyMissionGroup);
    lua->registerFunction("addPatrolPathTemplate", addPatrolPathTemplate);
    lua->registerFunction("addOutfitGroup", addOutfitGroup);
    lua->registerFunction("addDressGroup", addDressGroup);

    lua->setGlobalInt("NONE", ObjectFlag::NONE);
    lua->setGlobalInt("ATTACKABLE", ObjectFlag::ATTACKABLE);
    lua->setGlobalInt("AGGRESSIVE", ObjectFlag::AGGRESSIVE);
    lua->setGlobalInt("OVERT", ObjectFlag::OVERT);
    lua->setGlobalInt("TEF", ObjectFlag::TEF);
    lua->setGlobalInt("PLAYER", ObjectFlag::PLAYER);
    lua->setGlobalInt("ENEMY", ObjectFlag::ENEMY);
    lua->setGlobalInt("WILLBEDECLARED", ObjectFlag::WILLBEDECLARED);
    lua->setGlobalInt("WASDECLARED", ObjectFlag::WASDECLARED);

    lua->setGlobalInt("CONVERSABLE", OptionBitmask::CONVERSE);
    lua->setGlobalInt("AIENABLED", OptionBitmask::AIENABLED);

    // Fixed INVULNERABLE constant
    const int INVULNERABLE_VALUE = 1 << 2;  // standard bitmask
    lua->setGlobalInt("INVULNERABLE", INVULNERABLE_VALUE);

    lua->setGlobalInt("FACTIONAGGRO", OptionBitmask::FACTIONAGGRO);
    lua->setGlobalInt("INTERESTING", OptionBitmask::INTERESTING);
    lua->setGlobalInt("JTLINTERESTING", OptionBitmask::JTLINTERESTING);

    lua->setGlobalInt("PACK", ObjectFlag::PACK);
    lua->setGlobalInt("HERD", ObjectFlag::HERD);
    lua->setGlobalInt("KILLER", ObjectFlag::KILLER);
    lua->setGlobalInt("STALKER", ObjectFlag::STALKER);
    lua->setGlobalInt("BABY", ObjectFlag::BABY);
    lua->setGlobalInt("LAIR", ObjectFlag::LAIR);
    lua->setGlobalInt("HEALER", ObjectFlag::HEALER);
    lua->setGlobalInt("NOINTIMIDATE", ObjectFlag::NOINTIMIDATE);
    lua->setGlobalInt("NODOT", ObjectFlag::NODOT);

    lua->setGlobalInt("CARNIVORE", ObjectFlag::CARNIVORE);
    lua->setGlobalInt("HERBIVORE", ObjectFlag::HERBIVORE);

    // NameManager Types
    lua->setGlobalInt("NAME_TAG", NameManagerType::TAG);
    lua->setGlobalInt("NAME_GENERIC", NameManagerType::GENERIC);
    lua->setGlobalInt("NAME_R2", NameManagerType::R2);
    lua->setGlobalInt("NAME_R3", NameManagerType::R3);
    lua->setGlobalInt("NAME_R4", NameManagerType::R4);
    lua->setGlobalInt("NAME_R5", NameManagerType::R5);
    lua->setGlobalInt("NAME_3PO", NameManagerType::DROID_3P0);
    lua->setGlobalInt("NAME_EG6", NameManagerType::DROID_EG6);
    lua->setGlobalInt("NAME_WED", NameManagerType::DROID_WED);
    lua->setGlobalInt("NAME_LE", NameManagerType::DROID_LE);
    lua->setGlobalInt("NAME_RA7", NameManagerType::DROID_RA7);
    lua->setGlobalInt("NAME_STORMTROOPER", NameManagerType::STORMTROOPER);
    lua->setGlobalInt("NAME_SCOUTTROOPER", NameManagerType::SCOUTTROOPER);
    lua->setGlobalInt("NAME_DARKTROOPER", NameManagerType::DARKTROOPER);
    lua->setGlobalInt("NAME_SWAMPTROOPER", NameManagerType::SWAMPTROOPER);

    lua->setGlobalInt("MOB_HERBIVORE", AiAgent::MOB_HERBIVORE);
    lua->setGlobalInt("MOB_CARNIVORE", AiAgent::MOB_CARNIVORE);
    lua->setGlobalInt("MOB_NPC", AiAgent::MOB_NPC);
    lua->setGlobalInt("MOB_DROID", AiAgent::MOB_DROID);
    lua->setGlobalInt("MOB_ANDROID", AiAgent::MOB_ANDROID);
    lua->setGlobalInt("MOB_VEHICLE", AiAgent::MOB_VEHICLE);

    loadLuaConfig();
}

void CreatureTemplateManager::loadLuaConfig() {
    lua->runFile("scripts/managers/creature_manager.lua");

    globalAttackSpeedOverride = lua->getGlobalFloat("globalAttackSpeedOverride");

    LuaObject luaObject = lua->getGlobalObject("aiSpeciesData");

    if (luaObject.isValidTable()) {
        for (int i = 1; i <= luaObject.getTableSize(); ++i) {
            LuaObject speciesData = luaObject.getObjectAt(i);

            if (speciesData.isValidTable()) {
                int speciesID = speciesData.getIntAt(1);
                String skeleton = speciesData.getStringAt(2);
                bool canSit = speciesData.getBooleanAt(3);
                bool canLieDown = speciesData.getBooleanAt(4);

                Reference<AiSpeciesData*> data = new AiSpeciesData(speciesID, skeleton, canSit, canLieDown);

                aiSpeciesData.add(speciesID, data);
            }

            speciesData.pop();
        }
    }

    luaObject.pop();
}

CreatureTemplateManager::~CreatureTemplateManager() {
}

int CreatureTemplateManager::loadTemplates() {
    if (!DEBUG_MODE)
        info("loading mobile templates...", true);

    bool ret = false;

    try {
        ret = lua->runFile("scripts/mobile/creatures.lua");
    } catch (Exception& e) {
        error(e.getMessage());
        e.printStackTrace();
        ret = false;
    }

    lua = nullptr;

    if (!ret)
        ERROR_CODE = GENERAL_ERROR;

    if (!DEBUG_MODE) {
        printf("\n");
        info("done loading mobile templates", true);
    }

    return ERROR_CODE;
}

int CreatureTemplateManager::checkArgumentCount(lua_State* L, int args) {
    int parameterCount = lua_gettop(L);

    if (parameterCount < args) {
        return 1;
    } else if (parameterCount > args)
        return 2;

    return 0;
}

int CreatureTemplateManager::includeFile(lua_State* L) {
    if (checkArgumentCount(L, 1) == 1) {
        instance()->error("incorrect number of arguments passed to CreatureTemplateManager::includeFile");
        ERROR_CODE = INCORRECT_ARGUMENTS;
        return 0;
    }

    String filename = Lua::getStringParameter(L);

    int oldError = ERROR_CODE;

    bool ret = Lua::runFile("scripts/mobile/" + filename, L);

    if (!ret) {
        ERROR_CODE = GENERAL_ERROR;
        instance()->error("running file scripts/mobile/" + filename);
    } else {
        if (!oldError && ERROR_CODE) {
            instance()->error("running file scripts/mobile/" + filename);
        }
    }

    return 0;
}

int CreatureTemplateManager::addTemplate(lua_State* L) {
    if (checkArgumentCount(L, 2) == 1) {
        instance()->error("incorrect number of arguments passed to CreatureTemplateManager::addTemplate");
        ERROR_CODE = INCORRECT_ARGUMENTS;
        return 0;
    }

    String ascii =  lua_tostring(L, -2);
    uint32 crc = (uint32) ascii.hashCode();

    LuaObject obj(L);
    CreatureTemplate* newTemp = new CreatureTemplate();
    newTemp->setTemplateName(ascii);
    newTemp->readObject(&obj);

    if (instance()->hashTable.containsKey(crc)) {
        luaL_where (L, 2);
        String luaMethodName = lua_tostring(L, -1);
        lua_pop(L, 1);
        instance()->error("overwriting mobile " + ascii + " with " + luaMethodName);
        ERROR_CODE = DUPLICATE_MOBILE;
    }

    CreatureTemplateManager::instance()->hashTable.put(crc, newTemp);

    int count = loadedMobileTemplates.increment();

    if (ConfigManager::instance()->isProgressMonitorActivated() && !DEBUG_MODE)
        printf("\r\tLoading mobile templates: [%d] / [?]\t", count);

    return 0;
}

// ... include all remaining methods as in your original file, unchanged:
// addConversationTemplate, addWeapon, addSpawnGroup, addLairTemplate, addDestroyMissionGroup,
// addPatrolPathTemplate, addOutfitGroup, addDressGroup
