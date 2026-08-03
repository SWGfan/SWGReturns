/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System -- see CompanionMenuComponent.h and NOTES.md.
*/

#include "CompanionMenuComponent.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/objects/tangible/weapon/WeaponObject.h"
#include "server/zone/packets/object/ObjectMenuResponse.h"
#include "server/zone/managers/radial/RadialOptions.h"
#include "server/zone/managers/companion/CompanionSkillTrainer.h"
#include "server/zone/managers/companion/CompanionGearExchangeManager.h"
#include "server/zone/managers/companion/callbacks/CompanionHarvestChoiceSuiCallback.h"
#include "server/zone/managers/companion/callbacks/CompanionCraftPickSuiCallback.h"
#include "server/zone/managers/companion/callbacks/CompanionArmorTypeSuiCallback.h"
#include "server/zone/managers/companion/CompanionFieldStation.h" // Doctor Buff Radial -- craft-then-retry (2026-07-29)
#include "server/zone/managers/objectcontroller/ObjectController.h"
#include "server/zone/ZoneServer.h"
#include "server/zone/objects/creature/buffs/Buff.h"
#include "server/zone/objects/creature/BuffAttribute.h"
#include "server/zone/objects/tangible/pharmaceutical/EnhancePack.h"
#include "server/zone/objects/tangible/pharmaceutical/WoundPack.h" // Heal Wounds Radial (2026-07-29)
#include "server/zone/managers/player/PlayerManager.h"
#include "server/zone/managers/companion/CampDeploymentManager.h" // Entertainer Dance/Watch (2026-07-29)

namespace {
	/**
	 * Companion System (2026-07-27, SECOND PASS -- "the doctor must craft
	 * them" per Nick). Replaces the free-with-cooldown magic buff with REAL
	 * crafted-supply consumption. Scans the doctor's own inventory bag for
	 * real EnhancePack items -- one pack per physical attribute (health/
	 * strength/constitution/action/quickness/stamina), exactly like a real
	 * player's /healenhance (see HealEnhanceCommand.h's findEnhancePack()).
	 * Applies via the SAME PlayerManager::healEnhance() a real player enhance
	 * uses, so the buff CRC ("medical_enhance_<attribute>") and upgrade-only
	 * behavior exactly match real player-cast enhance buffs -- no separate
	 * buff type, no double-stacking. Uses the pack's own crafted
	 * getEffectiveness()/getDuration()/getAbsorption() values directly
	 * rather than EnhancePack::calculatePower() -- that formula is gated on
	 * the ENHANCER's own "private_medical_rating" environmental bonus (city/
	 * building/droid proximity, see EnhancePackImplementation.cpp), which a
	 * companion out in the field would almost always have as 0, silently
	 * zeroing every buff; a field medic applying their own crafted
	 * supplies directly is the intended feature here. Skips (without
	 * consuming) any attribute the recipient is already better-buffed on,
	 * so real supplies aren't wasted on a no-op. Consumes one charge
	 * (decreaseUseCount()) per attribute actually applied -- no matching
	 * pack for a given attribute just means that attribute is skipped
	 * (partial buffs are expected/realistic when supplies run low). Returns
	 * true if at least one attribute was actually enhanced, so callers can
	 * tell the player when the doctor has nothing to work with.
	 */
	bool applyCompanionDoctorBuffFromSupplies(CreatureObject* doctor, CreatureObject* recipient, Vector<int>* missingAttributes = nullptr) {
		// Doctor Buff Radial -- craft-ALL-then-buff, concurrency guard (2026-07-29
		// follow-up): missingAttributes (if supplied) collects EVERY attribute
		// this scan found no EnhancePack for at all -- widened from a single
		// firstMissingAttribute int* (Nick: "the doctor should be giving all 8
		// buffs from that one command" -- every missing pack now gets crafted
		// in sequence, not just the first one found). Default nullptr keeps
		// every pre-existing call site's behavior unchanged.
		if (doctor == nullptr || recipient == nullptr) {
			return false;
		}

		SceneObject* inventory = doctor->getSlottedObject("inventory");

		if (inventory == nullptr) {
			return false;
		}

		ZoneServer* zoneServer = doctor->getZoneServer();

		if (zoneServer == nullptr) {
			return false;
		}

		PlayerManager* playerManager = zoneServer->getPlayerManager();

		if (playerManager == nullptr) {
			return false;
		}

		Locker clocker(recipient, doctor);

		bool appliedAny = false;

		for (int attribute = BuffAttribute::HEALTH; attribute <= BuffAttribute::STAMINA; ++attribute) {
			EnhancePack* pack = nullptr;

			for (int i = 0; i < inventory->getContainerObjectsSize(); ++i) {
				SceneObject* object = inventory->getContainerObject(i);

				if (object == nullptr || !object->isPharmaceuticalObject()) {
					continue;
				}

				PharmaceuticalObject* pharma = cast<PharmaceuticalObject*>(object);

				if (!pharma->isEnhancePack()) {
					continue;
				}

				EnhancePack* candidate = cast<EnhancePack*>(pharma);

				if (candidate->getAttribute() == attribute) {
					pack = candidate;
					break;
				}
			}

			if (pack == nullptr) {
				if (missingAttributes != nullptr && !missingAttributes->contains(attribute)) {
					missingAttributes->add(attribute);
				}

				continue;
			}

			int buffValue = (int) pack->getEffectiveness();

			uint32 buffcrc = (String("medical_enhance_") + BuffAttribute::getName(attribute)).hashCode();

			if (recipient->hasBuff(buffcrc)) {
				Buff* existing = recipient->getBuff(buffcrc);

				if (existing != nullptr && existing->getAttributeModifierValue(attribute) >= buffValue) {
					continue; // already at least this strong -- don't waste real supplies
				}
			}

			playerManager->healEnhance(doctor, recipient, (byte) attribute, buffValue, pack->getDuration(), (int) pack->getAbsorption());

			Locker packLocker(pack);
			pack->decreaseUseCount();

			recipient->sendSystemMessage(doctor->getDisplayedName() + " enhances your " + BuffAttribute::getName(attribute, true) + ".");

			appliedAny = true;
		}

		return appliedAny;
	}

