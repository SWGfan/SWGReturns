/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-20, "crafting theater" pass) -- see
	CompanionCraftingManager.h for the full design writeup.
*/

#include "CompanionCraftingManager.h"

#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/managers/companion/CompanionChatter.h"
#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/draftschematic/DraftSchematic.h"
#include "server/zone/objects/manufactureschematic/ManufactureSchematic.h"
#include "server/zone/objects/manufactureschematic/ingredientslots/IngredientSlot.h"
#include "server/zone/objects/manufactureschematic/ingredientslots/ResourceSlot.h"
#include "server/zone/objects/manufactureschematic/craftingvalues/CraftingValues.h"
#include "server/zone/managers/crafting/CraftingManager.h"
#include "server/zone/objects/tangible/TangibleObject.h"
#include "server/zone/objects/tangible/deed/resource/ResourceDeed.h"
#include "server/zone/objects/resource/ResourceContainer.h"
#include "server/zone/objects/resource/ResourceSpawn.h"
#include "server/zone/objects/installation/harvester/HarvesterObject.h"
#include "server/zone/managers/resource/ResourceManager.h"
#include "server/zone/managers/resource/resourcespawner/ResourceSpawner.h"
#include "server/zone/managers/skill/SkillManager.h"
#include "server/zone/objects/creature/variables/Skill.h"
#include "server/zone/managers/skill/SkillModManager.h"
#include "server/zone/objects/creature/buffs/Buff.h"
#include "server/zone/objects/creature/buffs/BuffCRC.h"
#include "server/zone/managers/crafting/schematicmap/SchematicMap.h"
#include "server/zone/managers/crafting/schematicmap/DraftSchematicGroup.h"
#include "server/zone/objects/factorycrate/FactoryCrate.h"
#include "server/zone/managers/object/ObjectManager.h"
#include "server/zone/ZoneServer.h"
#include "server/zone/Zone.h"
#include "templates/crafting/resourceweight/ResourceWeight.h"
#include "templates/crafting/draftslot/DraftSlot.h"
#include "templates/SharedObjectTemplate.h"
#include "templates/manager/TemplateManager.h"

CompanionCraftingManager::CompanionCraftingManager() : Logger("CompanionCraftingManager") {
	setLogging(false);
}

// ---------------------------------------------------------------------------
// Wrong-slot resource steal fix (2026-07-28)
// ---------------------------------------------------------------------------

// Companion System (2026-07-28, wrong-slot resource steal fix -- see
// NOTES.md): draft schematics list ingredient slots in arbitrary order, and
// because the resource class tree is a strict single-parent tree (e.g. Steel
// IS-A Ferrous Metal IS-A Metal), a broad class slot (e.g. "Metal") passes
// fillResourceSlot()'s spawn->isType(resourceClass) check for a narrower
// class's resources too -- and scoreResourceSpawn()'s best-first quality
// scan actively PREFERS high-quality Steel over mediocre Iron even in a
// generic "Metal" slot. So a schematic listing a broad slot BEFORE a
// specific one can have the generic slot eat the specific resource first,
// and the craft fails with "missing X units of <specific class>" even with
// plenty on hand -- confirmed live (fed 110 units of a "metal" resource +
// 15 units of "metal_nonferrous," craft reported "missing 95 units of
// metal"). Fix: split the schematic's slots into resource slots and
// everything else, sort ONLY the resource slots by DESCENDING
// resource-class-tree depth (deepest/most specific class first; unknown
// depth (-1) sorts last; a STABLE sort, so slots tied on depth keep the
// schematic's original relative order), then run every resource slot
// before any non-resource slot. fillIngredientSlot() itself is completely
// unchanged -- this only reorders WHICH slot index it gets called with.
Vector<int> CompanionCraftingManager::buildSpecificFirstSlotFillOrder(ZoneServer* zoneServer, ManufactureSchematic* manufactureSchematic, DraftSchematic* draftSchematic) const {
	ResourceManager* resourceManagerForSlotOrder = zoneServer != nullptr ? zoneServer->getResourceManager() : nullptr;
	ResourceSpawner* resourceSpawnerForSlotOrder = resourceManagerForSlotOrder != nullptr ? resourceManagerForSlotOrder->getResourceSpawner() : nullptr;

	int slotCount = manufactureSchematic->getSlotCount();

	Vector<int> resourceSlotIndexes;
	Vector<int> resourceSlotDepths;
	Vector<int> otherSlotIndexes;

	for (int i = 0; i < slotCount; ++i) {
		DraftSlot* draftSlot = draftSchematic->getDraftSlot(i);

		if (draftSlot != nullptr && draftSlot->getSlotType() == IngredientSlot::RESOURCESLOT) {
			int depth = resourceSpawnerForSlotOrder != nullptr ? resourceSpawnerForSlotOrder->getResourceClassDepth(draftSlot->getResourceType()) : -1;

			resourceSlotIndexes.add(i);
			resourceSlotDepths.add(depth);
		} else {
			otherSlotIndexes.add(i);
		}
	}

	// Stable insertion sort, descending by depth -- schematics only ever have
	// a handful of slots, so O(n^2) here is negligible, and it needs nothing
	// beyond Vector's own get()/set().
	for (int i = 1; i < resourceSlotDepths.size(); ++i) {
		int depth = resourceSlotDepths.get(i);
		int slotIndex = resourceSlotIndexes.get(i);
		int j = i - 1;

		while (j >= 0 && resourceSlotDepths.get(j) < depth) {
			resourceSlotDepths.set(j + 1, resourceSlotDepths.get(j));
			resourceSlotIndexes.set(j + 1, resourceSlotIndexes.get(j));
			--j;
		}

		resourceSlotDepths.set(j + 1, depth);
		resourceSlotIndexes.set(j + 1, slotIndex);
	}

	Vector<int> slotFillOrder;

	for (int i = 0; i < resourceSlotIndexes.size(); ++i) {
		slotFillOrder.add(resourceSlotIndexes.get(i));
	}

	for (int i = 0; i < otherSlotIndexes.size(); ++i) {
		slotFillOrder.add(otherSlotIndexes.get(i));
	}

	return slotFillOrder;
}

// ---------------------------------------------------------------------------
// Quality / experimentation fix (2026-07-28) -- see NOTES.md
// ---------------------------------------------------------------------------

// Companion System (2026-07-28, quality/experimentation fix): grantSkill()
// (CompanionObjectImplementation.cpp) only ever appends to learnedSkills --
// it never calls addSkillMod(), so a companion's real skill mods
// (CreatureObject::getSkillMod()) never reflect what it has actually
// trained; only WEARABLE mods from equipped gear are real (equips route
// through addTemplateSkillMods() -- CompanionContainerComponent.cpp).
// calculateAssemblySuccess()/calculateExperimentationSuccess()/
// calculateExperimentationFailureRate() all read player->getSkillMod()
// directly on the plain CreatureObject* passed in, so without this helper
// a companion's crafting rolls would be no better than an untrained one
// no matter how many crafting skills it has learned. This sums
// (name -> total value) every skill modifier granted by every skill in
// learnedSkills (Skill::getSkillModifiers(), resolved via
// SkillManager::getSkill()) -- purely a lookup, does not touch
// learnedSkills or call addSkill().
VectorMap<String, int> CompanionCraftingManager::buildEffectiveCraftingSkillMods(CompanionObject* companion, ZoneServer* zoneServer) const {
	// COMPANION_CRAFTING_SKILLMOD_DEDUPE_2026_07_31 -- retired. This used to compute a temporary
	// stand-in for skill mods CompanionObjectImplementation::grantSkill()
	// never applied permanently. grantSkill() now applies those same
	// modifiers PERMANENTLY (see patch_companion_skillmod_grant_2026-07-31,
	// docs/companion_system/NOTES.md 2026-07-31), so companion->getSkillMod()
	// already reflects everything a companion has learned at all times.
	// Returning an empty map here makes both existing call sites (apply
	// with sign=+1 before crafting rolls, revert with sign=-1 after) into
	// complete no-ops without needing to touch either call site directly
	// -- keeps exactly ONE source of truth for companion skill mods
	// (grantSkill()/removeSkill()) instead of two independent, easily
	// desynced copies of the same logic.
	return VectorMap<String, int>();
}

// Applies (sign=1) or reverts (sign=-1) the mods built above via
// CreatureObject::addSkillMod(SkillModManager::SKILLBOX, ...) -- the exact
// same call real skill training uses (SkillManager.cpp's addSkill()), just
// scoped to the duration of one craft's rolls instead of permanent.
// Applying then reverting the identical map nets to zero -- no lasting
// skill-mod or combat-balance change survives past the craft that
// requested it, and learnedSkills/grantSkill()'s own behavior is
// completely untouched.
void CompanionCraftingManager::applyEffectiveCraftingSkillMods(CompanionObject* companion, const VectorMap<String, int>& mods, int sign) const {
	if (companion == nullptr) {
		return;
	}

	for (int i = 0; i < mods.size(); ++i) {
		const String& modName = mods.elementAt(i).getKey();
		int modValue = mods.elementAt(i).getValue();

		if (modValue == 0) {
			continue;
		}

		companion->addSkillMod(SkillModManager::SKILLBOX, modName, modValue * sign, false);
	}
}

// Companion System (2026-07-29, real-roll hardening): see isRegisteredLabratory()'s doc comment in
// CompanionCraftingManager.h.
bool CompanionCraftingManager::isRegisteredLabratory(int labratoryType) const {
	return labratoryType >= 0 && labratoryType <= 2;
}

// Companion System (2026-07-29, real-roll hardening): see foldFoodBuffBonusIntoEffectiveness()'s doc
// comment in CompanionCraftingManager.h.
float CompanionCraftingManager::foldFoodBuffBonusIntoEffectiveness(CreatureObject* owner, unsigned int buffCRC, const String& modifierName, float effectiveness) const {
	if (owner == nullptr || !owner->hasBuff(buffCRC)) {
		return effectiveness;
	}

	Buff* buff = owner->getBuff(buffCRC);

	if (buff == nullptr) {
		return effectiveness;
	}

	float bonus = (float) buff->getSkillModifierValue(modifierName);

	if (bonus == 0.0f) {
		return effectiveness;
	}

	return effectiveness + bonus + (effectiveness * bonus / 100.0f);
}

// Human-readable label for a CraftingManager tier constant, for the
// roll-tier chat message below (risk mitigation: variance should read as
// real gameplay feedback, not a silent/invisible bug).
String CompanionCraftingManager::craftingResultTierName(int tier) const {
	switch (tier) {
	case CraftingManager::AMAZINGSUCCESS:
		return "Amazing success!";
	case CraftingManager::GREATSUCCESS:
		return "Great success!";
	case CraftingManager::GOODSUCCESS:
		return "Good success";
	case CraftingManager::MODERATESUCCESS:
		return "Moderate success";
	case CraftingManager::SUCCESS:
		return "Success";
	case CraftingManager::MARGINALSUCCESS:
		return "Marginal success";
	case CraftingManager::OK:
		return "OK";
	case CraftingManager::BARELYSUCCESSFUL:
		return "Barely successful";
	case CraftingManager::CRITICALFAILURE:
		return "Critical failure";
	default:
		return "Unknown result";
	}
}

