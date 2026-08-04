/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System -- see CampDeploymentManager.h and NOTES.md.

	2026-07-18 FULL REWRITE ("wild camp & buff" phase 1): the original
	deployCamp() passed a TANGIBLE tent template into StructureManager::
	placeCamp() (CampStructureTemplate cast always failed) and never created
	a CampSiteActiveArea (no medical rating). This file mirrors the real
	CampKitMenuComponent.cpp flow end to end.

	2026-07-18 SECOND REVISION (live test feedback): (1) crafting now
	enforces a real per-tier RECIPE -- exact resource classes and amounts
	(hide/bone/metal) summed across the companion's bag -- instead of "any
	100 units of anything" (live-caught: a tent got crafted from bone alone
	with no hide anywhere); shortfalls are itemized in spatial chat, and the
	exact consumption is announced on completion. (2) The owner now CHOOSES
	the tent: deployCamp() opens a picker listing every tier within the
	companion's training, each marked carried / craftable-with-recipe
	(CompanionCampChoiceSuiCallback -> deployCampTier()).
*/

#include "CampDeploymentManager.h"
#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/objects/creature/buffs/PerformanceBuff.h"
#include "server/zone/objects/creature/buffs/PerformanceBuffType.h"
#include "server/zone/objects/area/CampSiteActiveArea.h"
#include "server/zone/managers/combat/CombatManager.h"
#include "server/zone/managers/skill/Performance.h"
#include "server/zone/managers/companion/CompanionCraftingManager.h"
#include "server/ServerCore.h"
#include "templates/params/creature/CreaturePosture.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/objects/scene/SceneObject.h"
#include "server/zone/objects/tangible/TangibleObject.h"
#include "server/zone/objects/tangible/terminal/Terminal.h"
#include "server/zone/objects/structure/StructureObject.h"
#include "server/zone/objects/area/ActiveArea.h"
#include "server/zone/objects/area/CampSiteActiveArea.h"
#include "server/zone/objects/resource/ResourceContainer.h"
#include "server/zone/objects/resource/ResourceSpawn.h"
#include "server/zone/objects/region/CityRegion.h"
#include "server/zone/managers/structure/StructureManager.h"
#include "server/zone/managers/planet/PlanetManager.h"
#include "server/zone/managers/companion/callbacks/CompanionCampChoiceSuiCallback.h"
#include "server/zone/packets/player/PlayMusicMessage.h"
#include "server/chat/ChatManager.h"
#include "server/zone/Zone.h"
#include "server/zone/ZoneServer.h"
#include "templates/tangible/CampKitTemplate.h"
#include "templates/building/CampStructureTemplate.h"
#include "templates/manager/TemplateManager.h"
#include "server/zone/managers/collision/CollisionManager.h" // Entertainer Dance/Watch (2026-07-29) -- one-time + ongoing LOS check
#include "server/zone/managers/skill/PerformanceManager.h" // Entertainer Dance/Watch (2026-07-29) -- PerformanceManager::HEAL_RANGE
#include "server/zone/objects/tangible/weapon/WeaponObject.h" // Entertainer Dance/Watch (2026-07-29) -- watcher weapon unequip/re-equip
#include <cmath>

namespace {

	// genesis port: PerformanceManager::HEAL_RANGE does not exist on this base.
	// Upstream's value is 60. Genesis's own entertainer code uses bare literals
	// (10.0f patron, 40.0f group), so this keeps upstream behaviour explicitly.
	static constexpr float COMPANION_HEAL_RANGE = 60.0f;

	// The six real camp kit tiers (c3r chat's camp catalog, NOTES.md
	// 2026-07-14), ordered worst -> best, each with its crafting recipe.
	// Recipe classes are resource-tree tokens (ResourceSpawn::isType() --
	// lowercase, same style as the stock "organic"/"inorganic" checks).
	struct CampKitTier {
		const char* templatePath;
		const char* displayName;
		int skillRequired;
		int hideUnits;
		int boneUnits;
		int metalUnits;
	};

	const CampKitTier CAMP_KIT_TIERS[] = {
		{ "object/tangible/scout/camp/camp_basic.iff", "Basic Camp", 5, 50, 30, 0 },
		{ "object/tangible/scout/camp/camp_multi.iff", "Multiperson Camp", 10, 80, 50, 0 },
		{ "object/tangible/scout/camp/camp_improved.iff", "Improved Camp", 30, 120, 80, 0 },
		{ "object/tangible/scout/camp/camp_quality.iff", "High Quality Camp", 50, 160, 100, 60 },
		{ "object/tangible/scout/camp/camp_elite.iff", "Field Base Camp", 65, 200, 140, 100 },
		{ "object/tangible/scout/camp/camp_luxury.iff", "Luxury Camp", 85, 250, 180, 140 }
	};

	constexpr int CAMP_KIT_TIER_COUNT = sizeof(CAMP_KIT_TIERS) / sizeof(CAMP_KIT_TIERS[0]);

	const char* RECIPE_CLASSES[3] = { "hide", "bone", "metal" };

	int recipeAmount(const CampKitTier& tier, int classIndex) {
		switch (classIndex) {
		case 0:
			return tier.hideUnits;
		case 1:
			return tier.boneUnits;
		case 2:
			return tier.metalUnits;
		default:
			return 0;
		}
	}

	void companionSay(CompanionObject* companion, const String& text) {
		if (companion == nullptr) {
			return;
		}

		ZoneServer* zoneServer = companion->getZoneServer();

		if (zoneServer == nullptr) {
			return;
		}

		ChatManager* chatManager = zoneServer->getChatManager();

		if (chatManager != nullptr) {
			chatManager->broadcastChatMessage(companion, UnicodeString(text), 0, 0, 0, 0, 1);
		}
	}

	// Scans the companion's top-level container AND its "inventory" bag
	// child, collecting every matching tangible (nullptr-safe).
	template <typename Predicate>
	void collectCompanionItems(CompanionObject* companion, Vector<ManagedReference<TangibleObject*> >& out, Predicate matches) {
		auto scan = [&](SceneObject* container) {
			if (container == nullptr) {
				return;
			}

			for (int i = 0; i < container->getContainerObjectsSize(); ++i) {
				ManagedReference<SceneObject*> obj = container->getContainerObject(i);

				if (obj == nullptr || !obj->isTangibleObject()) {
					continue;
				}

				TangibleObject* tano = obj.castTo<TangibleObject*>().get();

				if (tano != nullptr && matches(tano)) {
					out.add(tano);
				}
			}
		};

		if (companion != nullptr) {
			scan(companion);
			scan(companion->getSlottedObject("inventory"));
		}
	}

	/** All resource containers in the companion's bag matching one recipe
	 * class token. */
	void collectResourceContainers(CompanionObject* companion, const String& classToken, Vector<ManagedReference<TangibleObject*> >& out) {
		collectCompanionItems(companion, out, [&classToken](TangibleObject* tano) -> bool {
			if (!tano->isResourceContainer()) {
				return false;
			}

			ResourceContainer* rc = cast<ResourceContainer*>(tano);

			if (rc == nullptr || rc->getQuantity() <= 0) {
				return false;
			}

			ManagedReference<ResourceSpawn*> spawn = rc->getSpawnObject();

			return spawn != nullptr && spawn->isType(classToken);
		});
	}

	int countResourceUnits(CompanionObject* companion, const String& classToken) {
		Vector<ManagedReference<TangibleObject*> > containers;
		collectResourceContainers(companion, classToken, containers);

		int total = 0;

		for (int i = 0; i < containers.size(); ++i) {
			ResourceContainer* rc = cast<ResourceContainer*>(containers.get(i).get());

			if (rc != nullptr) {
				total += rc->getQuantity();
			}
		}

		return total;
	}

	/** Consumes `amount` units of the class across however many containers
	 * it takes; appends "N units of <SpawnName>" chunks to consumedDesc.
	 * @pre: enough units exist (checked by caller); companion locked. */
	void consumeResourceUnits(CompanionObject* companion, const String& classToken, int amount, String& consumedDesc) {
		Vector<ManagedReference<TangibleObject*> > containers;
		collectResourceContainers(companion, classToken, containers);

		int remaining = amount;

		for (int i = 0; i < containers.size() && remaining > 0; ++i) {
			ResourceContainer* rc = cast<ResourceContainer*>(containers.get(i).get());

			if (rc == nullptr) {
				continue;
			}

			int take = Math::min(remaining, rc->getQuantity());

			if (take <= 0) {
				continue;
			}

			Locker rlocker(rc, companion);

			String spawnName = rc->getSpawnName();
			// genesis port: dropped the 4th argument (destroyEmpty = true) -- genesis's
			// ResourceContainer::setQuantity(quantity, notifyClient, ignoreMax) has only 3
			// parameters. Nothing is lost: the newer base's destroyEmpty defaults to true and
			// this call passed true, and genesis unconditionally destroys the container when
			// stackQuantity drops below 1 -- identical behaviour.
			rc->setQuantity(rc->getQuantity() - take, true, false);

			remaining -= take;

			if (!consumedDesc.isEmpty()) {
				consumedDesc += ", ";
			}

			consumedDesc += String::valueOf(take) + " " + spawnName + " (" + classToken + ")";
		}
	}

	int tierIndexForKit(TangibleObject* kit) {
		if (kit == nullptr || kit->getObjectTemplate() == nullptr) {
			return -1;
		}

		String path = kit->getObjectTemplate()->getFullTemplateString();

		for (int i = 0; i < CAMP_KIT_TIER_COUNT; ++i) {
			if (path == CAMP_KIT_TIERS[i].templatePath) {
				return i;
			}
		}

		return -1;
	}

	TangibleObject* findCarriedKitOfTier(CompanionObject* companion, int tierIndex) {
		Vector<ManagedReference<TangibleObject*> > kits;

		collectCompanionItems(companion, kits, [tierIndex](TangibleObject* tano) -> bool {
			return tierIndexForKit(tano) == tierIndex;
		});

		return kits.size() > 0 ? kits.get(0).get() : nullptr;
	}

	String recipeSummary(const CampKitTier& tier) {
		String out;

		for (int c = 0; c < 3; ++c) {
			int amount = recipeAmount(tier, c);

			if (amount <= 0) {
				continue;
			}

			if (!out.isEmpty()) {
				out += ", ";
			}

			out += String::valueOf(amount) + " " + RECIPE_CLASSES[c];
		}

		return out;
	}

	/** All of the owner's currently summoned, living companions (duplicated
	 * per this project's per-file-copy convention). */
	void resolveOwnersCompanionsForFetch(CreatureObject* owner, Vector<CompanionObject*>& out) {
		if (owner == nullptr) {
			return;
		}

		ManagedReference<SceneObject*> datapad = owner->getSlottedObject("datapad");

		if (datapad == nullptr) {
			return;
		}

		for (int i = 0; i < datapad->getContainerObjectsSize(); ++i) {
			ManagedReference<SceneObject*> obj = datapad->getContainerObject(i);

			if (obj == nullptr || !obj->isCompanionControlDevice()) {
				continue;
			}

			CompanionControlDevice* device = cast<CompanionControlDevice*>(obj.get());

			if (device == nullptr || device->isCompanionDead()) {
				continue;
			}

			CompanionObject* comp = device->getCompanionObject();

			if (comp == nullptr || comp->getZone() == nullptr || comp->isDead()) {
				continue;
			}

			if (comp->getLinkedCreature().get() != owner) {
				continue;
			}

			out.add(comp);
		}
	}

