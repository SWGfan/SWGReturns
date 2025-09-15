/*
                Copyright <SWGEmu>
        See file COPYING for copying conditions.
 */

#include "SkillManager.h"
#include "SkillModManager.h"
#include "PerformanceManager.h"

#include "server/zone/objects/creature/variables/Skill.h"
#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/objects/player/badges/Badge.h"
#include "server/zone/objects/group/GroupObject.h"

#include "server/zone/managers/player/PlayerManager.h"
#include "server/zone/managers/jedi/JediManager.h"
#include "server/zone/managers/mission/MissionManager.h"
#include "server/zone/managers/frs/FrsManager.h"

#include "server/zone/objects/player/sui/messagebox/SuiMessageBox.h"
#include "server/zone/objects/player/sui/callbacks/SurrenderPilotSuiCallback.h"
#include "templates/faction/Factions.h"

#include "templates/manager/TemplateManager.h"
#include "templates/datatables/DataTableIff.h"
#include "templates/datatables/DataTableRow.h"

#include "server/zone/managers/crafting/schematicmap/SchematicMap.h"
#include "server/zone/packets/creature/CreatureObjectDeltaMessage4.h"

#include "server/zone/objects/transaction/TransactionLog.h"

// ---------------------------------------------------------------------------

SkillManager::SkillManager()
    : Logger("SkillManager") {

    rootNode = new Skill();

    performanceManager = new PerformanceManager();

    apprenticeshipEnabled = false;
}

SkillManager::~SkillManager() {
    delete performanceManager;
}

int SkillManager::includeFile(lua_State* L) {
    String filename = Lua::getStringParameter(L);
    Lua::runFile("scripts/skills/" + filename, L);
    return 0;
}

int SkillManager::addSkill(lua_State* L) {
    LuaObject obj(L);
    SkillManager::instance()->loadSkill(&obj);
    obj.pop();
    return 0;
}

void SkillManager::loadLuaConfig() {
    Lua* lua = new Lua();
    lua->init();

    lua->runFile("scripts/managers/skill_manager.lua");

    apprenticeshipEnabled = lua->getGlobalByte("apprenticeshipEnabled");

    delete lua;
    lua = nullptr;
}

void SkillManager::loadClientData() {
    // Load skills and ability map from skills.iff
    IffStream* iffStream = TemplateManager::instance()->openIffFile("datatables/skill/skills.iff");

    if (iffStream == nullptr) {
        error("Could not load skills.");
        return;
    }

    DataTableIff dtiff;
    dtiff.readObject(iffStream);
    delete iffStream;

    for (int i = 0; i < dtiff.getTotalRows(); ++i) {
        DataTableRow* row = dtiff.getRow(i);

        Reference<Skill*> skill = new Skill();
        skill->parseDataTableRow(row);

        Skill* parent = skillMap.get(skill->getParentName().hashCode());
        if (parent == nullptr)
            parent = rootNode;

        parent->addChild(skill);

        if (skillMap.put(skill->getSkillName().hashCode(), skill) != nullptr) {
            fatal("overwriting skill name");
        }

        // Add abilities from the skill into the ability map.
        const auto& commands = skill->commands;
        for (int j = 0; j < commands.size(); ++j) {
            const auto& command = commands.get(j);
            if (!abilityMap.containsKey(command)) {
                abilityMap.put(command, new Ability(command));
            }
        }
    }

    // Load droid space command program sizes + register abilities as "droid+<program>"
    iffStream = TemplateManager::instance()->openIffFile("datatables/space_command/droid_program_size.iff");
    if (iffStream != nullptr) {
        DataTableIff datatableIff;
        datatableIff.readObject(iffStream);
        delete iffStream;

        for (int i = 0; i < datatableIff.getTotalRows(); ++i) {
            DataTableRow* row = datatableIff.getRow(i);
            if (row == nullptr)
                continue;

            String programName = "";
            int programSize = 1;

            row->getValue(0, programName);
            row->getValue(1, programSize);

            if (programName.isEmpty())
                continue;

            droidProgramSizes.put(programName.hashCode(), programSize);

            String abilityName = "droid+" + programName;
            if (!abilityMap.containsKey(abilityName))
                abilityMap.put(abilityName, new Ability(abilityName));

            if (!droidCommands.contains(programName))
                droidCommands.put(programName);
        }
    }

    loadFromLua();

    // Add abilities not in skills.iff
    if (!abilityMap.containsKey("admin"))
        abilityMap.put("admin", new Ability("admin"));

    if (!abilityMap.containsKey("startMusic+western"))
        abilityMap.put("startMusic+western", new Ability("startMusic+western"));
    if (!abilityMap.containsKey("startDance+theatrical"))
        abilityMap.put("startDance+theatrical", new Ability("startDance+theatrical"));
    if (!abilityMap.containsKey("startDance+theatrical2"))
        abilityMap.put("startDance+theatrical2", new Ability("startDance+theatrical2"));

    loadXpLimits();

    info(true) << "Loaded " << skillMap.size() << " skills and " << abilityMap.size() << " abilities.";
    info(true) << "Loaded " << droidProgramSizes.size() << " Droid Space Command Sizes.";
}