// ---------------------------------------------------------------------------
// Top-level entry point
// ---------------------------------------------------------------------------

bool CompanionCraftingManager::craftItem(CreatureObject* owner, CompanionObject* companion, const String& draftSchematicTemplate, String& errorMessage, bool isSubComponentCraft, bool awardXp) const {
	if (owner == nullptr || companion == nullptr) {
		errorMessage = "Missing owner or companion.";
		return false;
	}

	// Sub-component recursion guard (2026-07-20): fillComponentSlot() may
	// call back into craftItem() to make a missing component. Cap the
	// nesting so a cyclic/deep recipe can't loop forever. RAII so every
	// return path below decrements.
	if (craftDepth > 6) {
		errorMessage = "Component recipe nests too deep to craft automatically.";
		return false;
	}

	struct DepthGuard {
		int& d;
		DepthGuard(int& dd) : d(dd) { ++d; }
		~DepthGuard() { --d; }
	} depthGuard(craftDepth);

	ZoneServer* zoneServer = owner->getZoneServer();

	if (zoneServer == nullptr) {
		errorMessage = "No zone server available.";
		return false;
	}

	ManagedReference<CraftingManager*> craftingManager = zoneServer->getCraftingManager();

	if (craftingManager == nullptr) {
		errorMessage = "Crafting manager unavailable.";
		return false;
	}

	String file = draftSchematicTemplate;

	if (file.indexOf("draft_schematic") == -1) {
		file = "object/draft_schematic/" + file;
	}

	if (file.indexOf(".iff") == -1) {
		file = file + ".iff";
	}

	ManagedReference<DraftSchematic*> draftSchematic = zoneServer->createObject(file.hashCode(), 0).castTo<DraftSchematic*>();

	if (draftSchematic == nullptr || !draftSchematic->isValidDraftSchematic()) {
		errorMessage = "Not a valid draft schematic: " + file;
		return false;
	}

	ManagedReference<ManufactureSchematic*> manufactureSchematic = draftSchematic->createManufactureSchematic(nullptr).castTo<ManufactureSchematic*>();

	if (manufactureSchematic == nullptr) {
		errorMessage = "Could not create a manufacture schematic for " + file;
		draftSchematic->destroyObjectFromDatabase(true);
		return false;
	}

	Locker manuLocker(manufactureSchematic);

	// Populates ingredientSlots from the draft schematic. Normally only ever
	// reached via synchronizedUIListen(), which early-returns for anything
	// that isn't isPlayerCreature() -- see initializeSlotsForHeadlessCraft()'s
	// own doc comment in ManufactureSchematic.idl for why this thin public
	// wrapper exists.
	manufactureSchematic->initializeSlotsForHeadlessCraft();

	ManagedReference<TangibleObject*> prototype = zoneServer->createObject(draftSchematic->getTanoCRC(), 0).castTo<TangibleObject*>();

	if (prototype == nullptr) {
		errorMessage = "Unable to create the target item -- is it implemented yet?";
		manuLocker.release();
		manufactureSchematic->destroyObjectFromDatabase(true);
		draftSchematic->destroyObjectFromDatabase(true);
		return false;
	}

	Locker protoLocker(prototype);

	prototype->createChildObjects();

	bool allSlotsFilled = true;

	Vector<int> slotFillOrder = buildSpecificFirstSlotFillOrder(zoneServer, manufactureSchematic, draftSchematic);

	for (int i = 0; i < slotFillOrder.size() && allSlotsFilled; ++i) {
		if (!fillIngredientSlot(owner, companion, manufactureSchematic, draftSchematic, slotFillOrder.get(i), errorMessage)) {
			allSlotsFilled = false;
		}
	}

	if (!allSlotsFilled || !manufactureSchematic->isReadyForAssembly()) {
		if (errorMessage.isEmpty()) {
			errorMessage = "Not all ingredients could be gathered for this item.";
		}

		// Return whatever was already consumed back to its owner before
		// giving up -- same cleanup call the real crafting session uses on
		// cancel.
		manufactureSchematic->cleanupIngredientSlots(companion);

		protoLocker.release();
		prototype->destroyObjectFromDatabase(true);

		manuLocker.release();
		manufactureSchematic->destroyObjectFromDatabase(true);
		draftSchematic->destroyObjectFromDatabase(true);
		return false;
	}

	manufactureSchematic->setAssembled();

	// 2026-07-28 FIX (live report: "companions are not experimenting on the
	// items and they are not coming out as the best quality" -- the SUI's
	// "Results are always crafted at maximum quality" text was never wired to
	// a real roll, it just described this always-GREATSUCCESS shortcut).
	// calculateAssemblySuccess()/calculateExperimentationFailureRate()/
	// calculateExperimentationSuccess()/experimentRow() are all public on
	// CraftingManager and take a plain CreatureObject* -- no CraftingSession/
	// live client needed (verified against CraftingSessionImplementation::
	// initialAssembly()/experiment(), the real player flow, which calls these
	// same functions the same way).

	// PREREQUISITE: companion skill mods are display-only otherwise (see
	// buildEffectiveCraftingSkillMods()'s doc comment) -- apply them for the
	// duration of this craft's rolls only, revert immediately after.
	VectorMap<String, int> effectiveSkillMods = buildEffectiveCraftingSkillMods(companion, zoneServer);
	applyEffectiveCraftingSkillMods(companion, effectiveSkillMods, 1);

	// Companions have no real CraftingTool -- effectiveness is a flat design
	// constant standing in for "a decent player tool" (+10, i.e. a 1.10x
	// toolModifier -- see SharedLabratory::calculateAssemblySuccess()/
	// CraftingManagerImplementation::calculateExperimentationSuccess()).
	const float COMPANION_CRAFTING_TOOL_EFFECTIVENESS = 10.0f;

	// Companion System (2026-07-29, real-roll hardening): defensive null-lab guard -- see
	// isRegisteredLabratory()'s doc comment. Unreachable today (only
	// 3 labratory ids exist and all 3 are registered) but cheap.
	if (!isRegisteredLabratory(draftSchematic->getLabratory())) {
		errorMessage = "This item's schematic has no registered crafting labratory.";

		applyEffectiveCraftingSkillMods(companion, effectiveSkillMods, -1);

		manufactureSchematic->cleanupIngredientSlots(companion);

		protoLocker.release();
		prototype->destroyObjectFromDatabase(true);

		manuLocker.release();
		manufactureSchematic->destroyObjectFromDatabase(true);
		draftSchematic->destroyObjectFromDatabase(true);
		return false;
	}

	// Owner food-buff fold-in (2026-07-29, real-roll hardening) -- see foldFoodBuffBonusIntoEffectiveness()'s
	// doc comment.
	int assemblyResult = craftingManager->calculateAssemblySuccess(companion, draftSchematic, foldFoodBuffBonusIntoEffectiveness(owner, BuffCRC::FOOD_CRAFT_BONUS, "craft_bonus", COMPANION_CRAFTING_TOOL_EFFECTIVENESS));

	// Sub-component crafts (fillComponentSlot()'s recursive craftItem() call)
	// are invisible to the player -- floor them at MODERATESUCCESS so ONE
	// unlucky sub-roll deep in a recipe can't silently wreck the final item.
	// (Lower enum value = better tier; MODERATESUCCESS=3.)
	if (isSubComponentCraft && assemblyResult > CraftingManager::MODERATESUCCESS) {
		assemblyResult = CraftingManager::MODERATESUCCESS;
	}

	craftingManager->setInitialCraftingValues(prototype, manufactureSchematic, assemblyResult);

	CraftingValues* craftingValues = manufactureSchematic->getCraftingValues();
	craftingValues->setManufactureSchematic(manufactureSchematic);
	craftingValues->setPlayer(companion);

	// Auto-experimentation: spend every point on the owner's remembered
	// preferred line (getPreferredLine() -- the same "optimize for"
	// preference fillResourceSlot()/claimResourceDeedForClass() already use)
	// for this schematic if one is set, otherwise round-robin evenly across
	// every visible attribute row so no stat is left completely untouched.
	int worstExperimentResult = -1;
	// genesis port: getTotalVisibleAttributeGroups() does not exist on genesis's
	// CraftingValues (that is the newer base's AttributesMap API). Genesis's own
	// experimentation loop uses getVisibleExperimentalPropertyTitleSize() for exactly
	// this row count -- see CraftingSessionImplementation.cpp:905. Nothing lost.
	int numberOfExperimentRows = craftingValues->getVisibleExperimentalPropertyTitleSize();

	if (numberOfExperimentRows > 0) {
		String experimentationSkillName = draftSchematic->getExperimentationSkill();
		int experimentationPointsTotal = companion->getSkillMod(experimentationSkillName) / 10;
		int experimentationPointsUsed = 0;

		int preferredLine = getPreferredLine(owner->getObjectID(), draftSchematic->getServerObjectCRC());
		bool hasPreferredRow = preferredLine >= 0 && preferredLine < numberOfExperimentRows;

		// Modest points-per-attempt (the real formula's failure rate worsens
		// with more points spent in a single attempt -- see
		// CraftingManagerImplementation::calculateExperimentationFailureRate()'s
		// "- 5.0f * pointsUsed" term) and a hard iteration cap so a
		// pathological skill-mod value can never loop unbounded.
		const int POINTS_PER_ATTEMPT = 2;
		int rowCursor = 0;
		int guardIterations = 0;

		while (experimentationPointsUsed < experimentationPointsTotal && guardIterations < 200) {
			++guardIterations;

			int pointsRemaining = experimentationPointsTotal - experimentationPointsUsed;
			int pointsAttempted = pointsRemaining < POINTS_PER_ATTEMPT ? pointsRemaining : POINTS_PER_ATTEMPT;

			if (pointsAttempted <= 0) {
				break;
			}

			int rowEffected = hasPreferredRow ? preferredLine : (rowCursor % numberOfExperimentRows);
			++rowCursor;

			int failure = craftingManager->calculateExperimentationFailureRate(companion, manufactureSchematic, pointsAttempted);
			// Owner food-buff fold-in (2026-07-29, real-roll hardening): only feeds the tier decision below,
			// NOT experimentRow()'s own `failure` magnitude parameter a few lines down
			// (deliberately out of scope -- see foldFoodBuffBonusIntoEffectiveness()'s
			// doc comment).
			float foldedFailureForTier = foldFoodBuffBonusIntoEffectiveness(owner, BuffCRC::FOOD_EXPERIMENT_BONUS, "experiment_bonus", (float) failure);
			int experimentationResult = craftingManager->calculateExperimentationSuccess(companion, draftSchematic, foldedFailureForTier);

			craftingManager->experimentRow(manufactureSchematic, craftingValues, rowEffected, pointsAttempted, failure, experimentationResult);

			experimentationPointsUsed += pointsAttempted;

			// Higher enum value = worse tier -- track the worst roll of the
			// session for the chat message below (same convention
			// CraftingSessionImplementation::experiment()'s own
			// "lowestExpSuccess" uses).
			if (experimentationResult > worstExperimentResult) {
				worstExperimentResult = experimentationResult;
			}
		}

		craftingValues->recalculateValues(false);
	}

	// Revert the temporary skill-mod boost now -- both rolls are done, and
	// nothing past this point reads companion->getSkillMod() for crafting.
	applyEffectiveCraftingSkillMods(companion, effectiveSkillMods, -1);

	// ALL experimentation happens before this, the single
	// updateCraftingValues() call -- calling it twice, or before
	// experimentation finishes, would either apply stale percentages or
	// double-apply them.
	prototype->updateCraftingValues(craftingValues, true);

	// Roll-tier messaging (risk mitigation -- variance should read as real
	// gameplay feedback, not a silent/invisible bug).
	String rollMessage = companion->getDisplayedName() + "'s assembly roll: " + craftingResultTierName(assemblyResult);

	if (worstExperimentResult >= 0) {
		rollMessage += " | Experimentation: " + craftingResultTierName(worstExperimentResult);
	}

	owner->sendSystemMessage(rollMessage);

	// Crafter name is the companion (flavor -- "who literally made this"),
	// custom name calls out it was made on the owner's behalf.
	// setCraftersName() takes a non-const String& (not a const ref), so it
	// can't bind directly to a temporary -- needs a named local first.
	String crafterName = companion->getDisplayedName();
	prototype->setCraftersName(crafterName);
	// genesis port: dropped prototype->setCraftersID(companion->getObjectID()) -- genesis's
	// TangibleObject records the crafter by NAME only (craftersName, TangibleObject.idl:67
	// / setCraftersName() :690); there is no craftersID field. setCraftersName(crafterName)
	// immediately above already stamps the companion as the crafter.
	prototype->setCustomObjectName(prototype->getDisplayedName() + " (Crafted by " + crafterName + ")", false);
	prototype->setSerialNumber(craftingManager->generateSerial());
	prototype->updateToDatabase();

	ManagedReference<SceneObject*> ownerInventory = owner->getSlottedObject("inventory");

	if (ownerInventory == nullptr || !ownerInventory->transferObject(prototype, -1, true)) {
		errorMessage = "Your inventory is full -- " + companion->getDisplayedName() + " couldn't hand over the finished item.";
		protoLocker.release();
		prototype->destroyObjectFromDatabase(true);

		manuLocker.release();
		manufactureSchematic->destroyObjectFromDatabase(true);
		draftSchematic->destroyObjectFromDatabase(true);
		return false;
	}

	ownerInventory->broadcastObject(prototype, true);

	// Companion System -- see docs/companion_system/NOTES.md: companion
	// reaction bark for a completed craft (outermost craft only -- sub-
	// component crafts inside craftBatch()/fillComponentSlot() stay silent).
	if (!isSubComponentCraft) {
		CompanionChatter::announceReaction(companion, owner, "craftdone");
	}

	// 2026-07-28 FIX (companion crafting XP parity; REV 2 -- gated on
	// awardXp, see this patch's own top-of-file doc comment for the
	// craftBatch()-triggered-sub-craft-suppression fix): award the
	// companion's own real per-xpType crafting XP now that the craft is
	// unambiguously complete (prototype already transferred into the
	// owner's inventory and broadcast; nothing below this point can still
	// fail). Mirrors the real player path (CraftingSessionImplementation.cpp
	// ~1407), which pulls the same native, stock-data-driven
	// getXpType()/getXpAmount() off the draft schematic.
	if (awardXp) {
		companion->addExperience(draftSchematic->getXpType(), draftSchematic->getXpAmount());
	}

	protoLocker.release();

	manuLocker.release();
	manufactureSchematic->destroyObjectFromDatabase(true);
	draftSchematic->destroyObjectFromDatabase(true);

	return true;
}