	/** Phase 4 fetch bookkeeping carried across the walk-to-meet chain. */
	class MaterialFetchState : public Object {
	public:
		uint64 rangerID = 0;
		uint64 donorID = 0;
		Vector<uint64> itemIDs;
		int tierIndex = 0;
		int steps = 0;
	};

	void scheduleFetchStep(ZoneServer* zoneServer, Reference<CreatureObject*> ownerRef, Reference<MaterialFetchState*> state, int delayMs);

	void resumeFollowAfterFetch(CompanionObject* comp, CreatureObject* owner) {
		if (comp == nullptr || comp->isDead() || comp->getZone() == nullptr) {
			return;
		}

		comp->setCompanionState(CompanionObject::FOLLOW);
		comp->setFollowObject(owner);
		comp->setFollowState(AiAgent::FOLLOWING); // genesis port: was setMovementState()
		comp->clearPatrolPoints();
	}

	void runFetchStep(ZoneServer* zoneServer, Reference<CreatureObject*> ownerRef, Reference<MaterialFetchState*> state) {
		CreatureObject* owner = ownerRef.get();

		if (owner == nullptr || state == nullptr || zoneServer == nullptr) {
			return;
		}

		ManagedReference<SceneObject*> rangerObj = zoneServer->getObject(state->rangerID);
		ManagedReference<SceneObject*> donorObj = zoneServer->getObject(state->donorID);

		CompanionObject* ranger = rangerObj != nullptr ? rangerObj.castTo<CompanionObject*>().get() : nullptr;
		CompanionObject* donor = donorObj != nullptr ? donorObj.castTo<CompanionObject*>().get() : nullptr;

		// 2026-07-20 (user: "companions get stuck after a failed tent setup")
		// -- if either party vanished, still send whichever survives back to
		// following the owner so it's never stranded in PATROL.
		if (ranger == nullptr || donor == nullptr) {
			if (owner != nullptr) {
				// Single locks -- resumeFollowAfterFetch only mutates the
				// companion, so no owner cross-lock is needed (and owner
				// isn't held here anyway -- crash lesson).
				if (ranger != nullptr) {
					Locker rl(ranger);
					resumeFollowAfterFetch(ranger, owner);
				}

				if (donor != nullptr) {
					Locker dl(donor);
					resumeFollowAfterFetch(donor, owner);
				}
			}

			return;
		}

		Locker rlocker(ranger);
		Locker dlocker(donor, ranger);

		// Abort conditions: someone died/despawned/zoned, or combat broke out.
		if (ranger->getZone() == nullptr || donor->getZone() == nullptr || ranger->getZone() != donor->getZone()
				|| ranger->isDead() || donor->isDead() || ranger->isInCombat() || donor->isInCombat() || owner->isInCombat()) {
			resumeFollowAfterFetch(ranger, owner);
			resumeFollowAfterFetch(donor, owner);
			return;
		}

		// 60-second cap on the meet-up.
		if (++state->steps > 150) {
			companionSay(ranger, "We couldn't meet up -- never mind for now.");
			resumeFollowAfterFetch(ranger, owner);
			resumeFollowAfterFetch(donor, owner);
			return;
		}

		float dist = ranger->getDistanceTo(donor);

		if (dist > 5.f) {
			// Both walk toward each other -- each targets the OTHER's
			// current position; points are re-added as pathing consumes
			// them, so the pair naturally converges in the middle.
			auto walkToward = [](CompanionObject* walker, CompanionObject* target) {
				walker->setCompanionState(CompanionObject::PATROL);
				walker->setFollowObject(nullptr);

				if (walker->getPatrolPointSize() == 0) {
					PatrolPoint point(target->getPositionX(), target->getPositionZ(), target->getPositionY());
					// genesis port: setMovementState() -> setFollowState(); genesis
					// setFollowState() calls clearPatrolPoints(), so the state must be
					// set BEFORE the point is queued (see PetPatrolCommand.h).
					walker->setFollowState(AiAgent::PATROLLING);
					walker->addPatrolPoint(point);
				}
			};

			walkToward(ranger, donor);
			walkToward(donor, ranger);

			scheduleFetchStep(zoneServer, ownerRef, state, 400);
			return;
		}

		// Met: face each other, hand everything over, celebrate.
		ranger->faceObject(donor, true);
		donor->faceObject(ranger, true);

		SceneObject* rangerBag = ranger->getSlottedObject("inventory");
		SceneObject* destination = rangerBag != nullptr ? rangerBag : static_cast<SceneObject*>(ranger);

		int handed = 0;

		for (int i = 0; i < state->itemIDs.size(); ++i) {
			ManagedReference<SceneObject*> item = zoneServer->getObject(state->itemIDs.get(i));

			if (item == nullptr || item->getRootParent() != donor) {
				continue; // moved/consumed since the fetch started
			}

			Locker itemLocker(item, ranger);

			if (destination->transferObject(item, -1, true)) {
				++handed;
			}
		}

		if (handed > 0) {
			companionSay(donor, "Here you go -- everything I was carrying for the job!");
			companionSay(ranger, "Perfect. High five!");

			// "highfive" if the client animation table has it; harmless
			// no-op otherwise (swap to "cheer" if it doesn't render --
			// flagged for live test).
			ranger->doAnimation("highfive");
			donor->doAnimation("highfive");
		} else {
			companionSay(donor, "Huh -- I don't have it anymore. Sorry!");
		}

		resumeFollowAfterFetch(ranger, owner);
		resumeFollowAfterFetch(donor, owner);

		// Retry the craft/deploy with the fresh materials aboard.
		if (handed > 0) {
			CampDeploymentManager::instance()->deployCampTier(owner, ranger, state->tierIndex);
		}
	}

	void scheduleFetchStep(ZoneServer* zoneServer, Reference<CreatureObject*> ownerRef, Reference<MaterialFetchState*> state, int delayMs) {
		Core::getTaskManager()->scheduleTask([zoneServer, ownerRef, state] () {
			runFetchStep(zoneServer, ownerRef, state);
		}, "CompanionMaterialFetchLambda", delayMs);
	}

}

CampDeploymentManager::CampDeploymentManager() : Logger("CampDeploymentManager") {
	setGlobalLogging(true);
	setLogging(false);
}

int CampDeploymentManager::getTierCount() const {
	return CAMP_KIT_TIER_COUNT;
}

bool CampDeploymentManager::isTierWithinTraining(CompanionObject* companion, int tierIndex) const {
	if (tierIndex < 0 || tierIndex >= CAMP_KIT_TIER_COUNT) {
		return false;
	}

	return CAMP_KIT_TIERS[tierIndex].skillRequired <= companionCampSkillCap(companion);
}

String CampDeploymentManager::describeTierForPicker(CompanionObject* companion, int tierIndex) const {
	if (tierIndex < 0 || tierIndex >= CAMP_KIT_TIER_COUNT) {
		return "";
	}

	const CampKitTier& tier = CAMP_KIT_TIERS[tierIndex];

	if (findCarriedKitOfTier(companion, tierIndex) != nullptr) {
		return String(tier.displayName) + "  [carried -- ready to deploy]";
	}

	return String(tier.displayName) + "  [craft: " + recipeSummary(tier) + "]";
}

bool CampDeploymentManager::companionHasRangerOrScoutTraining(CompanionObject* companion) const {
	if (companion == nullptr) {
		return false;
	}

	for (int i = 0; i < companion->getLearnedSkillCount(); ++i) {
		const String& skill = companion->getLearnedSkill(i);

		if (skill.beginsWith("outdoors_ranger_") || skill.beginsWith("outdoors_scout_")) {
			return true;
		}
	}

	return false;
}

int CampDeploymentManager::companionCampSkillCap(CompanionObject* companion) const {
	if (companion == nullptr) {
		return 0;
	}

	bool hasScout = false;

	for (int i = 0; i < companion->getLearnedSkillCount(); ++i) {
		const String& skill = companion->getLearnedSkill(i);

		if (skill.beginsWith("outdoors_ranger_")) {
			return 100; // full ranger: every tier, luxury included
		}

		if (skill.beginsWith("outdoors_scout_")) {
			hasScout = true;
		}
	}

	return hasScout ? 50 : 0; // scout-only tops out at High Quality
}

bool CampDeploymentManager::isSlopeAcceptable(Zone* zone, float x, float y, float maxSlope) const {
	if (zone == nullptr) {
		return false;
	}

	float centerHeight = zone->getHeight(x, y);

	static const float sampleOffsets[4][2] = {
		{ 1.f, 0.f }, { -1.f, 0.f }, { 0.f, 1.f }, { 0.f, -1.f }
	};

	for (int i = 0; i < 4; ++i) {
		float sampleX = x + sampleOffsets[i][0];
		float sampleY = y + sampleOffsets[i][1];

		float sampleHeight = zone->getHeight(sampleX, sampleY);

		if (std::fabs(sampleHeight - centerHeight) > maxSlope) {
			return false;
		}
	}

	return true;
}

void CampDeploymentManager::deployCamp(CreatureObject* owner, CompanionObject* companion) {
	if (owner == nullptr || companion == nullptr) {
		return;
	}

	if (!companionHasRangerOrScoutTraining(companion)) {
		owner->sendSystemMessage("@companion:camp_not_trained"); // Your companion has not been trained in the Ranger or Scout skill trees.
		return;
	}

	// 2026-07-18 second revision: the owner picks the tent.
	CompanionCampChoiceSuiCallback::sendChoiceBox(owner, companion);
}

void CampDeploymentManager::deployCampTier(CreatureObject* owner, CompanionObject* companion, int tierIndex) {
	if (owner == nullptr || companion == nullptr || tierIndex < 0 || tierIndex >= CAMP_KIT_TIER_COUNT) {
		return;
	}

	if (!companionHasRangerOrScoutTraining(companion)) {
		owner->sendSystemMessage("@companion:camp_not_trained");
		return;
	}

	if (!isTierWithinTraining(companion, tierIndex)) {
		owner->sendSystemMessage("Your companion's training isn't advanced enough for that tent.");
		return;
	}

	TangibleObject* kit = findCarriedKitOfTier(companion, tierIndex);

	if (kit != nullptr) {
		deployCampFromKit(owner, companion, kit, tierIndex);
	} else {
		craftCampKit(owner, companion, tierIndex);
	}
}