void SkillManager::loadFromLua() {
    Lua* lua = new Lua();
    lua->init();
    lua->registerFunction("includeFile", &includeFile);
    lua->registerFunction("addSkill", &addSkill);

    lua->runFile("scripts/skills/serverobjects.lua");

    delete lua;
}

void SkillManager::loadSkill(LuaObject* luaSkill) {
    Reference<Skill*> skill = new Skill();
    skill->parseLuaObject(luaSkill);
    Skill* parent = skillMap.get(skill->getParentName().hashCode());

    if (parent == nullptr)
        parent = rootNode;

    parent->addChild(skill);
    skillMap.put(skill->getSkillName().hashCode(), skill);

    Vector<String> commands = skill->commands;
    for (int i = 0; i < commands.size(); ++i) {
        String command = commands.get(i);
        if (!abilityMap.containsKey(command))
            abilityMap.put(command, new Ability(command));
    }
}

void SkillManager::loadXpLimits() {
    IffStream* iffStream = TemplateManager::instance()->openIffFile("datatables/skill/xp_limits.iff");

    if (iffStream == nullptr) {
        error("Could not load skills.");
        return;
    }

    DataTableIff dtiff;
    dtiff.readObject(iffStream);
    delete iffStream;

    for (int i = 0; i < dtiff.getTotalRows(); ++i) {
        DataTableRow* row = dtiff.getRow(i);

        String type;
        int value;
        row->getValue(0, type);
        row->getValue(1, value);
        defaultXpLimits.put(type, value);

        debug() << type << ": " << value;
    }
}

// ---------------------------------------------------------------------------
// Abilities
// ---------------------------------------------------------------------------

void SkillManager::addAbility(PlayerObject* ghost, const String& abilityName, bool notifyClient) {
    if (ghost == nullptr)
        return;

    Ability* ability = abilityMap.get(abilityName);
    if (ability != nullptr)
        ghost->addAbility(ability, notifyClient);
}

void SkillManager::removeAbility(PlayerObject* ghost, const String& abilityName, bool notifyClient) {
    if (ghost == nullptr)
        return;

    Ability* ability = abilityMap.get(abilityName);
    if (ability != nullptr)
        ghost->removeAbility(ability, notifyClient);
}

void SkillManager::addAbilities(PlayerObject* ghost, const Vector<String>& abilityNames, bool notifyClient) {
    if (ghost == nullptr)
        return;

    Vector<Ability*> abilities;

    for (int i = 0; i < abilityNames.size(); ++i) {
        const String& abilityName = abilityNames.get(i);
        Ability* ability = abilityMap.get(abilityName);
        if (ability != nullptr && !ghost->hasAbility(abilityName))
            abilities.add(ability);
    }

    ghost->addAbilities(abilities, notifyClient);
}

void SkillManager::removeAbilities(PlayerObject* ghost, const Vector<String>& abilityNames, bool notifyClient) {
    if (ghost == nullptr)
        return;

    Vector<Ability*> abilities;

    for (int i = 0; i < abilityNames.size(); ++i) {
        const String& abilityName = abilityNames.get(i);
        Ability* ability = abilityMap.get(abilityName);
        if (ability != nullptr && ghost->hasAbility(abilityName))
            abilities.add(ability);
    }

    ghost->removeAbilities(abilities, notifyClient);
}

// ---------------------------------------------------------------------------
// Droid command helpers
// ---------------------------------------------------------------------------

void SkillManager::addDroidCommands(PlayerObject* ghost, const Vector<String>& abilityNames, bool notifyClient) {
    if (ghost == nullptr || abilityNames.size() == 0)
        return;

    Vector<Ability*> addList;

    for (int i = 0; i < abilityNames.size(); ++i) {
        const String& abilityName = abilityNames.get(i);

        if (ghost->hasDroidCommand(abilityName))
            continue;

        Ability* ability = abilityMap.get(abilityName);
        if (ability == nullptr)
            continue;

        addList.add(ability);
    }

    ghost->addDroidCommands(addList, notifyClient);
}