// ---------------------------------------------------------------------------
// Batch / factory production (2026-07-27, "factory runs" per Nick)
// ---------------------------------------------------------------------------

bool CompanionCraftingManager::craftBatch(CreatureObject* owner, CompanionObject* companion, const String& draftSchematicTemplate, int quantity, String& errorMessage) const {
	if (owner == nullptr || companion == nullptr) {
		errorMessage = "Missing owner or companion.";
		return false;
	}

	if (quantity < 1) {
		errorMessage = "Quantity must be at least 1.";
		return false;
	}

	if (craftDepth > 6) {
		errorMessage = "Component recipe nests too deep to craft automatically.";
		return false;
	}

	struct DepthGuard {
		int& d;
		DepthGuard(int& dd) : d(dd) { ++d; }
		~DepthGuard() { --d; }
	} depthGuard(craftDepth);

	ZoneServer* zoneServer = owner->getZoneServer();

	if (zoneServer == nullptr) {
		errorMessage = "No zone server available.";
		return false;
	}

	ManagedReference<CraftingManager*> craftingManager = zoneServer->getCraftingManager();

	if (craftingManager == nullptr) {
		errorMessage = "Crafting manager unavailable.";
		return false;
	}

	String file = draftSchematicTemplate;

	if (file.indexOf("draft_schematic") == -1) {
		file = "object/draft_schematic/" + file;
	}

	if (file.indexOf(".iff") == -1) {
		file = file + ".iff";
	}

	ManagedReference<DraftSchematic*> draftSchematic = zoneServer->createObject(file.hashCode(), 0).castTo<DraftSchematic*>();

	if (draftSchematic == nullptr || !draftSchematic->isValidDraftSchematic()) {
		errorMessage = "Not a valid draft schematic: " + file;
		return false;
	}

	// Real stock gate (ManufactureSchematicImplementation::allowFactoryRun()
	// == getFactoryCrateSize() > 0) -- not every item can be mass-produced
	// (sliced/unique items included; the crate size itself is the tell).
	int crateSize = draftSchematic->getFactoryCrateSize();

	if (crateSize <= 0) {
		errorMessage = "This item can't be mass-produced (no factory crate defined for it) -- craft it one at a time instead.";
		draftSchematic->destroyObjectFromDatabase(true);
		return false;
	}

	// Cap at this item's own real crate capacity rather than silently only
	// making part of a bigger request -- errorMessage still gets a clear
	// note about it even though we return true.
	int actualQuantity = quantity;
	bool wasCapped = false;

	if (actualQuantity > crateSize) {
		actualQuantity = crateSize;
		wasCapped = true;
	}

	ManagedReference<ManufactureSchematic*> manufactureSchematic = draftSchematic->createManufactureSchematic(nullptr).castTo<ManufactureSchematic*>();

	if (manufactureSchematic == nullptr) {
		errorMessage = "Could not create a manufacture schematic for " + file;
		draftSchematic->destroyObjectFromDatabase(true);
		return false;
	}

	Locker manuLocker(manufactureSchematic);

	manufactureSchematic->initializeSlotsForHeadlessCraft();

	ManagedReference<TangibleObject*> prototype = zoneServer->createObject(draftSchematic->getTanoCRC(), 0).castTo<TangibleObject*>();

	if (prototype == nullptr) {
		errorMessage = "Unable to create the target item -- is it implemented yet?";
		manuLocker.release();
		manufactureSchematic->destroyObjectFromDatabase(true);
		draftSchematic->destroyObjectFromDatabase(true);
		return false;
	}

	Locker protoLocker(prototype);

	prototype->createChildObjects();

	bool allSlotsFilled = true;

	// The ONE real difference from craftItem(): every ingredient slot is
	// filled at actualQuantity-times its normal amount, drawn UP FRONT --
	// same "hopper" semantics the design calls for, so a shortfall fails
	// cleanly before anything is half-consumed, rather than discovering it
	// partway through N separate craftItem() calls (which would also
	// re-roll/re-gather per unit -- the entire point of a real factory is
	// ONE roll, cloned).
	Vector<int> slotFillOrder = buildSpecificFirstSlotFillOrder(zoneServer, manufactureSchematic, draftSchematic);

	for (int i = 0; i < slotFillOrder.size() && allSlotsFilled; ++i) {
		if (!fillIngredientSlot(owner, companion, manufactureSchematic, draftSchematic, slotFillOrder.get(i), errorMessage, actualQuantity, true)) {
			allSlotsFilled = false;
		}
	}

	if (!allSlotsFilled || !manufactureSchematic->isReadyForAssembly()) {
		if (errorMessage.isEmpty()) {
			errorMessage = "Not all ingredients could be gathered for " + String::valueOf(actualQuantity) + " of this item.";
		}

		manufactureSchematic->cleanupIngredientSlots(companion);

		protoLocker.release();
		prototype->destroyObjectFromDatabase(true);

		manuLocker.release();
		manufactureSchematic->destroyObjectFromDatabase(true);
		draftSchematic->destroyObjectFromDatabase(true);
		return false;
	}

	manufactureSchematic->setAssembled();

	// Real success roll (2026-07-28, batch quality fix -- mirrors the
	// craftItem() quality-roll fix; see that patch's own doc comment
	// and this script's module doc comment for the full writeup).
	// The companion's TRAINED skill (not a hardcoded GREATSUCCESS) now
	// drives a factory run's assembly tier too: temporarily apply the
	// companion's effective crafting skill mods, roll + auto-
	// experiment, THEN revert the temporary mods so nothing about the
	// companion's real, persistent skill-mod state changes.
	VectorMap<String, int> effectiveCraftingSkillMods = buildEffectiveCraftingSkillMods(companion, zoneServer);

	applyEffectiveCraftingSkillMods(companion, effectiveCraftingSkillMods, 1);

	// Companion tool effectiveness: companions have no real crafting
	// tool object, so use the same flat "decent player tool" constant
	// the craftItem() fix uses rather than inventing a second value.
	// Companion System (2026-07-29, real-roll hardening): defensive null-lab guard, same as craftItem()'s --
	// see isRegisteredLabratory()'s doc comment.
	if (!isRegisteredLabratory(draftSchematic->getLabratory())) {
		errorMessage = "This item's schematic has no registered crafting labratory.";

		applyEffectiveCraftingSkillMods(companion, effectiveCraftingSkillMods, -1);

		manufactureSchematic->cleanupIngredientSlots(companion);

		protoLocker.release();
		prototype->destroyObjectFromDatabase(true);

		manuLocker.release();
		manufactureSchematic->destroyObjectFromDatabase(true);
		draftSchematic->destroyObjectFromDatabase(true);
		return false;
	}

	// Owner food-buff fold-in (2026-07-29, real-roll hardening) -- see craftItem()'s identical call site.
	int assemblySuccess = craftingManager->calculateAssemblySuccess(companion, draftSchematic, foldFoodBuffBonusIntoEffectiveness(owner, BuffCRC::FOOD_CRAFT_BONUS, "craft_bonus", 10.0f));

	craftingManager->setInitialCraftingValues(prototype, manufactureSchematic, assemblySuccess);

	CraftingValues* craftingValues = manufactureSchematic->getCraftingValues();
	craftingValues->setManufactureSchematic(manufactureSchematic);
	craftingValues->setPlayer(companion);

	// Auto-experimentation (headless -- no client to spend points
	// row-by-row interactively): the same public roll math
	// CraftingSessionImplementation::initialAssembly()/experiment()
	// use (expskill/10 points, per-row
	// calculateExperimentationFailureRate -> calculateExperimentationSuccess
	// -> experimentRow), spent on the owner's remembered "optimize for"
	// line for this schematic first (existing getPreferredLine()), else
	// spread evenly across every visible row. ALL experimentation
	// happens before the single updateCraftingValues() call below --
	// calling it twice is an ordering hazard (same note as the
	// craftItem() fix).
	// genesis port: getTotalVisibleAttributeGroups() -> getVisibleExperimentalPropertyTitleSize()
	// (same row count; genesis's CraftingSessionImplementation.cpp:905 precedent). Nothing lost.
	int experimentationRowCount = craftingValues->getVisibleExperimentalPropertyTitleSize();
	int experimentationPoints = int(companion->getSkillMod(draftSchematic->getExperimentationSkill()) / 10);

	if (experimentationRowCount > 0 && experimentationPoints > 0) {
		int preferredLine = getPreferredLine(owner->getObjectID(), draftSchematic->getServerObjectCRC());

		if (preferredLine >= 0 && preferredLine < experimentationRowCount) {
			int experimentalFailureRate = craftingManager->calculateExperimentationFailureRate(companion, manufactureSchematic, experimentationPoints);
			// Owner food-buff fold-in (2026-07-29, real-roll hardening) -- see craftItem()'s identical call site.
			float foldedFailureForTier = foldFoodBuffBonusIntoEffectiveness(owner, BuffCRC::FOOD_EXPERIMENT_BONUS, "experiment_bonus", (float) experimentalFailureRate);
			int experimentationResult = craftingManager->calculateExperimentationSuccess(companion, draftSchematic, foldedFailureForTier);

			craftingManager->experimentRow(manufactureSchematic, craftingValues, preferredLine, experimentationPoints, experimentalFailureRate, experimentationResult);
		} else {
			int pointsPerRow = experimentationPoints / experimentationRowCount;

			if (pointsPerRow > 0) {
				for (int row = 0; row < experimentationRowCount; ++row) {
					int pointsAttempted = pointsPerRow;

					if (row == experimentationRowCount - 1) {
						pointsAttempted += experimentationPoints - (pointsPerRow * experimentationRowCount);
					}

					int experimentalFailureRate = craftingManager->calculateExperimentationFailureRate(companion, manufactureSchematic, pointsAttempted);
					// Owner food-buff fold-in (2026-07-29, real-roll hardening) -- see craftItem()'s identical call site.
					float foldedFailureForTier = foldFoodBuffBonusIntoEffectiveness(owner, BuffCRC::FOOD_EXPERIMENT_BONUS, "experiment_bonus", (float) experimentalFailureRate);
					int experimentationResult = craftingManager->calculateExperimentationSuccess(companion, draftSchematic, foldedFailureForTier);

					craftingManager->experimentRow(manufactureSchematic, craftingValues, row, pointsAttempted, experimentalFailureRate, experimentationResult);
				}
			}
		}
	}

	prototype->updateCraftingValues(craftingValues, true);

	// Revert the temporary skill-mod bump now that the roll/
	// experimentation that needed it is done -- the companion's real,
	// persistent skill mods are untouched by this craft.
	applyEffectiveCraftingSkillMods(companion, effectiveCraftingSkillMods, -1);

	String crafterName = companion->getDisplayedName();
	prototype->setCraftersName(crafterName);
	// genesis port: dropped prototype->setCraftersID(companion->getObjectID()) -- genesis's
	// TangibleObject records the crafter by NAME only (craftersName, TangibleObject.idl:67
	// / setCraftersName() :690); there is no craftersID field. setCraftersName(crafterName)
	// immediately above already stamps the companion as the crafter.
	prototype->setCustomObjectName(prototype->getDisplayedName() + " (Crafted by " + crafterName + ")", false);
	prototype->setSerialNumber(craftingManager->generateSerial());
	prototype->updateToDatabase();

	String itemDisplayName = prototype->getDisplayedName();

	// Package into a real FactoryCrate instead of delivering the single
	// prototype directly -- insertSelf=true so the crate holds THIS exact
	// crafted item (identical stats/serial/crafter for every unit -- a real
	// factory clones ONE prototype, it never re-rolls), then bump useCount to
	// the full run size.
	String crateType = draftSchematic->getFactoryCrateType();

	ManagedReference<FactoryCrate*> crate = prototype->createFactoryCrate(crateSize, crateType, true);

	if (crate == nullptr) {
		errorMessage = "Couldn't package the factory run into a crate.";
		protoLocker.release();
		prototype->destroyObjectFromDatabase(true);

		manuLocker.release();
		manufactureSchematic->destroyObjectFromDatabase(true);
		draftSchematic->destroyObjectFromDatabase(true);
		return false;
	}

	// Prototype now belongs to the crate (insertSelf=true transferred it in)
	// -- release its lock now, matching craftItem()'s own timing of releasing
	// protoLocker right after the hand-off succeeds.
	protoLocker.release();

	Locker crateLocker(crate);

	// Companion System (2026-07-27 follow-up, per Fable's review): the real
	// factory bumps useCount ONE AT A TIME (FactoryObjectImplementation.cpp:
	// 727-729, setUseCount(getUseCount()+1)) rather than jumping straight to
	// N. createFactoryCrate(insertSelf=true) already left the crate at
	// useCount=1 for the inserted prototype, so loop the remaining N-1
	// increments through the exact same tested call shape instead of one
	// direct jump to actualQuantity -- cheap, and zero divergence from the
	// only code path that's actually been exercised.
	for (int i = 1; i < actualQuantity; ++i) {
		crate->setUseCount(crate->getUseCount() + 1);
	}

	ManagedReference<SceneObject*> ownerInventory = owner->getSlottedObject("inventory");

	if (ownerInventory == nullptr || !ownerInventory->transferObject(crate, -1, true)) {
		errorMessage = "Your inventory is full -- " + companion->getDisplayedName() + " couldn't hand over the finished factory crate.";
		crateLocker.release();
		crate->destroyObjectFromDatabase(true);

		manuLocker.release();
		manufactureSchematic->destroyObjectFromDatabase(true);
		draftSchematic->destroyObjectFromDatabase(true);
		return false;
	}

	ownerInventory->broadcastObject(crate, true);

	crateLocker.release();

	manuLocker.release();
	manufactureSchematic->destroyObjectFromDatabase(true);
	draftSchematic->destroyObjectFromDatabase(true);

	owner->sendSystemMessage(companion->getDisplayedName() + " finishes a factory run: " + String::valueOf(actualQuantity) + "x " + itemDisplayName + " (Assembly: " + craftingResultTierName(assemblySuccess) + ").");

	if (wasCapped) {
		errorMessage = "(Capped at " + String::valueOf(actualQuantity) + " -- that's the most this item's crate can hold in one run.)";
	} else {
		errorMessage = "";
	}

	return true;
}