void CampDeploymentManager::deployCampFromKit(CreatureObject* owner, CompanionObject* companion, TangibleObject* kit, int tierIndex) {
	Zone* zone = companion->getZone();
	ZoneServer* zoneServer = companion->getZoneServer();

	if (zone == nullptr || zoneServer == nullptr || owner->getZone() != zone || kit == nullptr) {
		return;
	}

	CampKitTemplate* kitData = dynamic_cast<CampKitTemplate*>(kit->getObjectTemplate());

	if (kitData == nullptr) {
		return;
	}

	String campStructurePath = kitData->getSpawnObjectTemplate();
	CampStructureTemplate* campStructureData = dynamic_cast<CampStructureTemplate*>(TemplateManager::instance()->getTemplate(campStructurePath.hashCode()));

	if (campStructureData == nullptr) {
		error() << "No CampStructureTemplate for companion camp kit: " << campStructurePath;
		return;
	}

	if (owner->isInCombat() || companion->isInCombat()) {
		owner->sendSystemMessage("@camp:sys_not_in_combat"); // You cannot make camp in combat!
		return;
	}

	// Camps are outdoor-only; the tent goes up at the COMPANION's feet.
	if (companion->getParent().get() != nullptr) {
		owner->sendSystemMessage("@camp:error_inside"); // You must be outside to make camp.
		return;
	}

	ManagedReference<CityRegion*> region = owner->getCityRegion().get();

	if (region != nullptr) {
		owner->sendSystemMessage("@camp:error_muni_true"); // You cannot make camp within city limits.
		return;
	}

	ManagedReference<PlayerObject*> ghost = owner->getPlayerObject();

	if (ghost == nullptr) {
		return;
	}

	// One camp per owner (camps are placed in the OWNER's name so the
	// medical/buff machinery attributes correctly).
	for (int i = 0; i < ghost->getTotalOwnedStructureCount(); ++i) {
		uint64 oid = ghost->getOwnedStructure(i);

		ManagedReference<StructureObject*> structure = zoneServer->getObject(oid).castTo<StructureObject*>();

		if (structure != nullptr && structure->isCampStructure()) {
			owner->sendSystemMessage("@camp:sys_already_camping"); // But you are already the owner of a camp!
			return;
		}
	}

	if (owner->getCurrentCamp() != nullptr) {
		owner->sendSystemMessage("@camp:error_camp_exists"); // You are already in a camp.
		return;
	}

	float x = companion->getPositionX();
	float y = companion->getPositionY();

	if (!isSlopeAcceptable(zone, x, y, 2.5f)) {
		owner->sendSystemMessage("@camp:error_nobuild"); // The ground here is too steep to make camp.
		return;
	}

	// Nearby camps / buildings (condensed from CampKitMenuComponent).
	// genesis port: QuadTreeEntry (genesis predates the QuadTreeEntry -> TreeEntry
	// rename) and the 6-arg 2D Zone::getInRangeObjects(x, y, range, objects,
	// readLockZone, includeBuildingObjects) -- the newer base's 3D overload took
	// (x, z, y, range, ...). Dropped the z argument. LOST: the query is now a
	// cylinder around (x, y) instead of a sphere around (x, z, y), so objects far
	// above/below the caller that the 3D form excluded can now match.
	SortedVector<ManagedReference<QuadTreeEntry*>> nearbyObjects;
	zone->getInRangeObjects(x, y, 512, &nearbyObjects, true, true);

	for (int i = 0; i < nearbyObjects.size(); ++i) {
		SceneObject* scno = cast<SceneObject*>(nearbyObjects.get(i).get());

		if (scno == nullptr) {
			continue;
		}

		if (scno->isCampStructure() && scno->getDistanceTo(companion) <= scno->getObjectTemplate()->getNoBuildRadius() + campStructureData->getRadius()) {
			owner->sendSystemMessage("@camp:error_camp_too_close"); // You are too close to another camp.
			return;
		}

		if (!scno->isCampStructure() && scno->isStructureObject() && scno->getDistanceTo(companion) <= 100) {
			owner->sendSystemMessage("@camp:error_building_too_close"); // You are too close to a building.
			return;
		}
	}

	PlanetManager* planetManager = zone->getPlanetManager();

	if (planetManager == nullptr || !planetManager->isCampingPermittedAt(x, y, campStructureData->getRadius())) {
		owner->sendSystemMessage("@camp:error_nobuild");
		return;
	}

	owner->sendSystemMessage("@camp:starting_camp"); // You start setting up camp.

	// genesis port: StructureManager::placeCamp() is a newer-base addition and does not
	// exist here. Genesis places camps through the generic
	// placeStructure(creature, templatePath, x, y, angle, persistenceLevel = 1) -- that is
	// exactly what genesis's own CampKitMenuComponent.cpp:202 calls for a camp kit, with
	// the same arguments. The dropped 2nd argument was CustomizationVariables* customVars,
	// which this call already passed as nullptr (and which placeCamp only used inside a
	// commented-out block upstream), so no customization is lost.
	// LOST: placeCamp created the object with the "playerstructures" database table and
	// resolved the template as a CampStructureTemplate; genesis's placeStructure resolves a
	// SharedStructureObjectTemplate and additionally does terrain snapping / flora clearing.
	StructureObject* campObject = StructureManager::instance()->placeStructure(owner, campStructurePath, x, y, (int) companion->getDirectionAngle());

	if (campObject == nullptr) {
		owner->sendSystemMessage("@camp:error_cmd_fail"); // Unable to build camp here.
		return;
	}

	// Terminal child -> named for the owner (identical to the real flow).
	Terminal* campTerminal = nullptr;

	SortedVector<ManagedReference<SceneObject*>>* childObjects = campObject->getChildObjects();

	for (int i = 0; i < childObjects->size(); ++i) {
		auto child = childObjects->get(i);

		if (child == nullptr || !child->isTerminal()) {
			continue;
		}

		campTerminal = child.castTo<Terminal*>();
		break;
	}

	if (campTerminal == nullptr) {
		campObject->destroyObjectFromWorld(true);
		campObject->destroyObjectFromDatabase(true);

		error() << "Companion camp has no terminal: " << campStructurePath;
		owner->sendSystemMessage("@camp:error_cmd_fail");
		return;
	}

	String campName = owner->getFirstName();

	if (!owner->getLastName().isEmpty()) {
		campName += " " + owner->getLastName();
	}

	campName += "'s Camp";
	campTerminal->setCustomObjectName(campName, true);

	// The CampSiteActiveArea is what actually makes the camp function
	// (medical rating for phase 2's wild buffs, XP, abandonment logic).
	String areaPath = "object/camp_area.iff";
	ManagedReference<CampSiteActiveArea*> campArea = (zoneServer->createObject(areaPath.hashCode(), 1)).castTo<CampSiteActiveArea*>();

	if (campArea == nullptr) {
		campObject->destroyObjectFromWorld(true);
		campObject->destroyObjectFromDatabase(true);

		owner->sendSystemMessage("@camp:error_cmd_fail");
		return;
	}

	Locker areaLocker(campArea, owner);

	campArea->init(campStructureData);
	campArea->setTerminal(campTerminal);
	campArea->setCamp(campObject);
	campArea->setOwner(owner);
	campArea->setAbandoned(false);

	// genesis port: was campArea->addAreaFlag(ActiveArea::NOBUILDZONEAREA) -- genesis's
	// ActiveArea has a plain boolean noBuildArea field (ActiveArea.idl:17 /
	// setNoBuildArea() :129), not an area-flag bitmask. This is exactly what stock
	// camp deployment does (CampKitMenuComponent.cpp:252).
	campArea->setNoBuildArea(true);
	campArea->initializePosition(x, 0, y);

	if (!zone->transferObject(campArea, -1, true)) {
		campObject->destroyObjectFromWorld(true);
		campObject->destroyObjectFromDatabase(true);

		campArea->destroyObjectFromDatabase(true);
		owner->sendSystemMessage("@camp:error_cmd_fail");
		return;
	}

	campObject->addActiveArea(campArea);

	owner->sendSystemMessage("@camp:camp_complete"); // Camp Complete!
	owner->notifyObservers(ObserverEventType::DEPLOYEDCAMP, campArea, 0);

	// Consume a use off the kit (destroys it at zero, stock behavior).
	Locker kitLocker(kit, owner);
	kit->decreaseUseCount();

	companionSay(companion, String(CAMP_KIT_TIERS[tierIndex].displayName) + " is up! Come get comfortable.");

	// Camp life (2026-07-20): kick off the ambiance loop -- idle companions
	// sit & sheath, an entertainer dances & buffs.
	startCampAmbiance(owner);
}

// Phase 4: companion-to-companion material fetch (2026-07-18, per user
// request "companions should interact with each other... trade the items" --
// see NOTES.md). Returns true when a fetch actually started.
bool CampDeploymentManager::startMaterialFetch(CreatureObject* owner, CompanionObject* ranger, int tierIndex, bool needsTool) {
	if (owner == nullptr || ranger == nullptr || tierIndex < 0 || tierIndex >= CAMP_KIT_TIER_COUNT) {
		return false;
	}

	// One meet-up at a time per ranger.
	if (!ranger->checkCooldownRecovery("companion_material_fetch")) {
		return false;
	}

	ZoneServer* zoneServer = ranger->getZoneServer();
	Zone* zone = ranger->getZone();

	if (zoneServer == nullptr || zone == nullptr) {
		return false;
	}

	const CampKitTier& tier = CAMP_KIT_TIERS[tierIndex];

	// What exactly is the ranger short on?
	Vector<String> missingClasses;

	for (int c = 0; c < 3; ++c) {
		int needed = recipeAmount(tier, c);

		if (needed > 0 && countResourceUnits(ranger, RECIPE_CLASSES[c]) < needed) {
			missingClasses.add(String(RECIPE_CLASSES[c]));
		}
	}

	if (!needsTool && missingClasses.size() == 0) {
		return false;
	}

	// Find a donor among the owner's OTHER summoned companions.
	Vector<CompanionObject*> siblings;
	resolveOwnersCompanionsForFetch(owner, siblings);

	CompanionObject* donor = nullptr;
	Vector<uint64> itemIDs;

	for (int s = 0; s < siblings.size() && donor == nullptr; ++s) {
		CompanionObject* sibling = siblings.get(s);

		if (sibling == nullptr || sibling == ranger || sibling->getZone() != zone) {
			continue;
		}

		Vector<uint64> candidateItems;

		if (needsTool) {
			Vector<ManagedReference<TangibleObject*> > tools;
			collectCompanionItems(sibling, tools, [](TangibleObject* tano) -> bool {
				return tano->isCraftingTool();
			});

			if (tools.size() > 0) {
				candidateItems.add(tools.get(0)->getObjectID());
			}
		}

		for (int c = 0; c < missingClasses.size(); ++c) {
			Vector<ManagedReference<TangibleObject*> > containers;
			collectResourceContainers(sibling, missingClasses.get(c), containers);

			for (int i = 0; i < containers.size(); ++i) {
				candidateItems.add(containers.get(i)->getObjectID());
			}
		}

		if (candidateItems.size() > 0) {
			donor = sibling;
			itemIDs = candidateItems;
		}
	}

	if (donor == nullptr) {
		return false;
	}

	ranger->updateCooldownTimer("companion_material_fetch", 45000);

	companionSay(ranger, "Anyone carrying supplies for a " + String(tier.displayName) + "?");
	companionSay(donor, "I've got some of what you need -- heading your way!");

	Reference<MaterialFetchState*> state = new MaterialFetchState();
	state->rangerID = ranger->getObjectID();
	state->donorID = donor->getObjectID();
	state->itemIDs = itemIDs;
	state->tierIndex = tierIndex;

	Reference<CreatureObject*> ownerRef = owner;
	scheduleFetchStep(zoneServer, ownerRef, state, 200);

	return true;
}