	/**
	 * All of the player's currently summoned, living companions (same
	 * datapad-scan shape as CompanionStayCommand.h's resolveActiveCompanions()
	 * -- duplicated here per this project's per-file-copy convention).
	 */
	void resolveAllActiveCompanionsForBuff(CreatureObject* player, Vector<ManagedReference<CompanionObject*> >& out) {
		if (player == nullptr) {
			return;
		}

		ManagedReference<SceneObject*> datapad = player->getSlottedObject("datapad");

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

			CompanionObject* comp = device->getCompanionObject();

			if (comp == nullptr || comp->getZone() == nullptr) {
				continue;
			}

			if (comp->getLinkedCreature().get() != player) {
				continue;
			}

			out.add(comp);
		}
	}

	/**
	 * Doctor Buff Radial -- craft-then-retry (2026-07-29) -- per Nick: "if a doctor is
	 * asked for buffs and no buffs are available, the doctor should craft
	 * some, then buff." Schematic path for a missing attribute, tiered off
	 * the doctor's own trained skill. This project's companion medic tree
	 * only has two skill boxes (science_medic_novice / science_medic_master
	 * -- confirmed by direct read of CompanionSkillTrainer.cpp, no
	 * apprentice/journeyman boxes exist to pick tier 'b'/'c' for), so this
	 * reuses the SAME binary master-vs-not split
	 * CompanionDialogMenuSuiCallback::companionIsMasterDoctor() already
	 * established for buff power -- duplicated here rather than shared, per
	 * this project's own per-file-copy convention for small
	 * companion-lookup helpers.
	 */
	String enhancePackSchematicFor(CompanionObject* doctor, int attribute) {
		bool masterDoctor = false;

		for (int i = 0; i < doctor->getLearnedSkillCount(); ++i) {
			if (doctor->getLearnedSkill(i) == "science_medic_master") {
				masterDoctor = true;
				break;
			}
		}

		String tier = masterDoctor ? "d" : "a";

		return String("object/draft_schematic/chemistry/medpack_enhance_") + BuffAttribute::getName(attribute) + "_" + tier + ".iff";
	}

	/**
	 * Doctor Buff Radial -- craft-then-retry (2026-07-29) -- owner-vs-companion inventory
	 * gotcha. CompanionCraftingManager::craftItem() always hands the
	 * finished item to the OWNER's inventory (CompanionCraftingManager.cpp
	 * ~line 479, transferObject() into owner->getSlottedObject("inventory")),
	 * never the crafting companion's own bag -- confirmed by direct source
	 * read -- but applyCompanionDoctorBuffFromSupplies() only ever scans the
	 * DOCTOR's own bag. Moves the freshly crafted pack for `attribute` from
	 * the owner's inventory into the doctor's own bag, same
	 * Locker(item, currentOwner) + transferObject() pattern
	 * CompanionFieldStation.h's droid handoff already uses
	 * (deployDroidThenCraft()'s handoff step). Returns false (no crash,
	 * no hang) if the pack can't be found or the doctor's bag is full.
	 */
	bool moveFreshEnhancePackToDoctor(CreatureObject* owner, CompanionObject* doctor, int attribute) {
		if (owner == nullptr || doctor == nullptr) {
			return false;
		}

		SceneObject* ownerInventory = owner->getSlottedObject("inventory");
		SceneObject* doctorInventory = doctor->getSlottedObject("inventory");

		if (ownerInventory == nullptr || doctorInventory == nullptr) {
			return false;
		}

		ManagedReference<EnhancePack*> pack = nullptr;

		for (int i = 0; i < ownerInventory->getContainerObjectsSize(); ++i) {
			SceneObject* object = ownerInventory->getContainerObject(i);

			if (object == nullptr || !object->isPharmaceuticalObject()) {
				continue;
			}

			PharmaceuticalObject* pharma = cast<PharmaceuticalObject*>(object);

			if (!pharma->isEnhancePack()) {
				continue;
			}

			EnhancePack* candidate = cast<EnhancePack*>(pharma);

			if (candidate->getAttribute() == attribute) {
				pack = candidate;
				break;
			}
		}

		if (pack == nullptr) {
			return false;
		}

		Locker packLocker(pack, owner);

		if (!doctorInventory->transferObject(pack, -1, true)) {
			return false;
		}

		doctorInventory->broadcastObject(pack, true);

		return true;
	}

	/**
	 * Doctor Buff Radial -- craft-ALL-then-buff, concurrency guard (2026-07-29
	 * follow-up). Nick's live report: "i spammed the doctor buff" produced
	 * repeating FactoryCrateImplementation::getPrototype / ComponentSlot
	 * "doesn't contain a prototype" errors. Root cause: neither
	 * CompanionFieldStation::begin() nor the old craftMissingPackThenRetryBuff()
	 * had any guard against a second radial click starting a SECOND,
	 * overlapping craft for the SAME doctor while the first was still
	 * mid-flight (materials trade + walk + ~10s craft glow -- real elapsed
	 * time, not synchronous) -- two concurrent craft flows racing to fill
	 * ingredient/component slots out of the same inventory can drain/destroy
	 * a shared FactoryCrate out from under each other's slot-fill attempt.
	 * Tracked here by companion object ID, same Vector<uint64>+
	 * find()/add()/remove(idx) idiom
	 * CompanionCraftTheater::registerActiveCrafter()/unregisterActiveCrafter()
	 * already uses for its own active-crafters table -- a DEDICATED set
	 * rather than reusing that one directly, since it only covers
	 * CompanionCraftTheater's own beginCraftShimmer()->finishCraft() window,
	 * not the earlier station-request/build/handoff phase this flow can also
	 * run through via CompanionFieldStation::begin(). Function-local static,
	 * same header/cpp-local convention as CompanionFieldStation::deployedProps().
	 */
	Vector<uint64>& doctorBuffCraftBusy() {
		static Vector<uint64> ids;
		return ids;
	}

	bool isDoctorBuffCraftBusy(uint64 doctorID) {
		return doctorBuffCraftBusy().find(doctorID) != -1;
	}

	/** Cleared on EVERY exit path of craftMissingPacksThenBuffAll() below --
	 * normal completion, craft failure, pack-move failure, and the
	 * player/doctor going away mid-flight -- so a failed or interrupted
	 * craft can never leave a doctor permanently unable to buff again. */
	void setDoctorBuffCraftBusy(uint64 doctorID, bool busy) {
		Vector<uint64>& ids = doctorBuffCraftBusy();
		int idx = ids.find(doctorID);

		if (busy) {
			if (idx == -1) {
				ids.add(doctorID);
			}
		} else if (idx != -1) {
			ids.remove(idx);
		}
	}

	// Companion System (2026-07-29 hotfix -- forward declaration, see docs/companion_system/NOTES.md): craftMissingPacksThenBuffAll()
	// below calls runDoctorSquadBuffPass() directly in several of its exit
	// paths, but runDoctorSquadBuffPass() itself is defined further down in
	// this same anonymous namespace -- needs a forward declaration or this
	// won't compile ("use of undeclared identifier 'runDoctorSquadBuffPass'").
	bool runDoctorSquadBuffPass(CreatureObject* player, CompanionObject* companion, Vector<int>* missingAttributes);

	/**
	 * Doctor Buff Radial -- craft-ALL-then-buff, concurrency guard (2026-07-29
	 * follow-up). Crafts EVERY missing enhance pack in `missingAttributes`
	 * ONE AT A TIME IN SEQUENCE via CompanionFieldStation::begin() -- the
	 * station-gated craft entry point every other companion craft flow in
	 * this file already goes through (CompanionCraftPickSuiCallback::
	 * performCraft(), CompanionArmorPieceSuiCallback::craftAllPieces()) --
	 * NOT CompanionCraftTheater::begin() directly. Sequential, not parallel,
	 * because CompanionFieldStation::begin() is async/real-time and only one
	 * craft can be in flight at once per the doctorBuffCraftBusy() guard
	 * above. After each craft completes, the fresh pack is moved to the
	 * doctor's own bag (owner-vs-companion inventory gotcha, see
	 * moveFreshEnhancePackToDoctor()) and the next missing attribute starts;
	 * only after the LAST one completes does this run ONE final full buff
	 * pass covering every attribute (not just the ones just crafted --
	 * applyCompanionDoctorBuffFromSupplies()'s own "already at least this
	 * strong, skip" check makes re-checking already-applied attributes a
	 * harmless no-op). Uses the same ManagedReference-capture-and-refetch
	 * pattern CompanionArmorPieceSuiCallback::craftAllPieces() already
	 * established for surviving the real, unlocked time gap a craft takes --
	 * the player/doctor could log off or despawn before it resolves, which
	 * is why the busy flag is cleared by captured doctor OBJECT ID
	 * (doctorID), never by a live pointer that might already be null.
	 * @param missingAttributes passed BY VALUE (copied into the completion
	 * lambda chain) so it survives across the real-time gaps between crafts.
	 * @param squadPass true -> the final buff pass is runDoctorSquadBuffPass();
	 * false -> the single-target applyCompanionDoctorBuffFromSupplies(doctor, player).
	 * @pre companion cross-locked to player on the FIRST call only (same
	 * precondition CompanionFieldStation::begin() documents) -- callers must
	 * have already called setDoctorBuffCraftBusy(doctor->getObjectID(), true)
	 * before the first invocation; this function does not set it itself so
	 * the very first check-and-set at the radial handler stays a single,
	 * unambiguous decision point. */
	void craftMissingPacksThenBuffAll(CreatureObject* player, CompanionObject* doctor, Vector<int> missingAttributes, int index, bool squadPass) {
		if (player == nullptr || doctor == nullptr) {
			if (doctor != nullptr) {
				setDoctorBuffCraftBusy(doctor->getObjectID(), false);
			}

			return;
		}

		uint64 doctorID = doctor->getObjectID();

		if (index >= missingAttributes.size()) {
			// Every missing pack crafted -- clear busy FIRST (so the final buff
			// pass's own buff-application messages are the last thing sent, and
			// so a same-tick re-click during the final pass isn't refused).
			setDoctorBuffCraftBusy(doctorID, false);

			if (squadPass) {
				runDoctorSquadBuffPass(player, doctor, nullptr);
			} else {
				applyCompanionDoctorBuffFromSupplies(doctor, player, nullptr);
			}

			return;
		}

		int attribute = missingAttributes.get(index);
		String schematicPath = enhancePackSchematicFor(doctor, attribute);

		ManagedReference<CreatureObject*> playerRef = player;
		ManagedReference<CompanionObject*> doctorRef = doctor;

		if (index == 0) {
			String packWord = missingAttributes.size() > 1 ? " Enhance Packs" : " Enhance Pack";
			player->sendSystemMessage(doctor->getDisplayedName() + " is missing " + String::valueOf(missingAttributes.size()) + packWord + " -- crafting before buffing...");
		}

		CompanionFieldStation::begin(player, doctor, schematicPath, [playerRef, doctorRef, missingAttributes, index, squadPass, doctorID] (bool craftSuccess) {
			CreatureObject* p = playerRef.get();
			CompanionObject* d = doctorRef.get();

			if (p == nullptr || d == nullptr) {
				// Player logged off or doctor despawned mid-craft -- nobody left
				// to message, but the busy flag MUST still clear by the captured
				// ID (not a live pointer) or this doctor could never buff again.
				setDoctorBuffCraftBusy(doctorID, false);
				return;
			}

			if (!craftSuccess) {
				// CompanionCraftTheater::finishCraft() already told the player
				// exactly why ("...couldn't finish the craft: <reason>") -- just
				// add doctor-buff-specific context, clear the busy flag, and still
				// run one buff pass with whatever supplies already exist rather
				// than leaving the remaining queue stuck.
				p->sendSystemMessage(d->getDisplayedName() + " couldn't finish crafting -- buffing with whatever's on hand.");
				setDoctorBuffCraftBusy(doctorID, false);

				if (squadPass) {
					runDoctorSquadBuffPass(p, d, nullptr);
				} else {
					applyCompanionDoctorBuffFromSupplies(d, p, nullptr);
				}

				return;
			}

			if (!moveFreshEnhancePackToDoctor(p, d, missingAttributes.get(index))) {
				p->sendSystemMessage(d->getDisplayedName() + " crafted an Enhance Pack but couldn't fetch it from your inventory -- check your bag space.");
				setDoctorBuffCraftBusy(doctorID, false);

				if (squadPass) {
					runDoctorSquadBuffPass(p, d, nullptr);
				} else {
					applyCompanionDoctorBuffFromSupplies(d, p, nullptr);
				}

				return;
			}

			craftMissingPacksThenBuffAll(p, d, missingAttributes, index + 1, squadPass);
		});
	}

	/**
	 * Doctor Buff Radial -- craft-ALL-then-buff, concurrency guard (2026-07-29
	 * follow-up) -- shared squad-buff pass, used both by the normal
	 * COMPANION_DOCTOR_BUFF_SQUAD handler and its craft-ALL-then-buff path,
	 * so the exact same buff-everyone-in-the-squad logic only exists once.
	 * missingAttributes (widened from a single firstMissingAttribute int* --
	 * see applyCompanionDoctorBuffFromSupplies()) collects every attribute
	 * missing a pack across the player, the doctor itself, and every other
	 * squad member -- duplicates are naturally deduplicated since they all
	 * scan the SAME doctor inventory (contains() check inside
	 * applyCompanionDoctorBuffFromSupplies()). Pass missingAttributes ==
	 * nullptr on the final pass after every craft has completed -- nothing
	 * left to collect at that point.
	 */
	bool runDoctorSquadBuffPass(CreatureObject* player, CompanionObject* companion, Vector<int>* missingAttributes) {
		bool appliedAny = applyCompanionDoctorBuffFromSupplies(companion, player, missingAttributes);

		// Companion System (2026-07-27, per Nick: "buff the squad ... didn't
		// buff themselves") -- the doctor is part of its own squad too.
		if (applyCompanionDoctorBuffFromSupplies(companion, companion, missingAttributes)) {
			appliedAny = true;
		}

		Vector<ManagedReference<CompanionObject*> > squad;
		resolveAllActiveCompanionsForBuff(player, squad);

		for (int i = 0; i < squad.size(); ++i) {
			CompanionObject* member = squad.get(i);

			if (member == companion) {
				continue; // already buffed explicitly above
			}

			if (applyCompanionDoctorBuffFromSupplies(companion, member, missingAttributes)) {
				appliedAny = true;
			}
		}

		return appliedAny;
	}

	/**
	 * Heal Wounds Radial (2026-07-29, per Nick: "i would also like a medic
	 * to have an option to heal wounds, craft the wound packs if none are
	 * available"). Real wound mechanics confirmed by direct source read
	 * (HealWoundCommand.h / WoundPack.idl / WoundPackImplementation.cpp):
	 * wounds are a SEPARATE mechanic from HP/action/mind damage -- they
	 * reduce max HAM pools and are only healed by a real WoundPack item
	 * (never a StimPack), one CreatureAttribute pool at a time
	 * (HealWoundCommand::findAttribute()/doQueueCommand() only ever
	 * resolves and heals ONE wounded pool per command call, never all of
	 * them in a single call -- so this radial loops over every woundable
	 * pool itself, same as a real player repeatedly /healwound-ing each
	 * wound in turn). This codebase only ships WoundPack schematics for 6
	 * of the 9 CreatureAttribute pools -- health/strength/constitution/
	 * action/quickness/stamina (confirmed by directory listing of
	 * bin/scripts/object/draft_schematic/chemistry/medpack_wound_*.lua --
	 * no _mind/_focus/_willpower variant exists), so those 3 pools are out
	 * of scope here, same as they are for a real player (no wound pack
	 * exists for them at all; matches the exact HEALTH..STAMINA loop bound
	 * applyCompanionDoctorBuffFromSupplies() above already uses for
	 * EnhancePack). Scans the medic's own inventory for a real WoundPack
	 * per wounded pool, same isPharmaceuticalObject()/isWoundPack()/
	 * getMedicineUseRequired() <= getSkillMod("healing_ability") match
	 * HealWoundCommand::findWoundPack() uses for a real player. Skips any
	 * pool with zero wounds so real supplies aren't wasted on a no-op
	 * (creatureTarget->getWounds(attribute) == 0 check, same shape as the
	 * enhance-buff "already at least this strong, skip" check above).
	 * Applies via WoundPack::calculatePower() + CreatureObject::healWound()
	 * directly -- the same two calls HealWoundCommand::doQueueCommand()
	 * itself makes -- rather than routing through the full player command
	 * (which also gates on combat/LOS/mind-cost/private_medical_rating
	 * checks that don't apply to a companion-triggered action), same
	 * direct-application precedent applyCompanionDoctorBuffFromSupplies()
	 * already established for HealEnhanceCommand vs
	 * PlayerManager::healEnhance()). Returns true if at least one pool was
	 * actually healed.
	 */
	bool applyCompanionWoundHealFromSupplies(CreatureObject* medic, CreatureObject* recipient, Vector<int>* missingAttributes = nullptr) {
		if (medic == nullptr || recipient == nullptr) {
			return false;
		}

		SceneObject* inventory = medic->getSlottedObject("inventory");

		if (inventory == nullptr) {
			return false;
		}

		Locker clocker(recipient, medic);

		bool appliedAny = false;

		int medicineUse = medic->getSkillMod("healing_ability");

		for (int attribute = CreatureAttribute::HEALTH; attribute <= CreatureAttribute::STAMINA; ++attribute) {
			if (recipient->getWounds(attribute) == 0) {
				continue; // nothing wounded on this pool -- don't waste supplies
			}

			WoundPack* pack = nullptr;

			for (int i = 0; i < inventory->getContainerObjectsSize(); ++i) {
				SceneObject* object = inventory->getContainerObject(i);

				if (object == nullptr || !object->isPharmaceuticalObject()) {
					continue;
				}

				PharmaceuticalObject* pharma = cast<PharmaceuticalObject*>(object);

				if (!pharma->isWoundPack()) {
					continue;
				}

				WoundPack* candidate = cast<WoundPack*>(pharma);

				if (candidate->getAttribute() == attribute && candidate->getMedicineUseRequired() <= medicineUse) {
					pack = candidate;
					break;
				}
			}

			if (pack == nullptr) {
				if (missingAttributes != nullptr && !missingAttributes->contains(attribute)) {
					missingAttributes->add(attribute);
				}

				continue;
			}

			Locker packLocker(pack);

			uint32 woundPower = pack->calculatePower(medic, recipient);

			int woundHealed = recipient->healWound(medic, attribute, woundPower);

			pack->decreaseUseCount();

			recipient->sendSystemMessage(medic->getDisplayedName() + " heals " + String::valueOf(abs(woundHealed)) + " " + CreatureAttribute::getName(attribute) + " wound damage.");

			appliedAny = true;
		}

		return appliedAny;
	}

	/**
	 * Heal Wounds Radial (2026-07-29) -- schematic path for a missing
	 * WoundPack, tiered off the medic's own trained skill. Same binary
	 * science_medic_novice/science_medic_master split
	 * enhancePackSchematicFor() above already established for this
	 * project's 2-box medic tree (no apprentice/journeyman boxes to pick a
	 * middle tier for). WoundPack schematics ship FIVE tiers (a-e, one more
	 * than EnhancePack's a-d ceiling -- confirmed by direct directory read
	 * of bin/scripts/object/draft_schematic/chemistry/medpack_wound_*.lua),
	 * so master tier here is "e" (the real best tier that exists for wound
	 * packs), not "d" -- don't blindly copy enhancePackSchematicFor()'s
	 * literal tier letter, its "d" was EnhancePack's own ceiling, not a
	 * fixed project convention.
	 */
	String woundPackSchematicFor(CompanionObject* medic, int attribute) {
		bool masterMedic = false;

		for (int i = 0; i < medic->getLearnedSkillCount(); ++i) {
			if (medic->getLearnedSkill(i) == "science_medic_master") {
				masterMedic = true;
				break;
			}
		}

		String tier = masterMedic ? "e" : "a";

		return String("object/draft_schematic/chemistry/medpack_wound_") + CreatureAttribute::getName(attribute) + "_" + tier + ".iff";
	}

	/**
	 * Heal Wounds Radial (2026-07-29) -- same owner-vs-companion inventory
	 * gotcha as moveFreshEnhancePackToDoctor() above (CompanionCraftingManager::
	 * craftItem() always lands a freshly crafted item in the OWNER's
	 * inventory, never the crafting companion's own bag). Duplicated
	 * rather than generalized, per this project's own per-file-copy
	 * convention for small companion-lookup helpers (see
	 * enhancePackSchematicFor()'s comment above).
	 */
	bool moveFreshWoundPackToMedic(CreatureObject* owner, CompanionObject* medic, int attribute) {
		if (owner == nullptr || medic == nullptr) {
			return false;
		}

		SceneObject* ownerInventory = owner->getSlottedObject("inventory");
		SceneObject* medicInventory = medic->getSlottedObject("inventory");

		if (ownerInventory == nullptr || medicInventory == nullptr) {
			return false;
		}

		ManagedReference<WoundPack*> pack = nullptr;

		for (int i = 0; i < ownerInventory->getContainerObjectsSize(); ++i) {
			SceneObject* object = ownerInventory->getContainerObject(i);

			if (object == nullptr || !object->isPharmaceuticalObject()) {
				continue;
			}

			PharmaceuticalObject* pharma = cast<PharmaceuticalObject*>(object);

			if (!pharma->isWoundPack()) {
				continue;
			}

			WoundPack* candidate = cast<WoundPack*>(pharma);

			if (candidate->getAttribute() == attribute) {
				pack = candidate;
				break;
			}
		}

		if (pack == nullptr) {
			return false;
		}

		Locker packLocker(pack, owner);

		if (!medicInventory->transferObject(pack, -1, true)) {
			return false;
		}

		medicInventory->broadcastObject(pack, true);

		return true;
	}

	/**
	 * Heal Wounds Radial (2026-07-29) -- concurrency guard, same
	 * Vector<uint64>+find()/add()/remove(idx) idiom doctorBuffCraftBusy()
	 * above already uses (see that function's comment for the live-bug
	 * root cause it fixes -- two overlapping CompanionFieldStation::begin()
	 * craft flows on the SAME companion racing to fill slots out of the
	 * same inventory/FactoryCrate). A SEPARATE tracking set from
	 * doctorBuffCraftBusy() (own Vector, own function-local static), since
	 * this is a distinct craft-then-heal flow -- but both radial handlers
	 * below cross-check BOTH busy sets (isWoundHealCraftBusy() OR
	 * isDoctorBuffCraftBusy()) before starting a new craft chain, so a
	 * single medic-trained companion can never run two overlapping station-
	 * gated crafts at once regardless of which feature started first --
	 * this is the exact race class doctorBuffCraftBusy() was added to fix,
	 * and it applies identically here since Heal Wounds crafts through the
	 * same station-gated entry point on the same companion.
	 */
	Vector<uint64>& woundHealCraftBusy() {
		static Vector<uint64> ids;
		return ids;
	}

	bool isWoundHealCraftBusy(uint64 medicID) {
		return woundHealCraftBusy().find(medicID) != -1;
	}

	void setWoundHealCraftBusy(uint64 medicID, bool busy) {
		Vector<uint64>& ids = woundHealCraftBusy();
		int idx = ids.find(medicID);

		if (busy) {
			if (idx == -1) {
				ids.add(medicID);
			}
		} else if (idx != -1) {
			ids.remove(idx);
		}
	}

	// Heal Wounds Radial (2026-07-29) -- forward declaration, same reason as
	// runDoctorSquadBuffPass()'s forward declaration above:
	// craftMissingWoundPacksThenHealAll() below calls
	// runMedicSquadWoundHealPass() directly in several exit paths, but it's
	// defined further down in this same anonymous namespace.
	bool runMedicSquadWoundHealPass(CreatureObject* player, CompanionObject* medic, Vector<int>* missingAttributes);

	// Medic Stim Heal Radial (2026-07-29 night #3) -- own busy-guard, same
	// Vector<uint64>+find()/add()/remove(idx) idiom as doctorBuffCraftBusy()/
	// woundHealCraftBusy() above. Cross-checked against BOTH of those (and
	// vice versa isn't needed -- this feature crafts only a single plain
	// stim schematic per click, so it never re-enters itself) since all
	// three features craft through CompanionFieldStation::begin() for the
	// SAME companion and can race the same FactoryCrate/inventory state.
	Vector<uint64>& stimHealCraftBusy() {
		static Vector<uint64> ids;
		return ids;
	}

	bool isStimHealCraftBusy(uint64 medicID) {
		return stimHealCraftBusy().find(medicID) != -1;
	}

	void setStimHealCraftBusy(uint64 medicID, bool busy) {
		Vector<uint64>& ids = stimHealCraftBusy();
		int idx = ids.find(medicID);

		if (busy) {
			if (idx == -1) {
				ids.add(medicID);
			}
		} else if (idx != -1) {
			ids.remove(idx);
		}
	}

	// Medic Stim Heal Radial (2026-07-29 night #3) -- craft ONE missing
	// stim (single schematic path, no per-attribute loop -- stims don't
	// have attribute variants like EnhancePacks/WoundPacks do) then heal
	// the target. Async via CompanionFieldStation::begin(), same shape as
	// craftMissingPacksThenBuffAll()/craftMissingWoundPacksThenHealAll()
	// above. @pre { player and medic both locked (matches those two
	// functions' own documented precondition) }
	void craftStimThenHeal(CreatureObject* player, CompanionObject* medic, CreatureObject* target, bool squadPass) {
		String schematicPath = medic->getBestCraftableStimSchematicPath();

		if (schematicPath.isEmpty()) {
			player->sendSystemMessage(medic->getDisplayedName() + " doesn't know how to make any stims.");
			setStimHealCraftBusy(medic->getObjectID(), false);
			return;
		}

		ManagedReference<CreatureObject*> playerRef = player;
		ManagedReference<CompanionObject*> medicRef = medic;
		ManagedReference<CreatureObject*> targetRef = target;
		uint64 medicID = medic->getObjectID();

		CompanionFieldStation::begin(player, medic, schematicPath, [playerRef, medicRef, targetRef, squadPass, medicID] (bool craftSuccess) {
			CreatureObject* p = playerRef.get();
			CompanionObject* d = medicRef.get();
			CreatureObject* t = targetRef.get();

			if (p == nullptr || d == nullptr) {
				return;
			}

			Locker clocker(d, p);

			if (!craftSuccess) {
				p->sendSystemMessage(d->getDisplayedName() + " couldn't finish crafting a stim.");
				setStimHealCraftBusy(medicID, false);
				return;
			}

			if (t != nullptr) {
				d->applyStimHealTo(t);
			}

			if (squadPass) {
				Vector<ManagedReference<CompanionObject*> > squad;
				resolveAllActiveCompanionsForBuff(p, squad);

				for (int i = 0; i < squad.size(); ++i) {
					CompanionObject* member = squad.get(i);

					if (member != d) {
						d->applyStimHealTo(member);
					}
				}
			}

			setStimHealCraftBusy(medicID, false);
		});
	}

	/**
	 * Heal Wounds Radial (2026-07-29) -- crafts every missing WoundPack in
	 * `missingAttributes` ONE AT A TIME IN SEQUENCE via
	 * CompanionFieldStation::begin(), same shape as
	 * craftMissingPacksThenBuffAll() above (see that function's comment for
	 * the full real-time/sequencing rationale -- identical here, just
	 * WoundPack instead of EnhancePack and a final heal pass instead of a
	 * buff pass).
	 * @pre callers must have already called
	 * setWoundHealCraftBusy(medic->getObjectID(), true) before the first
	 * invocation, same precondition craftMissingPacksThenBuffAll() documents.
	 */
	void craftMissingWoundPacksThenHealAll(CreatureObject* player, CompanionObject* medic, Vector<int> missingAttributes, int index, bool squadPass) {
		if (player == nullptr || medic == nullptr) {
			if (medic != nullptr) {
				setWoundHealCraftBusy(medic->getObjectID(), false);
			}

			return;
		}

		uint64 medicID = medic->getObjectID();

		if (index >= missingAttributes.size()) {
			setWoundHealCraftBusy(medicID, false);

			if (squadPass) {
				runMedicSquadWoundHealPass(player, medic, nullptr);
			} else {
				applyCompanionWoundHealFromSupplies(medic, player, nullptr);
			}

			return;
		}

		int attribute = missingAttributes.get(index);
		String schematicPath = woundPackSchematicFor(medic, attribute);

		ManagedReference<CreatureObject*> playerRef = player;
		ManagedReference<CompanionObject*> medicRef = medic;

		if (index == 0) {
			String packWord = missingAttributes.size() > 1 ? " Wound Packs" : " Wound Pack";
			player->sendSystemMessage(medic->getDisplayedName() + " is missing " + String::valueOf(missingAttributes.size()) + packWord + " -- crafting before healing...");
		}

		CompanionFieldStation::begin(player, medic, schematicPath, [playerRef, medicRef, missingAttributes, index, squadPass, medicID] (bool craftSuccess) {
			CreatureObject* p = playerRef.get();
			CompanionObject* d = medicRef.get();

			if (p == nullptr || d == nullptr) {
				setWoundHealCraftBusy(medicID, false);
				return;
			}

			if (!craftSuccess) {
				p->sendSystemMessage(d->getDisplayedName() + " couldn't finish crafting -- healing with whatever's on hand.");
				setWoundHealCraftBusy(medicID, false);

				if (squadPass) {
					runMedicSquadWoundHealPass(p, d, nullptr);
				} else {
					applyCompanionWoundHealFromSupplies(d, p, nullptr);
				}

				return;
			}

			if (!moveFreshWoundPackToMedic(p, d, missingAttributes.get(index))) {
				p->sendSystemMessage(d->getDisplayedName() + " crafted a Wound Pack but couldn't fetch it from your inventory -- check your bag space.");
				setWoundHealCraftBusy(medicID, false);

				if (squadPass) {
					runMedicSquadWoundHealPass(p, d, nullptr);
				} else {
					applyCompanionWoundHealFromSupplies(d, p, nullptr);
				}

				return;
			}

			craftMissingWoundPacksThenHealAll(p, d, missingAttributes, index + 1, squadPass);
		});
	}

	/**
	 * Heal Wounds Radial (2026-07-29) -- shared squad-heal pass, used both
	 * by the normal COMPANION_HEAL_WOUNDS_SQUAD handler and its craft-ALL-
	 * then-heal path, same shape as runDoctorSquadBuffPass() above: heals
	 * the player, the medic itself, and every other active companion in
	 * the squad.
	 */
	bool runMedicSquadWoundHealPass(CreatureObject* player, CompanionObject* medic, Vector<int>* missingAttributes) {
		bool appliedAny = applyCompanionWoundHealFromSupplies(medic, player, missingAttributes);

		if (applyCompanionWoundHealFromSupplies(medic, medic, missingAttributes)) {
			appliedAny = true;
		}

		Vector<ManagedReference<CompanionObject*> > squad;
		resolveAllActiveCompanionsForBuff(player, squad);

		for (int i = 0; i < squad.size(); ++i) {
			CompanionObject* member = squad.get(i);

			if (member == medic) {
				continue; // already healed explicitly above
			}

			if (applyCompanionWoundHealFromSupplies(medic, member, missingAttributes)) {
				appliedAny = true;
			}
		}

		return appliedAny;
	}

}