// ---------------------------------------------------------------------------
// Slot filling
// ---------------------------------------------------------------------------

bool CompanionCraftingManager::fillIngredientSlot(CreatureObject* owner, CompanionObject* companion, ManufactureSchematic* manufactureSchematic, DraftSchematic* draftSchematic, int slotIndex, String& errorMessage, int quantityMultiplier, bool fromBatchRun) const {
	DraftSlot* draftSlot = draftSchematic->getDraftSlot(slotIndex);

	if (draftSlot == nullptr) {
		errorMessage = "Malformed schematic slot.";
		return false;
	}

	// Companion System (2026-07-27, "factory runs" pass): quantityMultiplier
	// defaults to 1 (unchanged behavior for craftItem()'s single-unit call);
	// craftBatch() passes the real run size so every slot draws enough for
	// the WHOLE run up front, same "hopper" semantics a real factory uses.
	int quantityNeeded = draftSlot->getQuantity() * quantityMultiplier;
	String contentType = draftSlot->getResourceType();

	bool filled;

	if (draftSlot->getSlotType() == IngredientSlot::RESOURCESLOT) {
		filled = fillResourceSlot(owner, companion, manufactureSchematic, draftSchematic, slotIndex, contentType, quantityNeeded, errorMessage);
	} else {
		filled = fillComponentSlot(owner, companion, manufactureSchematic, contentType, quantityNeeded, slotIndex, errorMessage, fromBatchRun);
	}

	if (filled) {
		return true;
	}

	// Companion System (2026-07-24 FIX, "optional additive blocks the whole
	// craft" bug -- per Nick's live report + screenshot of the real Assemble
	// window explicitly labeling a slot "Segment Enhancement (optional)"):
	// DraftSlot::OPTIONALIDENTICALSLOT/OPTIONALMIXEDSLOT are additive slots a
	// real player can leave empty and still assemble (see DraftSlot's own
	// insertToMessage(), which sends the client an explicit "additive is
	// optional" flag for these two slot types). This headless path used to
	// treat every slot as mandatory, so a companion that couldn't source or
	// sub-craft ONE optional additive (e.g. a "Segment Enhancement" bonus
	// component) had its entire craft aborted -- even though every truly
	// required slot (the base segment, the mounting tabs) was already full.
	// Skip silently and let assembly proceed for these two slot types only;
	// every other slot type still fails the craft exactly as before.
	if (draftSlot->getSlotType() == DraftSlot::OPTIONALIDENTICALSLOT || draftSlot->getSlotType() == DraftSlot::OPTIONALMIXEDSLOT) {
		errorMessage = "";
		return true;
	}

	return false;
}