void CampDeploymentManager::craftCampKit(CreatureObject* owner, CompanionObject* companion, int tierIndex) {
	if (owner == nullptr || companion == nullptr || tierIndex < 0 || tierIndex >= CAMP_KIT_TIER_COUNT) {
		return;
	}

	const CampKitTier& tier = CAMP_KIT_TIERS[tierIndex];

	// Re-entry guard: one crafting session per companion at a time.
	if (!companion->checkCooldownRecovery("companion_camp_craft")) {
		owner->sendSystemMessage("Your companion is already busy crafting.");
		return;
	}

	// Tool check.
	Vector<ManagedReference<TangibleObject*> > tools;
	collectCompanionItems(companion, tools, [](TangibleObject* tano) -> bool {
		return tano->isCraftingTool();
	});

	if (tools.size() == 0) {
		// Phase 4: try borrowing from another companion first.
		if (startMaterialFetch(owner, companion, tierIndex, true)) {
			return;
		}

		companionSay(companion, "I don't have a crafting tool to build a " + String(tier.displayName) + ". Anyone got a spare tool?");
		owner->sendSystemMessage("Your companion needs a crafting tool in its inventory to craft a tent.");
		return;
	}

	// Recipe check (2026-07-18 second revision, live-caught bug: the old
	// check accepted ANY resource -- a tent got built from bone alone).
	// Every class is checked and every shortfall reported in one message.
	String missing;

	for (int c = 0; c < 3; ++c) {
		int needed = recipeAmount(tier, c);

		if (needed <= 0) {
			continue;
		}

		int have = countResourceUnits(companion, RECIPE_CLASSES[c]);

		// 2026-07-20 (user request: "the ranger needs to be able to use the
		// resource deeds"): top up any shortfall from a carried resource
		// deed before giving up, exactly like the fireworks/general craft.
		if (have < needed) {
			CompanionCraftingManager::instance()->claimResourceDeedForClass(owner, companion, RECIPE_CLASSES[c]);
			have = countResourceUnits(companion, RECIPE_CLASSES[c]);
		}

		if (have < needed) {
			if (!missing.isEmpty()) {
				missing += ", ";
			}

			missing += String::valueOf(needed - have) + " more " + RECIPE_CLASSES[c] + " (have " + String::valueOf(have) + " of " + String::valueOf(needed) + ")";
		}
	}

	if (!missing.isEmpty()) {
		// Phase 4: try borrowing from another companion first.
		if (startMaterialFetch(owner, companion, tierIndex, false)) {
			return;
		}

		companionSay(companion, "I can't craft a " + String(tier.displayName) + " yet -- I still need " + missing + ".");
		owner->sendSystemMessage("Your companion is missing materials: " + missing);
		return;
	}

	companion->updateCooldownTimer("companion_camp_craft", 20000);

	companionSay(companion, "Give me a moment -- building a " + String(tier.displayName) + " (" + recipeSummary(tier) + ")...");

	ManagedReference<CompanionObject*> companionRef = companion;
	ManagedReference<CreatureObject*> ownerRef = owner;
	int capturedTier = tierIndex;

	Core::getTaskManager()->scheduleTask([companionRef] () {
		CompanionObject* comp = companionRef.get();

		if (comp != nullptr && comp->getZone() != nullptr) {
			Locker locker(comp);
			companionSay(comp, "Stretching the canvas over the frame...");
		}
	}, "CompanionCraftChatterLambda", 6000);

	Core::getTaskManager()->scheduleTask([companionRef, ownerRef, capturedTier] () {
		CompanionObject* comp = companionRef.get();
		CreatureObject* owner = ownerRef.get();

		if (comp == nullptr || owner == nullptr || comp->getZone() == nullptr || capturedTier < 0 || capturedTier >= CAMP_KIT_TIER_COUNT) {
			return;
		}

		const CampKitTier& tier = CAMP_KIT_TIERS[capturedTier];

		ZoneServer* zoneServer = comp->getZoneServer();

		if (zoneServer == nullptr) {
			return;
		}

		Locker plocker(owner);
		Locker clocker(comp, owner);

		// Re-verify the whole recipe (materials may have been removed
		// during the 15 seconds), then consume with an itemized record.
		for (int c = 0; c < 3; ++c) {
			int needed = recipeAmount(tier, c);

			if (needed > 0 && countResourceUnits(comp, RECIPE_CLASSES[c]) < needed) {
				companionSay(comp, "Hey -- my materials are gone! Never mind the tent.");
				return;
			}
		}

		String consumedDesc;

		for (int c = 0; c < 3; ++c) {
			int needed = recipeAmount(tier, c);

			if (needed > 0) {
				consumeResourceUnits(comp, RECIPE_CLASSES[c], needed, consumedDesc);
			}
		}

		// The finished kit -- straight into the companion's bag.
		ManagedReference<SceneObject*> kitObj = zoneServer->createObject(String(tier.templatePath).hashCode(), 1);

		if (kitObj == nullptr) {
			companionSay(comp, "Blast -- ruined the canvas. Never mind the tent.");
			return;
		}

		SceneObject* bag = comp->getSlottedObject("inventory");
		SceneObject* destination = bag != nullptr ? bag : static_cast<SceneObject*>(comp);

		Locker kitLocker(kitObj, comp);

		if (!destination->transferObject(kitObj, -1, true)) {
			kitObj->destroyObjectFromDatabase(true);
			companionSay(comp, "No room to stow the tent -- never mind.");
			return;
		}

		kitObj->sendTo(owner, true);

		// The chime + the exact consumption call-out (2026-07-18 second
		// revision, per user request "say in chat the amount of items/
		// resources it used").
		owner->sendMessage(new PlayMusicMessage("sound/music_mission_complete.snd"));
		companionSay(comp, String(tier.displayName) + " finished! Used: " + consumedDesc + ". Best work I've ever done!");

		// Supply report (accepted suggestion #4): itemize what's now low.
		String lowReport;

		for (int c = 0; c < 3; ++c) {
			if (recipeAmount(tier, c) <= 0) {
				continue;
			}

			int left = countResourceUnits(comp, RECIPE_CLASSES[c]);

			if (left < recipeAmount(tier, c)) {
				if (!lowReport.isEmpty()) {
					lowReport += ", ";
				}

				lowReport += String(RECIPE_CLASSES[c]) + " (" + String::valueOf(left) + " left)";
			}
		}

		if (!lowReport.isEmpty()) {
			companionSay(comp, "Running low on " + lowReport + " -- we should restock.");
		}

		CampDeploymentManager::instance()->deployCampTier(owner, comp, capturedTier);
	}, "CompanionCraftFinishLambda", 15000);
}

void CampDeploymentManager::packUpCamp(CreatureObject* owner, CompanionObject* companion) {
	if (owner == nullptr || companion == nullptr) {
		return;
	}

	if (!companionHasRangerOrScoutTraining(companion)) {
		owner->sendSystemMessage("@companion:camp_not_trained");
		return;
	}

	ZoneServer* zoneServer = owner->getZoneServer();
	ManagedReference<PlayerObject*> ghost = owner->getPlayerObject();

	if (zoneServer == nullptr || ghost == nullptr) {
		return;
	}

	// Find the owner's deployed camp.
	ManagedReference<StructureObject*> camp = nullptr;

	for (int i = 0; i < ghost->getTotalOwnedStructureCount(); ++i) {
		uint64 oid = ghost->getOwnedStructure(i);

		ManagedReference<StructureObject*> structure = zoneServer->getObject(oid).castTo<StructureObject*>();

		if (structure != nullptr && structure->isCampStructure()) {
			camp = structure;
			break;
		}
	}

	if (camp == nullptr || camp->getZone() == nullptr) {
		owner->sendSystemMessage("You don't have a camp set up.");
		return;
	}

	if (!owner->isInRange(camp.get(), 64)) {
		owner->sendSystemMessage("You are too far from your camp to have it packed up.");
		return;
	}

	// Identical teardown to the camp terminal's own Disband option.
	SortedVector<ManagedReference<ActiveArea*>>* areas = camp->getActiveAreas();
	ManagedReference<ActiveArea*> area = nullptr;

	for (int i = 0; i < areas->size(); ++i) {
		area = areas->get(i);

		if (area->isCampArea()) {
			break;
		}

		area = nullptr;
	}

	companionSay(companion, "Packing it all up -- won't be a minute.");

	CampSiteActiveArea* campArea = cast<CampSiteActiveArea*>(area.get());

	if (campArea != nullptr) {
		Locker areaLocker(campArea, owner);

		if (campArea->despawnCamp()) {
			owner->sendSystemMessage("Your companion packs up the camp.");
			return;
		}
	}

	StructureManager::instance()->destroyStructure(camp.get());

	if (campArea != nullptr) {
		Locker areaLocker(campArea, owner);
		campArea->destroyObjectFromWorld(true);
		campArea->destroyObjectFromDatabase(true);
	}

	owner->sendSystemMessage("Your companion packs up the camp.");
}

// ----------------------------------------------------------------------------
// CompanionCampChoiceSuiCallback -- method bodies live here (not in their own
// .cpp) so no new compilation unit is added (new .cpp files require a cmake
// reconfigure; headers don't -- same reasoning as the taxi callback, see
// NOTES.md). The header is included at the top of this file.
// ----------------------------------------------------------------------------

void CompanionCampChoiceSuiCallback::sendChoiceBox(CreatureObject* player, CompanionObject* comp) {
	if (player == nullptr || comp == nullptr) {
		return;
	}

	ManagedReference<PlayerObject*> ghost = player->getPlayerObject();

	if (ghost == nullptr) {
		return;
	}

	CampDeploymentManager* manager = CampDeploymentManager::instance();

	Vector<int> tiers;
	Vector<String> labels;

	for (int i = 0; i < manager->getTierCount(); ++i) {
		if (!manager->isTierWithinTraining(comp, i)) {
			continue;
		}

		tiers.add(i);
		labels.add(manager->describeTierForPicker(comp, i));
	}

	if (tiers.size() == 0) {
		player->sendSystemMessage("@companion:camp_not_trained");
		return;
	}

	ghost->closeSuiWindowType(SuiWindowType::COMPANION_CAMP_CHOICE);

	ManagedReference<SuiListBox*> sui = new SuiListBox(player, SuiWindowType::COMPANION_CAMP_CHOICE);
	sui->setPromptTitle(comp->getDisplayedName() + " -=COMPANION=- : Camp");
	sui->setPromptText("Which tent should your companion set up? Carried kits deploy immediately; the rest are crafted from the listed materials in its inventory.");
	sui->setCancelButton(true, "@ui:cancel");
	sui->setOkButton(true, "@ui:ok");
	sui->setCallback(new CompanionCampChoiceSuiCallback(player->getZoneServer(), comp, tiers));

	for (int i = 0; i < labels.size(); ++i) {
		sui->addMenuItem(labels.get(i));
	}

	ghost->addSuiBox(sui);
	player->sendMessage(sui->generateMessage());
}

void CompanionCampChoiceSuiCallback::run(CreatureObject* player, SuiBox* suiBox, uint32 eventIndex, Vector<UnicodeString>* args) {
	if (eventIndex == 1 || player == nullptr || args == nullptr || args->size() <= 0) {
		return;
	}

	int menuSelection = Integer::valueOf(args->get(0).toString());

	if (menuSelection < 0 || menuSelection >= tierIndexes.size()) {
		return;
	}

	ManagedReference<CompanionObject*> strongCompanion = companion;

	if (strongCompanion == nullptr) {
		return;
	}

	Locker clocker(strongCompanion, player);

	CampDeploymentManager::instance()->deployCampTier(player, strongCompanion, tierIndexes.get(menuSelection));
}

// ============================================================================
// Camp life / ambiance (2026-07-20, per user request) -- idle companions sit
// & sheath their weapons in camp; an entertainer companion auto-dances
// exotic4 with a flourish every ~3s and applies a real dance-mind
// PerformanceBuff to the owner + companions in the camp. Self-terminates
// when the owner no longer has a deployed camp.
// ============================================================================
void CampDeploymentManager::startCampAmbiance(CreatureObject* owner) {
	if (owner == nullptr) {
		return;
	}

	uint64 ownerID = owner->getObjectID();

	if (activeCampAmbiance.contains(ownerID)) {
		return; // one loop per owner
	}

	activeCampAmbiance.put(ownerID);

	Reference<CampDeploymentManager*> managerRef = this;

	Core::getTaskManager()->scheduleTask([managerRef, ownerID] () {
		managerRef->runCampAmbianceTick(ownerID);
	}, "CampAmbianceTickLambda", 3000);
}

