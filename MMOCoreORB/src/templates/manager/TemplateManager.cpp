/*
 * TemplateManager.cpp
 *
 *  Created on: 06/05/2010
 *      Author: victor
 */

#include "TemplateManager.h"
#include "TemplateCRCMap.h"

#include "templates/appearance/AppearanceRedirect.h"
#include "templates/appearance/ComponentAppearanceTemplate.h"
#include "templates/appearance/DetailAppearanceTemplate.h"
#include "templates/appearance/FloorMesh.h"
#include "templates/appearance/MeshAppearanceTemplate.h"
#include "templates/appearance/PaletteTemplate.h"
#include "templates/appearance/PortalLayout.h"

#include "templates/building/CampStructureTemplate.h"
#include "templates/building/CloningBuildingObjectTemplate.h"
#include "templates/building/HospitalBuildingObjectTemplate.h"
#include "templates/building/RecreationBuildingObjectTemplate.h"
#include "templates/building/SharedBuildingObjectTemplate.h"
#include "templates/building/InteriorLayoutTemplate.h"

#include "templates/creature/NonPlayerCreatureObjectTemplate.h"
#include "templates/creature/PlayerCreatureTemplate.h"
#include "templates/creature/SharedCreatureObjectTemplate.h"
#include "templates/creature/VehicleObjectTemplate.h"
#include "templates/creature/VendorCreatureTemplate.h"

#include "templates/customization/AssetCustomizationManagerTemplate.h"
#include "templates/customization/CustomizationIdManager.h"

#include "templates/footprint/StructureFootprint.h"

#include "templates/installation/FactoryObjectTemplate.h"
#include "templates/installation/SharedInstallationObjectTemplate.h"

#include "templates/intangible/DraftSchematicObjectTemplate.h"
#include "templates/intangible/SharedConstructionContractObjectTemplate.h"
#include "templates/intangible/SharedDraftSchematicObjectTemplate.h"
#include "templates/intangible/SharedManufactureSchematicObjectTemplate.h"
#include "templates/intangible/SharedMissionDataObjectTemplate.h"
#include "templates/intangible/SharedMissionListEntryObjectTemplate.h"
#include "templates/intangible/SharedMissionObjectTemplate.h"
#include "templates/intangible/SharedPlayerObjectTemplate.h"
#include "templates/intangible/SharedTokenObjectTemplate.h"
#include "templates/intangible/SharedWaypointObjectTemplate.h"

#include "templates/manager/DataArchiveStore.h"
#include "templates/manager/PortalLayoutMap.h"

#include "templates/params/creature/CreatureState.h"
#include "templates/params/creature/ObjectFlag.h"
#include "templates/params/creature/CreatureAttribute.h"
#include "templates/params/OptionBitmask.h"
#include "templates/params/ObserverEventType.h"
#include "templates/params/PaletteColorCustomizationVariable.h"

#include "templates/resource/ResourceSpawnTemplate.h"
#include "templates/slots/SlotId.h"

#include "templates/tangible/tool/CraftingStationTemplate.h"
#include "templates/tangible/tool/CraftingToolTemplate.h"
#include "templates/tangible/tool/RecycleToolTemplate.h"
#include "templates/tangible/tool/RepairToolTemplate.h"
#include "templates/tangible/tool/SlicingToolTemplate.h"
#include "templates/tangible/tool/SurveyToolTemplate.h"

#include "templates/tangible/ArmorObjectTemplate.h"
#include "templates/tangible/CamoKitTemplate.h"
#include "templates/tangible/CampKitTemplate.h"
#include "templates/tangible/CharacterBuilderTerminalTemplate.h"
#include "templates/tangible/ConsumableTemplate.h"
#include "templates/tangible/ContainerTemplate.h"
#include "templates/tangible/CreatureHabitatTemplate.h"
#include "templates/tangible/CurePackTemplate.h"
#include "templates/tangible/DeedTemplate.h"
#include "templates/tangible/DiceTemplate.h"
#include "templates/tangible/DnaSampleTemplate.h"
#include "templates/tangible/DotPackTemplate.h"
#include "templates/tangible/DroidComponentTemplate.h"
#include "templates/tangible/DroidCraftingModuleTemplate.h"
#include "templates/tangible/DroidCustomKitTemplate.h"
#include "templates/tangible/DroidDeedTemplate.h"
#include "templates/tangible/DroidEffectsModuleTemplate.h"
#include "templates/tangible/DroidPersonalityModuleTemplate.h"
#include "templates/tangible/ElevatorTerminalTemplate.h"
#include "templates/tangible/EnhancePackTemplate.h"
#include "templates/tangible/EventPerkDeedTemplate.h"
#include "templates/tangible/FireworkObjectTemplate.h"
#include "templates/tangible/GamblingTerminalTemplate.h"
#include "templates/tangible/InstrumentObjectTemplate.h"
#include "templates/tangible/LiveSampleTemplate.h"
#include "templates/tangible/LootkitObjectTemplate.h"
#include "templates/tangible/LootSchematicTemplate.h"
#include "templates/tangible/MissionTerminalTemplate.h"
#include "templates/tangible/NavicomputerDeedTemplate.h"
#include "templates/tangible/PetDeedTemplate.h"
#include "templates/tangible/PowerupTemplate.h"
#include "templates/tangible/RangedStimPackTemplate.h"
#include "templates/tangible/SchematicFragmentTemplate.h"
#include "templates/tangible/SharedBattlefieldMarkerObjectTemplate.h"
#include "templates/tangible/SharedCountingObjectTemplate.h"
#include "templates/tangible/SharedFactoryObjectTemplate.h"
#include "templates/tangible/SharedResourceContainerObjectTemplate.h"
#include "templates/tangible/SharedWeaponObjectTemplate.h"
#include "templates/tangible/SkillBuffTemplate.h"
#include "templates/tangible/StatePackTemplate.h"
#include "templates/tangible/StimPackTemplate.h"
#include "templates/tangible/StructureDeedTemplate.h"
#include "templates/tangible/TrapTemplate.h"
#include "templates/tangible/VehicleCustomKitTemplate.h"
#include "templates/tangible/VehicleDeedTemplate.h"
#include "templates/tangible/WoundPackTemplate.h"
#include "templates/tangible/XpPurchaseTemplate.h"

