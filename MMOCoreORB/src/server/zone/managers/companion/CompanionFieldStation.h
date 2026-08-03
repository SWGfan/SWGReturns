/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-23, "Field Crafting Droid" pass) -- design doc:
	claude/design-field-crafting-droid-2026-07-23.md (Fable, project docs).
	Source of truth for the workflow is Nick's own verbatim request: "the
	droid engineer needs to step up and make a droid that has the requested
	crafting station module inside the droid, the armorsmith needs to
	request a droid, be given the droid and automatically spawns the droid
	so it has access to a crafting station."

	PROBLEM this solves: confirmed by direct code read (2026-07-23,
	CompanionCraftingManager.cpp) that the headless companion-craft path has
	ZERO crafting-station gating anywhere -- it silently ignores station
	requirements entirely today. That's not "already working," it's a
	missing feature: nothing stops a companion from field-crafting a
	station-gated item with nothing nearby, which doesn't match how real
	SWG crafting works and isn't what this system is going for. This file
	adds a deliberate, NEW pre-flight gate in front of
	CompanionCraftTheater::begin() -- callers that used to call
	CompanionCraftTheater::begin() directly for a possibly-station-gated
	schematic should call CompanionFieldStation::begin() instead; it either
	passes straight through (no station needed, or one's already available)
	or runs the full request/build/handoff/deploy theater first.

	HONEST SCOPE NOTE (2026-07-23, first build pass): two things in Fable's
	design are deliberately NOT implemented yet, rather than guessed at,
	because guessing engine internals we haven't verified risks a server
	crash for zero benefit:
	  - Leg 1c of the pre-flight check ("a nearby OWNER droid already
	    carrying a real stock crafting-station module satisfies it") --
	    c3rr confirmed this stock mechanic EXISTS but the exact lookup
	    function/class (DroidCraftingStationModuleDataComponent or similar)
	    was never independently verified against live source in either
	    Companion's or c3rr's sandbox. hasNearbyOwnerDroidStation() below is
	    a stub that always returns false with this comment attached --
	    flip it on once that lookup is confirmed (c3rr VERIFY LIST item #2).
	  - The droid's VISUAL appearance: c3rr's item #5 (a real droid
	    appearance template + spawn/despawn effect) came back genuinely
	    blocked -- nothing reachable answered it. Rather than guess an
	    unverified droid mobile template (which could carry AI/loot-table
	    baggage that crashes a headless spawn), this reuses the EXACT same
	    proven-safe static prop mechanism CompanionCraftTheater::
	    spawnFactoryProp() already uses in production
	    ("object/static/installation/mockup_factory_item_style_1.iff"),
	    just relabeled/re-narrated as a crafting droid. Swap
	    DROID_PROP_TEMPLATE for a real droid template the moment c3rr finds
	    one -- nothing else needs to change.
	  - requiresStation()'s threshold is a simple, editable path-pattern
	    list rather than reading DraftSchematic's real complexity-level
	    field -- nobody in this project (Companion, Fable, or c3rr) has
	    DraftSchematic.h staged to confirm the real accessor name, and
	    guessing a nonexistent method name breaks the whole build. Extend
	    STATION_GATED_PATTERNS below as more schematic families are
	    identified; swap for the real complexity-level check once
	    confirmed.

	Droid-engineer recognition (2026-07-23): this codebase already
	recognizes companion professions by learned-skill-string PREFIX
	elsewhere (CompanionMenuComponent.cpp: "outdoors_ranger_"/
	"outdoors_scout_" for scouting, "crafting_" for general crafting gates).
	Stock SWG's real skill tree names the droid engineer profession's boxes
	"crafting_droidengineer_*" -- following the SAME convention already
	proven in this codebase. This is a high-confidence assumption from
	stock SWG's real skill naming, not independently verified against this
	specific server's skills.iff -- quick thing to confirm live if a
	droid-engineer companion doesn't get recognized.

	Header-only, all-static (same shape as CompanionCraftTheater /
	CompanionFireworksShow). Reuses CompanionCraftTheater's proven
	patrol-converge-then-interact pattern for the request/handoff walk, and
	its onComplete(bool) completion callback to chain into the actual craft
	once setup is done.
*/

#ifndef COMPANIONFIELDSTATION_H_
#define COMPANIONFIELDSTATION_H_

#include <functional>

#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/objects/tangible/TangibleObject.h"
#include "server/zone/objects/draftschematic/DraftSchematic.h"
#include "server/zone/objects/creature/ai/PatrolPoint.h"
#include "server/zone/managers/companion/CompanionCraftTheater.h"
#include "server/zone/Zone.h"
#include "server/zone/ZoneServer.h"
#include "server/zone/managers/player/PlayerManager.h"
#include "server/zone/objects/tangible/tool/CraftingTool.h"

class CompanionFieldStationState : public Object {
public:
	uint64 crafterID = 0;
	uint64 deID = 0;
	String stationType;
	String schematicPath;
	int buildTicksLeft = 0;
	std::function<void(bool)> onComplete;
};

class CompanionFieldStation {
public:

	// See the file-header "HONEST SCOPE NOTE" -- placeholder prop until a
	// real droid appearance template is confirmed (c3rr VERIFY LIST #5).
	// A #define (not a function) because STRING_HASHCODE() below needs a
	// genuine compile-time constant string -- a function call, even one
	// that just returns a literal, isn't accepted as a template argument.
	#define DROID_PROP_TEMPLATE "object/static/installation/mockup_factory_item_style_1.iff"

	// FIX (2026-07-23, live bug: "the crafting droid request fell through:
	// the droid item couldn't be created" -- caught by Nick testing
	// in-game). The PORTABLE item that goes into an inventory needs a real
	// TangibleObject template -- the installation prop above isn't one, so
	// castTo<TangibleObject*>() on it always returned null. Reusing
	// object/tangible/inventory/companion_inventory.iff -- confirmed a
	// plain TangibleObject in this exact codebase (see
	// CompanionBagContainerComponent.h's own header comment), so this is a
	// known-safe choice rather than another guess. It'll open like a bag if
	// examined/opened, which is a cosmetic quirk, not a functional one --
	// swap for a real droid deed/item template later if one's confirmed.
	#define DROID_ITEM_TEMPLATE "object/tangible/inventory/companion_inventory.iff"

	// See file header -- high-confidence stock-convention assumption, not
	// yet independently verified against this server's skills.iff.
	static const char* DE_SKILL_PREFIX() {
		return "crafting_droidengineer_";
	}

	static const int BUILD_TICKS = 10;
	static const int BUILD_TICK_MS = 1000;

	// ---- Owned-droid tracking / manual call-out & store (2026-07-23, per
	// user request: "can we make them a datapad i can access with the
	// radial menu so the companion can call out their droid when they
	// need it, and store the droid after use") -----------------------------
	// The droid ITEM itself already lives in the companion's own inventory
	// (see crafterOwnsDroidItem() -- no separate "datapad" slot needed,
	// since the companion's existing inventory already does that job; the
	// radial menu below is the "access" Nick asked for). What's new here is
	// tracking whether that droid's visual PROP is currently deployed, so a
	// player can explicitly call it out or store it early instead of it
	// only appearing automatically mid-craft. In-memory only (same
	// precedent as CompanionCraftingManager::preferredExperimentalLines --
	// resets on server restart, which just means a deployed prop won't
	// survive a restart; the owned ITEM is unaffected either way since it's
	// a real persisted object). Function-local static is safe here because
	// this whole class is header-only/all-static -- an inline function's
	// local static is guaranteed shared across every translation unit that
	// includes this header (C++11 rule), same as a real static member
	// without needing a .cpp to define it in.
	static VectorMap<uint64, VectorMap<String, uint64> >& deployedProps() {
		static VectorMap<uint64, VectorMap<String, uint64> > map;
		return map;
	}

	/** 0 if no droid of this type is currently deployed beside `comp`,
	 * otherwise the deployed prop's object ID. */
	static uint64 getDeployedPropID(CompanionObject* comp, const String& stationType) {
		if (comp == nullptr) {
			return 0;
		}

		auto& outer = deployedProps();

		if (!outer.contains(comp->getObjectID())) {
			return 0;
		}

		VectorMap<String, uint64>& inner = outer.get(comp->getObjectID());

		if (!inner.contains(stationType)) {
			return 0;
		}

		return inner.get(stationType);
	}

	static void setDeployedPropID(CompanionObject* comp, const String& stationType, uint64 propID) {
		if (comp == nullptr) {
			return;
		}

		auto& outer = deployedProps();

		if (!outer.contains(comp->getObjectID())) {
			outer.put(comp->getObjectID(), VectorMap<String, uint64>());
		}

		VectorMap<String, uint64>& inner = outer.get(comp->getObjectID());
		inner.drop(stationType);
		inner.put(stationType, propID);
	}

	/** Every station type `comp` currently owns a droid item for (scans its
	 * own body + inventory for "Crafting Droid (<type>)" custom names --
	 * same naming convention containerHasDroidItem() checks against). Used
	 * to populate the new radial "Crafting Droids..." picker. */
	static void collectOwnedDroidTypes(CompanionObject* comp, Vector<String>& types) {
		if (comp == nullptr) {
			return;
		}

		SceneObject* containers[2] = { comp, comp->getSlottedObject("inventory").get() };

		for (int c = 0; c < 2; ++c) {
			SceneObject* container = containers[c];

			if (container == nullptr) {
				continue;
			}

			for (int i = 0; i < container->getContainerObjectsSize(); ++i) {
				ManagedReference<SceneObject*> obj = container->getContainerObject(i);

				if (obj == nullptr || !obj->isTangibleObject()) {
					continue;
				}

				String name = obj->getCustomObjectName().toString();
				static const String PREFIX = "Crafting Droid (";

				int idx = name.indexOf(PREFIX);

				if (idx == -1) {
					continue;
				}

				int close = name.indexOf(')', idx);

				if (close == -1) {
					continue;
				}

				String type = name.subString(idx + PREFIX.length(), close);

				if (!types.contains(type)) {
					types.add(type);
				}
			}
		}
	}

	/** Manual radial control: deploys the owned droid item's visual prop
	 * beside the companion ON DEMAND, outside of an active craft. Unlike
	 * deployDroidThenCraft()'s auto-timed prop (despawns automatically
	 * after ~20s), a manually deployed prop stays up until explicitly
	 * stored (storeDroidManually()) or the server restarts. */
	static bool deployDroidManually(CreatureObject* owner, CompanionObject* comp, const String& stationType) {
		if (owner == nullptr || comp == nullptr || comp->getZone() == nullptr) {
			return false;
		}

		if (!crafterOwnsDroidItem(comp, stationType)) {
			owner->sendSystemMessage(comp->getDisplayedName() + " doesn't have a " + stationType + " crafting droid to call out.");
			return false;
		}

		if (getDeployedPropID(comp, stationType) != 0) {
			owner->sendSystemMessage(comp->getDisplayedName() + "'s " + stationType + " crafting droid is already out.");
			return false;
		}

		ZoneServer* zoneServer = comp->getZoneServer();
		Zone* zone = comp->getZone();

		if (zoneServer == nullptr || zone == nullptr) {
			return false;
		}

		float angle = comp->getDirectionAngle() * (M_PI / 180.f);
		float px = comp->getPositionX() + sin(angle) * 2.5f;
		float py = comp->getPositionY() + cos(angle) * 2.5f;
		float pz = zone->getHeight(px, py);

		ManagedReference<SceneObject*> prop = zoneServer->createObject(STRING_HASHCODE(DROID_PROP_TEMPLATE), 0);

		if (prop == nullptr) {
			return false;
		}

		Locker propLocker(prop);
		prop->initializePosition(px, pz, py);
		zone->transferObject(prop, -1, true);

		setDeployedPropID(comp, stationType, prop->getObjectID());
		CompanionCraftTheater::say(comp, "There -- crafting droid's out.");
		return true;
	}

	/** Stores (despawns) a manually- or session-deployed droid prop early. */
	static bool storeDroidManually(CreatureObject* owner, CompanionObject* comp, const String& stationType) {
		if (owner == nullptr || comp == nullptr) {
			return false;
		}

		uint64 propID = getDeployedPropID(comp, stationType);

		if (propID == 0) {
			owner->sendSystemMessage(comp->getDisplayedName() + "'s " + stationType + " crafting droid isn't out right now.");
			return false;
		}

		ZoneServer* zoneServer = comp->getZoneServer();
		ManagedReference<SceneObject*> prop = zoneServer != nullptr ? zoneServer->getObject(propID) : nullptr;

		if (prop != nullptr) {
			Locker propLocker(prop);
			prop->destroyObjectFromWorld(true);
		}

		setDeployedPropID(comp, stationType, 0);
		CompanionCraftTheater::say(comp, "Droid's stored away.");
		return true;
	}

	/** Companion System (2026-07-27, generalized per Nick: "this is an issue
	 * with all the crafting professions" -- station-gating used to be a
	 * hand-maintained path-substring list covering ONLY armor. Now reads the
	 * REAL stock complexity/tool-tab fields straight off the DraftSchematic,
	 * confirmed via source read (SchematicList.cpp:130-137,
	 * DraftSchematic.idl:122,129):
	 *   complexity  1-15  General Crafting Tool (no station)
	 *   complexity 16-20  Specialized Crafting Tool (no station)
	 *   complexity 21-25  Specialized Tool + Public Crafting Station
	 *   complexity 26+    Specialized Tool + Private Crafting Station
	 * Companions don't model a held tool at all today, so only the station
	 * threshold (complexity >= 21) actually gates anything here -- this now
	 * applies uniformly to EVERY crafting profession (weaponsmith, chef,
	 * tailor, architect, droid engineer, armorsmith...), not just armor. */
	static float getSchematicComplexity(ZoneServer* zoneServer, const String& schematicPath) {
		if (zoneServer == nullptr) {
			return 0.f;
		}

		String file = schematicPath;

		if (file.indexOf(".iff") == -1) {
			file = file + ".iff";
		}

		ManagedReference<DraftSchematic*> schematic = zoneServer->createObject(file.hashCode(), 0).castTo<DraftSchematic*>();

		if (schematic == nullptr) {
			return 0.f;
		}

		return schematic->getComplexity();
	}

	static bool requiresStation(ZoneServer* zoneServer, const String& schematicPath) {
		return getSchematicComplexity(zoneServer, schematicPath) >= 21.0f;
	}

	/** Station TYPE this schematic needs, derived from the real stock
	 * getToolTab() bitmask (DraftSchematicObjectTemplate.h) instead of
	 * path-string guessing, mapped onto the same clothing/weapon/food/space/
	 * structure/generic taxonomy this file already used (station types
	 * share the tool-type enum: CLOTHING/FOOD/GENERIC/JEDI/SPACE/STRUCTURE/
	 * WEAPON, CraftingTool.idl:33-39). */
	static String stationTypeForSchematic(ZoneServer* zoneServer, const String& schematicPath) {
		if (zoneServer == nullptr) {
			return "generic";
		}

		String file = schematicPath;

		if (file.indexOf(".iff") == -1) {
			file = file + ".iff";
		}

		ManagedReference<DraftSchematic*> schematic = zoneServer->createObject(file.hashCode(), 0).castTo<DraftSchematic*>();

		if (schematic == nullptr) {
			return "generic";
		}

		uint32 toolTab = schematic->getToolTab();

		// Bitmask values confirmed from DraftSchematicObjectTemplate.h:
		// weapons=1, armor=2, food=4, clothing=8, vehicle=16, droid=32,
		// chemical=64, tissues=128, creatures=256, furniture=512,
		// installation=1024, lightsaber=2048, generic=4096, genetics=8192,
		// tailor=16384, armorsmith=32768, droidengineer=65536,
		// starshipcomponents=131072, shiptools=262144, misc=524288.
		if (toolTab & (2 | 8 | 16384 | 32768)) { // armor, clothing, tailor, armorsmith
			return "clothing";
		}

		if (toolTab & (1 | 2048)) { // weapons, lightsaber
			return "weapon";
		}

		if (toolTab & (4 | 64)) { // food, chemical
			return "food";
		}

		if (toolTab & (131072 | 262144)) { // starship components/tools
			return "space";
		}

		if (toolTab & (512 | 1024)) { // furniture, installation
			return "structure";
		}

		return "generic";
	}

	static String droidItemName(const String& stationType) {
		return "Crafting Droid (" + stationType + ")";
	}

	/** True if `container` (or one level into its own contents) holds a
	 * tangible custom-named as the crafting droid item for `stationType`.
	 * Same "custom-named tangible" precedent already used elsewhere in
	 * this codebase (the toolkit-naming convention referenced in the
	 * design doc) -- avoids depending on a dedicated droid-deed item
	 * template we haven't confirmed exists. */
	static bool containerHasDroidItem(SceneObject* container, const String& stationType) {
		if (container == nullptr) {
			return false;
		}

		String wantName = droidItemName(stationType);

		for (int i = 0; i < container->getContainerObjectsSize(); ++i) {
			ManagedReference<SceneObject*> obj = container->getContainerObject(i);

			if (obj == nullptr || !obj->isTangibleObject()) {
				continue;
			}

			if (obj->getCustomObjectName().toString().indexOf(wantName) != -1) {
				return true;
			}
		}

		return false;
	}

	/** Leg 1a: the crafter already owns a matching droid item (in its own
	 * inventory) from a previous request -- future crafts of this station
	 * type skip straight to a silent auto-deploy. */
	static bool crafterOwnsDroidItem(CompanionObject* crafter, const String& stationType) {
		if (crafter == nullptr) {
			return false;
		}

		return containerHasDroidItem(crafter, stationType) || containerHasDroidItem(crafter->getSlottedObject("inventory"), stationType);
	}

	/** Leg 1b: is there a real, placed crafting station of the matching
	 * type within normal crafting range? Companion System (2026-07-27,
	 * fix for a live bug: "no station nearby" reported while the crafter
	 * was standing right next to a real station INSIDE A PLAYER HOUSE).
	 * Root cause confirmed by direct source read: the previous heuristic
	 * used zone->getInRangeObjects(..., includeBuildingObjects=false) --
	 * the LAST argument explicitly excludes anything parented to a
	 * building/cell, so a station placed inside a house (the normal way
	 * real players place crafting stations) was silently invisible to it,
	 * regardless of physical proximity. The same "false" mistake was found
	 * copy-pasted into 3 other companion files this same pass (camp
	 * deployment's too-close-to-a-building check, the crafting manager's
	 * harvester lookup, and the loot-sweep scan) -- all fixed separately,
	 * same root cause.
	 * Now routes through the REAL stock accessor instead of a path-string
	 * guess: PlayerManagerImplementation::getNearbyCraftingStation()
	 * (confirmed via direct source read) walks the player's own tracked
	 * close-objects list -- which DOES include building interiors -- and
	 * explicitly permits a station parented to a cell object, matching
	 * real client behavior exactly (this is the same function
	 * RequestCraftingSessionCommand -- the real player /crafting command
	 * -- uses). PlayerManager isn't a singleton in this codebase; obtained
	 * via ZoneServer, same convention already used in begin() below. */
	static bool hasNearbyRealStation(CreatureObject* owner, const String& stationType) {
		if (owner == nullptr || owner->getZoneServer() == nullptr) {
			return false;
		}

		ManagedReference<PlayerManager*> playerManager = owner->getZoneServer()->getPlayerManager();

		if (playerManager == nullptr) {
			return false;
		}

		int type = CraftingTool::GENERIC;

		if (stationType == "clothing") {
			type = CraftingTool::CLOTHING;
		} else if (stationType == "weapon") {
			type = CraftingTool::WEAPON;
		} else if (stationType == "food") {
			type = CraftingTool::FOOD;
		} else if (stationType == "space") {
			type = CraftingTool::SPACE;
		} else if (stationType == "structure") {
			type = CraftingTool::STRUCTURE;
		}

		return playerManager->getNearbyCraftingStation(owner, type) != nullptr;
	}

	/** Leg 1c: NOT IMPLEMENTED this pass -- see file-header "HONEST SCOPE
	 * NOTE". Always false until the real stock droid-module lookup is
	 * confirmed against live source. Kept as its own named function (not
	 * inlined into begin()) so re-enabling it later is a one-line change. */
	static bool hasNearbyOwnerDroidStation(CreatureObject* owner, const String& stationType) {
		return false;
	}

	/** Finds a squad companion (not the crafter itself) whose learned
	 * skills include a droid-engineer box, not currently busy. */
	static CompanionObject* findDroidEngineer(CreatureObject* owner, CompanionObject* excluding) {
		if (owner == nullptr) {
			return nullptr;
		}

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

			CompanionObject* comp = device->getCompanionObject();

			if (comp == nullptr || comp == excluding || comp->getZone() == nullptr
					|| comp->getLinkedCreature().get() != owner) {
				continue;
			}

			if (comp->isInCombat() || comp->isTaxiActive() || comp->isLootSweepActive()) {
				continue;
			}

			for (int s = 0; s < comp->getLearnedSkillCount(); ++s) {
				if (comp->getLearnedSkill(s).beginsWith(DE_SKILL_PREFIX())) {
					return comp;
				}
			}
		}

		return nullptr;
	}

	/** Entry point -- callers that used to call CompanionCraftTheater::
	 * begin() directly for a possibly station-gated schematic should call
	 * this instead. Passes straight through untouched (same onComplete
	 * signature) when no station is needed or one's already available;
	 * otherwise runs the request/build/handoff/deploy theater first, then
	 * chains into the real craft. @pre nothing locked (mirrors
	 * CompanionCraftTheater::begin()'s own precondition). */
	static void begin(CreatureObject* owner, CompanionObject* crafter, const String& schematicPath, std::function<void(bool)> onComplete = std::function<void(bool)>()) {
		if (owner == nullptr || crafter == nullptr) {
			if (onComplete) {
				onComplete(false);
			}
			return;
		}

		ZoneServer* zoneServer = owner->getZoneServer();

		if (!requiresStation(zoneServer, schematicPath)) {
			CompanionCraftTheater::begin(owner, crafter, schematicPath, onComplete);
			return;
		}

		String stationType = stationTypeForSchematic(zoneServer, schematicPath);

		if (crafterOwnsDroidItem(crafter, stationType)) {
			deployDroidThenCraft(owner, crafter, stationType, schematicPath, onComplete);
			return;
		}

		if (hasNearbyRealStation(owner, stationType) || hasNearbyOwnerDroidStation(owner, stationType)) {
			// Real station (or, once implemented, a real stock droid
			// station) already covers it -- no droid item needed at all.
			CompanionCraftTheater::begin(owner, crafter, schematicPath, onComplete);
			return;
		}

		CompanionObject* de = findDroidEngineer(owner, crafter);

		if (de == nullptr) {
			owner->sendSystemMessage(crafter->getDisplayedName() + " can't finish that -- it needs a " + stationType + " crafting station. No station nearby, and no droid engineer in the squad to build a crafting droid.");

			if (onComplete) {
				onComplete(false);
			}
			return;
		}

		beginRequestFlow(owner, crafter, de, stationType, schematicPath, onComplete);
	}

	// ---- Request/build/handoff/deploy theater ----------------------------

	static void beginRequestFlow(CreatureObject* owner, CompanionObject* crafter, CompanionObject* de, const String& stationType, const String& schematicPath, std::function<void(bool)> onComplete) {
		CompanionCraftTheater::say(crafter, "Anyone got a crafting droid? I need a " + stationType + " station for this.");
		CompanionCraftTheater::say(de, "One crafting droid coming up.");

		Reference<CompanionFieldStationState*> state = new CompanionFieldStationState();
		state->crafterID = crafter->getObjectID();
		state->deID = de->getObjectID();
		state->stationType = stationType;
		state->schematicPath = schematicPath;
		state->buildTicksLeft = BUILD_TICKS;
		state->onComplete = onComplete;

		Reference<CreatureObject*> ownerRef = owner;
		ZoneServer* zoneServer = owner->getZoneServer();

		scheduleBuildTick(zoneServer, ownerRef, state);
	}

	static void scheduleBuildTick(ZoneServer* zoneServer, Reference<CreatureObject*> ownerRef, Reference<CompanionFieldStationState*> state) {
		Core::getTaskManager()->scheduleTask([zoneServer, ownerRef, state] () {
			runBuildTick(zoneServer, ownerRef, state);
		}, "CompanionFieldStationBuildTickLambda", BUILD_TICK_MS);
	}

	static void runBuildTick(ZoneServer* zoneServer, Reference<CreatureObject*> ownerRef, Reference<CompanionFieldStationState*> state) {
		CreatureObject* owner = ownerRef.get();

		if (owner == nullptr || state == nullptr || zoneServer == nullptr) {
			return;
		}

		ManagedReference<SceneObject*> deObj = zoneServer->getObject(state->deID);
		CompanionObject* de = deObj != nullptr ? deObj.castTo<CompanionObject*>().get() : nullptr;

		if (de == nullptr) {
			failFlow(owner, zoneServer, state, "the droid engineer isn't available anymore");
			return;
		}

		Locker delocker(de);

		if (de->getZone() == nullptr || de->isDead() || de->isInCombat() || owner->isInCombat()) {
			CompanionCraftTheater::resumeFollow(de, owner);
			failFlow(owner, zoneServer, state, "the build was interrupted");
			return;
		}

		if (--state->buildTicksLeft > 0) {
			de->doAnimation("manipulate_low");
			de->playEffect("clienteffect/pl_force_meditate_self.cef", "");
			scheduleBuildTick(zoneServer, ownerRef, state);
			return;
		}

		// Build finished -- hand the finished droid item to the DE's own
		// inventory, then walk it over to the crafter.
		ManagedReference<SceneObject*> deInventory = de->getSlottedObject("inventory");

		if (deInventory == nullptr) {
			failFlow(owner, zoneServer, state, "the droid engineer has nowhere to put the finished droid");
			return;
		}

		ManagedReference<TangibleObject*> droidItem = zoneServer->createObject(STRING_HASHCODE(DROID_ITEM_TEMPLATE), 0).castTo<TangibleObject*>();

		if (droidItem == nullptr) {
			failFlow(owner, zoneServer, state, "the droid item couldn't be created");
			return;
		}

		Locker itemLocker(droidItem, de);
		droidItem->setCustomObjectName(droidItemName(state->stationType), false);

		if (!deInventory->transferObject(droidItem, -1, true)) {
			itemLocker.release();
			droidItem->destroyObjectFromDatabase(true);
			failFlow(owner, zoneServer, state, "the droid engineer's inventory is full");
			return;
		}

		deInventory->broadcastObject(droidItem, true);
		itemLocker.release();

		CompanionCraftTheater::say(de, "Done -- one " + state->stationType + " crafting droid, fresh off the line.");

		scheduleHandoffConverge(zoneServer, ownerRef, state, 400);
	}

	static void scheduleHandoffConverge(ZoneServer* zoneServer, Reference<CreatureObject*> ownerRef, Reference<CompanionFieldStationState*> state, int delayMs) {
		Core::getTaskManager()->scheduleTask([zoneServer, ownerRef, state] () {
			runHandoffConverge(zoneServer, ownerRef, state);
		}, "CompanionFieldStationHandoffLambda", delayMs);
	}

	/** Walks the DE to the crafter and hands off the finished droid item
	 * once in range -- same patrol-converge pattern CompanionCraftTheater
	 * uses for resource donors. */
	static void runHandoffConverge(ZoneServer* zoneServer, Reference<CreatureObject*> ownerRef, Reference<CompanionFieldStationState*> state) {
		CreatureObject* owner = ownerRef.get();

		if (owner == nullptr || state == nullptr || zoneServer == nullptr) {
			return;
		}

		ManagedReference<SceneObject*> crafterObj = zoneServer->getObject(state->crafterID);
		ManagedReference<SceneObject*> deObj = zoneServer->getObject(state->deID);

		CompanionObject* crafter = crafterObj != nullptr ? crafterObj.castTo<CompanionObject*>().get() : nullptr;
		CompanionObject* de = deObj != nullptr ? deObj.castTo<CompanionObject*>().get() : nullptr;

		if (crafter == nullptr || de == nullptr) {
			failFlow(owner, zoneServer, state, "one of the companions went missing mid-handoff");
			return;
		}

		Locker clocker(crafter);
		Locker dlocker(de, crafter);

		if (de->getZone() == nullptr || de->isDead() || de->isInCombat() || crafter->isDead() || owner->isInCombat()) {
			CompanionCraftTheater::resumeFollow(de, owner);
			failFlow(owner, zoneServer, state, "the handoff was interrupted");
			return;
		}

		if (de->getDistanceTo(crafter) > 5.f) {
			de->setCompanionState(CompanionObject::PATROL);
			de->setFollowObject(nullptr);

			if (de->getPatrolPointSize() == 0) {
				PatrolPoint point(crafter->getPositionX(), crafter->getPositionZ(), crafter->getPositionY());
				de->addPatrolPoint(point);
				de->setMovementState(AiAgent::PATROLLING);
			}

			scheduleHandoffConverge(zoneServer, ownerRef, state, 400);
			return;
		}

		ManagedReference<SceneObject*> deInventory = de->getSlottedObject("inventory");
		ManagedReference<SceneObject*> crafterInventory = crafter->getSlottedObject("inventory");

		if (deInventory == nullptr || crafterInventory == nullptr) {
			failFlow(owner, zoneServer, state, "no inventory to hand the droid off with");
			return;
		}

		ManagedReference<TangibleObject*> droidItem = nullptr;
		String wantName = droidItemName(state->stationType);

		for (int i = 0; i < deInventory->getContainerObjectsSize(); ++i) {
			ManagedReference<SceneObject*> obj = deInventory->getContainerObject(i);

			if (obj == nullptr || !obj->isTangibleObject()) {
				continue;
			}

			if (obj->getCustomObjectName().toString().indexOf(wantName) != -1) {
				droidItem = obj.castTo<TangibleObject*>();
				break;
			}
		}

		if (droidItem == nullptr) {
			failFlow(owner, zoneServer, state, "the finished droid went missing before handoff");
			return;
		}

		Locker itemLocker(droidItem, de);

		if (!crafterInventory->transferObject(droidItem, -1, true)) {
			itemLocker.release();
			failFlow(owner, zoneServer, state, crafter->getDisplayedName() + "'s inventory is full");
			return;
		}

		crafterInventory->broadcastObject(droidItem, true);
		itemLocker.release();

		de->faceObject(crafter, true);
		crafter->faceObject(de, true);
		de->doAnimation("bow");
		crafter->doAnimation("kowtow");
		CompanionCraftTheater::say(crafter, "Thanks -- that's exactly what I needed!");

		CompanionCraftTheater::stepAwayThenFollow(de, crafter, owner, 5.f);

		deployDroidThenCraft(owner, crafter, state->stationType, state->schematicPath, state->onComplete);
	}

	/** Spawns the (placeholder -- see file header) droid prop ~2.5m beside
	 * the crafter, runs the real craft, then despawns the prop afterward.
	 * Reuses CompanionCraftTheater::spawnFactoryProp()'s exact proven
	 * spawn/auto-remove mechanism, just at a shorter distance and with its
	 * own narration. */
	static void deployDroidThenCraft(CreatureObject* owner, CompanionObject* crafter, const String& stationType, const String& schematicPath, std::function<void(bool)> onComplete) {
		ZoneServer* zoneServer = crafter->getZoneServer();
		Zone* zone = crafter->getZone();

		if (zoneServer == nullptr || zone == nullptr) {
			CompanionCraftTheater::begin(owner, crafter, schematicPath, onComplete);
			return;
		}

		CompanionCraftTheater::say(crafter, "There -- a proper " + stationType + " workshop.");

		float angle = crafter->getDirectionAngle() * (M_PI / 180.f);
		float px = crafter->getPositionX() + sin(angle) * 2.5f;
		float py = crafter->getPositionY() + cos(angle) * 2.5f;
		float pz = zone->getHeight(px, py);

		ManagedReference<SceneObject*> prop = zoneServer->createObject(STRING_HASHCODE(DROID_PROP_TEMPLATE), 0);

		if (prop != nullptr) {
			Locker propLocker(prop);
			prop->initializePosition(px, pz, py);
			zone->transferObject(prop, -1, true);

			ManagedReference<SceneObject*> propRef = prop;

			// Lingers for the craft session + a little slack (chained
			// crafts of the same type will spawn their own prop again next
			// time -- keeping this simple rather than tracking reuse
			// across calls).
			Core::getTaskManager()->scheduleTask([propRef] () {
				SceneObject* p = propRef.get();

				if (p == nullptr) {
					return;
				}

				Locker locker(p);
				p->destroyObjectFromWorld(true);
			}, "CompanionFieldStationPropRemoveLambda", 20000);
		}

		CompanionCraftTheater::begin(owner, crafter, schematicPath, onComplete);
	}

	static void failFlow(CreatureObject* owner, ZoneServer* zoneServer, Reference<CompanionFieldStationState*> state, const String& reason) {
		if (owner != nullptr) {
			owner->sendSystemMessage("The crafting droid request fell through: " + reason + ".");
		}

		ManagedReference<SceneObject*> deObj = zoneServer != nullptr ? zoneServer->getObject(state->deID) : nullptr;
		CompanionObject* de = deObj != nullptr ? deObj.castTo<CompanionObject*>().get() : nullptr;

		if (de != nullptr && owner != nullptr) {
			Locker delocker(de);
			CompanionCraftTheater::resumeFollow(de, owner);
		}

		if (state->onComplete) {
			state->onComplete(false);
		}
	}

};

#endif // COMPANIONFIELDSTATION_H_
