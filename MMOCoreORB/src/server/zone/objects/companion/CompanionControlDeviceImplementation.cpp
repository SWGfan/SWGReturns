/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System -- see CompanionControlDevice.idl and NOTES.md.
*/

#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionThreatObserver.h"
#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/objects/cell/CellObject.h"
#include "server/zone/objects/building/BuildingObject.h"
#include "server/zone/objects/tangible/weapon/WeaponObject.h"
#include "server/zone/objects/scene/variables/ContainerPermissions.h"
#include "server/zone/objects/group/GroupObject.h"
#include "server/zone/managers/group/GroupManager.h"
#include "server/zone/Zone.h"
#include "server/zone/ZoneServer.h"
#include "server/zone/managers/radial/RadialOptions.h"
#include "templates/params/ObserverEventType.h"
#include "templates/params/creature/CreaturePosture.h"
#include "server/zone/managers/companion/CompanionSkillTrainer.h"
#include "server/zone/managers/combat/CombatManager.h"
#include "server/zone/packets/scene/AttributeListMessage.h"

// Companion System (2026-07-19 rewrite, per user request) -- death is no
// longer a permanent max-HAM penalty (the old spec 2C Resilience-branch
// scheme below was dead code anyway -- handleCompanionDeath() was never
// actually called from anywhere until this pass wired it up via
// CompanionObjectImplementation::notifyObjectDestructionObservers()). A
// companion always comes back at full max HAM (100) on its next summon;
// what it comes back WITH is a minimal current HAM fraction that has to
// regen back up naturally (AiAgentImplementation::doRecovery() ->
// activateHAMRegeneration() already ticks this for every AiAgent, companions
// included -- no new regen mechanism needed), matching "spawn in alive
// again with minimal health that regenerates."
#define COMPANION_REVIVE_HAM_FRACTION 0.10f

void CompanionControlDeviceImplementation::setVitality(int value) {
	if (value < 0) {
		value = 0;
	}

	if (value > maxVitality) {
		value = maxVitality;
	}

	vitality = value;

	// Persistence fix (2026-07-13, "companion state has no structural save
	// guarantee" -- see NOTES.md): same gap as CompanionObjectImplementation's
	// native setters -- this is the persisted-copy mirror healVitality()
	// writes to specifically so a store/restore cycle doesn't lose a heal,
	// but it never marked itself dirty, so that safety net could itself be
	// silently lost before the next save sweep/shutdown. Uses
	// zoneServer->updateObjectToDatabase(), not updateToDatabase() (confirmed
	// vestigial/empty-bodied in every class in this codebase).
	ZoneServer* zoneServer = getZoneServer();

	if (zoneServer != nullptr) {
		zoneServer->updateObjectToDatabase(_this.getReferenceUnsafeStaticCast());
	}
}

// Companion System (2026-07-15, "invisible datapad device" fix -- see the
// idl doc comment and companion_control_device.lua's revert comment).
void CompanionControlDeviceImplementation::initializeTransientMembers() {
	IntangibleObjectImplementation::initializeTransientMembers();

	setClientObjectCRC(STRING_HASHCODE("object/intangible/pet/shared_pet_control.iff"));
}

int CompanionControlDeviceImplementation::handleObjectMenuSelect(CreatureObject* player, byte selectedID) {
	if (player == nullptr) {
		return 1;
	}

	if (!isASubChildOf(player)) {
		return 1;
	}

	// Companion System: the client does NOT send RadialOptions::ITEM_ACTIVATE
	// for a "Call"/"Destroy" style datapad item selection -- confirmed by
	// diffing against the real PetControlDeviceImplementation, which checks
	// literal selectedID 44 (RadialOptions::PET_CALL) and 59
	// (RadialOptions::PET_STORE), never ITEM_ACTIVATE. The client's radial
	// menu for this item's game object type sends those same IDs regardless
	// of which C++ class the object actually dispatches to server-side, so
	// this device must check for them too, or every selection silently
	// no-ops (root-caused this session -- see docs/companion_system/NOTES.md).
	if (selectedID != RadialOptions::PET_CALL && selectedID != RadialOptions::PET_STORE) {
		return 0;
	}

	ManagedReference<CompanionObject*> companion = companionObject.get();

	if (companion == nullptr) {
		// Not summoned yet (or was never recruited) -- companion recruitment
		// itself (turning a companion recruitment token into a populated
		// CompanionControlDevice) is a separate acquisition flow outside the
		// scope of this deliverable; see NOTES.md.
		player->sendSystemMessage("@companion:no_companion_loaded"); // This datapad has no companion loaded.
		return 0;
	}

	// Companion System (2026-07-28 FIX, per live report "you cannot summon
	// or store your companion right now" on a companion sitting dead at
	// 0 vitality): companion->isInCombat() is the stock combat-state peace
	// timer, which normally decays on its own -- but a companion that DIES
	// mid-fight is despawned before that timer ever gets a chance to clear,
	// so it can stay stuck combat-flagged forever afterward even though it
	// is plainly dead, not fighting. A dead companion can never itself be
	// "in combat" in any sense that should block calling it back --
	// spawnObject()'s existing `if (isDead) { reviveCompanion(); ... }`
	// path is exactly what handles this, but was never reached because
	// this guard returned first. Only exempts the DEAD-companion case;
	// a genuinely alive, actively-fighting companion still blocks here,
	// same as before, as does the owner's own combat/death state.
	if ((companion->isInCombat() && !isDead) || player->isInCombat() || player->isDead()) {
		player->sendSystemMessage("@companion:cant_summon_now"); // You cannot summon or store your companion right now.
		return 0;
	}

	Reference<CompanionControlDevice*> thisReference = _this.getReferenceUnsafeStaticCast();
	Reference<CreatureObject*> playerReference = player;

	Core::getTaskManager()->executeTask([thisReference, playerReference] () {
		Locker locker(playerReference);
		Locker controlLocker(thisReference, playerReference);

		ManagedReference<CompanionObject*> comp = thisReference->getCompanionObject();

		if (comp == nullptr) {
			return;
		}

		// Companion System bug fix: spawnObject() asserts the companion
		// itself is locked by the current thread (it mutates position,
		// creature link, container/menu components, AI template, etc.) --
		// this lambda previously only locked the player and the control
		// device, never the companion, so every real summon attempt hit
		// `assert(companion->isLockedByCurrentThread())` and crashed the
		// server. Root-caused via the first real in-game test of this path
		// (SIGABRT at spawnObject(), CompanionControlDeviceImplementation.cpp:123).
		// Locked here for both branches since storeObject() also mutates
		// the companion (setFollowObject/destroyObjectFromWorld) even
		// though it has no explicit assert.
		Locker companionLocker(comp, playerReference);

		if (comp->getZone() == nullptr) {
			thisReference->spawnObject(playerReference);
		} else {
			thisReference->storeObject(playerReference, false);
		}
	}, "CompanionControlDeviceActivateLambda");

	return 0;
}

