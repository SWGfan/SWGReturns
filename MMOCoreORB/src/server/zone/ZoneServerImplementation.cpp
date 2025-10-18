/*
                Copyright <SWGEmu>
        See file COPYING for copying conditions.
*/

#include "server/zone/ZoneServer.h"

#include "server/zone/ZoneClientSession.h"

#include "server/zone/Zone.h"
#include "server/zone/GroundZone.h"
#include "server/zone/SpaceZone.h"

#include "server/db/ServerDatabase.h"
#ifdef WITH_SWGREALMS_API
#include "server/login/SWGRealmsAPI.h"
#endif

#include "conf/ConfigManager.h"

#include "server/zone/managers/object/ObjectManager.h"
#include "server/zone/managers/stringid/StringIdManager.h"
#include "server/zone/managers/player/PlayerManager.h"
#include "server/zone/managers/radial/RadialManager.h"
#include "server/zone/managers/resource/ResourceManager.h"
#include "server/zone/managers/crafting/CraftingManager.h"
#include "server/zone/managers/loot/LootManager.h"
#include "server/zone/managers/auction/AuctionManager.h"
#include "server/zone/managers/mission/MissionManager.h"
#include "server/zone/managers/creature/AiMap.h"
#include "server/zone/managers/space/SpaceAiMap.h"
#include "server/zone/managers/creature/CreatureTemplateManager.h"
#include "server/zone/managers/ship/ShipAgentTemplateManager.h"
#include "server/zone/managers/creature/DnaManager.h"
#include "server/zone/managers/creature/PetManager.h"
#include "server/zone/managers/guild/GuildManager.h"
#include "server/zone/managers/faction/FactionManager.h"
#include "server/zone/managers/reaction/ReactionManager.h"
#include "server/zone/managers/director/DirectorManager.h"
#include "server/zone/managers/city/CityManager.h"
#include "server/zone/managers/structure/StructureManager.h"
#include "server/zone/managers/frs/FrsManager.h"
#include "server/chat/ChatManager.h"
#include "server/zone/managers/ship/ShipManager.h"

#include "server/zone/ZoneProcessServer.h"
#include "ZonePacketHandler.h"
#include "ZoneHandler.h"

#include "SpaceZoneLoadManagersTask.h"
#include "ZoneLoadManagersTask.h"
#include "ShutdownTask.h"

ZoneServerImplementation::ZoneServerImplementation(ConfigManager* config) :
        ManagedServiceImplementation(), Logger("ZoneServer") {

    configManager = config;

    galaxyID = config->getZoneGalaxyID();
    galaxyName = "Core3";

    processor = nullptr;

    serverCap = 3000;

    phandler = nullptr;

    datagramService = new DatagramServiceThread("ZoneServer");
    datagramService->setLogging(false);
    datagramService->setLockName("ZoneServerLock");

    objectManager = nullptr;
    playerManager = nullptr;
    chatManager = nullptr;
    radialManager = nullptr;
    resourceManager = nullptr;
    craftingManager = nullptr;
    lootManager = nullptr;

    stringIdManager = nullptr;
    creatureTemplateManager = nullptr;
    shipAgentTemplateManager = nullptr;
    guildManager = nullptr;
    cityManager = nullptr;
    petManager = nullptr;

    totalSentPackets = 0;
    totalResentPackets = 0;

    currentPlayers = 0;
    maximumPlayers = 0;
    totalPlayers = 0;
    totalDeletedPlayers = 0;

    serverState = OFFLINE;
    deleteNavAreas = false;

    setLogLevel(Logger::INFO);
}

void ZoneServerImplementation::initializeTransientMembers() {
    phandler = nullptr;

    objectManager = nullptr;

    deleteNavAreas = false;

    ManagedObjectImplementation::initializeTransientMembers();
}

#ifndef WITH_SWGREALMS_API
void ZoneServerImplementation::loadGalaxyName() {
    try {
        const String query = "SELECT name FROM galaxy WHERE galaxy_id = " + String::valueOf(galaxyID);

        UniqueReference<ResultSet*> result(ServerDatabase::instance()->executeQuery(query));

        if (result->next())
            galaxyName = result->getString(0);

    } catch (const DatabaseException& e) {
        fatal(e.getMessage());
    }

    setLoggingName("ZoneServer " + galaxyName);

    loadLoginMessage();
}
#else // WITH_SWGREALMS_API
void ZoneServerImplementation::loadGalaxyName() {
    auto swgRealmsAPI = SWGRealmsAPI::instance();

    if (swgRealmsAPI != nullptr) {
        auto galaxyOpt = swgRealmsAPI->getGalaxyEntry(galaxyID);

        if (galaxyOpt.has_value()) {
            galaxyName = galaxyOpt.value().getName();
            setLoggingName("ZoneServer " + galaxyName);
            loadLoginMessage();
            return;
        }
    }

    error() << "Failed to load galaxy name for galaxy_id " << galaxyID;
    setLoggingName("ZoneServer UNKNOWN");

    loadLoginMessage();
}
#endif // WITH_SWGREALMS_API