void SkillManager::removeDroidCommands(PlayerObject* ghost) {
    if (ghost == nullptr)
        return;

    ghost->removeDroidCommands();
}

void SkillManager::getPlayerDroidCommands(PlayerObject* ghost, Vector<String>& playerDroidCommands) {
    if (ghost == nullptr)
        return;

    for (int i = 0; i < droidCommands.size(); ++i) {
        const String& name = droidCommands.get(i);
        if (ghost->hasAbility(name))
            playerDroidCommands.add(name);
    }
}

// ---------------------------------------------------------------------------
// Award / Surrender
// ---------------------------------------------------------------------------

bool SkillManager::awardSkill(const String& skillName, CreatureObject* creature, bool notifyClient, bool awardRequiredSkills, bool noXpRequired) {
    auto skill = skillMap.get(skillName.hashCode());
    if (skill == nullptr)
        return false;

    Locker locker(creature);

    TransactionLog trx(TrxCode::SKILLTRAININGSYSTEM, creature);
    trx.addState("skill", skillName);

    // Check for required skills.
    auto requiredSkills = skill->getSkillsRequired();
    for (int i = 0; i < requiredSkills->size(); ++i) {
        const String& requiredSkillName = requiredSkills->get(i);
        auto requiredSkill = skillMap.get(requiredSkillName.hashCode());
        if (requiredSkill == nullptr)
            continue;

        if (awardRequiredSkills)
            awardSkill(requiredSkillName, creature, notifyClient, awardRequiredSkills, noXpRequired);

        if (!creature->hasSkill(requiredSkillName))
            return false;
    }

    if (!canLearnSkill(skillName, creature, noXpRequired))
        return false;

    // Already has it
    if (creature->hasSkill(skill->getSkillName()))
        return true;

    ManagedReference<PlayerObject*> ghost = creature->getPlayerObject();

    if (ghost != nullptr) {
        // withdraw skill points
        ghost->addSkillPoints(-skill->getSkillPointsRequired());

        // withdraw experience
        if (!noXpRequired) {
            TransactionLog trxExperience(TrxCode::EXPERIENCE, creature);
            trxExperience.groupWith(trx);
            ghost->addExperience(trxExperience, skill->getXpType(), -skill->getXpCost(), true);
        }

        creature->addSkill(skill, notifyClient);

        // add skill modifiers
        auto skillModifiers = skill->getSkillModifiers();
        for (int i = 0; i < skillModifiers->size(); ++i) {
            auto entry = &skillModifiers->elementAt(i);
            creature->addSkillMod(SkillModManager::SKILLBOX, entry->getKey(), entry->getValue(), notifyClient);
        }

        // add abilities
        auto abilityNames = skill->getAbilities();
        addAbilities(ghost, *abilityNames, notifyClient);

        if (skill->isGodOnly()) {
            for (int i = 0; i < abilityNames->size(); ++i) {
                const String& ability = abilityNames->get(i);
                StringIdChatParameter params;
                params.setTU(ability);
                params.setStringId("ui", "skill_command_acquired_prose");
                creature->sendSystemMessage(params);
            }
        }

        // add draft schematics
        auto schematicsGranted = skill->getSchematicsGranted();
        SchematicMap::instance()->addSchematics(ghost, *schematicsGranted, notifyClient);

        // caps + force
        updateXpLimits(ghost);
        ghost->recalculateForcePower();

        ManagedReference<PlayerManager*> playerManager = creature->getZoneServer()->getPlayerManager();

        if (skillName.contains("master")) {
            if (playerManager != nullptr) {
                const Badge* badge = BadgeList::instance()->get(skillName);
                if (badge == nullptr && skillName == "crafting_shipwright_master")
                    badge = BadgeList::instance()->get("crafting_shipwright");
                if (badge != nullptr)
                    playerManager->awardBadge(ghost, badge);
            }
        }

        const SkillList* list = creature->getSkillList();

        // *** changed from 250 to 500 ***
        int totalSkillPointsWasted = 500;

        for (int i = 0; i < list->size(); ++i) {
            Skill* s = list->get(i);
            totalSkillPointsWasted -= s->getSkillPointsRequired();
        }

        if (ghost->getSkillPoints() != totalSkillPointsWasted) {
            creature->error("skill points mismatch calculated: " + String::valueOf(totalSkillPointsWasted) + " found: " + String::valueOf(ghost->getSkillPoints()));
            ghost->setSkillPoints(totalSkillPointsWasted);
        }

        if (playerManager != nullptr)
            creature->setLevel(playerManager->calculatePlayerLevel(creature));

        if (skill->getSkillName().contains("force_sensitive") && skill->getSkillName().contains("_04"))
            JediManager::instance()->onFSTreeCompleted(creature, skill->getSkillName());

        MissionManager* missionManager = creature->getZoneServer()->getMissionManager();

        if (skill->getSkillName() == "force_title_jedi_rank_02") {
            if (missionManager != nullptr)
                missionManager->addPlayerToBountyList(creature->getObjectID(), ghost->calculateBhReward());
        } else if (skill->getSkillName().contains("force_discipline")) {
            if (missionManager != nullptr)
                missionManager->updatePlayerBountyReward(creature->getObjectID(), ghost->calculateBhReward());
        } else if (skill->getSkillName().contains("squadleader")) {
            Reference<GroupObject*> group = creature->getGroup();
            if (group != nullptr && group->getLeader() == creature) {
                Core::getTaskManager()->executeTask([group] () {
                    Locker locker(group);
                    group->removeGroupModifiers();
                    group->addGroupModifiers();
                }, "UpdateGroupModsLambda");
            }
        }
    }

    // Update client movement stats (no terrain negotiation call here)
    CreatureObjectDeltaMessage4* msg4 = new CreatureObjectDeltaMessage4(creature);
    msg4->updateAccelerationMultiplierBase();
    msg4->updateAccelerationMultiplierMod();
    msg4->updateSpeedMultiplierBase();
    msg4->updateSpeedMultiplierMod();
    msg4->updateRunSpeed();
    msg4->updateWalkSpeed();
    msg4->updateSlopeModAngle();
    msg4->updateSlopeModPercent();
    msg4->updateWaterModPercent();
    msg4->close();
    creature->sendMessage(msg4);

    SkillModManager::instance()->verifySkillBoxSkillMods(creature);
    return true;
}
void SkillManager::removeSkillRelatedMissions(CreatureObject* creature, Skill* skill) {
    if (skill->getSkillName().hashCode() == STRING_HASHCODE("combat_bountyhunter_investigation_03")) {
        ManagedReference<ZoneServer*> zoneServer = creature->getZoneServer();
        if (zoneServer != nullptr) {
            ManagedReference<MissionManager*> missionManager = zoneServer->getMissionManager();
            if (missionManager != nullptr) {
                // new signature requires hunter + target; target not tracked here, pass 0
                missionManager->failPlayerBountyMission(creature->getObjectID(), 0);
            }
        }
    }
}