#include "templates/tangible/ship/SharedShipObjectTemplate.h"
#include "templates/tangible/ship/ShipChassisTemplate.h"
#include "templates/tangible/ship/ShipComponentTemplate.h"
#include "templates/tangible/ship/ShipDeedTemplate.h"

#include "templates/universe/SharedGroupObjectTemplate.h"
#include "templates/universe/SharedGuildObjectTemplate.h"
#include "templates/universe/SharedJediManagerTemplate.h"

#include "templates/SharedCellObjectTemplate.h"
#include "templates/SharedIntangibleObjectTemplate.h"
#include "templates/SharedObjectTemplate.h"
#include "templates/SharedStaticObjectTemplate.h"
#include "templates/SharedTangibleObjectTemplate.h"
#include "templates/SharedUniverseObjectTemplate.h"

#include "conf/ConfigManager.h"
#include "tre3/TreeArchive.h"

// Static members
Lua* TemplateManager::luaTemplatesInstance = nullptr;
AtomicInteger TemplateManager::loadedTemplatesCount;
int TemplateManager::ERROR_CODE = NO_ERROR;

// Constructor / Destructor
TemplateManager::TemplateManager() {
    setLogging(false);
    setGlobalLogging(true);
    setLoggingName("TemplateManager");

    registerTemplateObjects();

    luaTemplatesInstance = new Lua();
    luaTemplatesInstance->init();

    templateCRCMap = new TemplateCRCMap();
    clientTemplateCRCMap = new ClientTemplateCRCMap();

    portalLayoutMap = new PortalLayoutMap();
    floorMeshMap = new FloorMeshMap();
    appearanceMap = new AppearanceMap();
    interiorMap = new InteriorMap();

    registerFunctions();
    registerGlobals();

    loadTreArchive();
    loadSlotDefinitions();
    loadPlanetMapCategories();
    loadAssetCustomizationManager();
}

TemplateManager::~TemplateManager() {
    delete templateCRCMap; templateCRCMap = nullptr;
    delete clientTemplateCRCMap; clientTemplateCRCMap = nullptr;
    delete luaTemplatesInstance; luaTemplatesInstance = nullptr;
    delete portalLayoutMap; portalLayoutMap = nullptr;
    delete floorMeshMap; floorMeshMap = nullptr;
    delete interiorMap; interiorMap = nullptr;
    delete appearanceMap; appearanceMap = nullptr;
}

// --- Loading helpers ---
void TemplateManager::loadSlotDefinitions() {
    debug("Loading slot definitions");
    IffStream* iffStream = openIffFile("abstract/slot/slot_definition/slot_definitions.iff");

    if (!iffStream) {
        error("Slot definitions can't be found.");
        ERROR_CODE = SLOT_DEFINITION_FILE_NOT_FOUND;
        return;
    }

    iffStream->openForm('0006');
    Chunk* data = iffStream->openChunk('DATA');

    while (data->hasData()) {
        Reference<SlotId*> slotId = new SlotId();
        slotId->readObject(data);
        slotDefinitions.put(slotId->getSlotName(), slotId);
    }

    iffStream->closeChunk('DATA');
    iffStream->closeForm('0006');
    delete iffStream;

    info() << "Loaded " << slotDefinitions.size() << " slot definitions.";
}

void TemplateManager::loadAssetCustomizationManager() {
    debug("loading asset customization manager");

    auto readIFF = [&](const String& path) -> IffStream* {
        IffStream* iffStream = openIffFile(path);
        if (!iffStream) ERROR_CODE = ASSETCUSTOMIZATIONMANAGER_FILE_NOT_FOUND;
        return iffStream;
    };

    IffStream* iffStream = readIFF("customization/asset_customization_manager.iff");
    if (!iffStream) return;
    AssetCustomizationManagerTemplate::instance()->readObject(iffStream);
    delete iffStream;

    iffStream = readIFF("customization/customization_id_manager.iff");
    if (!iffStream) return;
    CustomizationIdManager::instance()->readObject(iffStream);
    delete iffStream;

    iffStream = readIFF("datatables/customization/palette_columns.iff");
    if (!iffStream) return;
    CustomizationIdManager::instance()->loadPaletteColumns(iffStream);
    delete iffStream;

    iffStream = readIFF("datatables/customization/hair_assets_skill_mods.iff");
    if (!iffStream) return;
    CustomizationIdManager::instance()->loadHairAssetsSkillMods(iffStream);
    delete iffStream;

    iffStream = readIFF("datatables/customization/allow_bald.iff");
    if (!iffStream) return;
    CustomizationIdManager::instance()->loadAllowBald(iffStream);
    delete iffStream;
}