bool CompanionCraftingManager::fillResourceSlot(CreatureObject* owner, CompanionObject* companion, ManufactureSchematic* manufactureSchematic, DraftSchematic* draftSchematic, int slotIndex, const String& resourceClass, int quantityNeeded, String& errorMessage) const {
	ManagedReference<SceneObject*> companionInventory = companion->getSlottedObject("inventory");
	ManagedReference<SceneObject*> ownerInventory = owner->getSlottedObject("inventory");

	if (companionInventory == nullptr) {
		errorMessage = companion->getDisplayedName() + " has no inventory.";
		return false;
	}

	// Companion System (2026-07-27 FIX -- live bug: "claims a resource deed:
	// 30,000 units of Kyinayst... still missing 15 units of steel," repeating
	// identically on retry, despite confirmed inventory room + deeds on hand).
	// Root cause (confirmed via direct source read of ResourceSlot::add()): a
	// slot can hold only ONE specific ResourceSpawn at a time -- real, correct
	// stock behavior, not a bug in ResourceSlot itself. Because this
	// ManufactureSchematic instance can persist partially-filled across
	// retries, check what's ALREADY committed to this slot up front: every
	// acquisition step below is pinned to that exact spawn once something is,
	// instead of independently chasing "best of class" and getting silently
	// rejected by the slot.
	ResourceSlot* resourceSlot = cast<ResourceSlot*>(manufactureSchematic->getSlot(slotIndex));
	ResourceSpawn* committedSpawn = resourceSlot != nullptr ? resourceSlot->getCurrentSpawn() : nullptr;

	// 2026-07-24 "prefer quality always" pass (per user decision, live
	// report: an already-banked but mediocre resource silently won over a
	// much better freshly-claimable one, because the tier loop below only
	// ever escalates on QUANTITY shortfall, never on QUALITY headroom --
	// once tier 0 already has "enough" of something, tiers 1/2 never even
	// run). Before touching the slot, score the best candidate ALREADY on
	// hand against the best candidate currently obtainable (harvester or
	// resource deed). If a fresh source would score meaningfully higher,
	// proactively stage it into companion inventory FIRST -- both
	// withdrawFromOwnerHarvester() and claimResourceDeedForClass() are
	// already safe no-ops when nothing's actually available (no matching
	// harvester / no deed on hand), so this can't ADD a failure mode, only
	// improve the candidate pool the existing best-first fill loop below
	// already picks from. For schematics that don't weigh this slot at all,
	// scoreResourceSpawn() returns 0 for every candidate either way, so the
	// comparison naturally never fires and plain/scrap crafts are
	// untouched -- no separate complexity threshold needed.
	// Companion System (2026-07-27 FIX): skip this pre-stage entirely once
	// something is already committed to the slot -- there's nothing left to
	// "upgrade," and running it anyway is exactly what claims a wasted
	// resource deed / harvester withdrawal of a DIFFERENT spawn that the
	// slot will silently reject below.
	if (committedSpawn == nullptr) {
		float bestOwnedScore = 0.0f;
		bool haveOwnedCandidate = false;

		SceneObject* ownedContainers[2] = { ownerInventory.get(), companionInventory.get() };

		for (int c = 0; c < 2; ++c) {
			SceneObject* container = ownedContainers[c];

			if (container == nullptr) {
				continue;
			}

			for (int i = 0; i < container->getContainerObjectsSize(); ++i) {
				ManagedReference<SceneObject*> obj = container->getContainerObject(i);

				if (obj == nullptr || !obj->isResourceContainer()) {
					continue;
				}

				ResourceContainer* resource = cast<ResourceContainer*>(obj.get());

				if (resource == nullptr || resource->getSpawnObject() == nullptr || !resource->getSpawnObject()->isType(resourceClass)) {
					continue;
				}

				float score = scoreResourceSpawn(draftSchematic, slotIndex, resource->getSpawnObject());

				if (!haveOwnedCandidate || score > bestOwnedScore) {
					bestOwnedScore = score;
					haveOwnedCandidate = true;
				}
			}
		}

		ZoneServer* zoneServer = owner->getZoneServer();
		ManagedReference<ResourceManager*> resourceManager = zoneServer != nullptr ? zoneServer->getResourceManager() : nullptr;

		if (resourceManager != nullptr) {
			ResourceSpawn* freshCandidate = resourceManager->getBestSpawnOfTypeWeighted(resourceClass, draftSchematic, slotIndex);

			if (freshCandidate == nullptr) {
				Zone* zone = owner->getZone();

				if (zone != nullptr) {
					freshCandidate = resourceManager->getBestSpawnOfType(resourceClass, zone->getZoneName());
				}
			}

			if (freshCandidate != nullptr) {
				float freshScore = scoreResourceSpawn(draftSchematic, slotIndex, freshCandidate);

				// 10% headroom margin -- avoids burning a harvester
				// withdrawal/resource deed over a rounding-level difference;
				// tune QUALITY_UPGRADE_MARGIN below if this proves too eager
				// or too conservative in practice.
				const float QUALITY_UPGRADE_MARGIN = 1.10f;

				if (!haveOwnedCandidate || freshScore > bestOwnedScore * QUALITY_UPGRADE_MARGIN) {
					withdrawFromOwnerHarvester(owner, companion, resourceClass, quantityNeeded);
					claimResourceDeedForClass(owner, companion, resourceClass, draftSchematic);
				}
			}
		}
	}

	// Try up to 3 tiers, escalating further only if the slot is STILL short
	// on quantity after the quality-upgrade staging above. Each tier scans +
	// best-first-adds until the slot is full or that tier's candidates are
	// exhausted -- the best-first pick already naturally prefers whatever
	// higher-quality resource was just staged over older, lower-quality
	// stock, so no change was needed here beyond what runs above.
	for (int tier = 0; tier < 3; ++tier) {
		if (manufactureSchematic->getSlot(slotIndex)->isFull()) {
			return true;
		}

		if (tier == 1) {
			int stillNeeded = quantityNeeded - manufactureSchematic->getSlot(slotIndex)->getSlotQuantity();
			withdrawFromOwnerHarvester(owner, companion, resourceClass, stillNeeded, committedSpawn);
		} else if (tier == 2) {
			// 2026-07-20: schematic passed through so the deed can claim
			// the resource that maximizes the owner's preferred
			// experimentation line for this exact item.
			claimResourceDeedForClass(owner, companion, resourceClass, draftSchematic, committedSpawn);
		}

		// Gather + score every matching ResourceContainer currently sitting in
		// owner or companion inventory (tier 0 sees whatever was already
		// there; tiers 1/2 also see whatever the acquisition step above just
		// staged into companion's inventory).
		Vector<ManagedReference<ResourceContainer*>> candidates;
		SceneObject* containers[2] = { ownerInventory.get(), companionInventory.get() };

		for (int c = 0; c < 2; ++c) {
			SceneObject* container = containers[c];

			if (container == nullptr) {
				continue;
			}

			for (int i = 0; i < container->getContainerObjectsSize(); ++i) {
				ManagedReference<SceneObject*> obj = container->getContainerObject(i);

				if (obj == nullptr || !obj->isResourceContainer()) {
					continue;
				}

				ResourceContainer* resource = cast<ResourceContainer*>(obj.get());

				if (resource == nullptr || resource->getSpawnObject() == nullptr) {
					continue;
				}

				if (!resource->getSpawnObject()->isType(resourceClass)) {
					continue;
				}

				candidates.add(resource);
			}
		}

		// Best-first: use the schematic's own real resource weight table to
		// prefer the stack that will make the finished item best (user
		// request), instead of just grabbing whatever's first. Repeatedly
		// pulls out whichever remaining candidate scores highest rather than
		// sorting in place (avoids relying on an in-place swap on Vector).
		while (candidates.size() > 0 && !manufactureSchematic->getSlot(slotIndex)->isFull()) {
			int bestIndex = 0;
			float bestScore = scoreResourceSpawn(draftSchematic, slotIndex, candidates.get(0)->getSpawnObject());

			for (int i = 1; i < candidates.size(); ++i) {
				float score = scoreResourceSpawn(draftSchematic, slotIndex, candidates.get(i)->getSpawnObject());

				if (score > bestScore) {
					bestScore = score;
					bestIndex = i;
				}
			}

			ManagedReference<ResourceContainer*> resource = candidates.get(bestIndex);
			candidates.remove(bestIndex);

			Locker resLocker(resource, companion);
			manufactureSchematic->addIngredientToSlot(companion, companionInventory, resource, slotIndex);
		}
	}

	if (manufactureSchematic->getSlot(slotIndex)->isFull()) {
		return true;
	}

	int stillShort = quantityNeeded - manufactureSchematic->getSlot(slotIndex)->getSlotQuantity();
	errorMessage = companion->getDisplayedName() + " is missing " + String::valueOf(stillShort) + " units of " + resourceClass + " (checked inventory, your placed harvesters, and resource deeds).";
	return false;
}

bool CompanionCraftingManager::fillComponentSlot(CreatureObject* owner, CompanionObject* companion, ManufactureSchematic* manufactureSchematic, const String& componentTemplate, int quantityNeeded, int slotIndex, String& errorMessage, bool fromBatchRun) const {
	ManagedReference<SceneObject*> companionInventory = companion->getSlottedObject("inventory");
	ManagedReference<SceneObject*> ownerInventory = owner->getSlottedObject("inventory");

	SceneObject* containers[2] = { ownerInventory.get(), companionInventory.get() };

	for (int c = 0; c < 2 && !manufactureSchematic->getSlot(slotIndex)->isFull(); ++c) {
		SceneObject* container = containers[c];

		if (container == nullptr) {
			continue;
		}

		for (int i = 0; i < container->getContainerObjectsSize() && !manufactureSchematic->getSlot(slotIndex)->isFull(); ++i) {
			ManagedReference<SceneObject*> obj = container->getContainerObject(i);

			if (obj == nullptr || !obj->isTangibleObject()) {
				continue;
			}

			TangibleObject* tano = cast<TangibleObject*>(obj.get());

			if (tano == nullptr) {
				continue;
			}

			// Unlike ResourceSlot::add() (which does NOT lock its incoming
			// resource -- see the Locker around the resource-slot call
			// below), ComponentSlot::add() locks the incoming component
			// itself internally, so it must NOT be pre-locked here too.
			manufactureSchematic->addIngredientToSlot(companion, companionInventory, tano, slotIndex);
		}
	}

	if (manufactureSchematic->getSlot(slotIndex)->isFull()) {
		return true;
	}

	// Sub-component crafting (2026-07-20, user request "make those crafting
	// steps if there is a pre-required item to craft the item we want"): if
	// the companion knows a schematic that PRODUCES this component, craft it
	// first (recursively -- that sub-craft gathers/crafts ITS own
	// ingredients too, guarded by craftDepth), then re-scan and slot the
	// freshly-made component. Loop until the slot is full or a craft fails.
	int safety = quantityNeeded + 2; // never more crafts than pieces needed (+slack)

	while (!manufactureSchematic->getSlot(slotIndex)->isFull() && safety-- > 0) {
		String producerPath;
		CompanionObject* producer = findCrafterForComponent(owner, companion, componentTemplate, producerPath);

		if (producer == nullptr || producerPath.isEmpty()) {
			break; // nobody in the squad can make this component
		}

		// 2026-07-24: name the ACTUAL crafter -- may be a squadmate with the
		// right profession (e.g. the Tailor making synthetic cloth for the
		// Armorsmith), not always the companion doing the main assembly.
		if (producer == companion) {
			owner->sendSystemMessage(companion->getDisplayedName() + " crafts a required part first...");
		} else {
			owner->sendSystemMessage(producer->getDisplayedName() + " crafts a required part for " + companion->getDisplayedName() + " first...");
		}

		String subError;
		bool subCraftOk;

		if (producer == companion) {
			// Same object the caller already has locked -- unchanged from
			// before this pass, no additional lock needed or safe to take
			// (would self-deadlock).
			subCraftOk = craftItem(owner, producer, producerPath, subError, true, !fromBatchRun);
		} else {
			// Cross-companion: producer is NOT already locked by this call
			// chain -- same cross-lock-off-an-already-held-object pattern as
			// CompanionCraftTheater::runStep's "Locker dlocker(donor, crafter)"
			// for resource-donor trades just above in that file.
			Locker producerLocker(producer, companion);
			subCraftOk = craftItem(owner, producer, producerPath, subError, true, !fromBatchRun);
		}

		if (!subCraftOk) {
			errorMessage = producer->getDisplayedName() + " couldn't craft the required part: " + subError;
			return false;
		}

		// The freshly-crafted component landed in the OWNER's inventory --
		// scan owner + companion inventory and slot it.
		bool addedAny = false;

		for (int c = 0; c < 2 && !manufactureSchematic->getSlot(slotIndex)->isFull(); ++c) {
			SceneObject* container = containers[c];

			if (container == nullptr) {
				continue;
			}

			for (int i = 0; i < container->getContainerObjectsSize() && !manufactureSchematic->getSlot(slotIndex)->isFull(); ++i) {
				ManagedReference<SceneObject*> obj = container->getContainerObject(i);

				if (obj == nullptr || !obj->isTangibleObject()) {
					continue;
				}

				TangibleObject* tano = cast<TangibleObject*>(obj.get());

				if (tano == nullptr) {
					continue;
				}

				int before = manufactureSchematic->getSlot(slotIndex)->getSlotQuantity();
				manufactureSchematic->addIngredientToSlot(companion, companionInventory, tano, slotIndex);

				if (manufactureSchematic->getSlot(slotIndex)->getSlotQuantity() > before) {
					addedAny = true;
				}
			}
		}

		if (!addedAny) {
			break; // the craft produced something the slot won't accept -- give up cleanly
		}
	}

	if (manufactureSchematic->getSlot(slotIndex)->isFull()) {
		return true;
	}

	errorMessage = companion->getDisplayedName() + " is missing a component it can't make: " + componentTemplate + ".";
	return false;
}