bool SkillManager::surrenderSkill(const String& skillName, CreatureObject* creature, bool notifyClient, bool checkFrs, bool allowPilot) {
    Skill* skill = skillMap.get(skillName.hashCode());
    if (skill == nullptr)
        return false;

    Locker locker(creature);

    // If they already surrendered, succeed
    if (!creature->hasSkill(skill->getSkillName()))
        return true;

    const SkillList* skillList = creature->getSkillList();

    for (int i = 0; i < skillList->size(); ++i) {
        Skill* checkSkill = skillList->get(i);
        if (checkSkill->isRequiredSkillOf(skill))
            return false;
    }

    ManagedReference<PlayerObject*> ghost = creature->getPlayerObject();
    if (ghost == nullptr)
        return false;

    if (skillName.beginsWith("force_") && !(JediManager::instance()->canSurrenderSkill(creature, skillName)))
        return false;
    else if (!allowPilot && skillName.beginsWith("pilot_")) {
        if (ghost->hasSuiBoxWindowType(SuiWindowType::SURRENDER_PILOT_DENY))
            return false;

        ManagedReference<SuiMessageBox*> pilotBox = new SuiMessageBox(creature, SuiWindowType::SURRENDER_PILOT_DENY);
        if (pilotBox == nullptr)
            return false;

        pilotBox->setPromptTitle("@space/space_interaction:retire_warning_title"); // "Surrender Skill"

        uint32 faction = Factions::FACTIONNEUTRAL;

        if (skillName.contains("rebel")) {
            pilotBox->setPromptText("@space/space_interaction:retire_rebel_warning");
            faction = Factions::FACTIONREBEL;
        } else if (skillName.contains("imperial")) {
            pilotBox->setPromptText("@space/space_interaction:retire_imperial_warning");
            faction = Factions::FACTIONIMPERIAL;
        } else {
            pilotBox->setPromptText("@space/space_interaction:retire_neutral_warning");
        }

        pilotBox->setCallback(new SurrenderPilotSuiCallback(creature->getZoneServer(), faction));
        pilotBox->setUsingObject(creature);
        pilotBox->setForceCloseDisabled();

        pilotBox->setOkButton(true, "@ok");
        pilotBox->setCancelButton(true, "@space/space_interaction:retire_waypoint_btn");
        pilotBox->setOtherButton(false, "");

        ghost->addSuiBox(pilotBox);
        creature->sendMessage(pilotBox->generateMessage());

        return false;
    }

    removeSkillRelatedMissions(creature, skill);

    creature->removeSkill(skill, notifyClient);

    // remove skill modifiers
    auto skillModifiers = skill->getSkillModifiers();
    for (int i = 0; i < skillModifiers->size(); ++i) {
        auto entry = &skillModifiers->elementAt(i);
        creature->removeSkillMod(SkillModManager::SKILLBOX, entry->getKey(), entry->getValue(), notifyClient);
    }

    // Give back skill points
    ghost->addSkillPoints(skill->getSkillPointsRequired());

    // Remove abilities only if no other skill still grants them
    auto skillAbilities = skill->getAbilities();
    if (skillAbilities->size() > 0) {
        SortedVector<String> abilitiesLost;
        for (int i = 0; i < skillAbilities->size(); i++)
            abilitiesLost.put(skillAbilities->get(i));

        for (int i = 0; i < skillList->size(); i++) {
            Skill* remainingSkill = skillList->get(i);
            auto remainingAbilities = remainingSkill->getAbilities();
            for(int j = 0; j < remainingAbilities->size(); j++) {
                if (abilitiesLost.contains(remainingAbilities->get(j))) {
                    abilitiesLost.drop(remainingAbilities->get(j));
                    if (abilitiesLost.size() == 0)
                        break;
                }
            }
        }

        if (abilitiesLost.size() > 0)
            removeAbilities(ghost, abilitiesLost, notifyClient);

        // remove schematics
        auto schematicsGranted = skill->getSchematicsGranted();
        SchematicMap::instance()->removeSchematics(ghost, *schematicsGranted, notifyClient);

        // caps + force
        updateXpLimits(ghost);

        FrsManager* frsManager = creature->getZoneServer()->getFrsManager();
        if (checkFrs && frsManager->isFrsEnabled())
            frsManager->handleSkillRevoked(creature, skillName);

        ghost->recalculateForcePower();

        const SkillList* list = creature->getSkillList();

        // *** changed from 250 to 500 ***
        int totalSkillPointsWasted = 500;

        for (int i = 0; i < list->size(); ++i) {
            Skill* s = list->get(i);
            totalSkillPointsWasted -= s->getSkillPointsRequired();
        }

        if (ghost->getSkillPoints() != totalSkillPointsWasted) {
            creature->error("skill points mismatch calculated: " + String::valueOf(totalSkillPointsWasted) + " found: " + String::valueOf(ghost->getSkillPoints()));
            ghost->setSkillPoints(totalSkillPointsWasted);
        }

        ManagedReference<PlayerManager*> playerManager = creature->getZoneServer()->getPlayerManager();
        if (playerManager != nullptr)
            creature->setLevel(playerManager->calculatePlayerLevel(creature));

        MissionManager* missionManager = creature->getZoneServer()->getMissionManager();

        if (skill->getSkillName() == "force_title_jedi_rank_02") {
            if (missionManager != nullptr)
                missionManager->removePlayerFromBountyList(creature->getObjectID());
        } else if (skill->getSkillName().contains("force_discipline")) {
            if (missionManager != nullptr)
                missionManager->updatePlayerBountyReward(creature->getObjectID(), ghost->calculateBhReward());
        } else if (skill->getSkillName().contains("squadleader")) {
            Reference<GroupObject*> group = creature->getGroup();
            if (group != nullptr && group->getLeader() == creature) {
                Core::getTaskManager()->executeTask([group] () {
                    Locker locker(group);
                    group->removeGroupModifiers();
                    if (group->hasSquadLeader())
                        group->addGroupModifiers();
                }, "UpdateGroupModsLambda2");
            }
        }
    }

    // Update client movement stats (no terrain negotiation call here)
    CreatureObjectDeltaMessage4* msg4 = new CreatureObjectDeltaMessage4(creature);
    msg4->updateAccelerationMultiplierBase();
    msg4->updateAccelerationMultiplierMod();
    msg4->updateSpeedMultiplierBase();
    msg4->updateSpeedMultiplierMod();
    msg4->updateRunSpeed();
    msg4->updateWalkSpeed();
    msg4->updateSlopeModAngle();
    msg4->updateSlopeModPercent();
    msg4->updateWaterModPercent();
    msg4->close();
    creature->sendMessage(msg4);

    SkillModManager::instance()->verifySkillBoxSkillMods(creature);
    JediManager::instance()->onSkillRevoked(creature, skill);

    return true;
}

