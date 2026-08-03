/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-23, "gear exchange" pass, per user request) --
	cross-companion armor/weapon hand-offs, built on top of the existing
	CompanionCraftingManager / CompanionCraftTheater machinery rather than
	inventing a new item-transfer path.

	Two entry points:

	  - checkAndExchangeArmor(owner, announceIfNone): an armorsmith companion
	    (getLearnedSkill().contains("armorsmith")) looks through its OWN spare
	    armor (loose ArmorObjects sitting in its inventory, not equipped --
	    i.e. leftovers from a prior /companionrequestarmor full-suit craft, or
	    anything else handed to it) and compares each spare's real
	    ArmorObject::getRating() against whatever every other active
	    companion currently has equipped in the matching slot(s). Anyone with
	    that slot empty, or wearing something with a lower rating, gets
	    queued for a hand-off -- even if they already have lower-end armor
	    equipped (matches the "even if they have a lower end armor equip"
	    request). Multiple recipients are processed one at a time (a
	    functional line -- see file-level note below on why this reuses
	    sequential processing instead of a literal FormationManager queue).
	    Called both from a manual command AND periodically per armorsmith
	    (see scheduleGearCheckTick()).

	  - offerWeaponAfterSkillGrant(owner, trainedCompanion, skillName): hooked
	    directly into CompanionObjectImplementation::grantSkill() (see that
	    file for the one-line call site addition). The moment ANY companion
	    learns a skill, if a weaponsmith companion is present in the same
	    squad, its spare weapons are checked against the new skill via the
	    real SharedTangibleObjectTemplate::getCertificationsRequired() list
	    (the same data-driven metadata PlayerContainerComponent::
	    canAddObject() already reads for players -- companions are not
	    cert-gated on what they can wear, so this is used purely as a
	    decision heuristic, not an equip restriction). If more than one spare
	    weapon qualifies, one is picked at random.

	Both paths share one theater engine (runGearStep() et al) -- mechanically
	identical to CompanionCraftTheater's material-trade walk/hand-off/bow-
	kowtow choreography, just carrying a whole TangibleObject (armor or
	weapon) instead of a partial ResourceContainer split. The actual
	auto-equip + old-gear-displacement is NOT reimplemented here: dropping
	the item into the recipient as a loose (-1) insert is enough --
	CompanionContainerComponent::notifyObjectInserted() -> attemptAutoEquip()
	/ tryEquipOntoCompanion() already auto-equips it and displaces whatever
	was in the matching slot into the recipient's own inventory bag (2026-07-
	15 "always swap out the occupied slot" policy). This manager's whole job
	is: decide WHO needs WHAT, then physically hand the item over -- the
	existing container hooks do the rest.

	"Line up" note: unlike FormationManager's persistent visual formation
	slots, gear hand-offs here process the recipient queue strictly one at a
	time (trades[tradeIndex]) -- the same functional choreography
	CompanionCraftTheater already uses for multi-donor material trades, which
	reads on-screen as a line (each companion walks up in turn, trades, steps
	away) without needing a second coordination system. A literal
	FormationManager "line" formation for everyone WAITING their turn is a
	nice-to-have visual polish pass, not required for the feature to work,
	and deliberately deferred -- see NOTES.md.

	Full-suit crafting (the player asking an armorsmith companion to build a
	whole matching set) is NOT in this file -- see CompanionRequestArmorCommand.h
	/ CompanionGearTypeSuiCallback.h, which drive CompanionCraftTheater
	per-piece and chain pieces using pollUntilSmithFree() below.
*/

#ifndef COMPANIONGEAREXCHANGEMANAGER_H_
#define COMPANIONGEAREXCHANGEMANAGER_H_

#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/objects/tangible/wearables/ArmorObject.h"
#include "server/zone/objects/tangible/weapon/WeaponObject.h"
#include "server/zone/objects/creature/ai/PatrolPoint.h"
#include "server/zone/managers/objectcontroller/ObjectController.h"
#include "server/zone/managers/companion/CompanionCraftTheater.h"
#include "templates/SharedTangibleObjectTemplate.h"
#include "server/zone/ZoneServer.h"
#include "server/zone/Zone.h"