void CompanionControlDeviceImplementation::spawnObject(CreatureObject* player) {
	if (player == nullptr) {
		return;
	}

	if (!isASubChildOf(player)) {
		return;
	}

	// Companion System (2026-07-19 rewrite, per user request: "the next
	// time the user spawns it in to the world, it has 100 health 100
	// action 100 mind so it doesn't stay dead ... spawn in alive again
	// with minimal health that regenerates") -- was a hard block here
	// ("@companion:dead_summon_error", return). Auto-revives instead: full
	// max HAM restored, current HAM set to a small fraction so it comes
	// back weak and has to regen up like a real player, then falls through
	// into the normal spawn flow below exactly as if it had never died.
	if (isDead) {
		reviveCompanion();

		if (player != nullptr) {
			player->sendSystemMessage("Your companion stirs and returns to your side, still weak from its wounds.");
		}
	}

	ManagedReference<CompanionObject*> companion = companionObject.get();

	if (companion == nullptr) {
		return;
	}

	assert(companion->isLockedByCurrentThread());

	// Companion System (2026-07-20, per user request "BEFORE the companion
	// spawns, ask what profession they want"): the one-time starter
	// profession picker now runs PRE-spawn -- the companion stays in the
	// datapad until the owner picks, and the picker's callback (which
	// grants the profession + the real per-profession starting loadout via
	// grantStartingGearTo(), so an artisan arrives holding its tool) calls
	// this spawnObject() again itself once firstLaunchComplete is set.
	// Cancelling the picker just leaves the companion stored; the next
	// summon attempt re-asks. Baseline stat migration stays idempotent and
	// safe on an unspawned companion.
	if (!companion->hasCompletedFirstLaunch()) {
		companion->migrateBaselineStats();
		CompanionSkillTrainer::instance()->sendStarterProfessionChoice(player, companion);
		return;
	}

	// Badge-gated / skill-rank-gated summon check (spec 2A + 3A): block
	// summoning if the companion's tier requirement exceeds the owner's
	// current companion_master rank. This is checked against the isolated
	// companion_master skill tree only -- never against Creature Handler
	// skills.
	if (!player->hasSkill(requiredMasterSkillBox)) {
		player->sendSystemMessage("@companion:insufficient_rank"); // Your Companion Master rank is not high enough to summon this companion.
		return;
	}

	Zone* zone = player->getZone();

	if (zone == nullptr) {
		return;
	}

	// Companion System (2026-07-17, "pet command port" pass) -- top up the
	// baseline order abilities on EVERY summon, not just the one-time
	// first-launch flow: players who completed first-launch before a new
	// order command existed (e.g. companion_formup, companion_guard) would
	// otherwise never receive the new abilities on their existing
	// characters. grantBaselineOwnerOrderAbilities() is hasAbility()-guarded
	// per entry, so this is a cheap no-op for anyone already current.
	CompanionSkillTrainer::instance()->grantBaselineOwnerOrderAbilities(player);

	// COMPANION_SKILLMOD_RESYNC_HOTFIX_2026_07_31 -- self-heal any companion skill mods that were
	// granted before patch_companion_skillmod_grant_2026-07-31 existed
	// (see CompanionSkillTrainer::resyncSkillMods()'s own doc comment
	// for the full rationale). Idempotent -- cheap no-op once a
	// companion is fully corrected.
	CompanionSkillTrainer::instance()->resyncSkillMods(companion);

	companion->initializePosition(player->getPositionX() + System::random(5) - 2, player->getPositionZ(), player->getPositionY() + System::random(5) - 2);

	companion->setCreatureLink(player);
	companion->setCompanionControlDevice(_this.getReferenceUnsafeStaticCast());
	companion->setFaction(player->getFaction());
	companion->setObjectMenuComponent("CompanionMenuComponent");
	companion->setContainerComponent("CompanionContainerComponent");

	// Companion System -- Auto-Equip (2026-07-12): the companion's own
	// SceneObject template (object/mobile/companion_actor.lua, inheriting
	// the shared NPC "dressed" appearance template) leaves
	// containerVolumeLimit at its default of 0, since ordinary NPCs are
	// never meant to hold loose bag items. The companion has no separate
	// "inventory" bag child object the way a player does (see
	// docs/companion_system/NOTES.md, File map) -- its own containerObjects
	// list *is* its inventory, already relied on by
	// CampDeploymentManager::deployCamp()'s camp-tent scan -- so it needs a
	// real, non-zero volume limit or every single item transfer into it
	// would fail with CONTAINERFULL (0 >= 0) before CompanionContainerComponent
	// ::canAddObject()'s auto-equip bypass is ever reached. 80 matches the
	// real player-character starting inventory bag's containerVolumeLimit
	// (object/tangible/inventory/objects.lua, base_inventory template) --
	// not an arbitrary number. Set unconditionally on every summon so it
	// self-heals for companions created/persisted before this change.
	companion->setContainerVolumeLimit(80);

	// Companion System (2026-07-29, "unarmed companion lands no hits" fix)
	// -- root cause: a companion's real combat weapon is whatever
	// CreatureObjectImplementation::getWeapon() returns (genesis port: the newer
	// fork's AiAgent current-weapon accessor does not exist here -- genesis's
	// getWeapon() itself falls back to the "default_weapon" slot,
	// CreatureObjectImplementation.cpp:3411). The only
	// place that ever gets set for an unarmed companion is
	// refreshCombatAttacks()'s getSlottedObject("default_weapon") fallback
	// -- but nothing ever creates an object in that slot for a companion,
	// because the engine's only mechanism for that (AiAgentImplementation::
	// createDefaultWeapon()) requires a real npcTemplate, which companions
	// never have (raw createObject() spawn path, not the CreatureTemplate
	// mob-spawn path). Net effect: an unarmed companion's getWeapon() is a
	// hard nullptr, which makes BOTH CombatQueueCommand::doCombatAction()
	// (attacker side, GENERALERROR before reaching CombatManager) and
	// CombatManager::doTargetCombatAction()'s
	// `if (defender->getWeapon() == nullptr) return -1;` (defender side)
	// silently bail out with zero damage and no combat spam -- looks like
	// combat is frozen with no hits landing on either side.
	// Fix: self-heal the slot exactly like the other unconditional
	// per-summon fixes above -- create the same innate unarmed weapon
	// object real humanoid NPCs get (the exact CRC
	// AiAgentImplementation::createDefaultWeapon() uses for isNpc() agents)
	// and slot it into containment 4 (matching createDefaultWeapon()'s own
	// transferObject(defaultWeap, 4) call) if nothing is there yet. Only
	// call refreshCombatAttacks() to pick it up as the active weapon if the
	// companion has no real weapon equipped right now (getWeapon()
	// == nullptr) -- never touches a companion that already has a real
	// weapon in hand.
	if (companion->getSlottedObject("default_weapon") == nullptr) {
		ZoneServer* defaultWeaponZoneServer = player->getZoneServer();

		if (defaultWeaponZoneServer != nullptr) {
			ManagedReference<SceneObject*> innateWeapon = defaultWeaponZoneServer->createObject(STRING_HASHCODE("object/weapon/melee/unarmed/unarmed_default.iff"), companion->isPersistent());

			if (innateWeapon != nullptr && innateWeapon->isWeaponObject()) {
				Locker innateWeaponLocker(innateWeapon, companion);

				if (!companion->transferObject(innateWeapon, 4)) {
					innateWeapon->destroyObjectFromDatabase(true);
				}
			}
		}
	}

	// genesis port: was companion->getCurrentWeapon() -- genesis has no
	// AiAgent current-weapon accessor; CreatureObject::getWeapon() (CreatureObject.idl:1719,
	// impl CreatureObjectImplementation.cpp:3411) is the equivalent and already falls
	// back to the "default_weapon" slot when no weapon is held.
	if (companion->getWeapon() == nullptr) {
		companion->refreshCombatAttacks(nullptr);
	}

	// Companion System (2026-07-14, "companion walks too slowly to keep up
	// with the owner" fix): the companion runs the generic wild-mobile
	// behavior trees (ai/default.lua -- see the homeLocation fix note
	// below), whose calm/FOLLOWING movement uses walkSpeed; only the
	// dedicated PET trees (ai/pet.lua, selected via CreatureFlag::PET in
	// ai/templates.lua's bitmaskLookup) write moveMode=RUN while following.
	// Deliberately NOT switching the companion onto the pet trees: they
	// lean on PetControlDevice-cast leaf nodes (CheckPetCommand/PetReturn),
	// the exact isPet()/PetControlDevice-cast bug family this project has
	// already hit twice (see CODEBASE_GUIDE/NOTES.md). Surgical fix
	// instead: make the companion's walking pace equal its running pace so
	// every movement state keeps up with the owner. Values come from the
	// shared appearance template's speed array at loadTemplateData() time
	// (trainer NPC shell -- see companion_actor.lua); if that ever ships a
	// zero/unset run speed, fall back to the real player-character run
	// speed (5.376). Set unconditionally every summon: cheap, idempotent,
	// self-heals existing companions.
	float companionRunSpeed = companion->getRunSpeed();

	if (companionRunSpeed <= 0.1f) {
		companionRunSpeed = 5.376f;
		companion->setRunSpeed(companionRunSpeed, true);
	}

	// genesis port: dropped companion->setWalkSpeed(companionRunSpeed, true) -- genesis's
	// CreatureObject.idl exposes walkSpeed READ-ONLY (field :100, getWalkSpeed() :1676);
	// setRunSpeed() (:468) is the only speed setter there is, and it cannot express
	// "walk as fast as you run". DEFERRED: the companion's walking pace stays at whatever
	// the appearance template shipped, so it can still lag while the default behavior tree
	// is in WALK moveMode; the run-speed self-heal just above and
	// CompanionObjectImplementation's keep-up boost tick (which raises setRunSpeed when the
	// companion falls >25m behind) remain in force.

	// Companion System (2026-07-15, "equipped gear never renders on the
	// companion" fix -- see companion_actor.lua's rebase comment and
	// NOTES.md): clientObjectCRC is PERSISTED per object (set only at
	// creation in loadTemplateData(); never refreshed on ODB load -- the
	// exact same trap the loadout backpack's visibility fix hit), so a
	// companion created under the old trainer "dressed_*" client template
	// would keep its canned non-composable mesh forever and never render
	// worn gear. Re-stamp the player human-male client template CRC every
	// summon: cheap, idempotent, and the summon's own fresh zone insert
	// sends the client a brand-new create with the corrected CRC.
	companion->setClientObjectCRC(STRING_HASHCODE("object/creature/player/shared_human_male.iff"));

	// Companion System (2026-07-13, "item vanishes when taken back out of
	// companion inventory" fix -- see NOTES.md): gives the companion a real
	// separate "inventory" child bag object. Loose (non-equipped) items now
	// get moved into this child bag (see CompanionContainerComponent.cpp's
	// attemptAutoEquip()) instead of sitting directly in the companion's
	// own inherited-SLOTTED top-level container the way they did before --
	// equipped gear (containmentType >= 4) is unaffected and still lives
	// directly on the companion, exactly matching how a real player keeps
	// worn gear on their own CreatureObject and only loose stuff in a
	// separate "inventory" bag. Idempotent (checked via getSlottedObject) so
	// it self-heals for companions created/persisted before this change
	// without recreating -- and orphaning the contents of -- an existing
	// bag on every re-summon.
	//
	// Bug fix (2026-07-14, "real root cause of the item-loss/'You can not
	// loot that' bug" -- see NOTES.md): this used to create the bag from
	// object/tangible/inventory/creature_inventory.iff, the exact same
	// template + setup CreatureManagerImplementation::respawnCreature() uses
	// for a real loot-bearing NPC's corpse-loot bag -- but that template
	// hardcodes containerComponent = "LootContainerComponent", which is
	// built for looting a DEAD creature's corpse, not a live companion's
	// everyday storage: its canAddObject() unconditionally rejects every
	// insert with "You cannot place items into a corpse." (confirmed live --
	// items relocated here by attemptAutoEquip() were silently failing to
	// actually land), and its checkContainerPermission()'s MOVEOUT branch
	// only allows access if the container's ContainerPermissions ownerID
	// field matches the requesting player -- a field nothing here ever set,
	// so it was permanently denied for everyone (the real cause of the
	// client's canned "You can not loot that." message on any attempt to
	// take an item back out). An earlier pass's comment here theorized that
	// leaving containerInheritPermissionsFromParent at its default (true)
	// would make checkContainerPermission() "fall through" to
	// CompanionContainerComponent's owner-only logic on the companion
	// itself -- that was never actually how it works (the component that
	// runs is fixed by the object's OWN containerComponent field, not
	// something inheritPermissionsFromParent redirects), so the fix instead
	// creates the bag from a new dedicated template,
	// object/tangible/inventory/companion_inventory.iff (companion_
	// inventory.lua), which reuses the identical real client-side
	// appearance (no new TRE content needed) but sets
	// containerComponent = "CompanionBagContainerComponent" (server/zone/
	// objects/companion/components/CompanionBagContainerComponent.h/.cpp),
	// whose checkContainerPermission() grants MOVEIN/MOVEOUT/OPEN via
	// CompanionObject::isAuthorizedActor(creature) (a real ownership check
	// that's actually populated) and whose canAddObject() is the plain,
	// unmodified ContainerComponent::canAddObject() -- an ordinary VOLUME-
	// container check, no CreatureObject-destination requirement.
	// Bug fix (2026-07-14, take 2 -- live server log showed
	// "errorNumber: 2" / PLAYERUSEMASKERROR on every insert attempt): this
	// bag originally got CompanionContainerComponent here (same component
	// the companion itself uses, below), on the mistaken assumption that
	// was generically safe for any companion-owned container. It isn't --
	// CompanionContainerComponent extends PlayerContainerComponent, and its
	// canAddObject() override only special-cases sceneObject-
	// >isCompanionObject() (true for the companion itself, false for this
	// bag); for this bag it fell through to the inherited
	// PlayerContainerComponent::canAddObject(), which unconditionally
	// requires dynamic_cast<CreatureObject*>(sceneObject) to succeed on the
	// destination -- always false for a TangibleObject bag, so every insert
	// was silently rejected. Confirmed via character_inventory.lua that a
	// real player's own inventory bag has no containerComponent override at
	// all (plain ContainerComponent) -- CompanionBagContainerComponent
	// mirrors that, plus the ownership check this bag still needs (a real
	// player's own bag doesn't, since it's never shared). The
	// setContainerDenyPermission() calls this block used to make (aimed at
	// LootContainerComponent's ContainerPermissions-based gating) are
	// removed -- they're meaningless against CompanionBagContainerComponent,
	// which never reads that object at all. Bag creation's own safety
	// checks (`hasSlotDescriptor("inventory")` guard, `transferObject()`
	// return-value logging) are unchanged from the prior pass, modeled on
	// CreatureManagerImplementation::respawnCreature()'s real-NPC
	// precedent.
	if (companion->hasSlotDescriptor("inventory") && companion->getSlottedObject("inventory") == nullptr) {
		ZoneServer* zoneServer = player->getZoneServer();

		if (zoneServer != nullptr) {
			Reference<SceneObject*> companionInventory = zoneServer->createObject(STRING_HASHCODE("object/tangible/inventory/companion_inventory.iff"), 1);

			if (companionInventory == nullptr) {
				error("CompanionSystem: could not create companion inventory bag object");
			} else {
				Locker invLocker(companionInventory, companion);

				if (!companion->transferObject(companionInventory, 4, true)) {
					error("CompanionSystem: could not attach companion inventory bag to companion "
							+ String::valueOf(companion->getObjectID()) + " (transferObject to slot 4 failed)");
				}
			}
		}
	} else {
		// Companion System (2026-07-14, migration for companions summoned
		// before the LootContainerComponent -> CompanionBagContainerComponent
		// fix above): a first attempt at this migrated an existing bag by
		// creating a brand new companion_inventory.iff bag, moving items
		// over, destroying the old bag, then re-attaching the new one to
		// slot 4 -- but that re-attach reliably failed
		// ("transferObject to slot 4 failed") because
		// ContainerComponent::removeObject()'s own slot-clearing logic
		// doesn't unconditionally guarantee the "inventory" key is dropped
		// from the companion's slottedObjects map before the new bag tries
		// to claim the same slot, and destroyObjectFromWorld()'s removal
		// path doesn't surface a clear signal either way when that happens
		// (see NOTES.md, "Live bug report: bag migration failing" for the
		// research-only chat's trace of this). Rather than fight that
		// remove/re-attach ordering at all, this migrates in place instead:
		// SceneObject::setContainerComponent(String) (@local, native -- the
		// exact same method spawnObject() already calls unconditionally on
		// the companion itself, every summon, with no ill effects) lets an
		// object's container-permission/canAddObject behavior be reassigned
		// live, with no slot/parent/contents changes of any kind -- the bag
		// keeps its same object ID, same slot 4 attachment, same contents,
		// only which C++ component class governs it changes. Container
		// components are stateless, shared-by-name singletons
		// (ComponentManager.cpp puts exactly one instance of each component
		// class in its registry, reused by every object that names it), so
		// there's nothing per-object to lose by switching. Detected by
		// the bag's server template CRC still matching the old
		// creature_inventory.iff template (setContainerComponent() never
		// changes this field, so the check still fires correctly even for
		// bags already migrated once); idempotent (checked every summon,
		// harmlessly re-applying the same value once the live component is
		// already correct -- exactly what the companion's own line above
		// already does with no issue).
		// Bug fix (2026-07-14, take 2): this line originally reassigned
		// "CompanionContainerComponent" -- the same bug as the fresh-bag
		// creation branch above (PLAYERUSEMASKERROR on every insert, since
		// CompanionContainerComponent's canAddObject() only special-cases
		// the companion object itself, not a nested bag). Reassigns
		// "CompanionBagContainerComponent" now instead; this self-heal will
		// pick up any companion whose bag is still on the old, broken
		// component (whichever one it happened to be) on its very next
		// summon, no server restart or manual DB fix needed.
		SceneObject* existingBag = companion->getSlottedObject("inventory");

		if (existingBag != nullptr && existingBag->getServerObjectCRC() == STRING_HASHCODE("object/tangible/inventory/creature_inventory.iff")) {
			Locker bagLocker(existingBag, companion);

			existingBag->setContainerComponent("CompanionBagContainerComponent");
		}
	}

	// Companion System (2026-07-20, "starting tools spawned into the
	// equipment window, need them in the inventory" -- see NOTES.md): the
	// pre-spawn loadout grant (CompanionStarterProfessionSuiCallback ->
	// grantStartingGearTo) lands loose items in the companion's TOP-LEVEL
	// container, because the bag child above doesn't exist until this
	// first summon creates it. Sweep every loose tangible into the bag
	// now -- runs every summon (cheap, idempotent), so it also self-heals
	// any older companion still carrying loose top-level items.
	{
		SceneObject* relocationBag = companion->getSlottedObject("inventory");

		if (relocationBag != nullptr) {
			Vector<ManagedReference<SceneObject*> > looseItems;

			for (int i = 0; i < companion->getContainerObjectsSize(); ++i) {
				ManagedReference<SceneObject*> obj = companion->getContainerObject(i);

				if (obj != nullptr && obj != relocationBag && obj->isTangibleObject()) {
					looseItems.add(obj);
				}
			}

			for (int i = 0; i < looseItems.size(); ++i) {
				ManagedReference<SceneObject*> item = looseItems.get(i);

				Locker itemLocker(item, companion);
				relocationBag->transferObject(item, -1, true);
			}
		}
	}

	// Companion System (2026-07-14, "@group:no_loot_permission blocks every
	// removal" fix -- see NOTES.md): the player's own inventory bag is a
	// tangible Container object, and ContainerImplementation::canAddObject()
	// (~line 280-295) carries a corpse-loot protection: any incoming item
	// whose CURRENT parent is an AiAgent is rejected with
	// "@group:no_loot_permission" unless that AiAgent's "inventory" bag's
	// ContainerPermissions ownerID equals the receiving player. The loot
	// system sets that owner on a dead NPC's bag when loot rights are
	// assigned -- but nothing ever set it on a live companion's bag, so
	// EVERY transfer of an equipped/held item off the companion into the
	// player's inventory (Retrieve Gear, per-item "Pick Up",
	// unequipItemToInventory(), the loadout backpack's displacement swap)
	// was rejected at the destination's precheck, regardless of all the
	// source-side container/permission fixes that came before. Confirmed
	// live via the RetrieveGear diagnostic logging: "canAddObject rejected
	// <item>: @group:no_loot_permission" for every item. Fix: mark the owner
	// as the bag's loot-permission owner, same field GroupManager::
	// transferLoot() sets via getContainerPermissionsForUpdate()->setOwner().
	// Applied unconditionally on every summon: cheap, idempotent, and
	// self-heals every existing companion -- and also keeps the owner
	// CORRECT if the companion is ever traded/re-linked to a different
	// player.
	{
		SceneObject* companionBag = companion->getSlottedObject("inventory");

		if (companionBag != nullptr) {
			Locker bagPermLocker(companionBag, companion);

			ContainerPermissions* bagPerms = companionBag->getContainerPermissionsForUpdate();

			if (bagPerms != nullptr) {
				bagPerms->setOwner(player->getObjectID());
			}

			// Window label (2026-07-14, user request: the companion-related
			// container windows need distinguishable titles). The loadout
			// backpack already shows "Companion Loadout" (STF-named); this
			// names the companion's own storage bag so its window stops
			// being blank. Plain text on purpose -- an @companion: STF key
			// would need another TRE rebuild + full client restart; migrate
			// into companion.stf next time that's rebuilt anyway. Idempotent
			// (re-set every summon, same value).
			//
			// Companion System (2026-07-15, "test 5 companions at once"
			// pass -- see NOTES.md): changed from a static "Companion
			// Storage" label to "<Companion's own display name> Inventory"
			// so multiple simultaneously-summoned companions' bags are
			// tellable apart -- getDisplayedName() returns whatever the
			// companion's own nameplate currently is (the default
			// "Companion N" assigned at grant time in SkillManager.cpp, or
			// whatever the owner later renamed it to via the Rename
			// Companion SUI -- see CompanionRenameSuiCallback.h). Re-set
			// every summon, so a rename after the bag was first created
			// still gets picked up on the companion's next summon.
			// 2026-07-20 (user: "windows have no names, can't tell whose it
			// is / if it's the gear window"): getDisplayedName() is the FULL
			// tagged nameplate ("camtw (Snoovi's -=COMPANION=-)"), so the bag
			// title became "camtw (Snoovi's -=COMPANION=-) Inventory" --
			// long enough that the window truncates it and the "Inventory"
			// label is lost. Use the companion's SHORT chosen name
			// (getFirstName() = the first token of the nameplate) for a clean,
			// clear "camtw's Inventory".
			String shortName = companion->getFirstName();

			if (shortName.isEmpty()) {
				shortName = companion->getDisplayedName();
			}

			companionBag->setCustomObjectName(shortName + "'s Inventory", true);
		}
	}

	// Companion System (2026-07-14, "player-side loadout backpack" redesign
	// -- see NOTES.md): self-heal for companions recruited before this
	// feature shipped. New companions get their loadout backpack created
	// once, at recruitment time, in SkillManager.cpp's companion_master_novice
	// grant block -- but that only fires the first time a player learns the
	// skill, so anyone who already had a companion before this pass would
	// otherwise never get one. Checked every summon (cheap scan of the
	// player's own inventory, no worse than the bag-component self-heal
	// above) but only ever actually creates one once -- after that, the
	// scan finds the existing backpack and does nothing further.
	ManagedReference<SceneObject*> playerInventory = player->getSlottedObject("inventory");

	if (playerInventory != nullptr) {
		bool hasLoadoutBackpack = false;
		unsigned int loadoutBackpackCRC = STRING_HASHCODE("object/tangible/inventory/companion_loadout_backpack.iff");

		// Companion System (2026-07-14, "invisible loadout backpack" fix --
		// see companion_loadout_backpack.lua): the backpack's Lua template
		// was originally based on shared_creature_inventory.iff, whose client
		// template has no appearance and the client's internal
		// creature-inventory gameObjectType -- so the bag rendered as an
		// invisible, un-openable object in the player's inventory. The
		// template now derives from shared_backpack_s01.iff, but
		// clientObjectCRC is PERSISTED per object (only set at creation in
		// loadTemplateData(); initializeTransientMembers() never refreshes it
		// on ODB load), so any backpack created under the old template stays
		// broken forever. Migrate: dump its contents into the player's main
		// inventory, destroy it, and fall through to the existing
		// create-a-fresh-one branch below. Idempotent -- a bag with the
		// correct client CRC is left alone.
		unsigned int loadoutBackpackClientCRC = STRING_HASHCODE("object/tangible/wearables/backpack/shared_backpack_s01.iff");

		for (int i = playerInventory->getContainerObjectsSize() - 1; i >= 0; --i) {
			SceneObject* obj = playerInventory->getContainerObject(i);

			if (obj == nullptr || obj->getServerObjectCRC() != loadoutBackpackCRC) {
				continue;
			}

			if (obj->getClientObjectCRC() == loadoutBackpackClientCRC) {
				hasLoadoutBackpack = true;
				break;
			}

			// Stale, invisible pre-fix backpack -- rescue contents, destroy.
			ManagedReference<SceneObject*> staleBag = obj;

			Locker staleBagLocker(staleBag, companion);

			Vector<ManagedReference<SceneObject*> > rescued;

			for (int j = 0; j < staleBag->getContainerObjectsSize(); ++j) {
				SceneObject* content = staleBag->getContainerObject(j);

				if (content != nullptr) {
					rescued.add(content);
				}
			}

			for (int j = 0; j < rescued.size(); ++j) {
				SceneObject* content = rescued.get(j);

				Locker contentLocker(content, companion);

				playerInventory->transferObject(content, -1, true);
			}

			staleBag->destroyObjectFromWorld(true);
			staleBag->destroyObjectFromDatabase(true);
		}

		if (!hasLoadoutBackpack) {
			ZoneServer* zoneServer = player->getZoneServer();

			if (zoneServer != nullptr) {
				ManagedReference<SceneObject*> loadoutBackpack = zoneServer->createObject(String("object/tangible/inventory/companion_loadout_backpack.iff").hashCode(), 1);

				if (loadoutBackpack == nullptr) {
					error("CompanionSystem: could not create companion loadout backpack object during self-heal");
				} else {
					Locker backpackLocker(loadoutBackpack, companion);

					if (!playerInventory->transferObject(loadoutBackpack, -1, true)) {
						error("CompanionSystem: could not place companion loadout backpack into player "
								+ String::valueOf(player->getObjectID()) + "'s own inventory during self-heal");
					} else {
						playerInventory->broadcastObject(loadoutBackpack, true);
					}
				}
			}
		}

		// Companion System (2026-08-09, "dynamic mirroring" pass, v3 work
		// order): summon is one of syncOwnerMirrorAbilities()'s trigger
		// points -- self-heals for companions recruited before this pass
		// shipped, same shape as the loadout backpack self-heal just above,
		// but now grants EXACTLY what this owner's datapad (summoned or
		// stored companions alike) has actually trained, instead of the old
		// grantAllAbilitiesForTesting()'s fixed 61-ability blanket grant --
		// see docs/companion_system/NOTES.md. Recompute-and-diff, so safe to
		// call every summon.
		CompanionSkillTrainer::instance()->syncOwnerMirrorAbilities(player);
	}

	companion->setVitality(vitality);
	companion->setMaxVitality(maxVitality);

	ManagedReference<CellObject*> parent = player->getParent().get().castTo<CellObject*>();

	if (parent != nullptr) {
		parent->transferObject(companion, -1, true);
	} else {
		zone->transferObject(companion, -1, true);
	}

	// Companion System bug fix (2026-07-13, "companion runs away the instant
	// it's spawned/Followed" -- see docs/companion_system/NOTES.md): this
	// spawnObject() never set a homeLocation for the companion, so it stayed
	// at its default-constructed value (no position ever assigned, no cell,
	// not "reached"). Now that the companion's optionsBitmask actually
	// enables AIENABLED (see the earlier "companion never follows" fix,
	// object/mobile/companion_actor.lua), AiAgentImplementation::
	// runBehaviorTree()/setDestination() actually execute every tick for
	// the first time -- and the companion has no custom AI map of its own
	// (customAiMap is never set anywhere in this feature), so it runs the
	// same generic, unmodified wild-mobile behavior tree/state machine every
	// ordinary NPC uses. That generic tree's OBLIVIOUS/PATHING_HOME logic
	// unconditionally paths toward homeLocation whenever the creature isn't
	// "in range" of it (AiAgentImplementation.cpp's setDestination(),
	// AiAgent::OBLIVIOUS/PATHING_HOME cases) -- with homeLocation never set,
	// every companion beelined toward the unset default location the moment
	// AI started ticking, independent of and overriding whatever
	// setFollowObject()/setCompanionState() below say -- exactly the "runs
	// away" symptom, both on fresh spawn and after Follow was pressed.
	// Modeled directly on the real Creature Handler pet system's own
	// spawnObject() equivalent (PetControlDeviceImplementation.cpp), which
	// already calls the identical setHomeLocation(owner position) before
	// setAITemplate() for this exact reason.
	companion->setHomeLocation(player->getPositionX(), player->getPositionZ(), player->getPositionY(), parent);

	// Companion System (2026-07-15, "companion stops following / leashes
	// back home" fix -- see ai/companion.lua and NOTES.md): assign the
	// dedicated companion AI map BEFORE setAITemplate() assembles the
	// trees. The generic wild-mobile trees the companion ran until now
	// leash a creature back to homeLocation whenever it strays -- correct
	// for wild spawns, exactly wrong for an owner-following companion:
	// the moment the owner moved any real distance, the companion got
	// yanked back to its summon spot (live-reported as "no longer
	// follows"). The companion map overrides only AWARE/IDLE/MOVE
	// (follow-at-a-run, pet-style); all other slots fall back to the
	// default trees already proven for companion combat.
	// genesis port: DEFERRED -- setCustomAiMap()/customAiMap does not exist on this
	// base and adding it would require an AiAgent.idl change (out of scope). Dropped
	// the setCustomAiMap(STRING_HASHCODE("companion")) call; the companion falls back
	// to the default AiMap trees selected from creatureBitmask. This is part of the
	// already-known behaviour-tree gap: genesis drives AI from Lua behaviour trees and
	// the native leaf classes the Companion System expects do not exist here, so the
	// "companion" AI map (follow-at-a-run, no leash-home) is not applied.
	// genesis port: setAITemplate() -> setupBehaviorTree() (AiAgent.idl:1178,
	// autogen/.../AiAgent.h:805) -- same no-arg "assemble the default trees" call.
		// genesis port FIX (2026-08-04) -- supersedes the DEFERRED note above.
		// That note concluded no leash-free option existed on this base. It was
		// wrong: leashing lives in exactly ONE place on the Lua-BT era codebase,
		// MoveBase:checkConditions in bin/scripts/ai/actions/movebase.lua, which
		// calls shouldRetreat(256) then leash(). MovePetBase OVERRIDES that method
		// and omits the check, so any template whose mover is MoveCreaturePet can
		// never leash. Genesis already ships one: templates/stationarynoleash.lua
		// is a selector with a full attack sequence plus {"idle0",
		// "MoveCreaturePet", "root", BEHAVIOR} -- follows, fights, never goes
		// home. Stock quest code uses exactly this (quest_tasks/encounter.lua).
		//
		// activateLoad() runs its AiLoadTask immediately (clearBehaviorList() ->
		// setupBehaviorTree(named template) -> activateMovementEvent()), so the
		// tree is in place before the setFollowObject() below.
		companion->activateLoad("companionfollow");

		// setFollowObject() is SILENTLY a no-op while isRetreating(), and
		// isRetreating() is literally !homeLocation.isReached() (AiAgent.idl:794,
		// guarding the setters at 658/670/682). setHomeLocation() ran a few lines
		// above, so if a fresh PatrolPoint starts unreached, every spawn-time
		// follow request was being discarded -- which is why companions did not
		// move at all rather than merely getting yanked back. Clearing it here is
		// idempotent and costs nothing if setHomeLocation() already did it.
		companion->getHomeLocation()->setReached(true);
	companion->activateRecovery();
	companion->setFollowObject(player);
	// genesis port FIX (2026-08-04). restoreFollowObject() restores whatever
	// storeFollowObject() last saved, and storeFollowObject() was called NOWHERE
	// in the companion code -- so every restore in the stock terminate handlers
	// (CombatMoveCreaturePet, GetTargetCreaturePet) has been restoring NULL, and
	// a companion that finished a fight had nothing to go back to. Stock pets do
	// exactly this, in PetFollowCommand, right after setFollowObject.
	companion->storeFollowObject();

	// COMPANION_MOVEMENT_EVENT_FIX_2026_08_04 -- THE "companion never moves" FIX.
	//
	// activateLoad() above already called activateMovementEvent() once, but it
	// ran while followObject was still nullptr and isRetreating() was false
	// (setHomeLocation() sets homeLocation.reached = true, AiAgent.idl:609).
	// That is precisely the self-destruct clause at the top of
	// AiAgentImplementation::activateMovementEvent():
	//
	//     if ((waitTime < 0 || numberOfPlayersInRange <= 0)
	//         && getFollowObject().get() == nullptr && !isRetreating()) {
	//             moveEvent = nullptr; return;
	//     }
	//
	// ...so the move event was created by AiLoadTask and immediately destroyed.
	// The companion ended up with a correct behaviour tree and a correct follow
	// object and no clock to tick either of them, which is exactly the reported
	// "companion stands still / only moves after I loot something" symptom
	// (loot + combat go through engine paths that re-arm the event themselves).
	//
	// Nothing else re-arms it: activateMovementEvent() is commented out in
	// setOblivious/setWatchObject/setStalkObject/setFollowObject
	// (AiAgent.idl:652/664/676/688) and in setCurrentBehavior.
	//
	// Calling it HERE -- after setFollowObject() -- means getFollowObject() is
	// non-null, the clause cannot fire, and the AiMoveEvent is created and
	// scheduled for real.
	companion->activateMovementEvent();
	companion->setCompanionState(CompanionObject::FOLLOW);
	companion->faceObject(player, true);

	// Companion System bug fix (2026-07-13, "companion doesn't fire its
	// equipped weapon" -- see NOTES.md): populate real attack maps at
	// spawn/re-summon time too, not just on a fresh auto-equip (see the
	// matching call in CompanionContainerComponent.cpp's attemptAutoEquip()).
	// Passing the currently-equipped weapon (if any, e.g. a companion being
	// re-summoned after already having a weapon equipped from a prior
	// session) -- refreshCombatAttacks() itself falls back to the innate
	// unarmed weapon if this is null.
	companion->refreshCombatAttacks(companion->getWeapon());

	// Keep-up monitor (2026-07-18, "never further than 25m behind" -- see
	// CompanionObject.idl): idempotent start at every summon.
	companion->startKeepUpMonitor();

	// Attach the owner-status threat observer (spec 4A) so the companion can
	// auto-intercept attackers while idle/passive.
	if (threatObserver == nullptr) {
		threatObserver = new CompanionThreatObserver(companion);
	}

	player->registerObserver(ObserverEventType::DAMAGERECEIVED, threatObserver);
	player->registerObserver(ObserverEventType::STARTCOMBAT, threatObserver);

	player->sendSystemMessage("@companion:summoned"); // Your companion has been summoned.

	// Companion System -- one-time first-launch flow: baseline stat
	// migration + starter profession choice, done in-world on the
	// companion's actual first summon (not at grant time in
	// SkillManager::awardSkill(), which only creates the object -- see
	// docs/companion_system/NOTES.md). migrateBaselineStats() is idempotent
	// so it's safe to re-apply if the player never completed the SUI on a
	// prior summon; firstLaunchComplete only flips true once
	// CompanionStarterProfessionSuiCallback actually fires with a valid
	// selection, so this block naturally re-prompts every summon until they
	// pick one. After this point, stat migration only ever happens through
	// the canonical places (per the user's explicit design direction), never
	// automatically again.
	// (2026-07-20: the first-launch profession picker moved to the TOP of
	// this function -- pre-spawn, per user request. By the time execution
	// reaches here, firstLaunchComplete is always already true.)
}