// ---------------------------------------------------------------------------
// Reverse component -> schematic lookup (sub-component crafting)
// ---------------------------------------------------------------------------

// Companion System (2026-07-24 fix -- confirmed via direct read of the real
// Lua schematic source on Nick's machine): some draft schematics declare
// their targetTemplate WITHOUT a "shared_" prefix (armor_segment_bone.lua:
// targetTemplate = "object/tangible/component/armor/armor_segment_bone.iff")
// while OTHER schematics that consume that exact component as an ingredient
// reference it WITH the prefix (clothing_armor_bone_chest.lua's
// resourceTypes entry "object/tangible/component/armor/
// shared_armor_segment_bone.iff"). findSchematicForComponent() below used to
// compare producer CRCs against only the literal wanted CRC, so this real,
// live schematic pair (and likely others like it) never matched -- the
// companion would report "is missing a component it can't make" even though
// it had a fully learned schematic that produces exactly that item, just
// under the other spelling. Toggling the "shared_" prefix on the filename
// (add if absent, strip if present) and checking BOTH CRCs closes the gap
// without needing to know in advance which side is "right".
static uint32 toggleSharedPrefixCRC(const String& templatePath) {
	int slashIdx = templatePath.lastIndexOf('/');
	String dir = slashIdx == -1 ? String("") : templatePath.subString(0, slashIdx + 1);
	String filename = slashIdx == -1 ? templatePath : templatePath.subString(slashIdx + 1);

	String toggled;

	if (filename.beginsWith("shared_")) {
		toggled = filename.subString(7); // 7 == strlen("shared_")
	} else {
		toggled = "shared_" + filename;
	}

	return (dir + toggled).hashCode();
}

// 2026-07-28 FIX: String-returning twin of toggleSharedPrefixCRC() above.
// Needed because SharedObjectTemplate::isDerivedFrom() (the real,
// player-facing check ComponentSlot::add() uses -- see ComponentSlot.h
// ~line 70) takes a template-path String, not a hash, so
// findSchematicForComponent()'s class-placeholder derivation fallback
// below needs the "shared_"-toggled spelling as a String too.
static String toggleSharedPrefixString(const String& templatePath) {
	int slashIdx = templatePath.lastIndexOf('/');
	String dir = slashIdx == -1 ? String("") : templatePath.subString(0, slashIdx + 1);
	String filename = slashIdx == -1 ? templatePath : templatePath.subString(slashIdx + 1);

	if (filename.beginsWith("shared_")) {
		return dir + filename.subString(7); // 7 == strlen("shared_")
	}

	return dir + "shared_" + filename;
}

String CompanionCraftingManager::findSchematicForComponent(CompanionObject* companion, const String& componentTemplate) const {
	if (companion == nullptr || componentTemplate.isEmpty()) {
		return "";
	}

	// The component slot wants a tangible whose template-CRC is this. A draft
	// schematic that PRODUCES that tangible reports it via getTanoCRC().
	// wantCRCToggled covers the same component under the OTHER "shared_"
	// spelling -- see the toggleSharedPrefixCRC() doc comment above.
	uint32 wantCRC = componentTemplate.hashCode();
	uint32 wantCRCToggled = toggleSharedPrefixCRC(componentTemplate);

	// 2026-07-28 FIX: String form of the toggled spelling, for the
	// isDerivedFrom() fallback below (see toggleSharedPrefixString() doc
	// comment above).
	String componentTemplateToggled = toggleSharedPrefixString(componentTemplate);

	// Only consider schematics the companion's learned skills actually grant
	// (same enumeration CompanionCraftPickSuiCallback uses for the craft UI).
	for (int s = 0; s < companion->getLearnedSkillCount(); ++s) {
		Skill* skill = SkillManager::instance()->getSkill(companion->getLearnedSkill(s));

		if (skill == nullptr) {
			continue;
		}

		const Vector<String>* groups = skill->getSchematicsGranted();

		if (groups == nullptr) {
			continue;
		}

		for (int g = 0; g < groups->size(); ++g) {
			DraftSchematicGroup* group = SchematicMap::instance()->getGroup(groups->get(g));

			if (group == nullptr) {
				continue;
			}

			for (int d = 0; d < group->size(); ++d) {
				DraftSchematic* schematic = group->get(d).get();

				if (schematic == nullptr || schematic->getObjectTemplate() == nullptr) {
					continue;
				}

				if (schematic->getTanoCRC() == wantCRC || schematic->getTanoCRC() == wantCRCToggled) {
					return schematic->getObjectTemplate()->getFullTemplateString();
				}

				// 2026-07-28 FIX: exact-CRC match above only catches schematics
				// whose own produced tano IS the wanted component template. Some
				// component slots instead want a SWG "class placeholder" base
				// template (e.g. object/tangible/component/food/base/
				// shared_drink_container_base.iff for Vasarian Brandy) that is
				// never produced directly by any schematic -- only concrete
				// templates DERIVED from it are (container_small_glass.iff,
				// container_large_glass.iff, container_cask.iff,
				// container_barrel.iff, etc, via the compiled .iff DERV chain).
				// No CRC toggling can bridge that. Mirror the real player-facing
				// check (ComponentSlot::add(), ComponentSlot.h ~line 70) by
				// asking the schematic's produced template whether it derives
				// from the wanted component template, or its "shared_"-toggled
				// spelling.
				// Companion System (2026-07-29 FIX (drink-container-base / class-placeholder lookup)): schematic->getObjectTemplate()
				// returns the DraftSchematic SCENE OBJECT'S OWN template (a
				// DraftSchematicObjectTemplate under object/draft_schematic/...) --
				// NOT the template of the tangible item the schematic PRODUCES. That
				// made the isDerivedFrom() checks below unreachable for every "class
				// placeholder" component (e.g. shared_drink_container_base.iff,
				// satisfied only by container_barrel/cask/large_glass/small_glass,
				// never produced directly) -- confirmed live, "Great success!" on an
				// unrelated sub-craft (e.g. the alcohol slot) followed immediately by
				// "is missing a component it can't make: ...shared_drink_container_base.iff"
				// for the SEPARATE, still-unresolved container slot. Resolve the real
				// produced-item template instead via TemplateManager + the schematic's
				// own public getTanoCRC() -- the same resolution
				// DraftSchematicObjectTemplate::getResourceWeights() already does
				// internally for its own private tangibleTemplate field.
				SharedObjectTemplate* producedTemplate = TemplateManager::instance()->getTemplate(schematic->getTanoCRC());

				if (producedTemplate != nullptr && (producedTemplate->isDerivedFrom(componentTemplate) || producedTemplate->isDerivedFrom(componentTemplateToggled))) {
					// 2026-07-29 night #3 FIX (return schematic path, not produced-item path): producedTemplate is only the lookup key for the
					// isDerivedFrom() check above -- craftItem() (this same file,
					// top of file) needs an actual draft_schematic template path
					// to instantiate, exactly like the exact-CRC-match branch
					// just above returns. Returning producedTemplate's own path
					// here instead handed craftItem() a bare tangible-item path
					// (no "draft_schematic" substring), which craftItem() then
					// mis-prefixed into a malformed double path -- confirmed live
					// via Nick's Chef: "Not a valid draft schematic: object/
					// draft_schematic/object/tangible/component/food/
					// container_small_glass.iff" trying to craft Vasarian Brandy.
					return schematic->getObjectTemplate()->getFullTemplateString();
				}
			}
		}
	}

	return "";
}

// ---------------------------------------------------------------------------
// Multi-companion collaboration (2026-07-24): cross-profession components
// ---------------------------------------------------------------------------

CompanionObject* CompanionCraftingManager::findCrafterForComponent(CreatureObject* owner, CompanionObject* primaryCompanion, const String& componentTemplate, String& producerPath) const {
	producerPath = "";

	if (owner == nullptr || primaryCompanion == nullptr || componentTemplate.isEmpty()) {
		return nullptr;
	}

	// Try the companion actually doing the assembly first -- unchanged
	// priority/behavior from before this pass.
	producerPath = findSchematicForComponent(primaryCompanion, componentTemplate);

	if (!producerPath.isEmpty()) {
		return primaryCompanion;
	}

	// Not something this companion knows -- check the rest of the owner's
	// active squad (same datapad-scan pattern CompanionCraftTheater::begin()
	// uses to find resource donors), in case a squadmate with a different
	// profession (e.g. a Tailor making synthetic cloth for an Armorsmith)
	// can make it instead.
	ManagedReference<SceneObject*> datapad = owner->getSlottedObject("datapad");

	if (datapad == nullptr) {
		return nullptr;
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

		CompanionObject* sibling = device->getCompanionObject();

		if (sibling == nullptr || sibling == primaryCompanion || sibling->getZone() == nullptr
				|| sibling->getLinkedCreature().get() != owner) {
			continue;
		}

		String siblingPath = findSchematicForComponent(sibling, componentTemplate);

		if (!siblingPath.isEmpty()) {
			producerPath = siblingPath;
			return sibling;
		}
	}

	return nullptr;
}

// ---------------------------------------------------------------------------
// Resource scoring
// ---------------------------------------------------------------------------

float CompanionCraftingManager::scoreResourceSpawn(DraftSchematic* draftSchematic, int slotIndex, ResourceSpawn* spawn) const {
	if (draftSchematic == nullptr || spawn == nullptr) {
		return 0.0f;
	}

	if (slotIndex >= draftSchematic->getResourceWeightCount()) {
		return 0.0f;
	}

	ResourceWeight* weight = draftSchematic->getResourceWeight(slotIndex);

	if (weight == nullptr) {
		return 0.0f;
	}

	float score = 0.0f;

	for (int i = 0; i < weight->getPropertyListSize(); ++i) {
		int propertyCode = weight->getTypeAndWeight(i) >> 4;

		if (propertyCode == 0) {
			continue;
		}

		score += spawn->getValueOf(propertyCode) * weight->getPropertyPercentage(i);
	}

	return score;
}

// ---------------------------------------------------------------------------
// Acquisition chain: harvester withdrawal + resource deed claim
// ---------------------------------------------------------------------------