void ZoneServerImplementation::initialize() {
    serverState = LOADING;

    loadGalaxyName();

    processor = new ZoneProcessServer(_this.getReferenceUnsafeStaticCast());
    processor->deploy("ZoneProcessServer");
    processor->initialize();

    zones = new VectorMap<String, ManagedReference<GroundZone*> >();
    spaceZones = new VectorMap<String, ManagedReference<SpaceZone*> >();

    objectManager = ObjectManager::instance();
    objectManager->setZoneProcessor(processor);
    objectManager->updateObjectVersion();

    stringIdManager = StringIdManager::instance();

    reactionManager = new ReactionManager(_this.getReferenceUnsafeStaticCast());
    reactionManager->loadLuaConfig();

    creatureTemplateManager = CreatureTemplateManager::instance();
    creatureTemplateManager->loadTemplates();

    shipAgentTemplateManager = ShipAgentTemplateManager::instance();
    shipAgentTemplateManager->loadTemplates();
    shipAgentTemplateManager->loadSpacePatrolPoints();

    AiMap::instance()->loadTemplates();
    SpaceAiMap::instance()->loadTemplates();

    dnaManager = DnaManager::instance();
    dnaManager->loadSampleData();

    phandler = new BasePacketHandler("ZoneServer", zoneHandler);
    phandler->setLogging(false);

    info("Initializing chat manager...", true);

    chatManager = new ChatManager(_this.getReferenceUnsafeStaticCast(), 10000);
    chatManager->deploy("ChatManager");
    chatManager->initiateRooms();

    playerManager = new PlayerManager(_this.getReferenceUnsafeStaticCast(), processor, true);
    playerManager->deploy("PlayerManager");

    chatManager->setPlayerManager(playerManager);

    craftingManager = new CraftingManager();
    craftingManager->deploy("CraftingManager");
    craftingManager->setZoneProcessor(processor);
    craftingManager->initialize();

    lootManager = new LootManager(craftingManager, objectManager, _this.getReferenceUnsafeStaticCast());
    lootManager->deploy("LootManager");
    lootManager->initialize();

    resourceManager = new ResourceManager(_this.getReferenceUnsafeStaticCast(), processor);
    resourceManager->deploy("ResourceManager");

    cityManager = new CityManager(_this.getReferenceUnsafeStaticCast());
    cityManager->deploy("CityManager");
    cityManager->loadLuaConfig();

    auctionManager = new AuctionManager(_this.getReferenceUnsafeStaticCast());
    auctionManager->deploy();

    missionManager = new MissionManager(_this.getReferenceUnsafeStaticCast(), processor);
    missionManager->deploy("MissionManager");

    petManager = new PetManager(_this.getReferenceUnsafeStaticCast());
    petManager->initialize();

    // Load ship data
    ShipManager::instance()->initialize();

    startGroundZones();
    startSpaceZones();

    startManagers();

    //serverState = LOCKED;
    serverState = ONLINE; //Test Center does not need to apply this change, but would be convenient for Dev Servers.

    ObjectDatabaseManager::instance()->commitLocalTransaction();
}

void ZoneServerImplementation::startGroundZones() {
    info(true) << "Loading Ground Zones...";

    auto enabledZones = configManager->getEnabledZones();

    StructureManager* structureManager = StructureManager::instance();
    structureManager->setZoneServer(_this.getReferenceUnsafeStaticCast());

    int totalZones = enabledZones.size();

    info(true) << "Total Enabled Ground Zones: " << totalZones;

    for (int i = 0; i < totalZones; ++i) {
        String zoneName = enabledZones.get(i);

        if (zoneName.toLowerCase().contains("space")) {
            error() << "Attempting to load Space Zone as a Ground Zone -- Zone: " << zoneName;
            continue;
        }

        GroundZone* zone = new GroundZone(processor, zoneName);
        zone->createContainerComponent();
        zone->initializePrivateData();
        zone->deploy("GroundZone " + zoneName);

        // Fixed displayName line
        String displayName = zoneName.subString(0,1).toUpperCase() + zoneName.subString(1, zoneName.length() - 1);

        info(true) << "Ground Zone: " + displayName + " deployed.";

        zones->put(zoneName, zone);
    }

    resourceManager->initialize();

    for (int i = 0; i < zones->size(); ++i) {
        GroundZone* zone = zones->get(i);

        if (zone != nullptr) {
            ZoneLoadManagersTask* task = new ZoneLoadManagersTask(_this.getReferenceUnsafeStaticCast(), zone);
            task->execute();
        }
    }

    for (int i = 0; i < zones->size(); ++i) {
        GroundZone* zone = zones->get(i);

        if (zone != nullptr) {
            while (!zone->hasManagersStarted())
                Thread::sleep(500);
        }
    }
}

// startSpaceZones, startManagers, and all other functions remain unchanged, except for the logging fix in createConnection():

ZoneClientSession* ZoneServerImplementation::createConnection(Socket* sock, SocketAddress& addr) {
    BaseClientProxy* session = new BaseClientProxy(sock, addr);

    StringBuffer loggingname;
    loggingname << "ZoneClientSession " << addr.getFullIPAddress();

    session->setLoggingName(loggingname.toString());
    session->setLogging(false);

    session->init(datagramService);

    ZoneClientSession* client = new ZoneClientSession(session);

    const auto& address = session->getFullIPAddress();

    // Fixed logging line
    debug() << "client connected from \'" << address << "\'";

    return client;
}