void CompanionControlDeviceImplementation::storeObject(CreatureObject* player, bool force) {
	if (player == nullptr) {
		return;
	}

	ManagedReference<CompanionObject*> companion = companionObject.get();

	if (companion == nullptr) {
		return;
	}

	if (!force && (companion->isInCombat() || player->isInCombat())) {
		player->sendSystemMessage("@companion:cant_store_now"); // You cannot store your companion while in combat.
		return;
	}

	// Sync the authoritative persisted copy before despawning -- @json
	// persistence on this (locked, dirty) object takes care of writing
	// vitality/learnedSkills/experiencePools/inventory to the database, so no
	// separate manual serialization step is required (see NOTES.md,
	// "Database persistence").
	vitality = companion->getVitality();
	maxVitality = companion->getMaxVitality();

	if (threatObserver != nullptr) {
		player->dropObserver(ObserverEventType::DAMAGERECEIVED, threatObserver);
		player->dropObserver(ObserverEventType::STARTCOMBAT, threatObserver);
	}

	companion->setFollowObject(nullptr);

	// Companion Taxi (2026-07-15): tear down any active ride (despawns the
	// cosmetic vehicle, restores speeds) before the companion leaves the
	// world. Safe no-op when no ride is active.
	companion->stopTaxiRide(false);

	// Companion System (2026-07-15, "/invite groups the companion" -- see
	// GroupManager.cpp and NOTES.md): a stored companion must leave the
	// owner's group, or the group keeps a dangling despawned member --
	// mirrors StorePetTask.cpp's identical cleanup for real pets.
	ManagedReference<GroupObject*> companionGroup = companion->getGroup();

	companion->destroyObjectFromWorld(true);

	if (companionGroup != nullptr) {
		GroupManager::instance()->leaveGroup(companionGroup, companion);
	}

	player->sendSystemMessage("@companion:stored"); // Your companion has been stored.
}

