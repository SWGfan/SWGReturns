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
#include "templates/manager/TemplateManager.h"
#include "templates/datatables/DataTableIff.h"
#include "templates/datatables/DataTableRow.h"
#include "server/zone/managers/crafting/schematicmap/SchematicMap.h"
#include "server/zone/objects/player/variables/SchematicList.h"
#include "server/zone/packets/creature/CreatureObjectDeltaMessage4.h"
#include "server/zone/managers/mission/MissionManager.h"
#include "server/zone/managers/frs/FrsManager.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/ZoneServer.h"

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

		//Load the abilities of the skill into the ability map.
		const auto& commands = skill->commands;

		for (int i = 0; i < commands.size(); ++i) {
			const auto& command = commands.get(i);

			if (!abilityMap.containsKey(command)) {
				abilityMap.put(command, new Ability(command));
			}

			// COMPANION_ABILITY_REGISTRATION_HOTFIX_2026_07_31 -- also register a "companion_"-prefixed
			// twin of every real ability command so AbilityList::loadFromNames()
			// (AbilityList.cpp) can resolve companion-granted abilities via
			// SkillManager::getAbility() on every future relog/restart. See
			// CompanionSkillTrainer::grantOwnerAbilitiesForSkill(), which grants
			// "companion_" + this same command string directly onto the
			// player's live ability list, bypassing this map entirely -- until
			// now, nothing ever registered that prefixed name here, so the
			// reload path silently dropped every companion_<ability> a player
			// had ever been granted (confirmed live: this is why "Heal Wounds",
			// gated on hasAbility("companion_healwound"), stopped working after
			// a server restart). Purely additive -- does not change lookup or
			// behavior for any non-"companion_" ability name.
			String companionTwin = "companion_" + command.toLowerCase();

			if (!abilityMap.containsKey(companionTwin)) {
				abilityMap.put(companionTwin, new Ability(companionTwin));
			}
		}
	}

	loadFromLua();

	//If the admin ability isn't in the ability map, then we want to add it manually.
	if (!abilityMap.containsKey("admin"))
		abilityMap.put("admin", new Ability("admin"));

	// These are not listed in skills.iff and need to be added manually
	if (!abilityMap.containsKey("startMusic+western"))
		abilityMap.put("startMusic+western", new Ability("startMusic+western"));
	if (!abilityMap.containsKey("startDance+theatrical"))
		abilityMap.put("startDance+theatrical", new Ability("startDance+theatrical"));
	if (!abilityMap.containsKey("startDance+theatrical2"))
		abilityMap.put("startDance+theatrical2", new Ability("startDance+theatrical2"));

	// COMPANION_ABILITY_HARDCODE_HOTFIX_2026_07_31_V2 -- CompanionSkillTrainer.cpp grants three more
	// ability lists as ad-hoc objects that bypass this map entirely:
	// baselineAbilities[] (order commands), combatAbilities[] and
	// starterAbilities[] (the "test everything from novice" macro grant).
	// None of these are guaranteed to already exist as real skill
	// commands in skills.iff -- confirmed live: companion_follow/
	// companion_attack/etc. kept failing "is null when trying to load
	// from database" even after the first companion_ registration hotfix
	// (which only mirrors skills.iff-sourced commands). Registering them
	// here, hardcoded to match CompanionSkillTrainer.cpp's own arrays
	// exactly, closes that gap so every ability CompanionSkillTrainer.cpp
	// can ever grant a player now resolves correctly on reload.
	static const char* companionBaselineAbilities[] = {
		"companion_follow", "companion_stay", "companion_patrol", "companion_store",
		"companion_attack", "companion_formup", "companion_guard", "companion_followother",
		"companion_rangedattack", "companion_specialone", "companion_specialtwo", "companion_group",
		"companion_friend", "companion_return", "companion_craft", "companion_jenkins",
	};

	for (int ci = 0; ci < (int) (sizeof(companionBaselineAbilities) / sizeof(companionBaselineAbilities[0])); ++ci) {
		const String companionBaselineName(companionBaselineAbilities[ci]);

		if (!abilityMap.containsKey(companionBaselineName)) {
			abilityMap.put(companionBaselineName, new Ability(companionBaselineName));
		}
	}

	static const char* companionRawAbilityNames[] = {
		"applyDisease", "applyPoison", "bleedingShot", "concealShot",
		"confusionShot", "eyeShot", "fastBlast", "fireAcidCone1",
		"fireAcidCone2", "fireAcidSingle1", "fireAcidSingle2", "fireLightningCone1",
		"fireLightningCone2", "fireLightningSingle1", "fireLightningSingle2", "flameCone1",
		"flameCone2", "flameSingle1", "flameSingle2", "flurryShot1",
		"flurryShot2", "flushingShot1", "flushingShot2", "headShot3",
		"healMind", "knockdownFire", "mindShot2", "sniperShot",
		"sprayShot", "startleShot1", "startleShot2", "strafeShot1",
		"strafeShot2", "surpriseShot", "torsoShot", "underHandShot",
		"healDamage", "healWound", "tendWound", "tendDamage",
		"diagnose", "medicalForage", "harvestCorpse", "startDance",
		"stopDance", "startMusic", "stopMusic", "sample",
		"survey", "warcry1", "intimidate1", "berserk1",
		"taunt", "polearmLunge1", "unarmedLunge1", "melee1hLunge1",
		"melee2hLunge1", "centerOfBeing", "pointBlankArea1", "pointBlankSingle1",
		"overchargeShot1",
	};

	for (int ci = 0; ci < (int) (sizeof(companionRawAbilityNames) / sizeof(companionRawAbilityNames[0])); ++ci) {
		const String companionAbilityTwin = "companion_" + String(companionRawAbilityNames[ci]).toLowerCase();

		if (!abilityMap.containsKey(companionAbilityTwin)) {
			abilityMap.put(companionAbilityTwin, new Ability(companionAbilityTwin));
		}
	}

	loadXpLimits();

	info(true) << "Successfully loaded " << skillMap.size() <<
	       	" skills and " << abilityMap.size() << " abilities.";
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

	if(parent == nullptr) {
		parent = rootNode;
	}

	parent->addChild(skill);
	skillMap.put(skill->getSkillName().hashCode(), skill);

	Vector<String> commands = skill->commands;

	for(int i = 0; i < commands.size(); ++i) {
		String command = commands.get(i);

		if(!abilityMap.containsKey(command)) {
			abilityMap.put(command, new Ability(command));
		}
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

void SkillManager::addAbility(PlayerObject* ghost, const String& abilityName, bool notifyClient) {
	Ability* ability = abilityMap.get(abilityName);

	if (ability != nullptr)
		ghost->addAbility(ability, notifyClient);
}

void SkillManager::removeAbility(PlayerObject* ghost, const String& abilityName, bool notifyClient) {
	Ability* ability = abilityMap.get(abilityName);

	if (ability != nullptr)
		ghost->removeAbility(ability, notifyClient);
}

void SkillManager::addAbilities(PlayerObject* ghost, const Vector<String>& abilityNames, bool notifyClient) {
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
	Vector<Ability*> abilities;

	for (int i = 0; i < abilityNames.size(); ++i) {
		const String& abilityName = abilityNames.get(i);

		Ability* ability = abilityMap.get(abilityName);

		if (ability != nullptr && ghost->hasAbility(abilityName))
			abilities.add(ability);
	}

	ghost->removeAbilities(abilities, notifyClient);
}

/*bool SkillManager::checkPrerequisiteSkill(const String& skillName, CreatureObject* creature) {
	return true;
}*/

bool SkillManager::awardSkill(const String& skillName, CreatureObject* creature, bool notifyClient, bool awardRequiredSkills, bool noXpRequired) {
	auto skill = skillMap.get(skillName.hashCode());

	if (skill == nullptr)
		return false;

	Locker locker(creature);

	//Check for required skills.
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

	if (!canLearnSkill(skillName, creature, noXpRequired)) {
		return false;
	}

	//Check for precluded skills.
	auto skillsPrecluded = skill->getSkillsPrecluded();
	for (int i = 0; i < skillsPrecluded->size(); ++i) {
		const String& precludedSkillName = skillsPrecluded->get(i);
		Skill* precludedSkill = skillMap.get(precludedSkillName.hashCode());

		if (precludedSkill == NULL) {
			continue;
		}

		if (creature->hasSkill(precludedSkillName)) {
			return false;
		}
	}


	//If they already have the skill, then return true.
	if (creature->hasSkill(skill->getSkillName()))
		return true;

	ManagedReference<PlayerObject*> ghost = creature->getPlayerObject();

	if (ghost != nullptr) {
		//Companion System: companion_master_* skill boxes are always 0 SP --
		//see docs/companion_system/NOTES.md.
		bool isCompanionMasterSkill = skillName.beginsWith("companion_master_");

		//Withdraw skill points.
		if (!isCompanionMasterSkill) {
			ghost->addSkillPoints(-skill->getSkillPointsRequired());
		}

		//Witdraw experience.
		//Companion System (2026-07-18, "no skills require xp" -- see
		//canLearnSkill()): with the XP gate disabled, the withdrawal is
		//skipped too (deducting a cost the player never had would drive
		//pools negative). Original kept for easy re-enabling:
		//if (!noXpRequired) {
		//	ghost->addExperience(skill->getXpType(), -skill->getXpCost(), true);
		//}

		creature->addSkill(skill, notifyClient);

		//Add skill modifiers
		auto skillModifiers = skill->getSkillModifiers();

		for (int i = 0; i < skillModifiers->size(); ++i) {
			auto entry = &skillModifiers->elementAt(i);
			creature->addSkillMod(SkillModManager::SKILLBOX, entry->getKey(), entry->getValue(), notifyClient);

		}

		//Add abilities
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

		//Add draft schematic groups
		auto schematicsGranted = skill->getSchematicsGranted();
		SchematicMap::instance()->addSchematics(ghost, *schematicsGranted, notifyClient);

		// Master Survey Tool (2026-07-29, Companion -- see NOTES.md "Master
		// Survey Tool"): one-off grant, bypassing the schematic-group system
		// entirely -- see this script's own docstring for why. KNOWN GAP:
		// only fires on the skill-LEARN event; existing masters aren't
		// retroactively granted (see NOTES.md for the suggested backfill).
		if (skillName == "crafting_artisan_master" && ghost != nullptr) {
			uint32 masterSurveyToolSchematicCRC = String("object/draft_schematic/item/item_master_survey_tool.iff").hashCode();
			DraftSchematic* masterSurveyToolSchematic = SchematicMap::instance()->get(masterSurveyToolSchematicCRC);

			if (masterSurveyToolSchematic != nullptr) {
				// MASTER_SURVEY_TOOL_UNLIMITED_USES_2026_08_01 -- addRewardedSchematic()'s 3rd
				// arg is a USE COUNT for QUEST-type reward schematics (the same
				// one-shot mechanic real one-time quest reward schematics use).
				// Passing 1 meant this schematic was CONSUMED after exactly one
				// craft (confirmed live, 2026-08-01: Nick crafted one, then it
				// vanished from his crafting list). This is meant to be a
				// permanent, unlimited-use unlock for Master Artisans, not a
				// one-shot reward, so grant an effectively-unlimited use count
				// instead.
				ghost->addRewardedSchematic(masterSurveyToolSchematic, SchematicList::QUEST, 999999, notifyClient);
			}
		}

		//Update maximum experience.
		updateXpLimits(ghost);


		// Update Force Power Max.
		ghost->recalculateForcePower();

		ManagedReference<PlayerManager*> playerManager = creature->getZoneServer()->getPlayerManager();

		if (skillName.contains("master")) {
			if (playerManager != nullptr) {
				const Badge* badge = BadgeList::instance()->get(skillName);

				if (badge == nullptr && skillName == "crafting_shipwright_master") {
					badge = BadgeList::instance()->get("crafting_shipwright");
				}

				if (badge != nullptr) {
					playerManager->awardBadge(ghost, badge);
				}
			}
		}

		const SkillList* list = creature->getSkillList();

		int totalSkillPointsWasted = 250;

		for (int i = 0; i < list->size(); ++i) {
			Skill* skill = list->get(i);

			totalSkillPointsWasted -= skill->getSkillPointsRequired();
		}

		if (ghost->getSkillPoints() != totalSkillPointsWasted) {
			creature->error("skill points mismatch calculated: " + String::valueOf(totalSkillPointsWasted) + " found: " + String::valueOf(ghost->getSkillPoints()));
			ghost->setSkillPoints(totalSkillPointsWasted);
		}

		if (playerManager != nullptr) {
			creature->setLevel(playerManager->calculatePlayerLevel(creature));
		}

		if (skill->getSkillName().contains("force_sensitive") && skill->getSkillName().contains("_04"))
			JediManager::instance()->onFSTreeCompleted(creature, skill->getSkillName());

		MissionManager* missionManager = creature->getZoneServer()->getMissionManager();

		if (skill->getSkillName() == "combat_bountyhunter_investigation_03"){
			if (missionManager != NULL)
				missionManager->addPlayerToBountyList(creature->getObjectID(), ghost->calculateBhReward());
		}

		// Bounty hunters get free access to healing (Medic) and Ranger abilities as soon as they enter the profession.
		if (skill->getSkillName() == "combat_bountyhunter_novice") {
			awardSkill("science_medic_master", creature, notifyClient, true, true);
			awardSkill("outdoors_ranger_master", creature, notifyClient, true, true);
		}

		if (skill->getSkillName() == "force_title_jedi_rank_02") {
			if (missionManager != nullptr)
				missionManager->addPlayerToBountyList(creature->getObjectID(), ghost->calculateBhReward());
		} else if (skill->getSkillName().contains("jedi")) {
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
		} else if (skill->getSkillName() == "companion_master_novice") {
			// Companion System -- grant-on-unlock (spec 2A/3A): the first time
			// a player learns Novice Companion Handler, create their
			// companion control device + linked companion actor and drop it
			// straight into their datapad, no separate acquisition/
			// recruitment step, no cooldown -- see
			// docs/companion_system/NOTES.md. Guarded so re-learning (e.g.
			// via awardRequiredSkills re-entrancy) never grants a second one.
			ManagedReference<SceneObject*> datapad = creature->getSlottedObject("datapad");

			if (datapad != nullptr) {
				int existingCompanionCount = 0;

				for (int i = 0; i < datapad->getContainerObjectsSize(); ++i) {
					ManagedReference<SceneObject*> obj = datapad->getContainerObject(i);

					if (obj != nullptr && obj->isCompanionControlDevice()) {
						++existingCompanionCount;
					}
				}

				// Companion System (2026-07-15, "test 5 companions at once"
				// pass -- see NOTES.md and the user's own explicit choice of
				// "instant-grant N on novice, same starter profession/loadout
				// each" over a repeatable one-at-a-time recruit flow).
				// companion_slots is read here, live, rather than hardcoded --
				// so raising/lowering the skill mod in skills.iff is the one
				// and only place the granted count needs to change. Falls
				// back to 1 if the skill mod is somehow missing/zero (should
				// never happen -- companion_master_novice's own SKILL_MODS
				// always sets it -- but a granted-count of 0 would silently
				// grant nothing at all, worse than a safe floor of 1).
				int companionSlotCount = creature->getSkillMod("companion_slots");

				if (companionSlotCount <= 0) {
					companionSlotCount = 1;
				}

				// Companion System (2026-07-15, "return to the trainer to claim
			// additional companions" -- user's revised design, replacing the
			// instant-grant-N shape): the novice grant hands out exactly ONE
			// companion; every further slot is claimed by conversing with the
			// Companion Master trainer (CompanionSkillTrainer::
			// claimAdditionalCompanion(), hooked in AiAgentImplementation::
			// sendConversationStartTo()) until companion_slots is reached.
			int countToCreate = existingCompanionCount == 0 ? 1 : 0;

			if (companionSlotCount < 1) { // keep the mod read alive for the claim flow's benefit
				countToCreate = 0;
			}

				if (countToCreate > 0) {
					ZoneServer* zoneServer = creature->getZoneServer();

					if (zoneServer != nullptr) {
						bool grantedLoadoutBackpack = false;

						for (int slot = 0; slot < countToCreate; ++slot) {
							ManagedReference<SceneObject*> deviceObj = zoneServer->createObject(String("object/intangible/companion/companion_control_device.iff").hashCode(), 1);
							ManagedReference<SceneObject*> companionObj = zoneServer->createObject(String("object/mobile/companion_actor.iff").hashCode(), 1);

							CompanionControlDevice* device = deviceObj != nullptr ? deviceObj.castTo<CompanionControlDevice*>() : nullptr;
							CompanionObject* companion = companionObj != nullptr ? companionObj.castTo<CompanionObject*>() : nullptr;

							if (device != nullptr && companion != nullptr) {
								// Companion System: device must stay locked across
								// the transferObject/broadcastObject calls below --
								// ContainerComponent::transferObject() mutates the
								// transferred object's parent/containment fields
								// directly with no internal locking of its own.
								// Matches the locking pattern in
								// TameCreatureTask.h and DirectorManager.cpp's
								// createControlDevice, both of which hold the
								// device's Locker through the transfer call.
								Locker dlocker(device, creature);

								{
									Locker clocker(companion, creature);
									companion->setCompanionControlDevice(device);

									// Companion System (2026-07-16, per user request):
									// flat "Companion" now, no number suffix -- was
									// "Companion " + a running index so multiple
									// simultaneously-summoned companions were tellable
									// apart before renaming. Each companion gets renamed
									// almost immediately anyway via the first-launch
									// profession picker's auto-popup rename box
									// (CompanionStarterProfessionSuiCallback.h), which
									// now defaults ITS OWN input to the chosen
									// profession's name (e.g. "MARKSMAN") rather than
									// this generic placeholder.
									String defaultName = "Companion";
									companion->setCustomObjectName(defaultName, true);
									// Companion System (2026-07-30): one-time random personality roll -- see NOTES.md.
									companion->setPersonalityType(1 + System::random(4));
								}

								device->setCompanionObject(companion);

								// Companion System UX-parity fix: a companion is bound to
								// the owner's own Companion Master skill investment (rank
								// gating, isolated skill-point ledger, first-launch
								// profession choice) the same way a real pet is bound to
								// the trainer who tamed it -- but unlike PetControlDevice,
								// which is gated from player-trading via
								// PetControlDeviceImplementation::canBeTradedTo() (a
								// virtual PlayerManagerImplementation's trade path only
								// calls for isPetControlDevice()/isVehicleControlDevice()/
								// isShipControlDevice() objects), CompanionControlDevice
								// extends IntangibleObject directly and was never covered
								// by any of those branches -- so without this, the device
								// (and the companion bound to it) could be freely traded,
								// vendor-sold, or bazaar-listed to any other player. Using
								// the generic SceneObject-level no-trade flag instead of
								// building out a full canBeTradedTo() override, since
								// there is no legitimate case for transferring a
								// rank-gated, skill-progress-bearing companion to another
								// player. See docs/companion_system/NOTES.md.
								device->setForceNoTrade(true);

								datapad->transferObject(device, -1, true);
								datapad->broadcastObject(device, true);

								// Companion System (2026-07-14, "player-side loadout
								// backpack" redesign -- see NOTES.md): a real backpack
								// dropped straight into the player's own inventory the
								// moment they get their first companion -- whatever
								// weapon/wearable is placed inside it auto-equips onto
								// the companion (CompanionLoadoutContainerComponent),
								// with any previously-equipped occupant displaced into
								// the player's main inventory to make room. Created
								// once per PLAYER (not once per companion -- see
								// "test 5 companions at once" pass below), not
								// recreated on every summon the way the old
								// companion-side bag was, since this backpack belongs
								// to the player, not any one (re)spawned companion
								// actor. Not fatal if this fails (logged, not
								// blocking) -- companions are already fully usable
								// via the existing radial/equip commands either way.
								//
								// Companion System (2026-07-15, "test 5 companions at
								// once" pass): still just ONE shared backpack even
								// with multiple companions granted in this same loop
								// -- extending its auto-equip target resolution to
								// pick which of several summoned companions a
								// dropped item should go to is a real, separate
								// design question (flagged in NOTES.md, not answered
								// by the user yet), out of scope for this pass.
								// grantedLoadoutBackpack guards against creating one
								// per loop iteration.
								if (!grantedLoadoutBackpack) {
									ManagedReference<SceneObject*> playerInventory = creature->getSlottedObject("inventory");

									if (playerInventory != nullptr) {
										ManagedReference<SceneObject*> loadoutBackpack = zoneServer->createObject(String("object/tangible/inventory/companion_loadout_backpack.iff").hashCode(), 1);

										if (loadoutBackpack == nullptr) {
											creature->error("Companion System: could not create companion loadout backpack object");
										} else {
											Locker backpackLocker(loadoutBackpack, creature);

											// 2026-07-20 (user: window labeling) -- name it
											// clearly so the loadout window title reads
											// "Companion Loadout (equipped gear)" instead of
											// a bare/blank label.
											loadoutBackpack->setCustomObjectName("Companion Loadout (equipped gear)", false);

											if (!playerInventory->transferObject(loadoutBackpack, -1, true)) {
												creature->error("Companion System: could not place companion loadout backpack into player "
														+ String::valueOf(creature->getObjectID()) + "'s own inventory");
											} else {
												playerInventory->broadcastObject(loadoutBackpack, true);
											}
										}
									}

									grantedLoadoutBackpack = true;
								}
							} else {
								creature->error("Companion System: failed to create companion control device / companion actor on companion_master_novice grant");
							}
						}

						creature->sendSystemMessage("@companion:companion_granted"); // A companion has been added to your datapad. Use it to call your companion out whenever you like.
					}
				}
			}
		}
	}

	/// Update client with new values for things like Terrain Negotiation
	CreatureObjectDeltaMessage4* msg4 = new CreatureObjectDeltaMessage4(creature);
	msg4->updateAccelerationMultiplierBase();
	msg4->updateAccelerationMultiplierMod();
	msg4->updateSpeedMultiplierBase();
	msg4->updateSpeedMultiplierMod();
	msg4->updateRunSpeed();
	msg4->updateTerrainNegotiation();
	msg4->close();
	creature->sendMessage(msg4);

	SkillModManager::instance()->verifySkillBoxSkillMods(creature);

	return true;
}

void SkillManager::removeSkillRelatedMissions(CreatureObject* creature, Skill* skill) {
	if(skill->getSkillName().hashCode() == STRING_HASHCODE("combat_bountyhunter_investigation_03")) {
		ManagedReference<ZoneServer*> zoneServer = creature->getZoneServer();
		if(zoneServer != nullptr) {
			ManagedReference<MissionManager*> missionManager = zoneServer->getMissionManager();
			if(missionManager != nullptr) {
				missionManager->failPlayerBountyMission(creature->getObjectID());
			}
		}
	}
}

bool SkillManager::surrenderSkill(const String& skillName, CreatureObject* creature, bool notifyClient, bool checkFrs) {
	Skill* skill = skillMap.get(skillName.hashCode());

	if (skill == nullptr)
		return false;

	Locker locker(creature);

	//If they have already surrendered the skill, then return true.
	if (!creature->hasSkill(skill->getSkillName()))
		return true;

	const SkillList* skillList = creature->getSkillList();

	for (int i = 0; i < skillList->size(); ++i) {
		Skill* checkSkill = skillList->get(i);

		if (checkSkill->isRequiredSkillOf(skill))
			return false;
	}

	if (skillName.beginsWith("force_") && !(JediManager::instance()->canSurrenderSkill(creature, skillName)))
		return false;

	removeSkillRelatedMissions(creature, skill);

	creature->removeSkill(skill, notifyClient);

	//Remove skill modifiers
	auto skillModifiers = skill->getSkillModifiers();

	ManagedReference<PlayerObject*> ghost = creature->getPlayerObject();

	for (int i = 0; i < skillModifiers->size(); ++i) {
		auto entry = &skillModifiers->elementAt(i);
		creature->removeSkillMod(SkillModManager::SKILLBOX, entry->getKey(), entry->getValue(), notifyClient);

	}

	if (ghost != nullptr) {
		//Give the player the used skill points back.
		ghost->addSkillPoints(skill->getSkillPointsRequired());

		//Remove abilities but only if the creature doesn't still have a skill that grants the
		//ability.  Some abilities are granted by multiple skills. For example Dazzle for dancers
		//and musicians.
		auto skillAbilities = skill->getAbilities();
		if (skillAbilities->size() > 0) {
			SortedVector<String> abilitiesLost;
			for (int i = 0; i < skillAbilities->size(); i++) {
				abilitiesLost.put(skillAbilities->get(i));
			}
			for (int i = 0; i < skillList->size(); i++) {
				Skill* remainingSkill = skillList->get(i);
				auto remainingAbilities = remainingSkill->getAbilities();
				for(int j = 0; j < remainingAbilities->size(); j++) {
					if (abilitiesLost.contains(remainingAbilities->get(j))) {
						abilitiesLost.drop(remainingAbilities->get(j));
						if (abilitiesLost.size() == 0) {
							break;
						}
					}
				}
			}
			if (abilitiesLost.size() > 0) {
				removeAbilities(ghost, abilitiesLost, notifyClient);
			}
		}

		//Remove draft schematic groups
		auto schematicsGranted = skill->getSchematicsGranted();
		SchematicMap::instance()->removeSchematics(ghost, *schematicsGranted, notifyClient);

		//Update maximum experience.
		updateXpLimits(ghost);

		FrsManager* frsManager = creature->getZoneServer()->getFrsManager();

		if (checkFrs && frsManager->isFrsEnabled()) {
			frsManager->handleSkillRevoked(creature, skillName);
		}

		/// Update Force Power Max
		ghost->recalculateForcePower();

		const SkillList* list = creature->getSkillList();

		int totalSkillPointsWasted = 250;

		for (int i = 0; i < list->size(); ++i) {
			Skill* skill = list->get(i);

			totalSkillPointsWasted -= skill->getSkillPointsRequired();
		}

		if (ghost->getSkillPoints() != totalSkillPointsWasted) {
			creature->error("skill points mismatch calculated: " + String::valueOf(totalSkillPointsWasted) + " found: " + String::valueOf(ghost->getSkillPoints()));
			ghost->setSkillPoints(totalSkillPointsWasted);
		}

		ManagedReference<PlayerManager*> playerManager = creature->getZoneServer()->getPlayerManager();
		if (playerManager != nullptr) {
			creature->setLevel(playerManager->calculatePlayerLevel(creature));
		}

		MissionManager* missionManager = creature->getZoneServer()->getMissionManager();

		if (skill->getSkillName() == "combat_bountyhunter_investigation_03"){
			if (missionManager != NULL)
				missionManager->removePlayerFromBountyList(creature->getObjectID());
		}

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

	/// Update client with new values for things like Terrain Negotiation
	CreatureObjectDeltaMessage4* msg4 = new CreatureObjectDeltaMessage4(creature);
	msg4->updateAccelerationMultiplierBase();
	msg4->updateAccelerationMultiplierMod();
	msg4->updateSpeedMultiplierBase();
	msg4->updateSpeedMultiplierMod();
	msg4->updateRunSpeed();
	msg4->updateTerrainNegotiation();
	msg4->close();
	creature->sendMessage(msg4);

	SkillModManager::instance()->verifySkillBoxSkillMods(creature);
	JediManager::instance()->onSkillRevoked(creature, skill);

	return true;
}

void SkillManager::surrenderAllSkills(CreatureObject* creature, bool notifyClient, bool removeForceProgression) {
	ManagedReference<PlayerObject*> ghost = creature->getPlayerObject();

	const SkillList* skillList = creature->getSkillList();

	Vector<String> listOfNames;
	skillList->getStringList(listOfNames);
	SkillList copyOfList;

	copyOfList.loadFromNames(listOfNames);

	for (int i = 0; i < copyOfList.size(); i++) {
		Skill* skill = copyOfList.get(i);

		if (skill->getSkillPointsRequired() >= 0) {
			if (!removeForceProgression and skill->getSkillName().contains("force_title_"))
				continue;

			if (skill->getSkillName().contains("admin_"))
				continue;

			removeSkillRelatedMissions(creature, skill);

			creature->removeSkill(skill, notifyClient);

			//Remove skill modifiers
			auto skillModifiers = skill->getSkillModifiers();

			for (int i = 0; i < skillModifiers->size(); ++i) {
				auto entry = &skillModifiers->elementAt(i);
				creature->removeSkillMod(SkillModManager::SKILLBOX, entry->getKey(), entry->getValue(), notifyClient);
			}

			if (ghost != nullptr) {
				//Give the player the used skill points back.
				ghost->addSkillPoints(skill->getSkillPointsRequired());

				//Remove abilities
				auto abilityNames = skill->getAbilities();
				removeAbilities(ghost, *abilityNames, notifyClient);

				//Remove draft schematic groups
				auto schematicsGranted = skill->getSchematicsGranted();
				SchematicMap::instance()->removeSchematics(ghost, *schematicsGranted, notifyClient);
				JediManager::instance()->onSkillRevoked(creature, skill);
			}
		}
	}

	SkillModManager::instance()->verifySkillBoxSkillMods(creature);

	if (ghost != nullptr) {
		//Update maximum experience.
		updateXpLimits(ghost);

		/// update force
		ghost->recalculateForcePower();
	}

	ManagedReference<PlayerManager*> playerManager = creature->getZoneServer()->getPlayerManager();
	if (playerManager != nullptr) {
		creature->setLevel(playerManager->calculatePlayerLevel(creature));
	}

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
		//Add draft schematic groups
		auto schematicsGranted = skill->getSchematicsGranted();
		SchematicMap::instance()->addSchematics(ghost, *schematicsGranted, notifyClient);
	}
}

void SkillManager::updateXpLimits(PlayerObject* ghost) {
	if (ghost == nullptr || !ghost->isPlayerObject()) {
		return;
	}

	VectorMap<String, int>* xpTypeCapList = ghost->getXpTypeCapList();
	xpTypeCapList->removeAll();

	//Iterate over the player skills and update xp limits accordingly.
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

	//Add defaults when no skill box caps exist
	for (int i = 0; i < defaultXpLimits.size(); ++i) {
		String xpType = defaultXpLimits.elementAt(i).getKey();
		int xpLimit = defaultXpLimits.elementAt(i).getValue();

		if (!xpTypeCapList->contains(xpType))
			xpTypeCapList->put(xpType, xpLimit);
	}

	//Iterate over the player xp types and cap all xp types to the limits.
	DeltaVectorMap<String, int>* experienceList = ghost->getExperienceList();

	for (int i = 0; i < experienceList->size(); ++i) {
		String xpType = experienceList->getKeyAt(i);
		if (experienceList->get(xpType) > xpTypeCapList->get(xpType)) {
			ghost->addExperience(xpType, xpTypeCapList->get(xpType) - experienceList->get(xpType), true);
		}
	}
}

bool SkillManager::canLearnSkill(const String& skillName, CreatureObject* creature, bool noXpRequired) {
	Skill* skill = skillMap.get(skillName.hashCode());

	if (skill == nullptr) {
		return false;
	}

	//If they already have the skill, then return false.
	if (creature->hasSkill(skillName)) {
		return false;
	}

	if (!fulfillsSkillPrerequisites(skillName, creature)) {
		return false;
	}

	ManagedReference<PlayerObject* > ghost = creature->getPlayerObject();
	if (ghost != nullptr) {
		//Companion System (2026-07-18, per user request "no skills require
		//xp"): the XP-cost gate is disabled server-wide -- training any
		//skill (including the Companion Handler tree) never requires the
		//player to have earned the XP first. The original check is kept
		//here, commented, for easy re-enabling:
		//if (!noXpRequired) {
		//	if (ghost->getExperience(skill->getXpType()) < skill->getXpCost()) {
		//		return false;
		//	}
		//}

		//Companion System (spec: Core3 Registry Patches): the companion_master
		//profession always costs 0 Skill Points, regardless of the value
		//authored in the skill tree data -- see docs/companion_system/NOTES.md,
		//"Skill point isolation / companion_master player profession".
		bool isCompanionMasterSkill = skillName.beginsWith("companion_master_");

		//Check if player has enough skill points to learn the skill.
		if (!isCompanionMasterSkill && ghost->getSkillPoints() < skill->getSkillPointsRequired()) {
			return false;
		}
	} else {
		//Could not retrieve player object.
		return false;
	}

	//Check for precluded skills.
	auto skillsPrecluded = skill->getSkillsPrecluded();
	for (int i = 0; i < skillsPrecluded->size(); ++i) {
		const String& precludedSkillName = skillsPrecluded->get(i);
		Skill* precludedSkill = skillMap.get(precludedSkillName.hashCode());

		if (precludedSkill == NULL) {
			continue;
		}

		if (creature->hasSkill(precludedSkillName)) {
			return false;
		}
	}



	return true;
}

bool SkillManager::fulfillsSkillPrerequisitesAndXp(const String& skillName, CreatureObject* creature) {
	if (!fulfillsSkillPrerequisites(skillName, creature)) {
		return false;
	}

	Skill* skill = skillMap.get(skillName.hashCode());

	if (skill == nullptr) {
		return false;
	}

	ManagedReference<PlayerObject* > ghost = creature->getPlayerObject();
	if (ghost != nullptr) {
		//Companion System (2026-07-18, "no skills require xp" -- see
		//canLearnSkill() above): XP gate disabled server-wide. Original
		//check kept for easy re-enabling:
		//if (skill->getXpCost() > 0 && ghost->getExperience(skill->getXpType()) < skill->getXpCost()) {
		//	return false;
		//}
	}

	//Check for precluded skills.
	auto skillsPrecluded = skill->getSkillsPrecluded();
	for (int i = 0; i < skillsPrecluded->size(); ++i) {
		const String& precludedSkillName = skillsPrecluded->get(i);
		Skill* precludedSkill = skillMap.get(precludedSkillName.hashCode());

		if (precludedSkill == NULL) {
			continue;
		}

		if (creature->hasSkill(precludedSkillName)) {
			return false;
		}
	}


	return true;
}

bool SkillManager::fulfillsSkillPrerequisites(const String& skillName, CreatureObject* creature) {
	Skill* skill = skillMap.get(skillName.hashCode());

	if (skill == nullptr) {
		return false;
	}

	if (skillName.contains("admin_") && !(creature->getPlayerObject()->getAdminLevel() > 0)) {
		return false;
	}

	auto requiredSpecies = skill->getSpeciesRequired();
	if (requiredSpecies->size() > 0) {
		bool foundSpecies = false;
		for (int i = 0; i < requiredSpecies->size(); i++) {
			if (creature->getSpeciesName() == requiredSpecies->get(i)) {
				foundSpecies = true;
				break;
			}
		}
		if (!foundSpecies) {
			return false;
		}
	}

	//Check for required skills.
	auto requiredSkills = skill->getSkillsRequired();
	for (int i = 0; i < requiredSkills->size(); ++i) {
		const String& requiredSkillName = requiredSkills->get(i);
		Skill* requiredSkill = skillMap.get(requiredSkillName.hashCode());

		if (requiredSkill == nullptr) {
			continue;
		}

		if (!creature->hasSkill(requiredSkillName)) {
			return false;
		}
	}

	//Check for precluded skills.
	auto skillsPrecluded = skill->getSkillsPrecluded();
	for (int i = 0; i < skillsPrecluded->size(); ++i) {
		const String& precludedSkillName = skillsPrecluded->get(i);
		Skill* precludedSkill = skillMap.get(precludedSkillName.hashCode());

		if (precludedSkill == NULL) {
			continue;
		}

		if (creature->hasSkill(precludedSkillName)) {
			return false;
		}
	}


	PlayerObject* ghost = creature->getPlayerObject();
	if (ghost == nullptr || ghost->getJediState() < skill->getJediStateRequired()) {
		return false;
	}

	if (ghost->isPrivileged())
		return true;

	if (skillName.beginsWith("force_")) {
		return JediManager::instance()->canLearnSkill(creature, skillName);
	}

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


	for (int i = 0; i < skillList->size(); ++i) {
		Skill* skill = skillList->get(i);

		String skillName = skill->getSkillName();
		if (skillName.contains("jedi_") &&
			(skillName.contains("_master_master"))) {
			return true;

		}
	}


	return false;
}