int CompanionCraftingManager::withdrawFromOwnerHarvester(CreatureObject* owner, CompanionObject* companion, const String& resourceClass, int quantityNeeded, ResourceSpawn* requiredSpawn) const {
	if (quantityNeeded <= 0) {
		return 0;
	}

	Zone* zone = owner->getZone();
	ZoneServer* zoneServer = owner->getZoneServer();

	if (zone == nullptr || zoneServer == nullptr) {
		return 0;
	}

	// genesis port: QuadTreeEntry (genesis predates the QuadTreeEntry -> TreeEntry
	// rename) and the 6-arg 2D Zone::getInRangeObjects(x, y, range, objects,
	// readLockZone, includeBuildingObjects) -- the newer base's 3D overload took
	// (x, z, y, range, ...). Dropped the z argument. LOST: the query is now a
	// cylinder around (x, y) instead of a sphere around (x, z, y), so objects far
	// above/below the caller that the 3D form excluded can now match.
	SortedVector<ManagedReference<QuadTreeEntry*>> nearbyObjects;
	// 512m is a generous "somewhere on the owner's homestead" radius -- a
	// placed harvester can legitimately be well outside normal interaction
	// range of wherever the owner happens to be standing right now.
	zone->getInRangeObjects(owner->getPositionX(), owner->getPositionY(), 512, &nearbyObjects, true, true);

	int withdrawn = 0;

	for (int i = 0; i < nearbyObjects.size() && withdrawn < quantityNeeded; ++i) {
		SceneObject* scno = cast<SceneObject*>(nearbyObjects.get(i).get());

		if (scno == nullptr || !scno->isHarvesterObject()) {
			continue;
		}

		HarvesterObject* harvester = cast<HarvesterObject*>(scno);

		if (harvester == nullptr || harvester->getOwnerObjectID() != owner->getObjectID()) {
			continue;
		}

		uint64 spawnID = harvester->getActiveResourceSpawnID();

		if (spawnID == 0) {
			continue;
		}

		ManagedReference<ResourceSpawn*> spawn = zoneServer->getObject(spawnID).castTo<ResourceSpawn*>();

		if (spawn == nullptr || !spawn->isType(resourceClass)) {
			continue;
		}

		// 2026-07-27 FIX: once a slot already has a different spawn committed,
		// only THIS exact spawn can merge into it -- see fillResourceSlot().
		if (requiredSpawn != nullptr && spawn.get() != requiredSpawn) {
			continue;
		}

		ManagedReference<ResourceContainer*> hopperContainer = harvester->getContainerFromHopper(spawn);

		if (hopperContainer == nullptr) {
			continue;
		}

		Locker hopperLocker(hopperContainer, companion);

		int available = hopperContainer->getQuantity();
		int take = Math::min(available, quantityNeeded - withdrawn);

		if (take <= 0) {
			continue;
		}

		// Generic overload -- deposits straight into companion's own
		// inventory, no isPlayerCreature() gate in the impl itself (only the
		// player-only command wrapper, ResourceContainerSplitCommand, is
		// gated -- confirmed via research this pass).
		hopperContainer->split(take, companion);
		withdrawn += take;
	}

	return withdrawn;
}

int CompanionCraftingManager::getPreferredLine(uint64 playerID, uint32 schematicCRC) const {
	String key = String::valueOf(playerID) + ":" + String::valueOf(schematicCRC);

	if (!preferredExperimentalLines.contains(key)) {
		return -1;
	}

	return preferredExperimentalLines.get(key);
}

void CompanionCraftingManager::setPreferredLine(uint64 playerID, uint32 schematicCRC, int lineIndex) {
	String key = String::valueOf(playerID) + ":" + String::valueOf(schematicCRC);

	preferredExperimentalLines.drop(key);
	preferredExperimentalLines.put(key, lineIndex);
}

void CompanionCraftingManager::stampSharedSerial(const Vector<ManagedReference<TangibleObject*> >& components) const {
	if (components.size() == 0) {
		return;
	}

	// One shared serial for the whole batch -- built from the first
	// component's template CRC + a time seed, formatted like a real
	// factory serial ("(####-####-####)" style is what the client shows;
	// setSerialNumber() takes the raw string).
	TangibleObject* first = components.get(0).get();

	if (first == nullptr) {
		return;
	}

	uint32 base = first->getServerObjectCRC() ^ (uint32) System::getMiliTime();
	String serial = String::valueOf((uint64) base);

	for (int i = 0; i < components.size(); ++i) {
		TangibleObject* component = components.get(i).get();

		if (component == nullptr) {
			continue;
		}

		Locker clocker(component);
		component->setSerialNumber(serial);
	}
}

bool CompanionCraftingManager::claimResourceDeedForClass(CreatureObject* owner, CompanionObject* companion, const String& resourceClass, DraftSchematic* draftSchematic, ResourceSpawn* requiredSpawn) const {
	// 2026-07-20 FIX (live report: "companion can't use a crate of free
	// resource deeds"): the old detection required a template lineage of
	// object/tangible/deed/resource/resource_deed.iff -- a path that does
	// NOT exist in this install's data (verified: bin/scripts has NO
	// deed/resource directory at all), so no deed was EVER matched. The
	// authoritative test is the C++ class itself: ObjectManager binds
	// ResourceDeed by SceneObjectType::RESOURCEDEED, so a successful
	// cast<ResourceDeed*> IS the check -- no path assumptions. Also: the
	// deeds commonly arrive INSIDE a crate ("crate of free resources"),
	// and may have been handed to the COMPANION -- so the scan now walks
	// the owner's inventory, the companion (and its bag), AND one level
	// into any sub-containers (crates) of all of the above.
	ManagedReference<ResourceDeed*> deed = nullptr;

	auto scanContainer = [&deed](SceneObject* container, bool descendIntoCrates) {
		if (container == nullptr || deed != nullptr) {
			return;
		}

		for (int i = 0; i < container->getContainerObjectsSize() && deed == nullptr; ++i) {
			ManagedReference<SceneObject*> obj = container->getContainerObject(i);

			if (obj == nullptr) {
				continue;
			}

			ResourceDeed* candidate = cast<ResourceDeed*>(obj.get());

			if (candidate != nullptr) {
				deed = candidate;
				return;
			}

			// One level into crates/boxes (any tangible that itself holds
			// items) -- exactly where the "free resources" deeds live.
			if (descendIntoCrates && obj->isTangibleObject() && obj->getContainerObjectsSize() > 0) {
				for (int j = 0; j < obj->getContainerObjectsSize(); ++j) {
					ManagedReference<SceneObject*> inner = obj->getContainerObject(j);

					if (inner == nullptr) {
						continue;
					}

					ResourceDeed* innerCandidate = cast<ResourceDeed*>(inner.get());

					if (innerCandidate != nullptr) {
						deed = innerCandidate;
						return;
					}
				}
			}
		}
	};

	scanContainer(owner->getSlottedObject("inventory"), true);
	scanContainer(companion, true);
	scanContainer(companion->getSlottedObject("inventory"), true);

	if (deed == nullptr) {
		return false;
	}

	ZoneServer* zoneServer = owner->getZoneServer();
	Zone* zone = owner->getZone();

	if (zoneServer == nullptr || zone == nullptr) {
		return false;
	}

	ManagedReference<ResourceManager*> resourceManager = zoneServer->getResourceManager();

	if (resourceManager == nullptr) {
		return false;
	}

	// Highest-quality in-spawn match for the class (2026-07-20, live-debug
	// history: getCurrentSpawn()'s getType() substring matching NEVER
	// matches broad class tokens like "chemical" -- confirmed "NONE on
	// this planet" for chemical AND metal on tatooine via checkpoints --
	// while getBestSpawnOfType() ALSO tests ResourceSpawn::isType(), the
	// real class-token check, and its scoring is bounds-safe as of today's
	// ResourceSpawner.cpp fix).
	// 2026-07-20 optimization pass: with a schematic + a remembered
	// preferred experimental line (BER etc.), the pick maximizes THAT
	// line's real weight formula instead of generic quality.
	ResourceSpawn* liveSpawn = nullptr;

	// 2026-07-27 FIX: a slot that already has a different spawn committed can
	// only ever accept MORE of that exact spawn -- pin the deed straight to it
	// and skip the "best of class" search entirely (see fillResourceSlot()).
	if (requiredSpawn != nullptr) {
		liveSpawn = requiredSpawn;
	}

	int preferredLine = draftSchematic != nullptr ? getPreferredLine(owner->getObjectID(), draftSchematic->getServerObjectCRC()) : -1;

	// 2026-07-24 fix (live bug: composite armor schematic showed 75%/80%
	// potential but crafted at 11%/16% actual): this used to only use the
	// schematic-weighted pick when the player had explicitly gone through
	// the "optimize for" picker (CompanionCraftOptimizeSuiCallback, SUI
	// 1219) for this exact schematic -- but that picker only ASKS when a
	// schematic defines 2+ real weight lines (maybeAskOptimizeLine's own
	// lineCount<2 guard, CompanionCraftPickSuiCallback.h), so single-line
	// schematics (and multi-line ones nobody's answered yet) fell all the
	// way back to a GENERIC "best overall quality" pick with zero
	// awareness of which properties this schematic's resource slots
	// actually weigh. c3rr confirmed (2026-07-24) preferredLine indexes
	// the schematic's own DraftSchematic::getResourceWeight() line table
	// -- the same table scoreResourceSpawn() already uses -- so line 0 is
	// always a real, meaningful weight profile as long as the schematic
	// defines at least one. Defaulting to it here means EVERY deed claim
	// gets schematic-weighted quality, not just ones the player has
	// explicitly optimized; an explicit preference (once set) still wins.
	if (requiredSpawn == nullptr) {
		if (draftSchematic != nullptr && draftSchematic->getResourceWeightCount() > 0) {
			int lineToUse = preferredLine >= 0 ? preferredLine : 0;
			liveSpawn = resourceManager->getBestSpawnOfTypeWeighted(resourceClass, draftSchematic, lineToUse);
		}

		if (liveSpawn == nullptr) {
			liveSpawn = resourceManager->getBestSpawnOfType(resourceClass, zone->getZoneName());
		}
	}

	if (liveSpawn == nullptr) {
		// Not one resource of this class in the server's entire spawn
		// history -- essentially impossible on an established server.
		owner->sendSystemMessage("Your companion found a resource deed, but no resource of class '" + resourceClass + "' has ever spawned on this server.");
		return false;
	}

	// Companion System (2026-07-27 FIX): confirmed root cause of "claims a
	// resource deed... still missing N units," repeating on every retry
	// while burning a real deed each time for nothing: ResourceManager::
	// getResourceSpawn() (used internally by the old givePlayerResource()
	// call this replaced) looks up the resourceMap by
	// spawnName.toLowerCase(), but every insertion path in ResourceSpawner
	// stores the spawn under its natural, capitalized name (e.g.
	// "Kyinayst") -- so that lookup can never match for any normally-cased
	// resource name, and givePlayerResource() silently no-ops (hits its own
	// "spawn does not exist" early return) every single time. We already
	// hold the correct ResourceSpawn* directly (liveSpawn) from the search
	// above, so this bypasses that broken by-name re-lookup entirely by
	// creating/depositing the resource straight from the object we already
	// have -- the exact same createResource()/extractResource()/
	// transferObject() sequence givePlayerResource() itself uses
	// internally, just without the broken name lookup in front of it.
	// Deed is now destroyed AFTER a confirmed successful deposit (it was
	// previously destroyed unconditionally beforehand), so a failed claim
	// no longer wastes a real deed for nothing.
	ManagedReference<SceneObject*> companionInventoryForDeed = companion->getSlottedObject("inventory");

	if (companionInventoryForDeed == nullptr || companionInventoryForDeed->isContainerFullRecursive()) {
		owner->sendSystemMessage(companion->getDisplayedName() + "'s inventory has no room for a resource deed claim -- nothing spent.");
		return false;
	}

	Locker spawnLocker(liveSpawn);
	ManagedReference<ResourceContainer*> newResource = liveSpawn->createResource(ResourceManager::RESOURCE_DEED_QUANTITY);

	if (newResource == nullptr) {
		spawnLocker.release();
		owner->sendSystemMessage("Your companion tried to claim a resource deed for " + liveSpawn->getName() + ", but the claim failed -- nothing spent, try again.");
		return false;
	}

	liveSpawn->extractResource("", ResourceManager::RESOURCE_DEED_QUANTITY);
	spawnLocker.release();

	Locker newResLocker(newResource);

	if (!companionInventoryForDeed->transferObject(newResource, -1, true)) {
		newResource->destroyObjectFromDatabase(true);
		owner->sendSystemMessage(companion->getDisplayedName() + "'s inventory rejected the resource deed claim -- nothing spent, try again.");
		return false;
	}

	companionInventoryForDeed->broadcastObject(newResource, true);
	newResLocker.release();

	Locker deedLocker(deed, companion);
	deed->destroyDeed();
	deedLocker.release();

	owner->sendSystemMessage("Your companion claims a resource deed: 10,000 units of " + liveSpawn->getName() + " (best '" + resourceClass + "' ever in spawn).");

	return true;
}