void CompanionControlDeviceImplementation::handleCompanionDeath(CreatureObject* owner) {
	// Companion System (2026-07-19 rewrite, per user request) -- no more
	// permanent penalty math (see the file header comment). isDead=true is
	// the whole story here now; maxVitality/maxHAM are left exactly as they
	// are and get forced back to full by reviveCompanion() on the next
	// summon regardless, so there's nothing to compute or clamp.
	isDead = true;

	ManagedReference<CompanionObject*> companion = companionObject.get();

	if (companion != nullptr) {
		// Bug fix (2026-07-19, per user report: "you cannot summon or store
		// your companion right now" on every attempt after a death) --
		// handleCompanionDeath() fires from notifyObjectDestructionObservers()
		// at the exact moment the companion's health hits zero, which is
		// necessarily WHILE it's still isInCombat() (defenders list
		// populated). destroyObjectFromWorld() below despawns the companion
		// but never clears that combat state on its own -- so the stored,
		// "dead" companion kept isInCombat()==true forever after, and
		// callObject()'s own guard (`companion->isInCombat() || ...`) then
		// permanently blocked every future summon AND store attempt, with no
		// way to clear it since the companion was already out of the world.
		// Every OTHER companion order command clears combat with
		// attemptPeace() (see CompanionFollowCommand.h etc.), but that call
		// can flatly FAIL and leave combat state untouched if the attacker
		// is still in range and is this creature's main defender --
		// precisely true at the instant of death, since the killer is
		// necessarily still right there. forcePeace() (CombatManager.cpp,
		// "Called for AiAgents to break their combat state") has no such
		// escape hatch -- it unconditionally clears the defender list and
		// combat state, which is what an actual death needs. It's a
		// self-locking deferred task (see its own implementation), safe to
		// call here even though destroyObjectFromWorld() below removes the
		// companion from the world moments later.
		if (companion->isInCombat()) {
			CombatManager::instance()->forcePeace(companion);
		}

		companion->setCompanionState(CompanionObject::STAY);

		// Companion Taxi (2026-07-15): same teardown as storeObject() above.
		companion->stopTaxiRide(false);

		// Same group cleanup as storeObject() above (2026-07-15) -- a dead,
		// despawned companion must not linger as a group member.
		ManagedReference<GroupObject*> companionGroup = companion->getGroup();

		companion->destroyObjectFromWorld(true);

		if (companionGroup != nullptr) {
			GroupManager::instance()->leaveGroup(companionGroup, companion);
		}
	}

	if (owner != nullptr) {
		owner->sendSystemMessage("@companion:companion_died"); // Your companion has fallen.
	}
}