void SkillManager::surrenderAllSkills(CreatureObject* creature, bool notifyClient, bool removeForceProgression, bool removePilot) {
    ManagedReference<PlayerObject*> ghost = creature->getPlayerObject();

    const SkillList* skillList = creature->getSkillList();

    Vector<String> listOfNames;
    skillList->getStringList(listOfNames);
    SkillList copyOfList;

    copyOfList.loadFromNames(listOfNames);

    bool surrenderedPilot = false;

    for (int i = 0; i < copyOfList.size(); i++) {
        Skill* skill = copyOfList.get(i);

        surrenderedPilot = (removePilot && skill->getSkillName().contains("pilot"));

        if (skill->getSkillPointsRequired() > 0 || surrenderedPilot) {
            if (!removeForceProgression && skill->getSkillName().contains("force_"))
                continue;

            removeSkillRelatedMissions(creature, skill);

            creature->removeSkill(skill, notifyClient);

            // remove modifiers
            auto skillModifiers = skill->getSkillModifiers();
            for (int j = 0; j < skillModifiers->size(); ++j) {
                auto entry = &skillModifiers->elementAt(j);
                creature->removeSkillMod(SkillModManager::SKILLBOX, entry->getKey(), entry->getValue(), notifyClient);
            }

            if (ghost != nullptr) {
                ghost->addSkillPoints(skill->getSkillPointsRequired());

                auto abilityNames = skill->getAbilities();
                removeAbilities(ghost, *abilityNames, notifyClient);

                auto schematicsGranted = skill->getSchematicsGranted();
                SchematicMap::instance()->removeSchematics(ghost, *schematicsGranted, notifyClient);
                JediManager::instance()->onSkillRevoked(creature, skill);
            }
        }
    }

    if (surrenderedPilot && ghost != nullptr)
        ghost->resetPilotTier();

    SkillModManager::instance()->verifySkillBoxSkillMods(creature);

    if (ghost != nullptr) {
        updateXpLimits(ghost);
        ghost->recalculateForcePower();
    }

    ManagedReference<PlayerManager*> playerManager = creature->getZoneServer()->getPlayerManager();
    if (playerManager != nullptr)
        creature->setLevel(playerManager->calculatePlayerLevel(creature));

    MissionManager* missionManager = creature->getZoneServer()->getMissionManager();
    if (missionManager != nullptr)
        missionManager->removePlayerFromBountyList(creature->getObjectID());

    Reference<GroupObject*> group = creature->getGroup();
    if (group != nullptr && group->getLeader() == creature) {
        Core::getTaskManager()->executeTask([group] () {
            Locker locker(group);
            group->removeGroupModifiers();
        }, "UpdateGroupModsLambda3");
    }
}