void CompanionMenuComponent::fillObjectMenuResponse(SceneObject* sceneObject, ObjectMenuResponse* menuResponse, CreatureObject* player) const {
	if (!sceneObject->isCompanionObject()) {
		return;
	}

	CompanionObject* companion = cast<CompanionObject*>(sceneObject);

	if (companion == nullptr || player == nullptr) {
		return;
	}

	bool isOwner = companion->isAuthorizedActor(player);

	if (isOwner) {
		// Radial reorganization (2026-07-18, per user request "clean up the
		// radial, it's getting cluttered -- sub categories, similar options
		// together"): three top-level entries instead of five --
		//   Talk to Companion (or Revive)      <- most used, stays on top
		//     +- Companion Skill Sheet
		//   Storage & Equipment (click = open storage bag, the most common
		//     action, so the parent still DOES something)
		//     +- View Equipment
		//     +- Retrieve Gear
		//   Harvesting: <current choice>       <- ranger-trained only
		//     +- Harvest Meat / Hide / Bone
		// All the original handler IDs are unchanged -- only the visual
		// grouping moved. Plain-text labels, same no-new-STF rationale as
		// before (see NOTES.md).
		if (companion->isIncapacitated()) {
			menuResponse->addRadialMenuItem(RadialOptions::SERVER_MENU4, 3, "@companion:menu_revive"); // Revive (incapacitation recovery)
		} else {
			menuResponse->addRadialMenuItem(RadialOptions::SERVER_MENU4, 3, "@companion:menu_talk"); // Talk to Companion
		}

		menuResponse->addRadialMenuItemToRadialID(RadialOptions::SERVER_MENU4, RadialOptions::SERVER_MENU2, 3, "@companion:menu_skill_sheet"); // Companion Skill Sheet

		menuResponse->addRadialMenuItem(RadialOptions::SERVER_MENU1, 3, "Storage & Equipment"); // click = open storage bag
		menuResponse->addRadialMenuItemToRadialID(RadialOptions::SERVER_MENU1, RadialOptions::SERVER_MENU6, 3, "View Equipment");
		menuResponse->addRadialMenuItemToRadialID(RadialOptions::SERVER_MENU1, RadialOptions::SERVER_MENU5, 3, "Retrieve Gear");

		// Ranger auto-harvest preference (2026-07-18 -- see NOTES.md and
		// CompanionHarvestChoiceSuiCallback.h). Only shown on companions
		// with ranger/scout training; parent label shows the current
		// choice, clicking the parent re-opens the full choice box.
		bool rangerTrained = false;

		for (int i = 0; i < companion->getLearnedSkillCount(); ++i) {
			const String& skill = companion->getLearnedSkill(i);

			if (skill.beginsWith("outdoors_ranger_") || skill.beginsWith("outdoors_scout_")) {
				rangerTrained = true;
				break;
			}
		}

		if (rangerTrained) {
			int preference = companion->getHarvestPreference();
			String currentLabel = preference == 234 ? "Meat" : (preference == 235 ? "Hide" : (preference == 236 ? "Bone" : "Choose..."));

			menuResponse->addRadialMenuItem(RadialOptions::SERVER_MENU10, 3, "Harvesting: " + currentLabel);
			menuResponse->addRadialMenuItemToRadialID(RadialOptions::SERVER_MENU10, RadialOptions::SERVER_MENU7, 3, "Harvest Meat");
			menuResponse->addRadialMenuItemToRadialID(RadialOptions::SERVER_MENU10, RadialOptions::SERVER_MENU8, 3, "Harvest Hide");
			menuResponse->addRadialMenuItemToRadialID(RadialOptions::SERVER_MENU10, RadialOptions::SERVER_MENU9, 3, "Harvest Bone");
		}

		// Artisan craft picker (2026-07-20, per user request "artisan
		// companions get a radial option to pick items to craft" -- see
		// CompanionCraftPickSuiCallback.h). SERVER_MENU3 is free in the
		// owner branch (only the non-owner branch uses it, for Inspect).
		bool artisanTrained = false;

		for (int i = 0; i < companion->getLearnedSkillCount(); ++i) {
			const String& artisanCheckSkill = companion->getLearnedSkill(i);

			// Companion System (2026-07-27, "craft the doctor buffs" per Nick):
			// medic-trained companions can craft too (real enhance packs/stims),
			// same flow as artisans -- CompanionCraftPickSuiCallback::
			// sendCraftList() already generically scans whatever schematics the
			// companion's learned skills actually grant, it was never hardcoded
			// to crafting_ specifically. This is just the button's show/hide gate.
			if (artisanCheckSkill.beginsWith("crafting_") || artisanCheckSkill.beginsWith("science_medic_")) {
				artisanTrained = true;
				break;
			}
		}

		if (artisanTrained) {
			menuResponse->addRadialMenuItem(RadialOptions::SERVER_MENU3, 3, "Craft: Choose Item...");

			// Armorsmith "Request Full Suit..." (2026-07-23, per user
			// request "there should be a radial option, make it a sub
			// radial option under the craft item radial") -- child of the
			// same SERVER_MENU3 parent, only shown when this companion is
			// actually an armorsmith (see
			// CompanionGearExchangeManager::isArmorsmith()). Opens the same
			// picker /companionrequestarmor does
			// (CompanionArmorTypeSuiCallback::sendTypeList()).
			if (CompanionGearExchangeManager::isArmorsmith(companion)) {
				menuResponse->addRadialMenuItemToRadialID(RadialOptions::SERVER_MENU3, RadialOptions::COMPANION_REQUEST_ARMOR, 3, "Request Full Suit...");
			}

			// "Get Test Resources..." (2026-07-24, per user request "can I
			// have a radial that requests the best resources for a certain
			// item, so I can test what the companions are using -- place a
			// bag in the user's inventory and fill it"). Same SERVER_MENU3
			// craft-radial group as above -- shown for any artisan-trained
			// companion, not just armorsmiths, since it's a generic
			// resource-quality comparison tool for whatever this companion
			// knows how to craft. Opens CompanionCraftPickSuiCallback::
			// sendTestResourceList(), which reuses the exact same known-
			// schematic list as "Craft: Choose Item..." but fills a test
			// bag instead of crafting (CompanionCraftingManager::
			// giveTestResourceBag()).
			//
			// Admin-only gate (2026-07-29, per Nick: "we need to make it so
			// that only admins can see this option, 'Get Test Resources...'").
			// Reuses the exact same privileged-account idiom already established
			// in this codebase at CompanionObjectImplementation::
			// isAuthorizedActor() ("ManagedReference<PlayerObject*> ghost =
			// requester->getPlayerObject(); if (ghost != nullptr && ghost->
			// isPrivileged())"). `player` here is the human interacting with the
			// companion (fillObjectMenuResponse's own parameter, NOT the
			// companion's own ghost, which per Iron Rule 16 is always null) so
			// this is a real, non-null ghost lookup. Only gates whether the
			// radial button is OFFERED -- the underlying
			// CompanionCraftPickSuiCallback::sendTestResourceList()/
			// CompanionCraftingManager::giveTestResourceBag() logic behind it is
			// untouched; a non-privileged owner's radial simply has one fewer
			// entry.
			ManagedReference<PlayerObject*> testResourcesGhost = player->getPlayerObject();

			if (testResourcesGhost != nullptr && testResourcesGhost->isPrivileged()) {
				menuResponse->addRadialMenuItemToRadialID(RadialOptions::SERVER_MENU3, RadialOptions::COMPANION_TEST_RESOURCES, 3, "Get Test Resources...");
			}

			// "Craft: Factory Run..." (2026-07-27, per Nick: "we need to make it
			// so we can make factory runs of items"). Same SERVER_MENU3 group,
			// same known-schematic list as "Craft: Choose Item..." -- picking an
			// item here asks for a quantity, then crafts ONE real item and
			// clones it into a real FactoryCrate (CompanionCraftingManager::
			// craftBatch()) instead of crafting one at a time.
			menuResponse->addRadialMenuItemToRadialID(RadialOptions::SERVER_MENU3, RadialOptions::COMPANION_CRAFT_BATCH, 3, "Craft: Factory Run...");
		}

		// Doctor Buff Radial (2026-07-27, moved to the right-click radial to
		// match the Craft/artisan precedent directly above -- a direct,
		// discoverable radial entry instead of being buried inside the "Talk
		// to Companion" sub-dialog).
		bool doctorTrained = false;

		for (int i = 0; i < companion->getLearnedSkillCount(); ++i) {
			if (companion->getLearnedSkill(i).beginsWith("science_medic_")) {
				doctorTrained = true;
				break;
			}
		}

		if (doctorTrained) {
			menuResponse->addRadialMenuItem(RadialOptions::COMPANION_DOCTOR_BUFF_ME, 3, "Medical: Buff Me");
			menuResponse->addRadialMenuItemToRadialID(RadialOptions::COMPANION_DOCTOR_BUFF_ME, RadialOptions::COMPANION_DOCTOR_BUFF_SQUAD, 3, "Medical: Buff The Squad");

			// Heal Wounds Radial (2026-07-29, per Nick: "i would also like a
			// medic to have an option to heal wounds, craft the wound packs
			// if none are available") -- same ME/SQUAD split as the Doctor
			// Buff Radial immediately above, same doctorTrained gate (wounds
			// are healed by the SAME science_medic_ skill line as buffing --
			// see HealWoundCommand.h -- no separate skill check needed).
			menuResponse->addRadialMenuItem(RadialOptions::COMPANION_HEAL_WOUNDS_ME, 3, "Medical: Heal Wounds");
			menuResponse->addRadialMenuItemToRadialID(RadialOptions::COMPANION_HEAL_WOUNDS_ME, RadialOptions::COMPANION_HEAL_WOUNDS_SQUAD, 3, "Medical: Heal The Squad's Wounds");

			// Medic Stim Heal Radial (2026-07-29 night #3, per Nick: "give the
			// medics radial a way to heal with stims for me, and for the whole
			// group") -- manual, on-demand HAM-damage healing via StimPack,
			// same doctorTrained gate (this project already treats Medic and
			// Doctor as the same science_medic_ skill line for radial-gating
			// purposes -- see Doctor Buff Radial / Heal Wounds Radial above).
			menuResponse->addRadialMenuItem(RadialOptions::COMPANION_STIM_HEAL_ME, 3, "Medical: Heal Me (Stims)");
			menuResponse->addRadialMenuItemToRadialID(RadialOptions::COMPANION_STIM_HEAL_ME, RadialOptions::COMPANION_STIM_HEAL_SQUAD, 3, "Medical: Heal The Squad (Stims)");
		}

		// Entertainer Dance/Watch (2026-07-29, per Nick's dance-radial
		// request) -- gated to Entertainer-trained companions only, same
		// beginsWith()-prefix shape as doctorTrained above. Shows "Dance"
		// while idle, or "Stop Dance" while THIS companion is the one
		// currently dancing for this owner; hides "Dance" entirely while a
		// DIFFERENT entertainer is already dancing for this owner (avoids
		// offering a second session that would just be rejected -- see
		// CampDeploymentManager::startEntertainerDanceWatch()).
		bool entertainerTrained = false;

		for (int i = 0; i < companion->getLearnedSkillCount(); ++i) {
			if (companion->getLearnedSkill(i).beginsWith("social_entertainer_")) {
				entertainerTrained = true;
				break;
			}
		}

		if (entertainerTrained) {
			if (CampDeploymentManager::instance()->isEntertainerDancing(player, companion)) {
				menuResponse->addRadialMenuItem(RadialOptions::COMPANION_STOP_DANCE, 3, "Stop Dance");
			} else if (!CampDeploymentManager::instance()->hasActiveDanceSession(player)) {
				menuResponse->addRadialMenuItem(RadialOptions::COMPANION_DANCE, 3, "Dance");
			}
		}

		// Companion System (2026-07-29, "Play Music" -- per Nick, "we need
		// a musician as well", the Musician-trained analog of Entertainer
		// Dance/Watch above). Gated on social_musician_ (this project's
		// real, separate Musician skill tree -- confirmed via
		// CompanionSkillTrainer.cpp's ALL_REAL_PROFESSION_NOVICES list,
		// NOT the same social_entertainer_ prefix Dance uses above, since
		// Dancer/Entertainer/Musician are three separate real professions
		// in this server's skills.iff). Shares the SAME one-performance-
		// per-owner session slot as Dance (see CampDeploymentManager.h) --
		// starting Music while a Dance is active for this owner (or vice
		// versa) is refused with the same "already performing" message
		// shape.
		bool musicianTrained = false;

		for (int i = 0; i < companion->getLearnedSkillCount(); ++i) {
			if (companion->getLearnedSkill(i).beginsWith("social_musician_")) {
				musicianTrained = true;
				break;
			}
		}

		if (musicianTrained) {
			if (CampDeploymentManager::instance()->isEntertainerPlayingMusic(player, companion)) {
				menuResponse->addRadialMenuItem(RadialOptions::COMPANION_STOP_MUSIC, 3, "Stop Music");
			} else if (!CampDeploymentManager::instance()->hasActiveDanceSession(player)) {
				menuResponse->addRadialMenuItem(RadialOptions::COMPANION_PLAY_MUSIC, 3, "Play Music");
			}
		}
	} else {
		// Anyone else may publicly inspect equipment/skills (spec 2B).
		menuResponse->addRadialMenuItem(RadialOptions::SERVER_MENU3, 3, "@companion:menu_inspect"); // Inspect Companion
	}
}