void CampDeploymentManager::runCampAmbianceTick(uint64 ownerID) {
	ZoneServer* zoneServer = ServerCore::getZoneServer();

	if (zoneServer == nullptr) {
		activeCampAmbiance.drop(ownerID);
		return;
	}

	ManagedReference<SceneObject*> ownerObj = zoneServer->getObject(ownerID);
	CreatureObject* owner = ownerObj != nullptr ? ownerObj->asCreatureObject() : nullptr;

	// Owner gone / no camp -> stop the loop.
	if (owner == nullptr || owner->getZone() == nullptr) {
		activeCampAmbiance.drop(ownerID);
		return;
	}

	// CRASH FIX (2026-07-20, live SIGABRT): every per-companion cross-lock
	// below is `Locker(companion, owner)`, which asserts the OWNER is
	// already locked by this thread -- but this runs on a task worker with
	// nothing held. Lock the owner (the root) for the whole tick, so every
	// companion cross-lock and the applyDanceBuff(owner) addBuff are valid.
	Locker ownerLocker(owner);

	ManagedReference<PlayerObject*> ghost = owner->getPlayerObject();
	bool ownerHasCamp = false;

	if (ghost != nullptr) {
		for (int i = 0; i < ghost->getTotalOwnedStructureCount(); ++i) {
			ManagedReference<StructureObject*> s = zoneServer->getObject(ghost->getOwnedStructure(i)).castTo<StructureObject*>();

			if (s != nullptr && s->isCampStructure()) {
				ownerHasCamp = true;
				break;
			}
		}
	}

	if (!ownerHasCamp) {
		// Camp packed up -> restore any camp-clothed companions to their
		// armor before the loop ends.
		ManagedReference<SceneObject*> pad = owner->getSlottedObject("datapad");

		if (pad != nullptr) {
			for (int i = 0; i < pad->getContainerObjectsSize(); ++i) {
				ManagedReference<SceneObject*> o = pad->getContainerObject(i);

				if (o == nullptr || !o->isCompanionControlDevice()) {
					continue;
				}

				CompanionControlDevice* d = cast<CompanionControlDevice*>(o.get());
				CompanionObject* c = d != nullptr ? d->getCompanionObject() : nullptr;

				if (c != nullptr && c->getZone() != nullptr && campAttireRemovedArmor.contains(c->getObjectID())) {
					Locker cl(c, owner);
					restoreArmorFromCamp(c, owner);

					if (c->getPosture() == CreaturePosture::SITTING) {
						c->setPosture(CreaturePosture::UPRIGHT, true);
					}
				}
			}
		}

		activeCampAmbiance.drop(ownerID);
		return;
	}

	// Walk the owner's summoned companions.
	ManagedReference<SceneObject*> datapad = owner->getSlottedObject("datapad");

	if (datapad != nullptr) {
		for (int i = 0; i < datapad->getContainerObjectsSize(); ++i) {
			ManagedReference<SceneObject*> obj = datapad->getContainerObject(i);

			if (obj == nullptr || !obj->isCompanionControlDevice()) {
				continue;
			}

			CompanionControlDevice* device = cast<CompanionControlDevice*>(obj.get());

			if (device == nullptr || device->isCompanionDead()) {
				continue;
			}

			CompanionObject* companion = device->getCompanionObject();

			if (companion == nullptr || companion->getZone() == nullptr
					|| companion->getLinkedCreature().get() != owner) {
				continue;
			}

			Locker clocker(companion, owner);

			// Battle-ready restore (2026-07-20, user request "weapons go
			// back on after they leave camp"): a companion that's SITTING
			// but is now in combat or has left the camp must stand up
			// immediately -- its weapon was only SHEATHED (attemptPeace,
			// never unequipped), so standing + the normal combat draw makes
			// it fully ready again. Handled here so it fires the moment the
			// idle conditions stop holding.
			bool inCamp = companion->getCurrentCamp() != nullptr;

			if ((companion->isInCombat() || !inCamp) && companion->getPosture() == CreaturePosture::SITTING) {
				companion->setPosture(CreaturePosture::UPRIGHT, true);
			}

			// Left the camp or entered combat while in camp clothes ->
			// restore armor immediately (battle-ready).
			if ((companion->isInCombat() || !inCamp) && campAttireRemovedArmor.contains(companion->getObjectID())) {
				restoreArmorFromCamp(companion, owner);
			}

			// Only idle companions participate -- busy ones (combat, taxi,
			// active order other than plain follow/stay) are left alone.
			if (companion->isInCombat() || companion->isTaxiActive()) {
				continue;
			}

			int state = companion->getCompanionState();

			if (state != CompanionObject::FOLLOW && state != CompanionObject::STAY) {
				continue;
			}

			// In the owner's camp?
			ManagedReference<CampSiteActiveArea*> camp = companion->getCurrentCamp();

			if (camp == nullptr) {
				continue;
			}

			bool isEntertainer = false;

			for (int s = 0; s < companion->getLearnedSkillCount(); ++s) {
				if (companion->getLearnedSkill(s).beginsWith("social_entertainer_")) {
					isEntertainer = true;
					break;
				}
			}

			if (isEntertainer) {
				// Stand and dance exotic4; flourish each tick (~3s).
				if (companion->getPosture() != CreaturePosture::UPRIGHT) {
					companion->setPosture(CreaturePosture::UPRIGHT, true);
				}

				// genesis port: dropped ->setPerformanceType(PerformanceType::DANCE, true) -- genesis's
				// CreatureObject.idl has no performanceType field (only performanceAnimation /
				// performanceCounter). setPerformanceAnimation() is genesis's real dance-visual
				// API (EntertainingSessionImplementation::sendEntertainingUpdate()) and already
				// carries this beat on its own.
				companion->setPerformanceAnimation("exotic4", true);
				companion->doAnimation("skill_action_1"); // flourish

				// Apply the real dance-mind buff to the owner + every
				// companion in this camp (same PerformanceBuff the
				// entertainer session uses).
				applyDanceBuff(owner);

				ManagedReference<SceneObject*> pad = owner->getSlottedObject("datapad");

				if (pad != nullptr) {
					for (int j = 0; j < pad->getContainerObjectsSize(); ++j) {
						ManagedReference<SceneObject*> o2 = pad->getContainerObject(j);

						if (o2 == nullptr || !o2->isCompanionControlDevice()) {
							continue;
						}

						CompanionControlDevice* d2 = cast<CompanionControlDevice*>(o2.get());
						CompanionObject* c2 = d2 != nullptr ? d2->getCompanionObject() : nullptr;

						if (c2 != nullptr && c2 != companion && c2->getZone() != nullptr && c2->getCurrentCamp() != nullptr) {
							Locker c2locker(c2, companion);
							applyDanceBuff(c2);
						}
					}
				}
			} else {
				// Idle worker: change into camp clothes (armor -> clothes if
				// carried), sheath weapon (peace), and take a seat.
				changeIntoCampClothes(companion, owner);

				if (companion->isInCombat()) {
					CombatManager::instance()->attemptPeace(companion);
				}

				if (companion->getPosture() != CreaturePosture::SITTING) {
					companion->setPosture(CreaturePosture::SITTING, true);
				}
			}
		}
	}

	// Reschedule.
	Reference<CampDeploymentManager*> managerRef = this;

	Core::getTaskManager()->scheduleTask([managerRef, ownerID] () {
		managerRef->runCampAmbianceTick(ownerID);
	}, "CampAmbianceTickLambda", 3000);
}

void CampDeploymentManager::applyDanceBuff(CreatureObject* target, float strength, int duration) const {
	if (target == nullptr) {
		return;
	}

	// Buff accrual redesign (2026-07-29): strength/duration now come from
	// the caller (accrued watched-seconds at stopEntertainerDanceWatch(),
	// or the camp-ambiance loop's own defaults) instead of being
	// hardcoded here. Defensive clamp to the real system's own 125% cap
	// -- strength is a FRACTION (0.0-1.25), not a raw percent; passing a
	// raw percent like the old hardcoded 250.f directly into
	// PerformanceBuffImplementation::activate()'s
	// `strength * baseHAM(MIND)` formula was the original bug.
	if (strength > 1.25f) {
		strength = 1.25f;
	} else if (strength < 0.f) {
		strength = 0.f;
	}

	uint32 mindBuffCRC = STRING_HASHCODE("performance_enhance_dance_mind");

	PerformanceBuff* existing = cast<PerformanceBuff*>(target->getBuff(mindBuffCRC));

	if (existing != nullptr && existing->getBuffStrength() >= strength) {
		return; // already at least this strong
	}

	ManagedReference<PerformanceBuff*> mindBuff = new PerformanceBuff(target, mindBuffCRC, strength, duration, PerformanceBuffType::DANCE_MIND);

	Locker locker(mindBuff);
	target->addBuff(mindBuff);
}

// "Play Music" (2026-07-29): real per-source-verified music buff shape
// -- TWO buffs (Focus + Willpower), confirmed via direct read of
// EntertainingSessionImplementation::activateEntertainerBuff()'s
// PerformanceType::MUSIC case. Deliberately NOT a reskinned copy of the
// single dance-mind buff above.
void CampDeploymentManager::applyMusicBuff(CreatureObject* target, float strength, int duration) const {
	if (target == nullptr) {
		return;
	}

	// Buff accrual redesign (2026-07-29): see applyDanceBuff() above for
	// the full rationale -- same fraction/clamp/param treatment.
	if (strength > 1.25f) {
		strength = 1.25f;
	} else if (strength < 0.f) {
		strength = 0.f;
	}

	uint32 focusBuffCRC = STRING_HASHCODE("performance_enhance_music_focus");
	PerformanceBuff* existingFocus = cast<PerformanceBuff*>(target->getBuff(focusBuffCRC));

	if (existingFocus == nullptr || existingFocus->getBuffStrength() < strength) {
		ManagedReference<PerformanceBuff*> focusBuff = new PerformanceBuff(target, focusBuffCRC, strength, duration, PerformanceBuffType::MUSIC_FOCUS);

		Locker locker(focusBuff);
		target->addBuff(focusBuff);
	}

	uint32 willBuffCRC = STRING_HASHCODE("performance_enhance_music_willpower");
	PerformanceBuff* existingWill = cast<PerformanceBuff*>(target->getBuff(willBuffCRC));

	if (existingWill == nullptr || existingWill->getBuffStrength() < strength) {
		ManagedReference<PerformanceBuff*> willBuff = new PerformanceBuff(target, willBuffCRC, strength, duration, PerformanceBuffType::MUSIC_WILLPOWER);

		Locker locker2(willBuff);
		target->addBuff(willBuff);
	}
}