void SkillManager::awardDraftSchematics(Skill* skill, PlayerObject* ghost, bool notifyClient) {
    if (ghost != nullptr) {
        auto schematicsGranted = skill->getSchematicsGranted();
        SchematicMap::instance()->addSchematics(ghost, *schematicsGranted, notifyClient);
    }
}

// ---------------------------------------------------------------------------
// XP caps & clamping
// ---------------------------------------------------------------------------

void SkillManager::updateXpLimits(PlayerObject* ghost) {
    if (ghost == nullptr || !ghost->isPlayerObject())
        return;

    VectorMap<String, int>* xpTypeCapList = ghost->getXpTypeCapList();
    xpTypeCapList->removeAll();

    // derive caps from the player's current skills
    ManagedReference<CreatureObject*> player = ghost->getParentRecursively(SceneObjectType::PLAYERCREATURE).castTo<CreatureObject*>();
    if(player == nullptr)
        return;

    const SkillList* playerSkillBoxList = player->getSkillList();

    for(int i = 0; i < playerSkillBoxList->size(); ++i) {
        Skill* skillBox = playerSkillBoxList->get(i);
        if (skillBox == nullptr || skillBox->getXpCap() == 0)
            continue;

        if (!xpTypeCapList->contains(skillBox->getXpType())) {
            xpTypeCapList->put(skillBox->getXpType(), skillBox->getXpCap());
        } else if (xpTypeCapList->get(skillBox->getXpType()) < skillBox->getXpCap()) {
            xpTypeCapList->get(skillBox->getXpType()) = skillBox->getXpCap();
        }
    }

    // add defaults for any xp types not covered by skills
    for (int i = 0; i < defaultXpLimits.size(); ++i) {
        String xpType = defaultXpLimits.elementAt(i).getKey();
        int xpLimit = defaultXpLimits.elementAt(i).getValue();

        if (!xpTypeCapList->contains(xpType))
            xpTypeCapList->put(xpType, xpLimit);
    }

    // cap current experience to limits
    DeltaVectorMap<String, int>* experienceList = ghost->getExperienceList();

    for (int i = 0; i < experienceList->size(); ++i) {
        String xpType = experienceList->getKeyAt(i);
        if (experienceList->get(xpType) > xpTypeCapList->get(xpType)) {
            TransactionLog trx(TrxCode::EXPERIENCE, player);
            ghost->addExperience(trx, xpType, xpTypeCapList->get(xpType) - experienceList->get(xpType), true);
        }
    }
}