void CompanionControlDeviceImplementation::reviveCompanion() {
	if (!isDead) {
		return;
	}

	isDead = false;

	// Companion System (2026-07-19 rewrite, per user request: "100 health
	// 100 action 100 mind so it doesn't stay dead ... spawn in alive again
	// with minimal health that regenerates") -- max is always forced back
	// to full (no permanent penalty), but CURRENT starts at a small
	// fraction of that so the companion has to regen up naturally instead
	// of popping back in at full health. Device-level vitality mirrors the
	// same rule as the companion's own HAM below.
	// Companion System (2026-07-28 FIX, per Nick: "make it so they can
	// always be called if they have no HAM give them 100 of each") --
	// full vitality on revive now, superseding the 2026-07-19 rewrite's
	// 10% "weak, has to regen up" design per this explicit new request.
	maxVitality = 100;
	vitality = maxVitality;

	ManagedReference<CompanionObject*> companion = companionObject.get();

	if (companion == nullptr) {
		return;
	}

	companion->setPosture(CreaturePosture::UPRIGHT, true, true);
	companion->setCompanionState(CompanionObject::FOLLOW);

	for (int i = 0; i < 9; ++i) {
		companion->setMaxHAM(i, 100);
		companion->setHAM(i, 100);
	}

	companion->setMaxVitality(maxVitality);
	companion->setVitality(vitality);
}