void CampDeploymentManager::changeIntoCampClothes(CompanionObject* companion, CreatureObject* owner) {
	if (companion == nullptr || owner == nullptr) {
		return;
	}

	uint64 id = companion->getObjectID();

	if (campAttireRemovedArmor.contains(id)) {
		return; // already in camp clothes
	}

	SceneObject* bag = companion->getSlottedObject("inventory");

	if (bag == nullptr) {
		return;
	}

	// Does the companion carry any CLOTHES (wearable, non-armor) to change
	// into? No clothes -> leave the armor on (nothing to change into).
	bool hasClothes = false;

	for (int i = 0; i < bag->getContainerObjectsSize(); ++i) {
		ManagedReference<SceneObject*> obj = bag->getContainerObject(i);

		if (obj != nullptr && obj->isWearableObject() && !obj->isArmorObject()) {
			hasClothes = true;
			break;
		}
	}

	if (!hasClothes) {
		return;
	}

	// Collect currently-worn armor pieces (unique -- multi-slot items appear
	// once), then unequip each into the bag via the proven path.
	SortedVector<ManagedReference<TangibleObject*> > armor;
	armor.setNoDuplicateInsertPlan();

	for (int i = 0; i < companion->getSlottedObjectsSize(); ++i) {
		SceneObject* slotted = companion->getSlottedObject(i);

		if (slotted != nullptr && slotted->isArmorObject()) {
			TangibleObject* tano = slotted->asTangibleObject();

			if (tano != nullptr) {
				armor.put(tano);
			}
		}
	}

	Vector<uint64> removed;

	for (int i = 0; i < armor.size(); ++i) {
		TangibleObject* piece = armor.get(i).get();

		if (piece == nullptr) {
			continue;
		}

		removed.add(piece->getObjectID());
		companion->unequipItemToInventory(piece, owner);
	}

	campAttireRemovedArmor.put(id, removed);

	// Equip clothes from the bag (a fresh scan -- the armor just landed
	// there too, so filter to non-armor wearables).
	Vector<ManagedReference<TangibleObject*> > clothes;

	for (int i = 0; i < bag->getContainerObjectsSize(); ++i) {
		ManagedReference<SceneObject*> obj = bag->getContainerObject(i);

		if (obj != nullptr && obj->isWearableObject() && !obj->isArmorObject()) {
			TangibleObject* tano = obj->asTangibleObject();

			if (tano != nullptr) {
				clothes.add(tano);
			}
		}
	}

	for (int i = 0; i < clothes.size(); ++i) {
		TangibleObject* piece = clothes.get(i).get();

		if (piece != nullptr) {
			companion->equipItemFromInventory(piece, owner);
		}
	}

	companionSay(companion, "Ahh -- much comfier out of the armor.");
}

void CampDeploymentManager::restoreArmorFromCamp(CompanionObject* companion, CreatureObject* owner) {
	if (companion == nullptr || owner == nullptr) {
		return;
	}

	uint64 id = companion->getObjectID();

	if (!campAttireRemovedArmor.contains(id)) {
		return;
	}

	Vector<uint64> removed = campAttireRemovedArmor.get(id);
	campAttireRemovedArmor.drop(id);

	ZoneServer* zoneServer = companion->getZoneServer();

	if (zoneServer == nullptr) {
		return;
	}

	for (int i = 0; i < removed.size(); ++i) {
		ManagedReference<SceneObject*> obj = zoneServer->getObject(removed.get(i));

		if (obj == nullptr || obj->getRootParent() != companion) {
			continue; // sold/moved away meanwhile -- skip
		}

		TangibleObject* piece = obj->asTangibleObject();

		if (piece != nullptr) {
			companion->equipItemFromInventory(piece, owner);
		}
	}

	companionSay(companion, "Armor's back on -- ready for anything.");
}

// ============================================================================
// Entertainer Dance/Watch (2026-07-29, per Nick's dance-radial request) --
// see CampDeploymentManager.h's doc comments for the full design. Reuses the
// exact same dance+flourish call shape, dance-mind PerformanceBuff, and
// companionSay() helper the camp-ambiance feature above already uses; the
// weapon unequip/re-equip reuses CompanionObjectImplementation's own
// unequipItemToInventory()/equipItemFromInventory() directly (finding #4).
// Locking discipline throughout mirrors runCampAmbianceTick()'s own
// documented "lock owner first, cross-lock everything else to it" rule --
// see the doc comments on startEntertainerDanceWatch()/
// stopEntertainerDanceWatch() in the header for the exact preconditions.
// ============================================================================
void CampDeploymentManager::resolveActiveCompanionsForDance(CreatureObject* owner, Vector<ManagedReference<CompanionObject*> >& out) const {
	if (owner == nullptr) {
		return;
	}

	ManagedReference<SceneObject*> datapad = owner->getSlottedObject("datapad");

	if (datapad == nullptr) {
		return;
	}

	for (int i = 0; i < datapad->getContainerObjectsSize(); ++i) {
		ManagedReference<SceneObject*> obj = datapad->getContainerObject(i);

		if (obj == nullptr || !obj->isCompanionControlDevice()) {
			continue;
		}

		CompanionControlDevice* device = cast<CompanionControlDevice*>(obj.get());

		if (device == nullptr || device->isCompanionDead()) {
			continue;
		}

		CompanionObject* comp = device->getCompanionObject();

		if (comp == nullptr || comp->getZone() == nullptr) {
			continue;
		}

		if (comp->getLinkedCreature().get() != owner) {
			continue;
		}

		out.add(comp);
	}
}

bool CampDeploymentManager::hasActiveDanceSession(CreatureObject* owner) const {
	return owner != nullptr && activeEntertainerDance.contains(owner->getObjectID());
}

bool CampDeploymentManager::isEntertainerDancing(CreatureObject* owner, CompanionObject* entertainer) const {
	if (owner == nullptr || entertainer == nullptr) {
		return false;
	}

	uint64 ownerID = owner->getObjectID();

	if (!activeEntertainerDance.contains(ownerID) || !entertainerDanceEntertainer.contains(ownerID)) {
		return false;
	}

	if (entertainerDanceEntertainer.get(ownerID) != entertainer->getObjectID()) {
		return false;
	}

	// "Play Music" (2026-07-29): the shared session slot now also
	// covers Music -- only report "dancing" if this session's mode
	// really is DANCE (missing entry defaults to DANCE, preserving
	// this method's exact original behavior from before Music
	// existed).
	return !entertainerPerformanceMode.contains(ownerID) || entertainerPerformanceMode.get(ownerID) == PerformanceType::DANCE;
}

bool CampDeploymentManager::isEntertainerPlayingMusic(CreatureObject* owner, CompanionObject* entertainer) const {
	if (owner == nullptr || entertainer == nullptr) {
		return false;
	}

	uint64 ownerID = owner->getObjectID();

	if (!activeEntertainerDance.contains(ownerID) || !entertainerDanceEntertainer.contains(ownerID)) {
		return false;
	}

	if (entertainerDanceEntertainer.get(ownerID) != entertainer->getObjectID()) {
		return false;
	}

	return entertainerPerformanceMode.contains(ownerID) && entertainerPerformanceMode.get(ownerID) == PerformanceType::MUSIC;
}

void CampDeploymentManager::startEntertainerDanceWatch(CreatureObject* owner, CompanionObject* entertainer) {
	if (owner == nullptr || entertainer == nullptr) {
		return;
	}

	uint64 ownerID = owner->getObjectID();

	if (activeEntertainerDance.contains(ownerID)) {
		// Edge case (a second "Dance" click while already dancing): no
		// stacked loops, just a clean message either way.
		uint64 currentEntertainerID = entertainerDanceEntertainer.contains(ownerID) ? entertainerDanceEntertainer.get(ownerID) : 0;

		if (currentEntertainerID == entertainer->getObjectID()) {
			owner->sendSystemMessage(entertainer->getDisplayedName() + " is already dancing for you.");
		} else {
			owner->sendSystemMessage("Another companion is already dancing for you -- stop that dance first.");
		}

		return;
	}

	// One-time line-of-sight check at watch-start (same real primitive
	// PlayerManagerImplementation::startWatch() uses) plus the ongoing
	// range gate (PerformanceManager::HEAL_RANGE, the same distance
	// EntertainingSessionImplementation::doEntertainerPatronEffects()
	// already uses every tick to decide who's still watching).
	if (!owner->isInRange(entertainer, COMPANION_HEAL_RANGE) /* genesis port: was PerformanceManager::HEAL_RANGE */) {
		owner->sendSystemMessage(entertainer->getDisplayedName() + " is too far away to dance for you.");
		return;
	}

	if (!CollisionManager::checkLineOfSight(owner, entertainer)) {
		owner->sendSystemMessage("@healing:no_line_of_sight"); // You cannot see your target.
		return;
	}

	// Resolve the squad, excluding the entertainer itself.
	Vector<ManagedReference<CompanionObject*> > squad;
	resolveActiveCompanionsForDance(owner, squad);

	Vector<uint64> watcherIDs;
	Vector<uint64> watcherWeapons;

	for (int i = 0; i < squad.size(); ++i) {
		CompanionObject* companion = squad.get(i);

		if (companion == nullptr || companion->getObjectID() == entertainer->getObjectID()) {
			continue;
		}

		Locker clocker(companion, owner);

		if (companion->isInCombat() || companion->isDead()) {
			continue; // skip a companion mid-fight/dead rather than yank its weapon
		}

		companion->faceObject(entertainer, true);

		ManagedReference<WeaponObject*> weapon = companion->getWeapon();
		uint64 weaponID = 0;

		if (weapon != nullptr) {
			weaponID = weapon->getObjectID();
			companion->unequipItemToInventory(weapon, owner);
		}

		watcherIDs.add(companion->getObjectID());
		watcherWeapons.add(weaponID);
	}

	activeEntertainerDance.put(ownerID);
	entertainerDanceEntertainer.put(ownerID, entertainer->getObjectID());
	entertainerDanceWatchers.put(ownerID, watcherIDs);
	entertainerDanceWatcherWeapons.put(ownerID, watcherWeapons);
	entertainerPerformanceMode.put(ownerID, PerformanceType::DANCE); // "Play Music" (2026-07-29): explicit mode, shared session slot

	// Buff accrual redesign (2026-07-29): fresh accrual state for a NEW
	// session, matching the real system's addPatron() "fresh
	// EntertainingData every time" behavior -- defensively overwrites any
	// stale leftover value rather than assuming stopEntertainerDanceWatch()
	// always cleaned up first.
	entertainerWatchAccrual.put(ownerID, 0);

	for (int i = 0; i < watcherIDs.size(); ++i) {
		entertainerWatchAccrual.put(watcherIDs.get(i), 0);
	}

	{
		Locker entLocker(entertainer, owner);

		// Fix (2026-07-29 night, live-test bug #2: "the dancer also did not
		// appear to be dancing"). Root cause: the entertainer was left
		// in its normal FOLLOW movementState the whole time --
		// AiAgentImplementation's FOLLOWING case (setDestination())
		// re-issues a fresh nextPos every AI tick regardless of
		// whether it actually needs to move, and any movement update
		// cancels/prevents a performance animation from ever
		// rendering client-side (same as real SWG: you can't dance
		// while walking). The confirmed-working camp-ambiance dancer
		// never hits this because it's only eligible to dance while
		// parked, stationary, inside a deployed camp. Freeze movement
		// here -- the same setFollowObject(nullptr)+setOblivious()
		// idiom recoverFromAbortedIntercept()'s STAY branch already
		// uses to halt an AiAgent in place -- so the dance visual
		// actually sticks; restored via the standingOrder-based
		// restore in stopEntertainerDanceWatch() below.
		entertainer->setFollowObject(nullptr);
		entertainer->setOblivious();

		if (entertainer->getPosture() != CreaturePosture::UPRIGHT) {
			entertainer->setPosture(CreaturePosture::UPRIGHT, true);
		}

		// Kick off the first dance+flourish immediately rather than
		// waiting a full 3s for the first tick.
		// genesis port: dropped ->setPerformanceType(PerformanceType::DANCE, true) -- genesis's
		// CreatureObject.idl has no performanceType field (only performanceAnimation /
		// performanceCounter). setPerformanceAnimation() is genesis's real dance-visual
		// API (EntertainingSessionImplementation::sendEntertainingUpdate()) and already
		// carries this beat on its own.
		entertainer->setPerformanceAnimation("exotic4", true);
		entertainer->doAnimation("skill_action_1"); // flourish
	}

	Reference<CampDeploymentManager*> managerRef = this;

	Core::getTaskManager()->scheduleTask([managerRef, ownerID] () {
		managerRef->runEntertainerDanceWatchTick(ownerID);
	}, "EntertainerDanceWatchTickLambda", 3000);
}