// ---------------------------------------------------------------------------
// Requirements & helpers
// ---------------------------------------------------------------------------

bool SkillManager::canLearnSkill(const String& skillName, CreatureObject* creature, bool noXpRequired) {
    Skill* skill = skillMap.get(skillName.hashCode());
    if (skill == nullptr)
        return false;

    // already have it
    if (creature->hasSkill(skillName))
        return false;

    if (!fulfillsSkillPrerequisites(skillName, creature))
        return false;

    ManagedReference<PlayerObject* > ghost = creature->getPlayerObject();
    if (ghost != nullptr) {
        // xp
        if (!noXpRequired) {
            if (ghost->getExperience(skill->getXpType()) < skill->getXpCost())
                return false;
        }
        // skill points
        if (ghost->getSkillPoints() < skill->getSkillPointsRequired())
            return false;
    } else {
        return false;
    }

    return true;
}

bool SkillManager::fulfillsSkillPrerequisitesAndXp(const String& skillName, CreatureObject* creature) {
    if (!fulfillsSkillPrerequisites(skillName, creature))
        return false;

    Skill* skill = skillMap.get(skillName.hashCode());
    if (skill == nullptr)
        return false;

    ManagedReference<PlayerObject* > ghost = creature->getPlayerObject();
    if (ghost != nullptr) {
        if (skill->getXpCost() > 0 && ghost->getExperience(skill->getXpType()) < skill->getXpCost())
            return false;
    }

    return true;
}

bool SkillManager::fulfillsSkillPrerequisites(const String& skillName, CreatureObject* creature) {
    Skill* skill = skillMap.get(skillName.hashCode());
    if (skill == nullptr)
        return false;

    if (skillName.contains("admin_") && !(creature->getPlayerObject()->getAdminLevel() > 0))
        return false;

    auto requiredSpecies = skill->getSpeciesRequired();
    if (requiredSpecies->size() > 0) {
        bool foundSpecies = false;
        for (int i = 0; i < requiredSpecies->size(); i++) {
            if (creature->getSpeciesName() == requiredSpecies->get(i)) {
                foundSpecies = true;
                break;
            }
        }
        if (!foundSpecies)
            return false;
    }

    // required skill boxes
    auto requiredSkills = skill->getSkillsRequired();
    for (int i = 0; i < requiredSkills->size(); ++i) {
        const String& requiredSkillName = requiredSkills->get(i);
        Skill* requiredSkill = skillMap.get(requiredSkillName.hashCode());
        if (requiredSkill == nullptr)
            continue;

        if (!creature->hasSkill(requiredSkillName))
            return false;
    }

    PlayerObject* ghost = creature->getPlayerObject();
    if (ghost == nullptr || ghost->getJediState() < skill->getJediStateRequired())
        return false;

    if (ghost->isPrivileged())
        return true;

    if (skillName.beginsWith("force_"))
        return JediManager::instance()->canLearnSkill(creature, skillName);

    return true;
}

int SkillManager::getForceSensitiveSkillCount(CreatureObject* creature, bool includeNoviceMasterBoxes) {
    const SkillList* skills =  creature->getSkillList();
    int forceSensitiveSkillCount = 0;

    for (int i = 0; i < skills->size(); ++i) {
        const String& skillName = skills->get(i)->getSkillName();
        if (skillName.contains("force_sensitive") && (includeNoviceMasterBoxes || skillName.indexOf("0") != -1)) {
            forceSensitiveSkillCount++;
        }
    }

    return forceSensitiveSkillCount;
}