int CompanionMenuComponent::handleObjectMenuSelect(SceneObject* sceneObject, CreatureObject* player, byte selectedID) const {
	if (!sceneObject->isCompanionObject() || player == nullptr) {
		return 0;
	}

	CompanionObject* companion = cast<CompanionObject*>(sceneObject);

	if (companion == nullptr) {
		return 0;
	}

	bool isOwner = companion->isAuthorizedActor(player);

	switch (selectedID) {
	case RadialOptions::SERVER_MENU1: { // Open Companion Inventory (storage bag)
		if (!isOwner) {
			return 0;
		}

		// Companion System fix (2026-07-14, "You can not loot that" on item
		// removal -- see NOTES.md): this used to call
		// companion->openContainerTo(player), opening the container window on
		// the companion CREATURE itself. The client treats dragging anything
		// out of a live creature's container as a LOOT attempt and blocks it
		// entirely client-side with its canned "You can not loot that."
		// message -- the request never reaches the server, which is why every
		// verified server-side permission fix appeared to change nothing
		// in-game. The real droid item-storage module
		// (DroidItemStorageModuleDataComponent.cpp, proven working in stock
		// Core3 with these same creature-inventory client templates) opens
		// the BAG object instead -- a plain tangible container, so the client
		// treats drag-out as an ordinary container transfer and lets the
		// server's checkContainerPermission() decide.
		ManagedReference<SceneObject*> bag = companion->getSlottedObject("inventory");

		if (bag != nullptr) {
			bag->openContainerTo(player);
		}

		break;
	}

	case RadialOptions::SERVER_MENU2: // Companion Skill Sheet -- swapped
		// (2026-07-30, per Nick: "change that to the companion profession
		// skill window") to open the real trainable skill/profession tree
		// instead of the old read-only stats sheet. See file header for
		// the sendSkillSheet() vs. sendSkillTree() distinction.
		if (!isOwner) {
			return 0;
		}

		CompanionSkillTrainer::instance()->sendSkillTree(player, companion);
		break;

	case RadialOptions::SERVER_MENU3: // Owner: artisan craft picker; non-owner: Inspect (public)
		if (isOwner) {
			CompanionCraftPickSuiCallback::sendCraftList(player, companion);
		} else {
			CompanionSkillTrainer::instance()->sendInspectionSheet(player, companion);
		}
		break;

	case RadialOptions::SERVER_MENU4: { // Revive, or Talk to Companion (mutually exclusive by state)
		if (!isOwner) {
			return 0;
		}

		if (companion->isIncapacitated()) {
			ManagedReference<CompanionControlDevice*> device = companion->getCompanionControlDevice();

			if (device != nullptr) {
				Locker clocker(device, player);
				device->reviveCompanion();
			}
		} else {
			CompanionSkillTrainer::instance()->sendDialogMenu(player, companion);
		}

		break;
	}

	case RadialOptions::SERVER_MENU5: { // Retrieve Gear (unequip everything onto the owner)
		if (!isOwner) {
			return 0;
		}

		// Companion System (2026-07-14, "can't remove equipped gear" fix --
		// see NOTES.md and the SERVER_MENU1 comment above): moves every worn
		// weapon/wearable off the companion into the owner's main inventory.
		//
		// Rewrite (2026-07-14, live-test follow-up): this used to run its own
		// inline canAddObject()/transferObject()/client-resync loop, which
		// left items invisible in the player's inventory until relog even
		// after the resync was added -- while the per-item "Pick Up" radial
		// (unequipItemToInventory(), which runs each transfer in its own
		// deferred, companion-locked task) worked perfectly with the exact
		// same resync code. Rather than chase the packet-ordering difference
		// between the inline radial-handler context and the task context,
		// this now simply collects the worn gear and hands each item to that
		// proven path -- Retrieve Gear IS "Pick Up everything" now, one
		// battle-tested code path instead of two divergent copies. Inventory
		// space, current-weapon clearing, client resync, and per-item error
		// messages are all handled inside unequipItemToInventory() already.

		// Collect unique worn gear first -- multi-slot items (rifles,
		// two-handers, some armor) appear in several slots. Skip the storage
		// bag and the creature's built-in default weapon.
		SceneObject* bag = companion->getSlottedObject("inventory");
		// genesis port: CreatureObject::getDefaultWeapon() does not exist on this
		// base -- the innate weapon lives in the "default_weapon" slot (same idiom
		// as CompanionObjectImplementation.cpp's refreshCombatAttacks()). Only used
		// for pointer identity against getSlottedObject(i) below, so SceneObject*
		// is sufficient.
		SceneObject* defaultWeapon = companion->getSlottedObject("default_weapon");

		SortedVector<ManagedReference<SceneObject*> > gear;
		gear.setNoDuplicateInsertPlan();

		for (int i = 0; i < companion->getSlottedObjectsSize(); ++i) {
			SceneObject* slotted = companion->getSlottedObject(i);

			if (slotted == nullptr || slotted == bag || slotted == defaultWeapon) {
				continue;
			}

			if (!slotted->isWeaponObject() && !slotted->isWearableObject()) {
				continue;
			}

			gear.put(slotted);
		}

		if (gear.size() == 0) {
			player->sendSystemMessage("Your companion has no removable gear equipped.");
			break;
		}

		for (int i = 0; i < gear.size(); ++i) {
			TangibleObject* item = gear.get(i)->asTangibleObject();

			if (item != nullptr) {
				companion->unequipItemToInventory(item, player);
			}
		}

		break;
	}

	case RadialOptions::SERVER_MENU6: { // View Equipment (creature container window)
		if (!isOwner) {
			return 0;
		}

		// See fillObjectMenuResponse() comment: view-only window for EQUIPPED
		// items (client blocks dragging from a live creature, but the
		// per-item "Pick Up" radial inside it works and routes through
		// unequipItemToInventory()).
		companion->openContainerTo(player);
		break;
	}

	// Ranger auto-harvest preference (2026-07-18 -- see NOTES.md).
	case RadialOptions::SERVER_MENU7: // Harvest Meat
	case RadialOptions::SERVER_MENU8: // Harvest Hide
	case RadialOptions::SERVER_MENU9: { // Harvest Bone
		if (!isOwner) {
			return 0;
		}

		int preference = selectedID == RadialOptions::SERVER_MENU7 ? 234 : (selectedID == RadialOptions::SERVER_MENU8 ? 235 : 236);

		Locker clocker(companion, player);
		companion->setHarvestPreference(preference);

		String label = preference == 234 ? "meat" : (preference == 235 ? "hide" : "bone");
		player->sendSystemMessage("Your companion will harvest " + label + " from creature corpses after combat.");
		break;
	}

	case RadialOptions::SERVER_MENU10: { // Harvesting parent -> full choice box
		if (!isOwner) {
			return 0;
		}

		CompanionHarvestChoiceSuiCallback::sendChoiceBox(player, companion);
		break;
	}

	case RadialOptions::COMPANION_REQUEST_ARMOR: { // Armorsmith: Request Full Suit... (radial twin of /companionrequestarmor)
		if (!isOwner) {
			return 0;
		}

		if (!CompanionGearExchangeManager::isArmorsmith(companion)) {
			return 0;
		}

		if (companion->isInCombat()) {
			player->sendSystemMessage(companion->getDisplayedName() + " can't take orders while in combat.");
			break;
		}

		CompanionArmorTypeSuiCallback::sendTypeList(player, companion);
		break;
	}

	case RadialOptions::COMPANION_TEST_RESOURCES: { // "Get Test Resources..." (2026-07-24)
		if (!isOwner) {
			return 0;
		}

		bool artisanTrained = false;

		for (int i = 0; i < companion->getLearnedSkillCount(); ++i) {
			const String& artisanCheckSkill = companion->getLearnedSkill(i);

			// Companion System (2026-07-27, "craft the doctor buffs" per Nick):
			// medic-trained companions can craft too (real enhance packs/stims),
			// same flow as artisans -- CompanionCraftPickSuiCallback::
			// sendCraftList() already generically scans whatever schematics the
			// companion's learned skills actually grant, it was never hardcoded
			// to crafting_ specifically. This is just the button's show/hide gate.
			if (artisanCheckSkill.beginsWith("crafting_") || artisanCheckSkill.beginsWith("science_medic_")) {
				artisanTrained = true;
				break;
			}
		}

		if (!artisanTrained) {
			return 0;
		}

		CompanionCraftPickSuiCallback::sendTestResourceList(player, companion);
		break;
	}

	case RadialOptions::COMPANION_CRAFT_BATCH: { // "Craft: Factory Run..." (2026-07-27)
		if (!isOwner) {
			return 0;
		}

		bool artisanTrained = false;

		for (int i = 0; i < companion->getLearnedSkillCount(); ++i) {
			const String& artisanCheckSkill = companion->getLearnedSkill(i);

			if (artisanCheckSkill.beginsWith("crafting_") || artisanCheckSkill.beginsWith("science_medic_")) {
				artisanTrained = true;
				break;
			}
		}

		if (!artisanTrained) {
			return 0;
		}

		CompanionCraftPickSuiCallback::sendBatchCraftList(player, companion);
		break;
	}

	case RadialOptions::COMPANION_DOCTOR_BUFF_ME: { // "Medical: Buff Me" (Doctor Buff Radial, 2026-07-27)
		if (!isOwner) {
			return 0;
		}

		bool doctorTrained = false;

		for (int i = 0; i < companion->getLearnedSkillCount(); ++i) {
			if (companion->getLearnedSkill(i).beginsWith("science_medic_")) {
				doctorTrained = true;
				break;
			}
		}

		if (!doctorTrained) {
			return 0;
		}

		// Doctor Buff Radial -- craft-ALL-then-buff, concurrency guard (2026-07-29
		// follow-up): refuse a second overlapping craft-then-buff sequence for
		// the SAME doctor instead of racing CompanionFieldStation::begin()
		// twice onto the same inventory/FactoryCrate state -- see
		// doctorBuffCraftBusy() above for the live-bug root cause.
		if (isDoctorBuffCraftBusy(companion->getObjectID())) {
			player->sendSystemMessage(companion->getDisplayedName() + " is already working on your Enhance Packs -- hang on.");
			break;
		}

		Vector<int> missingAttributes;
		bool appliedAny = applyCompanionDoctorBuffFromSupplies(companion, player, &missingAttributes);

		if (missingAttributes.size() > 0) {
			// One or more attributes have no pack at all -- craft EVERY one of
			// them in sequence (Nick: "the doctor should be giving all 8 buffs
			// from that one command"), regardless of whether some attributes
			// already got applied above -- the final pass at the end of the
			// craft sequence covers every attribute, so nothing gets missed.
			Locker clocker(companion, player);
			setDoctorBuffCraftBusy(companion->getObjectID(), true);
			craftMissingPacksThenBuffAll(player, companion, missingAttributes, 0, false);
		} else if (!appliedAny) {
			// No missing packs at all, and nothing applied -- the recipient was
			// already at least as well buffed on every attribute.
			player->sendSystemMessage(companion->getDisplayedName() + " has no better enhancements to offer right now.");
		}

		break;
	}

	case RadialOptions::COMPANION_DOCTOR_BUFF_SQUAD: { // "Medical: Buff The Squad" (same pass)
		if (!isOwner) {
			return 0;
		}

		bool doctorTrained = false;

		for (int i = 0; i < companion->getLearnedSkillCount(); ++i) {
			if (companion->getLearnedSkill(i).beginsWith("science_medic_")) {
				doctorTrained = true;
				break;
			}
		}

		if (!doctorTrained) {
			return 0;
		}

		// Doctor Buff Radial -- craft-ALL-then-buff, concurrency guard (2026-07-29
		// follow-up): same guard as COMPANION_DOCTOR_BUFF_ME above -- see that
		// case for the live-bug root cause.
		if (isDoctorBuffCraftBusy(companion->getObjectID())) {
			player->sendSystemMessage(companion->getDisplayedName() + " is already working on Enhance Packs for the squad -- hang on.");
			break;
		}

		Vector<int> missingAttributes;
		bool appliedAny = runDoctorSquadBuffPass(player, companion, &missingAttributes);

		if (missingAttributes.size() > 0) {
			// Same craft-ALL decision as COMPANION_DOCTOR_BUFF_ME above -- craft
			// every missing attribute in sequence, then run one final full
			// squad buff pass covering everyone/everything.
			Locker clocker(companion, player);
			setDoctorBuffCraftBusy(companion->getObjectID(), true);
			craftMissingPacksThenBuffAll(player, companion, missingAttributes, 0, true);
		} else if (!appliedAny) {
			player->sendSystemMessage(companion->getDisplayedName() + " has no better enhancements to offer right now.");
		}

		break;
	}

	case RadialOptions::COMPANION_HEAL_WOUNDS_ME: { // "Medical: Heal Wounds" (Heal Wounds Radial, 2026-07-29)
		if (!isOwner) {
			return 0;
		}

		bool doctorTrained = false;

		for (int i = 0; i < companion->getLearnedSkillCount(); ++i) {
			if (companion->getLearnedSkill(i).beginsWith("science_medic_")) {
				doctorTrained = true;
				break;
			}
		}

		if (!doctorTrained) {
			return 0;
		}

		// Heal Wounds Radial (2026-07-29) -- cross-checks BOTH busy sets
		// (this feature's own AND the Doctor Buff Radial's) since both craft
		// through CompanionFieldStation::begin() for the SAME companion --
		// see woundHealCraftBusy()'s comment above for why a single shared
		// race class covers both features.
		if (isWoundHealCraftBusy(companion->getObjectID()) || isDoctorBuffCraftBusy(companion->getObjectID())) {
			player->sendSystemMessage(companion->getDisplayedName() + " is already busy crafting -- hang on.");
			break;
		}

		Vector<int> missingAttributes;
		bool appliedAny = applyCompanionWoundHealFromSupplies(companion, player, &missingAttributes);

		if (missingAttributes.size() > 0) {
			Locker clocker(companion, player);
			setWoundHealCraftBusy(companion->getObjectID(), true);
			craftMissingWoundPacksThenHealAll(player, companion, missingAttributes, 0, false);
		} else if (!appliedAny) {
			player->sendSystemMessage(companion->getDisplayedName() + " finds no wounds to heal right now.");
		}

		break;
	}

	case RadialOptions::COMPANION_HEAL_WOUNDS_SQUAD: { // "Medical: Heal The Squad's Wounds" (same pass)
		if (!isOwner) {
			return 0;
		}

		bool doctorTrained = false;

		for (int i = 0; i < companion->getLearnedSkillCount(); ++i) {
			if (companion->getLearnedSkill(i).beginsWith("science_medic_")) {
				doctorTrained = true;
				break;
			}
		}

		if (!doctorTrained) {
			return 0;
		}

		if (isWoundHealCraftBusy(companion->getObjectID()) || isDoctorBuffCraftBusy(companion->getObjectID())) {
			player->sendSystemMessage(companion->getDisplayedName() + " is already busy crafting -- hang on.");
			break;
		}

		Vector<int> missingAttributes;
		bool appliedAny = runMedicSquadWoundHealPass(player, companion, &missingAttributes);

		if (missingAttributes.size() > 0) {
			Locker clocker(companion, player);
			setWoundHealCraftBusy(companion->getObjectID(), true);
			craftMissingWoundPacksThenHealAll(player, companion, missingAttributes, 0, true);
		} else if (!appliedAny) {
			player->sendSystemMessage(companion->getDisplayedName() + " finds no wounds to heal right now.");
		}

		break;
	}

	case RadialOptions::COMPANION_STIM_HEAL_ME: { // "Medical: Heal Me (Stims)" (Medic Stim Heal Radial, 2026-07-29 night #3)
		if (!isOwner) {
			return 0;
		}

		bool doctorTrained = false;

		for (int i = 0; i < companion->getLearnedSkillCount(); ++i) {
			if (companion->getLearnedSkill(i).beginsWith("science_medic_")) {
				doctorTrained = true;
				break;
			}
		}

		if (!doctorTrained) {
			return 0;
		}

		if (isStimHealCraftBusy(companion->getObjectID()) || isDoctorBuffCraftBusy(companion->getObjectID()) || isWoundHealCraftBusy(companion->getObjectID())) {
			player->sendSystemMessage(companion->getDisplayedName() + " is already busy crafting -- hang on.");
			break;
		}

		if (companion->applyStimHealTo(player)) {
			break;
		}

		Locker clocker(companion, player);
		setStimHealCraftBusy(companion->getObjectID(), true);
		craftStimThenHeal(player, companion, player, false);

		break;
	}

	case RadialOptions::COMPANION_STIM_HEAL_SQUAD: { // "Medical: Heal The Squad (Stims)" (same pass)
		if (!isOwner) {
			return 0;
		}

		bool doctorTrained = false;

		for (int i = 0; i < companion->getLearnedSkillCount(); ++i) {
			if (companion->getLearnedSkill(i).beginsWith("science_medic_")) {
				doctorTrained = true;
				break;
			}
		}

		if (!doctorTrained) {
			return 0;
		}

		if (isStimHealCraftBusy(companion->getObjectID()) || isDoctorBuffCraftBusy(companion->getObjectID()) || isWoundHealCraftBusy(companion->getObjectID())) {
			player->sendSystemMessage(companion->getDisplayedName() + " is already busy crafting -- hang on.");
			break;
		}

		companion->applyStimHealTo(player);

		Vector<ManagedReference<CompanionObject*> > squad;
		resolveAllActiveCompanionsForBuff(player, squad);

		for (int i = 0; i < squad.size(); ++i) {
			CompanionObject* member = squad.get(i);

			if (member != companion) {
				companion->applyStimHealTo(member);
			}
		}

		Locker clocker(companion, player);
		setStimHealCraftBusy(companion->getObjectID(), true);
		craftStimThenHeal(player, companion, nullptr, true);

		break;
	}

	case RadialOptions::COMPANION_DANCE: { // "Dance" (Entertainer Dance/Watch, 2026-07-29)
		if (!isOwner) {
			return 0;
		}

		bool entertainerTrained = false;

		for (int i = 0; i < companion->getLearnedSkillCount(); ++i) {
			if (companion->getLearnedSkill(i).beginsWith("social_entertainer_")) {
				entertainerTrained = true;
				break;
			}
		}

		if (!entertainerTrained) {
			return 0;
		}

		CampDeploymentManager::instance()->startEntertainerDanceWatch(player, companion);
		break;
	}

	case RadialOptions::COMPANION_STOP_DANCE: { // "Stop Dance" (Entertainer Dance/Watch, 2026-07-29)
		if (!isOwner) {
			return 0;
		}

		CampDeploymentManager::instance()->stopEntertainerDanceWatch(player->getObjectID());
		break;
	}

	case RadialOptions::COMPANION_PLAY_MUSIC: { // "Play Music" (Musician Play/Watch, 2026-07-29)
		if (!isOwner) {
			return 0;
		}

		bool musicianTrained = false;

		for (int i = 0; i < companion->getLearnedSkillCount(); ++i) {
			if (companion->getLearnedSkill(i).beginsWith("social_musician_")) {
				musicianTrained = true;
				break;
			}
		}

		if (!musicianTrained) {
			return 0;
		}

		CampDeploymentManager::instance()->startEntertainerMusicWatch(player, companion);
		break;
	}

	case RadialOptions::COMPANION_STOP_MUSIC: { // "Stop Music" (Musician Play/Watch, 2026-07-29)
		if (!isOwner) {
			return 0;
		}

		CampDeploymentManager::instance()->stopEntertainerDanceWatch(player->getObjectID());
		break;
	}

	default:
		break;
	}

	return 0;
}