void CampDeploymentManager::runEntertainerDanceWatchTick(uint64 ownerID) {
	ZoneServer* zoneServer = ServerCore::getZoneServer();

	if (zoneServer == nullptr) {
		stopEntertainerDanceWatch(ownerID);
		return;
	}

	ManagedReference<SceneObject*> ownerObj = zoneServer->getObject(ownerID);
	CreatureObject* owner = ownerObj != nullptr ? ownerObj->asCreatureObject() : nullptr;

	// Owner logged off/despawned mid-dance -- clean up the companions'
	// side (re-equip/buff/speak) without trying to message the owner.
	if (owner == nullptr || owner->getZone() == nullptr) {
		stopEntertainerDanceWatch(ownerID);
		return;
	}

	// Same "lock the root for the whole tick" crash fix runCampAmbianceTick()
	// documents above -- every cross-lock below asserts the owner already
	// locked by this thread.
	Locker ownerLocker(owner);

	if (!activeEntertainerDance.contains(ownerID)) {
		return; // already stopped by another path between scheduling and now
	}

	uint64 entertainerID = entertainerDanceEntertainer.contains(ownerID) ? entertainerDanceEntertainer.get(ownerID) : 0;
	ManagedReference<SceneObject*> entObj = zoneServer->getObject(entertainerID);
	CompanionObject* entertainer = (entObj != nullptr && entObj->isCompanionObject()) ? cast<CompanionObject*>(entObj.get()) : nullptr;

	// Entertainer stored/despawned mid-dance.
	if (entertainer == nullptr || entertainer->getZone() == nullptr) {
		stopEntertainerDanceWatch(ownerID);
		return;
	}

	// The three ways this can end (per Nick): (1) explicit "Stop Dance"
	// radial / /companionenddance call stopEntertainerDanceWatch()
	// directly and never reach here again; (2) auto-stop below -- combat
	// (owner OR entertainer -- "combat interrupts dancing", the safe
	// default), range (re-checked every tick, same
	// PerformanceManager::HEAL_RANGE the real doEntertainerPatronEffects()
	// tick already uses), and line-of-sight (also re-checked every tick,
	// same real CollisionManager::checkLineOfSight() primitive
	// startWatch() uses to gate a watch session -- cheap, one raycast per
	// owner per tick, not per watcher). All reads/mutations of entertainer
	// happen in ONE short-lived cross-locked scope below so this never
	// double-locks entertainer before calling stopEntertainerDanceWatch().
	bool shouldStop = false;

	{
		Locker entLocker(entertainer, owner);

		if (entertainer->getLinkedCreature().get() != owner || entertainer->isDead() || entertainer->isIncapacitated()
				|| entertainer->isInCombat() || owner->isInCombat()) {
			shouldStop = true;
		} else if (!owner->isInRange(entertainer, COMPANION_HEAL_RANGE) /* genesis port: was PerformanceManager::HEAL_RANGE */ || !CollisionManager::checkLineOfSight(owner, entertainer)) {
			shouldStop = true;
		} else {
			// Dance + flourish (or Play Music + flourish for a music
			// session -- "Play Music", 2026-07-29, see
			// entertainerPerformanceMode).
			if (entertainer->getPosture() != CreaturePosture::UPRIGHT) {
				entertainer->setPosture(CreaturePosture::UPRIGHT, true);
			}

			int tickPerformanceMode = entertainerPerformanceMode.contains(ownerID) ? entertainerPerformanceMode.get(ownerID) : PerformanceType::DANCE;

			if (tickPerformanceMode == PerformanceType::MUSIC) {
				// genesis port: dropped ->setPerformanceType(PerformanceType::MUSIC, true) -- genesis's
				// CreatureObject.idl has no performanceType field (only performanceAnimation /
				// performanceCounter). setPerformanceAnimation() is genesis's real dance-visual
				// API (EntertainingSessionImplementation::sendEntertainingUpdate()) and already
				// carries this beat on its own.
				entertainer->setPerformanceAnimation("music_3", true);
			} else {
				// genesis port: dropped ->setPerformanceType(PerformanceType::DANCE, true) -- genesis's
				// CreatureObject.idl has no performanceType field (only performanceAnimation /
				// performanceCounter). setPerformanceAnimation() is genesis's real dance-visual
				// API (EntertainingSessionImplementation::sendEntertainingUpdate()) and already
				// carries this beat on its own.
				entertainer->setPerformanceAnimation("exotic4", true);
			}

			entertainer->doAnimation("skill_action_1"); // flourish
		}
	}

	if (shouldStop) {
		stopEntertainerDanceWatch(ownerID);
		return;
	}

	// Buff accrual redesign (2026-07-29): no buff is applied mid-session
	// anymore -- matches the real EntertainingSessionImplementation
	// "bank silently while watching, grant once at stop" model. Just
	// accrue the owner's watched-seconds here; the real buff(s) are
	// constructed once, in stopEntertainerDanceWatch(), from this
	// accrual (see entertainerWatchAccrual's doc comment in the header).
	{
		int ownerAccrued = entertainerWatchAccrual.contains(ownerID) ? entertainerWatchAccrual.get(ownerID) : 0;
		ownerAccrued += 3; // tick interval (this loop reschedules every 3000ms)

		if (ownerAccrued > 7210) {
			ownerAccrued = 7210; // real system's own cap: 120 min + 10s
		}

		entertainerWatchAccrual.put(ownerID, ownerAccrued);
	}

	// Keep every still-valid, non-combat watcher facing the entertainer. A
	// watcher that's mid-fight on its own is simply left alone this tick
	// (it keeps its weapon off until the session ends via one of the three
	// stop paths -- a deliberate, documented simplification; see
	// CampDeploymentManager.h's doc comment on this method).
	if (entertainerDanceWatchers.contains(ownerID)) {
		Vector<uint64> watcherIDs = entertainerDanceWatchers.get(ownerID);

		for (int i = 0; i < watcherIDs.size(); ++i) {
			ManagedReference<SceneObject*> watcherObj = zoneServer->getObject(watcherIDs.get(i));
			CompanionObject* watcher = (watcherObj != nullptr && watcherObj->isCompanionObject()) ? cast<CompanionObject*>(watcherObj.get()) : nullptr;

			if (watcher == nullptr || watcher->getZone() == nullptr) {
				continue;
			}

			Locker wLocker(watcher, owner);

			if (watcher->isInCombat() || watcher->isDead()) {
				continue;
			}

			watcher->faceObject(entertainer, true);

			// Buff accrual redesign (2026-07-29): no buff is applied
			// mid-session anymore (see the owner accrual block above --
			// same reasoning, same entertainerWatchAccrual map, keyed by
			// this watcher's own object ID). Incidentally fixes a THIRD
			// bug this same block used to have: it hardcoded
			// applyDanceBuff() even during a MUSIC session, giving
			// watchers the wrong buff type mid-session -- moot now that
			// nothing is applied here at all; the real mode-aware buff
			// is constructed once in stopEntertainerDanceWatch().
			{
				uint64 watcherAccrualID = watcher->getObjectID();
				int watcherAccrued = entertainerWatchAccrual.contains(watcherAccrualID) ? entertainerWatchAccrual.get(watcherAccrualID) : 0;
				watcherAccrued += 3; // tick interval (this loop reschedules every 3000ms)

				if (watcherAccrued > 7210) {
					watcherAccrued = 7210; // real system's own cap: 120 min + 10s
				}

				entertainerWatchAccrual.put(watcherAccrualID, watcherAccrued);
			}

			// Fix (bug #3: "the companions were not clapping").
			// No real audience-clap animation exists server-side for
			// NPCs/companions in this codebase -- stock "clap"/"applaud"
			// are player-only client social emotes tracked via
			// EntertainingSession::incrementApplauseCount(), not a
			// server-triggered animation. "happy" is the closest
			// confirmed-real creature/companion reaction animation (see
			// PetEmoteCommand.h's praise() handler) -- best guess, swap
			// if it doesn't render, same as exotic4/skill_action_1 above.
			watcher->doAnimation("happy");
		}
	}

	// Reschedule.
	Reference<CampDeploymentManager*> managerRef = this;

	Core::getTaskManager()->scheduleTask([managerRef, ownerID] () {
		managerRef->runEntertainerDanceWatchTick(ownerID);
	}, "EntertainerDanceWatchTickLambda", 3000);
}