bool SkillManager::villageKnightPrereqsMet(CreatureObject* creature, const String& skillToDrop) {
    const SkillList* skillList = creature->getSkillList();

    int fullTrees = 0;
    int totalJediPoints = 0;

    for (int i = 0; i < skillList->size(); ++i) {
        Skill* skill = skillList->get(i);

        String sname = skill->getSkillName();
        if (sname.contains("force_discipline_") &&
            (sname.indexOf("0") != -1 || sname.contains("novice") || sname.contains("master") )) {
            totalJediPoints += skill->getSkillPointsRequired();

            if (sname.indexOf("4") != -1) {
                fullTrees++;
            }
        }
    }

    if (!skillToDrop.isEmpty()) {
        Skill* skillBeingDropped = skillMap.get(skillToDrop.hashCode());

        if (skillToDrop.indexOf("4") != -1) {
            fullTrees--;
        }

        totalJediPoints -= skillBeingDropped->getSkillPointsRequired();
    }

    return fullTrees >= 2 && totalJediPoints >= 206;
}

void SkillManager::getPlayerDroidCommands(PlayerObject* ghost, Vector<String>& playerDroidCommands) {
	if (ghost == nullptr) {
		return;
	}

	for (int i = 0; i < droidCommands.size(); ++i) {
		if (ghost->hasAbility(droidCommands.get(i)))
			playerDroidCommands.add(droidCommands.get(i));
	}
}

// ---------------------------------------------------------------------------
// Combat/Grey Jedi helpers (used by admin/repair flows)
// ---------------------------------------------------------------------------

void SkillManager::awardForceFromSkills(CreatureObject* creature) {
    int forceMax = 0;
    int forceRegen = 0;

    if (creature == nullptr)
        return;

    Locker locker(creature);

    ManagedReference<PlayerObject*> ghost = creature->getPlayerObject();

    const SkillList* skillList = creature->getSkillList();

    Vector<String> listOfNames;
    skillList->getStringList(listOfNames);
    SkillList copyOfList;

    copyOfList.loadFromNames(listOfNames);

    for (int i = 0; i < copyOfList.size(); i++) {
        Skill* skill = copyOfList.get(i);
        auto skillModifiers = skill->getSkillModifiers();

        for (int j = 0; j < skillModifiers->size(); ++j) {
            auto entry = &skillModifiers->elementAt(j);
            if (entry->getKey() == "jedi_force_power_max"){
                forceMax += entry->getValue();
            }
            if (entry->getKey() == "jedi_force_power_regen"){
                forceRegen += entry->getValue();
            }
        }
    }

    int currentFPR = creature->getSkillMod("jedi_force_power_regen");
    int currentFMax = creature->getSkillMod("jedi_force_power_max");

    error("Current force max: " + String::valueOf(currentFMax) + " Current regen: " + String::valueOf(currentFPR) );

    if (currentFPR < forceRegen){
        creature->addSkillMod(SkillModManager::PERMANENTMOD, "jedi_force_power_regen", forceRegen - currentFPR, true);
        error("difference of " + String::valueOf(currentFPR - forceRegen) + " detected in force regen, correcting");
    }
    if (currentFMax < forceMax){
        creature->addSkillMod(SkillModManager::PERMANENTMOD, "jedi_force_power_max", forceMax - currentFMax, true);
        error("difference of " + String::valueOf(currentFMax - forceMax) + " detected in force max, correcting");
    }
    error("New Force max: " + String::valueOf(forceMax) + " New Regen: " + String::valueOf(forceRegen));

    if (ghost != nullptr)
        ghost->setForcePowerMax(forceMax, true);

    ManagedReference<PlayerObject*> playerObject = creature->getPlayerObject();

    if (playerObject != nullptr)
        playerObject->setForcePower(forceMax);
}

void SkillManager::awardResetSkills(CreatureObject* creature) {
    if (creature == nullptr)
        return;

    Locker locker(creature);

    SkillManager* sm = SkillManager::instance();
    const SkillList* skillList = creature->getSkillList();
    if (skillList == nullptr)
        return;

    Vector<String> listOfNames;
    skillList->getStringList(listOfNames);
    SkillList copyOfList;
    copyOfList.loadFromNames(listOfNames);

    for (int i = 0; i < copyOfList.size(); i++) {
        Skill* skill = copyOfList.get(i);
        String skillName = skill->getSkillName();

        if (!skillName.beginsWith("admin")) {
            sm->surrenderSkill(skillName, creature, true);
            bool skillGranted = sm->awardSkill(skillName, creature, true, true, true);
            (void)skillGranted;
            creature->sendSystemMessage("Regranting Skill: " + skillName);
        }
    }
}