void CompanionControlDeviceImplementation::fillAttributeList(AttributeListMessage* alm, CreatureObject* player) {
	// Companion System UX-parity fix: reuse the exact same real,
	// client-recognized attribute keys PetControlDeviceImplementation uses
	// (see PetControlDeviceImplementation.cpp's own fillAttributeList) so
	// examining a companion's datapad item shows a real stat sheet instead
	// of the near-empty IntangibleObjectImplementation default. No new
	// client-side obj_attr_n.stf entries are required since every key below
	// already ships with the base client.
	if (isDead) {
		alm->insertAttribute("creature_vitality", "0/" + String::valueOf(maxVitality));
		return;
	}

	alm->insertAttribute("creature_vitality", String::valueOf(vitality) + "/" + String::valueOf(maxVitality));

	ManagedReference<CompanionObject*> companion = companionObject.get();

	if (companion == nullptr) {
		return;
	}

	// HAM sub-attribute indices 0/3/6 = Health/Action/Mind base pools
	// (templates/params/creature/CreatureAttribute.h), the same three the
	// real pet tooltip shows -- Strength/Constitution/Quickness/Stamina/
	// Focus/Willpower are secondary pools not surfaced on the pet tooltip
	// either, so this intentionally matches that same level of detail.
	alm->insertAttribute("challenge_level", companion->getCombatLevel());
	alm->insertAttribute("creature_health", companion->getBaseHAM(0));
	alm->insertAttribute("creature_action", companion->getBaseHAM(3));
	alm->insertAttribute("creature_mind", companion->getBaseHAM(6));
}