void CampDeploymentManager::stopEntertainerDanceWatch(uint64 ownerID) {
	if (!activeEntertainerDance.contains(ownerID)) {
		return; // already stopped / never started -- idempotent (no-op)
	}

	activeEntertainerDance.drop(ownerID); // drop FIRST -- a concurrent tick/second stop-call can't double-process

	uint64 entertainerID = entertainerDanceEntertainer.contains(ownerID) ? entertainerDanceEntertainer.get(ownerID) : 0;
	Vector<uint64> watcherIDs = entertainerDanceWatchers.contains(ownerID) ? entertainerDanceWatchers.get(ownerID) : Vector<uint64>();
	Vector<uint64> watcherWeapons = entertainerDanceWatcherWeapons.contains(ownerID) ? entertainerDanceWatcherWeapons.get(ownerID) : Vector<uint64>();

	entertainerDanceEntertainer.drop(ownerID);
	entertainerDanceWatchers.drop(ownerID);
	entertainerDanceWatcherWeapons.drop(ownerID);

	// Buff accrual redesign (2026-07-29): this is the ONE choke point
	// where a watch session ends -- capture + clear every accrual entry
	// for this session (owner + each watcher) here, alongside the other
	// session-state maps above, so nothing leaks even if zoneServer is
	// unavailable below. The real buff(s) are granted further down,
	// mode-aware, only if accrued >= 60 seconds (the real system's own
	// minimum continuous-watch requirement).
	int ownerAccrued = entertainerWatchAccrual.contains(ownerID) ? entertainerWatchAccrual.get(ownerID) : 0;
	entertainerWatchAccrual.drop(ownerID);

	Vector<int> watcherAccruals;

	for (int i = 0; i < watcherIDs.size(); ++i) {
		uint64 watcherAccrualID = watcherIDs.get(i);
		watcherAccruals.add(entertainerWatchAccrual.contains(watcherAccrualID) ? entertainerWatchAccrual.get(watcherAccrualID) : 0);
		entertainerWatchAccrual.drop(watcherAccrualID);
	}

	// "Play Music" (2026-07-29): this session's performance type,
	// read BEFORE dropping so the watcher-buff loop below knows which
	// real buff to apply. Missing entry defaults to DANCE (preserves
	// this session's exact original pre-Music behavior).
	int stopPerformanceMode = entertainerPerformanceMode.contains(ownerID) ? entertainerPerformanceMode.get(ownerID) : PerformanceType::DANCE;
	entertainerPerformanceMode.drop(ownerID);

	ZoneServer* zoneServer = ServerCore::getZoneServer();

	if (zoneServer == nullptr) {
		return;
	}

	ManagedReference<SceneObject*> ownerObj = zoneServer->getObject(ownerID);
	CreatureObject* owner = ownerObj != nullptr ? ownerObj->asCreatureObject() : nullptr;

	// Buff accrual redesign (2026-07-29): grant the owner's real buff(s)
	// HERE -- the single stop-path choke point -- instead of every tick.
	// strengthFraction and duration both scale directly with how long the
	// owner watched, capped at the same real caps the player system uses
	// (125% / 7210s).
	if (owner != nullptr && ownerAccrued >= 60) {
		float ownerStrength = (ownerAccrued / 7210.0f) * 1.25f;

		if (ownerStrength > 1.25f) {
			ownerStrength = 1.25f;
		}

		if (stopPerformanceMode == PerformanceType::MUSIC) {
			applyMusicBuff(owner, ownerStrength, ownerAccrued);
		} else {
			applyDanceBuff(owner, ownerStrength, ownerAccrued);
		}
	}

	ManagedReference<SceneObject*> entObj = zoneServer->getObject(entertainerID);
	CompanionObject* entertainer = (entObj != nullptr && entObj->isCompanionObject()) ? cast<CompanionObject*>(entObj.get()) : nullptr;

	if (entertainer != nullptr && entertainer->getZone() != nullptr) {
		// Clear the dance visual state -- same "0 = not performing" idiom
		// the real EntertainingSessionImplementation::stopDancing() uses
		// for its own performanceIndex field. Cross-locked to owner when
		// owner is still resolvable (the normal case); plain single-lock
		// otherwise (owner logged off/despawned mid-dance).
		if (owner != nullptr) {
			Locker entLocker(entertainer, owner);
			// genesis port: dropped ->setPerformanceType(0, true) -- genesis's
			// CreatureObject.idl has no performanceType field (only performanceAnimation /
			// performanceCounter). setPerformanceAnimation() is genesis's real dance-visual
			// API (EntertainingSessionImplementation::sendEntertainingUpdate()) and already
			// carries this beat on its own.
			entertainer->setPerformanceAnimation("", true);

			// Bug #2 movement-freeze fix companion piece -- restore normal movement now
			// that the dance has ended, honoring the owner's real
			// standing order. Same restore idiom
			// recoverFromAbortedIntercept()/endSweep() already use
			// elsewhere in this file (STAY: stay put; GUARD: resume
			// guarding; FOLLOW/escort: resume following owner or the
			// escort target).
			int standing = entertainer->getStandingOrder();

			if (standing == CompanionObject::STAY) {
				entertainer->setFollowObject(nullptr);
				entertainer->setOblivious();
			} else if (standing == CompanionObject::GUARD) {
				CreatureObject* guardTarget = entertainer->getGuardTarget().get();

				if (guardTarget != nullptr && guardTarget->getZone() != nullptr) {
					entertainer->setFollowObject(guardTarget);
				} else {
					entertainer->setFollowObject(owner);
				}

				entertainer->setFollowState(AiAgent::FOLLOWING); // genesis port: was setMovementState()
			} else {
				CreatureObject* escortTarget = entertainer->getEscortTarget().get();

				if (escortTarget != nullptr && escortTarget != owner && escortTarget->getZone() != nullptr) {
					entertainer->setFollowObject(escortTarget);
				} else {
					entertainer->setFollowObject(owner);
				}

				entertainer->setFollowState(AiAgent::FOLLOWING); // genesis port: was setMovementState()
			}
		} else {
			Locker entLocker(entertainer);
			// genesis port: dropped ->setPerformanceType(0, true) -- genesis's
			// CreatureObject.idl has no performanceType field (only performanceAnimation /
			// performanceCounter). setPerformanceAnimation() is genesis's real dance-visual
			// API (EntertainingSessionImplementation::sendEntertainingUpdate()) and already
			// carries this beat on its own.
			entertainer->setPerformanceAnimation("", true);

			// Owner gone -- just stop dancing/moving cleanly rather
			// than guessing a restore target.
			entertainer->setFollowObject(nullptr);
			entertainer->setOblivious();
		}
	}

	for (int i = 0; i < watcherIDs.size(); ++i) {
		ManagedReference<SceneObject*> watcherObj = zoneServer->getObject(watcherIDs.get(i));
		CompanionObject* watcher = (watcherObj != nullptr && watcherObj->isCompanionObject()) ? cast<CompanionObject*>(watcherObj.get()) : nullptr;

		if (watcher == nullptr || watcher->getZone() == nullptr) {
			continue; // stored/despawned mid-dance -- nothing to restore
		}

		uint64 weaponID = (i < watcherWeapons.size()) ? watcherWeapons.get(i) : 0;

		// Buff accrual redesign (2026-07-29): derive this watcher's real
		// buff strength/duration from its captured accrual (see the
		// capture+drop block above) -- same formula as the owner's grant
		// above. Under the 60s minimum -> grant nothing, no message (a
		// squad-wide bulk grant isn't worth per-member chat spam for a
		// short watch).
		int watcherAccrued = (i < watcherAccruals.size()) ? watcherAccruals.get(i) : 0;
		float watcherStrength = (watcherAccrued / 7210.0f) * 1.25f;

		if (watcherStrength > 1.25f) {
			watcherStrength = 1.25f;
		}

		bool watcherEarnedBuff = watcherAccrued >= 60;

		if (owner != nullptr) {
			Locker wLocker(watcher, owner);

			if (weaponID != 0) {
				ManagedReference<SceneObject*> weaponObj = zoneServer->getObject(weaponID);
				TangibleObject* weapon = weaponObj != nullptr ? weaponObj->asTangibleObject() : nullptr;

				// Same "sold/moved away meanwhile -- skip" guard
				// restoreArmorFromCamp() above already uses for its own
				// stored-object-ID restore.
				if (weapon != nullptr && weapon->getRootParent() == watcher) {
					watcher->equipItemFromInventory(weapon, owner);
				}
			}

			if (watcherEarnedBuff) {
				if (stopPerformanceMode == PerformanceType::MUSIC) {
					applyMusicBuff(watcher, watcherStrength, watcherAccrued);
					companionSay(watcher, "Whew -- got my focus and willpower buffed!");
				} else {
					applyDanceBuff(watcher, watcherStrength, watcherAccrued);
					companionSay(watcher, "Whew -- got my mind buffed!");
				}
			}
		} else {
			Locker wLocker(watcher);

			if (watcherEarnedBuff) {
				if (stopPerformanceMode == PerformanceType::MUSIC) {
					applyMusicBuff(watcher, watcherStrength, watcherAccrued);
					companionSay(watcher, "Whew -- got my focus and willpower buffed!");
				} else {
					applyDanceBuff(watcher, watcherStrength, watcherAccrued);
					companionSay(watcher, "Whew -- got my mind buffed!");
				}
			}
		}
	}
}

// "Play Music" (2026-07-29, per Nick: "we need a musician as well") --
// copy-adapted from startEntertainerDanceWatch() above (kept as its own
// function rather than a shared-parameter refactor, per the explicit
// low-risk-first instruction for this feature -- the already-live
// Dance function above is left untouched). Shares the exact same
// session dedupe (activeEntertainerDance), entertainer/watcher/weapon
// storage maps, and recurring tick (runEntertainerDanceWatchTick(), now
// mode-aware -- see entertainerPerformanceMode) as Dance -- only the
// messages, the stored performance mode, and the initial animation
// differ.
void CampDeploymentManager::startEntertainerMusicWatch(CreatureObject* owner, CompanionObject* entertainer) {
	if (owner == nullptr || entertainer == nullptr) {
		return;
	}

	uint64 ownerID = owner->getObjectID();

	if (activeEntertainerDance.contains(ownerID)) {
		// Same shared one-session-per-owner dedupe as Dance -- a musician
		// can't start while a dancer (or another musician) is already
		// active for this owner, and vice versa (see
		// startEntertainerDanceWatch() above).
		uint64 currentEntertainerID = entertainerDanceEntertainer.contains(ownerID) ? entertainerDanceEntertainer.get(ownerID) : 0;

		if (currentEntertainerID == entertainer->getObjectID()) {
			owner->sendSystemMessage(entertainer->getDisplayedName() + " is already playing music for you.");
		} else {
			owner->sendSystemMessage("Another companion is already performing for you -- stop that first.");
		}

		return;
	}

	if (!owner->isInRange(entertainer, COMPANION_HEAL_RANGE) /* genesis port: was PerformanceManager::HEAL_RANGE */) {
		owner->sendSystemMessage(entertainer->getDisplayedName() + " is too far away to play music for you.");
		return;
	}

	if (!CollisionManager::checkLineOfSight(owner, entertainer)) {
		owner->sendSystemMessage("@healing:no_line_of_sight"); // You cannot see your target.
		return;
	}

	Vector<ManagedReference<CompanionObject*> > squad;
	resolveActiveCompanionsForDance(owner, squad);

	Vector<uint64> watcherIDs;
	Vector<uint64> watcherWeapons;

	for (int i = 0; i < squad.size(); ++i) {
		CompanionObject* companion = squad.get(i);

		if (companion == nullptr || companion->getObjectID() == entertainer->getObjectID()) {
			continue;
		}

		Locker clocker(companion, owner);

		if (companion->isInCombat() || companion->isDead()) {
			continue;
		}

		companion->faceObject(entertainer, true);

		ManagedReference<WeaponObject*> weapon = companion->getWeapon();
		uint64 weaponID = 0;

		if (weapon != nullptr) {
			weaponID = weapon->getObjectID();
			companion->unequipItemToInventory(weapon, owner);
		}

		watcherIDs.add(companion->getObjectID());
		watcherWeapons.add(weaponID);
	}

	activeEntertainerDance.put(ownerID);
	entertainerDanceEntertainer.put(ownerID, entertainer->getObjectID());
	entertainerDanceWatchers.put(ownerID, watcherIDs);
	entertainerDanceWatcherWeapons.put(ownerID, watcherWeapons);
	entertainerPerformanceMode.put(ownerID, PerformanceType::MUSIC);

	// Buff accrual redesign (2026-07-29): see startEntertainerDanceWatch()
	// above for the full rationale -- same fresh-state reset.
	entertainerWatchAccrual.put(ownerID, 0);

	for (int i = 0; i < watcherIDs.size(); ++i) {
		entertainerWatchAccrual.put(watcherIDs.get(i), 0);
	}

	{
		Locker entLocker(entertainer, owner);

		// Kick off the first music+flourish immediately rather than
		// waiting a full 3s for the first tick -- "music_3", the single
		// most common real instrument-animation literal (see the header
		// doc comment on this method for why).
		// genesis port: dropped ->setPerformanceType(PerformanceType::MUSIC, true) -- genesis's
		// CreatureObject.idl has no performanceType field (only performanceAnimation /
		// performanceCounter). setPerformanceAnimation() is genesis's real dance-visual
		// API (EntertainingSessionImplementation::sendEntertainingUpdate()) and already
		// carries this beat on its own.
		entertainer->setPerformanceAnimation("music_3", true);
		entertainer->doAnimation("skill_action_1"); // flourish
	}

	Reference<CampDeploymentManager*> managerRef = this;

	Core::getTaskManager()->scheduleTask([managerRef, ownerID] () {
		managerRef->runEntertainerDanceWatchTick(ownerID);
	}, "EntertainerDanceWatchTickLambda", 3000);
}