class CompanionGearTrade : public Object {
public:
	uint64 recipientID = 0;
	uint64 itemID = 0;
};

class CompanionGearTheaterState : public Object {
public:
	uint64 smithID = 0;
	Vector<Reference<CompanionGearTrade*> > trades;
	int tradeIndex = 0;
	int steps = 0;
};

class CompanionGearExchangeManager {
public:

	// ---- profession detection (same idiom CompanionChatter::resolveProfessionFlavor
	// already uses) ---------------------------------------------------------
	static bool isArmorsmith(CompanionObject* companion) {
		if (companion == nullptr) {
			return false;
		}

		for (int i = 0; i < companion->getLearnedSkillCount(); ++i) {
			if (companion->getLearnedSkill(i).contains("armorsmith")) {
				return true;
			}
		}

		return false;
	}

	static bool isWeaponsmith(CompanionObject* companion) {
		if (companion == nullptr) {
			return false;
		}

		for (int i = 0; i < companion->getLearnedSkillCount(); ++i) {
			if (companion->getLearnedSkill(i).contains("weaponsmith")) {
				return true;
			}
		}

		return false;
	}

	/** Same datapad-scan every Companion*Command.h already duplicates locally
	 * (CompanionFormupCommand.h, CompanionCraftTheater.h) -- kept as its own
	 * copy here for the same reason those do: this file has no shared base
	 * to hang a protected helper off. */
	static void resolveActiveCompanions(CreatureObject* owner, Vector<ManagedReference<CompanionObject*> >& companions) {
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

			if (device->isCompanionDead()) {
				continue;
			}

			CompanionObject* companion = device->getCompanionObject();

			if (companion == nullptr || companion->getZone() == nullptr || companion->getLinkedCreature().get() != owner) {
				continue;
			}

			companions.add(companion);
		}
	}

	static CompanionObject* findFirstWithFlavor(CreatureObject* owner, bool wantArmorsmith) {
		Vector<ManagedReference<CompanionObject*> > companions;
		resolveActiveCompanions(owner, companions);

		for (int i = 0; i < companions.size(); ++i) {
			CompanionObject* c = companions.get(i).get();

			if (wantArmorsmith ? isArmorsmith(c) : isWeaponsmith(c)) {
				return c;
			}
		}

		return nullptr;
	}

	// ---- busy-guard (2026-07-29, Gear Exchange hardening batch) -----------
	// Same Vector<uint64>+find()/add()/remove(idx) idiom as
	// CompanionMenuComponent.cpp's doctorBuffCraftBusy()/woundHealCraftBusy()
	// guards. ONE shared set keyed by smith objectID -- both
	// checkAndExchangeArmor() (periodic) and offerWeaponAfterSkillGrant()
	// (event-driven) funnel through the same runGearStep() theater engine on
	// the same smith, so a single shared guard covers both without needing
	// two separate sets to cross-check each other.
	static Vector<uint64>& gearExchangeBusy() {
		static Vector<uint64> ids;
		return ids;
	}

	static bool isGearExchangeBusy(uint64 smithID) {
		return gearExchangeBusy().find(smithID) != -1;
	}

	/** Cleared on EVERY exit path of runGearStep() below (despawn/combat
	 * interrupt, 2-minute hard cap, and normal trade-queue completion) so a
	 * failed or interrupted hand-off never leaves a smith permanently unable
	 * to hand off gear again. */
	static void setGearExchangeBusy(uint64 smithID, bool busy) {
		Vector<uint64>& ids = gearExchangeBusy();
		int idx = ids.find(smithID);

		if (busy) {
			if (idx == -1) {
				ids.add(smithID);
			}
		} else if (idx != -1) {
			ids.remove(idx);
		}
	}

	// ---- ARMOR: rating comparison ------------------------------------------

	/** True if `candidate` (a spare the armorsmith is holding) is an upgrade
	 * for `recipient` -- either the matching slot(s) are empty, or whatever's
	 * equipped there has a lower real ArmorObject::getRating(). */
	static bool candidateIsUpgrade(CompanionObject* recipient, ArmorObject* candidate) {
		if (recipient == nullptr || candidate == nullptr) {
			return false;
		}

		int arrangementSize = candidate->getArrangementDescriptorSize();

		if (arrangementSize == 0) {
			return false;
		}

		bool sawOccupant = false;
		int bestOccupantRating = -1;

		for (int i = 0; i < arrangementSize; ++i) {
			const Vector<String>* descriptors = candidate->getArrangementDescriptor(i);

			for (int j = 0; j < descriptors->size(); ++j) {
				SceneObject* slotted = recipient->getSlottedObject(descriptors->get(j));

				if (slotted == nullptr) {
					continue;
				}

				sawOccupant = true;

				if (slotted->isArmorObject()) {
					int rating = cast<ArmorObject*>(slotted)->getRating();

					if (rating > bestOccupantRating) {
						bestOccupantRating = rating;
					}
				}
			}
		}

		if (!sawOccupant) {
			return true; // empty slot -- needs armor outright
		}

		return candidate->getRating() > bestOccupantRating;
	}

	/** Every loose (unequipped, containmentType -1) ArmorObject sitting
	 * directly on `smith` or in its "inventory" bag -- these are the
	 * "spares" available to hand out. */
	static void collectSpareArmor(CompanionObject* smith, Vector<ManagedReference<ArmorObject*> >& out) {
		auto scan = [&](SceneObject* container) {
			if (container == nullptr) {
				return;
			}

			for (int i = 0; i < container->getContainerObjectsSize(); ++i) {
				ManagedReference<SceneObject*> obj = container->getContainerObject(i);

				if (obj == nullptr || !obj->isArmorObject()) {
					continue;
				}

				out.add(cast<ArmorObject*>(obj.get()));
			}
		};

		scan(smith);
		scan(smith->getSlottedObject("inventory"));
	}

	static void collectSpareWeapons(CompanionObject* smith, Vector<ManagedReference<WeaponObject*> >& out) {
		auto scan = [&](SceneObject* container) {
			if (container == nullptr) {
				return;
			}

			for (int i = 0; i < container->getContainerObjectsSize(); ++i) {
				ManagedReference<SceneObject*> obj = container->getContainerObject(i);

				if (obj == nullptr || !obj->isWeaponObject()) {
					continue;
				}

				out.add(cast<WeaponObject*>(obj.get()));
			}
		};

		scan(smith);
		scan(smith->getSlottedObject("inventory"));
	}

	/** Manual entry point (command) AND periodic entry point (scheduleGearCheckTick).
	 * announceIfNone: only bark "nobody needs anything" when explicitly asked
	 * (the manual command) -- the periodic tick stays silent when there's
	 * nothing to do, so it doesn't spam chat every cycle. */
	static void checkAndExchangeArmor(CreatureObject* owner, bool announceIfNone) {
		if (owner == nullptr) {
			return;
		}

		CompanionObject* smith = findFirstWithFlavor(owner, true);

		if (smith == nullptr) {
			if (announceIfNone) {
				owner->sendSystemMessage("You don't have an armorsmith companion active.");
			}

			return;
		}

		Locker smithLocker(smith);

		if (smith->getZone() == nullptr || smith->isDead() || smith->isInCombat()
				|| smith->getCompanionState() == CompanionObject::PATROL) {
			// Already busy (crafting theater, another gear theater, etc.) --
			// skip this cycle rather than stacking overlapping choreography.
			return;
		}

		// Companion System (2026-07-29, Gear Exchange hardening batch): the
		// PATROL check just above only ever catches a RECIPIENT mid-walk from
		// a previous cycle -- the smith itself never changes state during a
		// hand-off, so without this guard a periodic tick firing again
		// mid-hand-off (or the event-driven weapon trigger landing on the
		// same smith) could stack a second, overlapping trade queue.
		if (isGearExchangeBusy(smith->getObjectID())) {
			if (announceIfNone) {
				owner->sendSystemMessage(smith->getDisplayedName() + " is still in the middle of handing off gear.");
			}

			return;
		}

		// Skip while the smith is actively mid-craft (the CraftTheater glow
		// window) -- e.g. a player-directed "build me a full suit" request
		// (CompanionArmorTypeSuiCallback) is in progress on this same
		// companion right now.
		if (CompanionCraftTheater::isCompanionCrafting(owner, smith->getObjectID())) {
			return;
		}

		Vector<ManagedReference<ArmorObject*> > spares;
		collectSpareArmor(smith, spares);

		if (spares.size() == 0) {
			if (announceIfNone) {
				owner->sendSystemMessage(smith->getDisplayedName() + " doesn't have any spare armor on hand right now.");
			}

			return;
		}

		Vector<ManagedReference<CompanionObject*> > companions;
		resolveActiveCompanions(owner, companions);

		Reference<CompanionGearTheaterState*> state = new CompanionGearTheaterState();
		state->smithID = smith->getObjectID();

		SortedVector<uint64> claimedItems;
		claimedItems.setNoDuplicateInsertPlan();

		for (int c = 0; c < companions.size(); ++c) {
			CompanionObject* recipient = companions.get(c).get();

			if (recipient == nullptr || recipient == smith || recipient->isDead() || recipient->getZone() == nullptr) {
				continue;
			}

			if (CompanionCraftTheater::isCompanionCrafting(owner, recipient->getObjectID())) {
				continue; // mid-craft -- don't interrupt with a hand-off
			}

			Locker recipientLocker(recipient, smith);

			for (int s = 0; s < spares.size(); ++s) {
				ArmorObject* candidate = spares.get(s).get();

				if (candidate == nullptr || claimedItems.contains(candidate->getObjectID())) {
					continue;
				}

				if (candidateIsUpgrade(recipient, candidate)) {
					Reference<CompanionGearTrade*> trade = new CompanionGearTrade();
					trade->recipientID = recipient->getObjectID();
					trade->itemID = candidate->getObjectID();
					state->trades.add(trade);

					claimedItems.put(candidate->getObjectID());
					break; // one piece per recipient per pass
				}
			}
		}

		if (state->trades.size() == 0) {
			if (announceIfNone) {
				owner->sendSystemMessage("Nobody needs armor right now.");
			}

			return;
		}

		setGearExchangeBusy(smith->getObjectID(), true);

		CompanionCraftTheater::say(smith, "Hold up -- does anyone need armor, or better armor than what they've got?");

		ZoneServer* zoneServer = owner->getZoneServer();
		Reference<CreatureObject*> ownerRef = owner;
		scheduleGearStep(zoneServer, ownerRef, state, 600);
	}

	// ---- WEAPON: skill-unlock trigger --------------------------------------

	/** Called from CompanionObjectImplementation::grantSkill() -- see that
	 * file's call site. Silent no-op when there's no weaponsmith or nothing
	 * matches (fires on EVERY skill grant, so it must not spam chat). */
	static void offerWeaponAfterSkillGrant(CreatureObject* owner, CompanionObject* trainedCompanion, const String& skillName) {
		if (owner == nullptr || trainedCompanion == nullptr || skillName.isEmpty()) {
			return;
		}

		CompanionObject* smith = findFirstWithFlavor(owner, false);

		if (smith == nullptr || smith == trainedCompanion) {
			return;
		}

		Locker smithLocker(smith);

		if (smith->getZone() == nullptr || smith->isDead() || smith->isInCombat()
				|| smith->getCompanionState() == CompanionObject::PATROL) {
			return;
		}

		Locker recipientLocker(trainedCompanion, smith);

		if (trainedCompanion->getZone() == nullptr || trainedCompanion->isDead() || trainedCompanion->isInCombat()) {
			return;
		}

		// Companion System (2026-07-29, Gear Exchange hardening batch): same
		// busy-guard + mid-craft skip as checkAndExchangeArmor() above -- this
		// event-driven weapon trigger shares the same smith/runGearStep()
		// engine, so it must respect the same guards (see
		// gearExchangeBusy()'s own comment for why ONE shared set covers
		// both directions).
		if (isGearExchangeBusy(smith->getObjectID())) {
			return;
		}

		if (CompanionCraftTheater::isCompanionCrafting(owner, smith->getObjectID())
				|| CompanionCraftTheater::isCompanionCrafting(owner, trainedCompanion->getObjectID())) {
			return;
		}

		Vector<ManagedReference<WeaponObject*> > spares;
		collectSpareWeapons(smith, spares);

		Vector<ManagedReference<WeaponObject*> > matches;

		for (int i = 0; i < spares.size(); ++i) {
			WeaponObject* weapon = spares.get(i).get();

			if (weapon == nullptr || weapon->getObjectTemplate() == nullptr) {
				continue;
			}

			SharedTangibleObjectTemplate* tanoData = dynamic_cast<SharedTangibleObjectTemplate*>(weapon->getObjectTemplate());

			if (tanoData == nullptr) {
				continue;
			}

			const Vector<String>& required = tanoData->getCertificationsRequired();

			for (int r = 0; r < required.size(); ++r) {
				if (!required.get(r).isEmpty() && skillName.contains(required.get(r))) {
					matches.add(weapon);
					break;
				}
			}
		}

		if (matches.size() == 0) {
			return; // nothing this skill unlocks -- stay quiet, this fires constantly
		}

		WeaponObject* chosen = matches.get(System::random(matches.size() - 1)).get();

		Reference<CompanionGearTheaterState*> state = new CompanionGearTheaterState();
		state->smithID = smith->getObjectID();

		Reference<CompanionGearTrade*> trade = new CompanionGearTrade();
		trade->recipientID = trainedCompanion->getObjectID();
		trade->itemID = chosen->getObjectID();
		state->trades.add(trade);

		setGearExchangeBusy(smith->getObjectID(), true);

		CompanionCraftTheater::say(smith, "Hey -- now that you've trained that up, I've got just the thing for you.");

		ZoneServer* zoneServer = owner->getZoneServer();
		Reference<CreatureObject*> ownerRef = owner;
		scheduleGearStep(zoneServer, ownerRef, state, 600);
	}

	// ---- shared theater engine (armor + weapon both funnel through this) ---

	static void scheduleGearStep(ZoneServer* zoneServer, Reference<CreatureObject*> ownerRef, Reference<CompanionGearTheaterState*> state, int delayMs) {
		Core::getTaskManager()->scheduleTask([zoneServer, ownerRef, state] () {
			runGearStep(zoneServer, ownerRef, state);
		}, "CompanionGearTheaterStepLambda", delayMs);
	}

	static void runGearStep(ZoneServer* zoneServer, Reference<CreatureObject*> ownerRef, Reference<CompanionGearTheaterState*> state) {
		CreatureObject* owner = ownerRef.get();

		if (owner == nullptr || state == nullptr || zoneServer == nullptr) {
			return;
		}

		ManagedReference<SceneObject*> smithObj = zoneServer->getObject(state->smithID);
		CompanionObject* smith = smithObj != nullptr ? smithObj.castTo<CompanionObject*>().get() : nullptr;

		if (smith == nullptr) {
			setGearExchangeBusy(state->smithID, false);
			return;
		}

		Locker slocker(smith);

		if (smith->getZone() == nullptr || smith->isDead() || smith->isInCombat() || owner->isInCombat()) {
			CompanionCraftTheater::resumeFollow(smith, owner);
			setGearExchangeBusy(state->smithID, false);
			return;
		}

		if (++state->steps > 300) { // 2-minute hard cap, same as CompanionCraftTheater
			CompanionCraftTheater::resumeFollow(smith, owner);
			setGearExchangeBusy(state->smithID, false);
			return;
		}

		if (state->tradeIndex >= state->trades.size()) {
			CompanionCraftTheater::resumeFollow(smith, owner);
			setGearExchangeBusy(state->smithID, false);
			return;
		}

		CompanionGearTrade* trade = state->trades.get(state->tradeIndex).get();

		ManagedReference<SceneObject*> recipientObj = zoneServer->getObject(trade->recipientID);
		CompanionObject* recipient = recipientObj != nullptr ? recipientObj.castTo<CompanionObject*>().get() : nullptr;

		ManagedReference<SceneObject*> itemObj = zoneServer->getObject(trade->itemID);
		TangibleObject* item = itemObj != nullptr ? itemObj.castTo<TangibleObject*>().get() : nullptr;

		if (recipient == nullptr || item == nullptr || recipient->getZone() != smith->getZone()
				|| recipient->isDead() || recipient->isInCombat() || item->getParent().get() != smith) {
			// Skip this hand-off (recipient gone, item already moved/gone) --
			// move on to the next one.
			++state->tradeIndex;
			scheduleGearStep(zoneServer, ownerRef, state, 400);
			return;
		}

		Locker rlocker(recipient, smith);

		// Walk the recipient to the smith.
		if (recipient->getDistanceTo(smith) > 5.f) {
			recipient->setCompanionState(CompanionObject::PATROL);
			recipient->setFollowObject(nullptr);

			if (recipient->getPatrolPointSize() == 0) {
				PatrolPoint point(smith->getPositionX(), smith->getPositionZ(), smith->getPositionY());
				recipient->addPatrolPoint(point);
				recipient->setMovementState(AiAgent::PATROLLING);
			}

			scheduleGearStep(zoneServer, ownerRef, state, 400);
			return;
		}

		// At the smith -- hand the item over as a loose (-1) insert.
		// CompanionContainerComponent::notifyObjectInserted() ->
		// attemptAutoEquip() takes it from here: auto-equips it and displaces
		// whatever was in the matching slot into the recipient's own
		// inventory bag (2026-07-15 "always swap out the occupied slot"
		// policy) -- no equip logic needs reimplementing here.
		ZoneServer* zs = smith->getZoneServer();
		ObjectController* objectController = zs != nullptr ? zs->getObjectController() : nullptr;

		if (objectController != nullptr) {
			Locker ilocker(item, smith);

			String itemName = item->getDisplayedName();
			bool isWeapon = item->isWeaponObject();

			objectController->transferObject(item, recipient, -1, true);

			smith->faceObject(recipient, true);
			recipient->faceObject(smith, true);
			smith->doAnimation("bow");
			recipient->doAnimation("kowtow");

			CompanionCraftTheater::say(smith, isWeapon
					? ("Here -- try this " + itemName + ".")
					: ("Here -- this " + itemName + " will serve you better."));
			CompanionCraftTheater::say(recipient, "Much appreciated.");
		}

		CompanionCraftTheater::stepAwayThenFollow(recipient, smith, owner, 5.f);

		++state->tradeIndex;
		scheduleGearStep(zoneServer, ownerRef, state, 2500);
	}

	// ---- periodic auto-check (armor only -- weapon side is event-driven) --

	static const int GEAR_CHECK_TICK_MS = 90000; // 90s -- see design doc for tuning

	/** Arm (or re-arm) the periodic gear check for `armorsmith`. Self-
	 * terminating: if the companion is no longer an armorsmith, no longer
	 * summoned, or no longer linked to `owner`, it just stops re-scheduling
	 * itself instead of looping forever. Safe to call once at every point a
	 * companion becomes active (same call sites as scheduleCompanionTaxiTick()
	 * in CompanionObjectImplementation.cpp) -- it no-ops every tick for any
	 * companion that isn't actually an armorsmith. */
	static void scheduleGearCheckTick(CompanionObject* companion) {
		if (companion == nullptr) {
			return;
		}

		Reference<CompanionObject*> companionRef = companion;

		Core::getTaskManager()->scheduleTask([companionRef] () {
			CompanionObject* companion = companionRef.get();

			if (companion == nullptr) {
				return;
			}

			Locker locker(companion);

			if (companion->getZone() == nullptr || companion->isDead() || !isArmorsmith(companion)) {
				return; // stop re-arming -- not an active armorsmith anymore
			}

			CreatureObject* owner = companion->getLinkedCreature().get();

			if (owner != nullptr && !companion->isInCombat() && !owner->isInCombat()
					&& companion->getCompanionState() != CompanionObject::PATROL) {
				checkAndExchangeArmor(owner, false);
			}

			scheduleGearCheckTick(companion);
		}, "CompanionGearCheckTickLambda", GEAR_CHECK_TICK_MS);
	}

};

#endif // COMPANIONGEAREXCHANGEMANAGER_H_