// ---------------------------------------------------------------------------
// Test-resource bag (2026-07-24, per user request "can I have a radial that
// requests the best resources for a certain item, so I can test what the
// companions are using -- place a bag in the user's inventory and fill it
// with the same resources the companion used, so we can compare")
// ---------------------------------------------------------------------------

bool CompanionCraftingManager::giveTestResourceBag(CreatureObject* owner, CompanionObject* companion, const String& draftSchematicTemplate, String& errorMessage) const {
	if (owner == nullptr) {
		errorMessage = "Missing owner.";
		return false;
	}

	ZoneServer* zoneServer = owner->getZoneServer();
	Zone* zone = owner->getZone();

	if (zoneServer == nullptr || zone == nullptr) {
		errorMessage = "No zone server available.";
		return false;
	}

	ManagedReference<ResourceManager*> resourceManager = zoneServer->getResourceManager();

	if (resourceManager == nullptr) {
		errorMessage = "Resource manager unavailable.";
		return false;
	}

	String file = draftSchematicTemplate;

	if (file.indexOf("draft_schematic") == -1) {
		file = "object/draft_schematic/" + file;
	}

	if (file.indexOf(".iff") == -1) {
		file = file + ".iff";
	}

	ManagedReference<DraftSchematic*> draftSchematic = zoneServer->createObject(file.hashCode(), 0).castTo<DraftSchematic*>();

	if (draftSchematic == nullptr || !draftSchematic->isValidDraftSchematic()) {
		errorMessage = "Not a valid draft schematic: " + file;
		return false;
	}

	ManagedReference<SceneObject*> ownerInventory = owner->getSlottedObject("inventory");

	if (ownerInventory == nullptr) {
		errorMessage = "You have no inventory.";
		draftSchematic->destroyObjectFromDatabase(true);
		return false;
	}

	// Reuse the same container template CompanionControlDeviceImplementation
	// already creates elsewhere in this codebase (companion_loadout_backpack)
	// -- a proven-working tangible container, rather than guessing at a
	// generic backpack template that may or may not exist in this install's
	// data. Purely a comparison tool, so which bag model it renders as
	// doesn't matter.
	ManagedReference<SceneObject*> bag = zoneServer->createObject(String("object/tangible/inventory/companion_loadout_backpack.iff").hashCode(), 1).castTo<SceneObject*>();

	if (bag == nullptr) {
		errorMessage = "Couldn't create the test bag (missing container template).";
		draftSchematic->destroyObjectFromDatabase(true);
		return false;
	}

	Locker bagLocker(bag);

	// Companion System (2026-07-29 FIX, per Nick's raw-schematic-name bug
	// report): same root cause as CompanionCraftPickSuiCallback.h's craft-
	// list labels -- prefer the schematic's own real display name
	// (DraftSchematic::getCustomName(), backed by the schematic template's
	// literal customObjectName field) over the raw internal filename
	// fragment. draftSchematic is already loaded and in scope above.
	String niceName = draftSchematic->getCustomName();

	if (niceName.isEmpty()) {
		niceName = file.subString(file.lastIndexOf('/') + 1, file.lastIndexOf('.'));
	}

	bag->setCustomObjectName("Test Resources: " + niceName, false);

	if (!ownerInventory->transferObject(bag, -1, true)) {
		errorMessage = "Your inventory is full -- couldn't place the test bag.";
		bagLocker.release();
		bag->destroyObjectFromDatabase(true);
		draftSchematic->destroyObjectFromDatabase(true);
		return false;
	}

	ownerInventory->broadcastObject(bag, true);
	bagLocker.release();

	// Same slot/type introspection craftItem() uses -- create a throwaway
	// ManufactureSchematic purely to get a reliable getSlotCount() and per-
	// slot DraftSlot lookup; nothing is ever assembled from it.
	ManagedReference<ManufactureSchematic*> manufactureSchematic = draftSchematic->createManufactureSchematic(nullptr).castTo<ManufactureSchematic*>();

	if (manufactureSchematic == nullptr) {
		errorMessage = "Could not inspect schematic slots for " + file;
		draftSchematic->destroyObjectFromDatabase(true);
		return false;
	}

	Locker manuLocker(manufactureSchematic);
	manufactureSchematic->initializeSlotsForHeadlessCraft();

	int slotsFilled = 0;

	for (int i = 0; i < manufactureSchematic->getSlotCount(); ++i) {
		DraftSlot* draftSlot = draftSchematic->getDraftSlot(i);

		if (draftSlot == nullptr || draftSlot->getSlotType() != IngredientSlot::RESOURCESLOT) {
			continue; // components skipped -- resources only, per Nick's request
		}

		String resourceClass = draftSlot->getResourceType();

		if (resourceClass.isEmpty()) {
			continue;
		}

		// 2026-07-24 fix (live report: an already-BANKED resource beat this
		// tool's pick): best CURRENTLY-spawning candidate, schematic-weighted
		// on this exact slot index, falling back to generic best-of-type if
		// the schematic defines no weight for it. This alone is only "best
		// of what a fresh deed claim could get" -- not necessarily the true
		// best available, see the inventory comparison right after.
		ResourceSpawn* bestSpawningCandidate = resourceManager->getBestSpawnOfTypeWeighted(resourceClass, draftSchematic, i);

		if (bestSpawningCandidate == nullptr) {
			bestSpawningCandidate = resourceManager->getBestSpawnOfType(resourceClass, zone->getZoneName());
		}

		ResourceSpawn* winner = bestSpawningCandidate;
		float winnerScore = scoreResourceSpawn(draftSchematic, i, bestSpawningCandidate);

		// Also check whatever's ALREADY banked in owner/companion inventory --
		// same candidate scan + scoreResourceSpawn() comparison
		// fillResourceSlot() itself uses. A stack from an earlier,
		// since-rotated spawn can easily outscore anything spawning right
		// now, and the real companion craft would use it via tier 0 --
		// so the test bag needs to consider it too, or it misrepresents
		// what the companion would actually pick.
		SceneObject* inventoriesToScan[2] = { ownerInventory.get(), companion != nullptr ? companion->getSlottedObject("inventory").get() : nullptr };

		for (int c = 0; c < 2; ++c) {
			SceneObject* container = inventoriesToScan[c];

			if (container == nullptr) {
				continue;
			}

			for (int k = 0; k < container->getContainerObjectsSize(); ++k) {
				ManagedReference<SceneObject*> obj = container->getContainerObject(k);

				if (obj == nullptr || !obj->isResourceContainer()) {
					continue;
				}

				ResourceContainer* owned = cast<ResourceContainer*>(obj.get());

				if (owned == nullptr || owned->getSpawnObject() == nullptr || !owned->getSpawnObject()->isType(resourceClass)) {
					continue;
				}

				float ownedScore = scoreResourceSpawn(draftSchematic, i, owned->getSpawnObject());

				if (winner == nullptr || ownedScore > winnerScore) {
					winner = owned->getSpawnObject();
					winnerScore = ownedScore;
				}
			}
		}

		if (winner == nullptr) {
			owner->sendSystemMessage("No resource of class '" + resourceClass + "' has ever spawned on this server -- skipped that slot.");
			continue;
		}

		// Companion System (2026-07-27 FIX): same broken by-name lookup as
		// claimResourceDeedForClass() above: givePlayerResource(target, name,
		// qty) resolves "name" via ResourceManagerImplementation::
		// getResourceSpawn(), which lowercases the lookup key but every
		// resourceMap insertion keeps the spawn's natural capitalized name --
		// so the lookup NEVER matches, and nothing was ever actually being
		// deposited (explains "test bags aren't giving steel at all"). We
		// already hold the correct ResourceSpawn* directly (winner) from the
		// search above, so create/deposit straight from it -- and straight
		// into the test bag directly, skipping the old owner-inventory
		// round-trip and its fragile "scan for a matching object" step
		// entirely. slotsFilled now only increments on confirmed success.
		Locker spawnLocker(winner);
		ManagedReference<ResourceContainer*> newResource = winner->createResource(30000);

		if (newResource == nullptr) {
			spawnLocker.release();
			owner->sendSystemMessage("Couldn't claim " + winner->getName() + " for the test bag -- skipped that slot.");
			continue;
		}

		winner->extractResource("", 30000);
		spawnLocker.release();

		Locker newResLocker(newResource);

		if (!bag->transferObject(newResource, -1, true)) {
			newResource->destroyObjectFromDatabase(true);
			owner->sendSystemMessage("The test bag couldn't accept " + winner->getName() + " -- skipped that slot.");
			continue;
		}

		bag->broadcastObject(newResource, true);
		newResLocker.release();

		slotsFilled++;
	}

	manuLocker.release();
	manufactureSchematic->destroyObjectFromDatabase(true);
	draftSchematic->destroyObjectFromDatabase(true);

	if (slotsFilled == 0) {
		errorMessage = "No resource slots found for this item.";
		return false;
	}

	owner->sendSystemMessage("Filled a test bag with 30,000 units of your companion's best available resource for each material slot in " + niceName + " -- craft it yourself and compare.");

	return true;
}
