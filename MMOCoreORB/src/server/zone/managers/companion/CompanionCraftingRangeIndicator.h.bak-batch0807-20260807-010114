/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-29, "in crafting range" indicator) -- design
	doc: claude/design-crafting-quality-batch-2026-07-24.md section 5
	(Fable, project docs). Verified against real source before build (see
	NOTES.md / build agent report): no true nameplate icon exists in this
	pre-CU-era client -- buff icons are player-only and target-window-only,
	states don't render over nameplates. The proven, buildable kit instead:

	  1. PRIMARY -- showFlyText() (SceneObject.idl:904,
	     SceneObjectImplementation.cpp:1858-1867; real precedent usage:
	     PetManagerImplementation.cpp:344-408). Signature confirmed:
	     showFlyText(file, aux, red, green, blue, isPrivate = false).
	     isPrivate routes through sendMessage() (SceneObjectImplementation.
	     cpp:947), which only actually delivers a packet on an object that
	     has a real client connection (CreatureObjectImplementation::
	     sendMessage(), CreatureObjectImplementation.cpp:3343, checks
	     owner.get() -- the ZoneClientSession). A companion's ghost/client
	     is ALWAYS null (this file's own project convention), so the
	     private flytext call below is made on the OWNER (a real player,
	     definitely has a client), not on the companion -- that's the only
	     way isPrivate=true actually reaches a screen.
	  2. SECONDARY -- a nameplate suffix appended to the companion's OWN
	     customName via setCustomObjectName(), added on entering range and
	     stripped on leaving. Same suffix-concatenation precedent already
	     proven safe in this exact codebase (CompanionRenameSuiCallback.h's
	     " (<Owner>'s -=COMPANION=-)" suffix, LootValues.h's " (Legendary)"
	     suffix, PlaceStructureSessionImplementation.cpp's "'s House"
	     suffix) -- deliberately done on the COMPANION's name, never the
	     player's own (NOTES.md ~7939 documents a past incident from doing
	     this to a real player's name -- name macros/the target UI key off
	     player names; a companion has no such dependency).
	  3. DRIVER -- rides the companion's EXISTING self-rescheduling keep-up
	     tick (CompanionObjectImplementation::runKeepUpTick(), started by
	     startKeepUpMonitor(), ticks every 2000ms and reschedules itself)
	     rather than spinning up a second, independent scheduled task per
	     companion. One call added to that tick (see the .cpp patch),
	     exactly the same integration pattern already used for
	     runMedicAutoCareTick(). tick() below is a no-op fast-return unless
	     the companion is actually crafting-capable, so this costs nothing
	     for companions with no crafting skills.

	RANGE CONSTANT -- 7.0f meters. This is NOT an arbitrary/independent
	pick: it's the exact real crafting-station range this project's own
	code already enforces. Confirmed by direct source read:
	PlayerManagerImplementation::getNearbyCraftingStation()
	(PlayerManagerImplementation.cpp:4514,4535) gates on
	`player->isInRange(scno, 7.0f)` for both placed stations and droid
	stations -- and CompanionFieldStation::hasNearbyRealStation() (this
	same managers/companion/ directory) calls straight into that function.
	So 7.0f here tells the truth about the same range this project's
	crafting-station gate already uses; it happens to also match stock
	SWG's real crafting-station range, which is a nice coincidence, not
	the reason for the number.

	CRAFTING-CAPABLE gating -- reuses the EXACT enumeration pattern
	CompanionCraftingManager::findSchematicForComponent() already uses
	(CompanionCraftingManager.cpp ~1265-1275): walk the companion's
	learnedSkills, resolve each via SkillManager::instance()->getSkill(),
	and check Skill::getSchematicsGranted() for a non-empty group list.
	Deliberately NOT re-resolving through SchematicMap to confirm the
	group's schematics still exist -- "grants at least one schematic
	group" is exactly what "crafting-capable" means for this feature, and
	skipping the deeper resolution keeps this a cheap per-tick check.

	PULSE CADENCE (documented, Nick's call defaulted since none was
	specified): immediate private pulse the instant the owner enters
	range, then a repeat pulse every ~10s (5 keep-up ticks @ 2000ms) for
	as long as they stay in range. No pulse at all while out of range.
	Chosen as a reasonable "reminder without spam" cadence -- easy to
	retune via PULSE_EVERY_N_TICKS below if Nick wants it louder/quieter.

	Header-only, all-static (same shape as CompanionFieldStation /
	CompanionCraftTheater) -- no new .cpp, no cmake reconfigure needed.
	Per-companion state (in-range flag, pulse countdown, captured base
	name) lives in function-local static VectorMaps keyed by companion
	object ID, same in-memory-only precedent as CompanionFieldStation's
	deployedProps().

	** 2026-08-01 CORRECTION ** -- this block previously claimed the maps
	resetting on restart "just means a suffixed nameplate reverts to its
	base name on the next boot; harmless". THAT WAS WRONG AND CAUSED REAL,
	CUMULATIVE DATA CORRUPTION. setCustomObjectName() PERSISTS to the
	database, so the suffix survives a restart while baseNames() does not.
	On the next boot applySuffix()'s map-based guard therefore passed, it
	captured the ALREADY-SUFFIXED name as the new "base", and appended a
	second suffix -- permanently, once per restart, forever. Nick's
	DroidEngineer had accumulated dozens (the client renders each 3-byte
	UTF-8 suffix as three unprintable boxes, so they arrive in threes).
	Fixed below by (a) guarding on the ACTUAL NAME rather than only the
	in-memory map, and (b) healOrphanedSuffixes(), which strips leftover
	suffixes off any untracked companion on its very next tick.

	⚠ STF DEPENDENCY: the flytext key referenced below (companion.stf's
	"crafting_range_flytext") is NEW. Per iron rule 4, this needs the
	regen pipeline (build_companion_content.py -> build_tre_patch.py from
	docs/companion_system/tools/) run and the resulting companion_patch.tre
	deployed, THEN the server needs a boot with `r reloadstrings` before
	the new string will actually resolve client-side -- until then the
	flytext will render as a raw "[crafting_range_flytext]"-style
	fallback rather than the intended text. The nameplate suffix has no
	such dependency (setCustomObjectName takes a raw literal string, not
	an STF lookup) -- it works immediately on rebuild with no reload step.
*/

#ifndef COMPANIONCRAFTINGRANGEINDICATOR_H_
#define COMPANIONCRAFTINGRANGEINDICATOR_H_

#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/managers/skill/SkillManager.h"
#include "server/zone/objects/creature/variables/Skill.h"

class CompanionCraftingRangeIndicator {
public:

	// See file header -- the real, enforced crafting-station range this
	// project's own CompanionFieldStation path uses (via
	// PlayerManagerImplementation::getNearbyCraftingStation()'s
	// isInRange(scno, 7.0f), PlayerManagerImplementation.cpp:4514,4535).
	static constexpr float CRAFTING_RANGE_METERS = 7.0f;

	// Rides runKeepUpTick()'s existing 2000ms cadence (see file header) --
	// this is NOT a separately scheduled interval, just documentation of
	// the tick spacing tick() below is actually called at.
	static const int TICK_MS = 2000;

	// 5 ticks * 2000ms = ~10s repeat-pulse cadence while still in range.
	static const int PULSE_EVERY_N_TICKS = 5;

	/** True if `comp` has learned at least one skill that grants at least
	 * one schematic group -- same enumeration
	 * CompanionCraftingManager::findSchematicForComponent() already uses
	 * (CompanionCraftingManager.cpp ~1265-1275). */
	static bool isCraftingCapable(CompanionObject* comp) {
		if (comp == nullptr) {
			return false;
		}

		for (int s = 0; s < comp->getLearnedSkillCount(); ++s) {
			Skill* skill = SkillManager::instance()->getSkill(comp->getLearnedSkill(s));

			if (skill == nullptr) {
				continue;
			}

			const Vector<String>* groups = skill->getSchematicsGranted();

			if (groups != nullptr && groups->size() > 0) {
				return true;
			}
		}

		return false;
	}

	/** Called once per keep-up tick (every TICK_MS, see runKeepUpTick())
	 * for EVERY companion. `comp` is already locked by the caller (same
	 * convention every other keep-up-tick-adjacent helper in this file
	 * uses). Cheap no-op fast-return for non-crafting-capable companions. */
	static void tick(CompanionObject* comp, CreatureObject* owner) {
		if (comp == nullptr) {
			return;
		}

		uint64 id = comp->getObjectID();

		// 2026-08-01: repair any marker left on the persisted name by a
		// previous boot BEFORE any other decision is made. Deliberately runs
		// for every companion, including non-crafting ones and ones out of
		// range, because that is exactly where the stale markers ended up.
		healOrphanedSuffixes(comp, id);

		if (owner == nullptr || comp->getZone() == nullptr || owner->getZone() != comp->getZone() || !isCraftingCapable(comp)) {
			clearIndicator(comp, id);
			return;
		}

		bool nowInRange = owner->isInRange(comp, CRAFTING_RANGE_METERS);
		bool wasInRange = inRangeStates().contains(id) && inRangeStates().get(id);

		if (nowInRange && !wasInRange) {
			// Entering range: suffix the nameplate + immediate private pulse.
			applySuffix(comp, id);
			pulse(owner);
			pulseCountdowns().drop(id);
			pulseCountdowns().put(id, PULSE_EVERY_N_TICKS);
		} else if (nowInRange && wasInRange) {
			int remaining = pulseCountdowns().contains(id) ? pulseCountdowns().get(id) : PULSE_EVERY_N_TICKS;
			remaining--;

			if (remaining <= 0) {
				pulse(owner);
				remaining = PULSE_EVERY_N_TICKS;
			}

			pulseCountdowns().drop(id);
			pulseCountdowns().put(id, remaining);
		} else if (!nowInRange && wasInRange) {
			// Leaving range: restore the original nameplate, drop tracking.
			removeSuffix(comp, id);
			pulseCountdowns().drop(id);
		}

		inRangeStates().drop(id);
		inRangeStates().put(id, nowInRange);
	}

private:

	static VectorMap<uint64, bool>& inRangeStates() {
		static VectorMap<uint64, bool> map;
		return map;
	}

	static VectorMap<uint64, int>& pulseCountdowns() {
		static VectorMap<uint64, int> map;
		return map;
	}

	// Companion's customName captured at the moment the suffix was
	// applied, so it can be restored EXACTLY on exit rather than guessing
	// at string-stripping the suffix back off (safe against the player
	// renaming the companion while the suffix happens to be showing --
	// worst case on a rename-while-suffixed is the next range-exit
	// restores the pre-rename name, which self-corrects on the next
	// enter/exit cycle; not worth extra complexity for that edge case).
	static VectorMap<uint64, String>& baseNames() {
		static VectorMap<uint64, String> map;
		return map;
	}

	/** 2026-08-01: the nameplate marker is now plain ASCII. The original
	 * was U+2692 HAMMER AND PICK encoded as UTF-8 (" \xE2\x9A\x92"), which
	 * the SWG client's nameplate font cannot render -- it showed as three
	 * unprintable boxes, one per byte, and the same glyph is unrenderable
	 * in the flytext too. Change this string if a different marker is
	 * wanted; keep it ASCII. */
	static const String& rangeSuffix() {
		static String suffix(" [+]");
		return suffix;
	}

	/** The pre-2026-08-01 UTF-8 marker, kept ONLY so already-corrupted
	 * names can still be recognised and stripped. Do not apply this. */
	static const String& legacySuffix() {
		static String suffix(" \xE2\x9A\x92");
		return suffix;
	}

	/** True if `s` ends with `suf`. Uses only length()/subString(int,int),
	 * both bedrock Core3 String methods. */
	static bool endsWith(const String& s, const String& suf) {
		if (suf.length() == 0 || s.length() < suf.length()) {
			return false;
		}

		return s.subString(s.length() - suf.length(), s.length()) == suf;
	}

	/** Strips EVERY trailing marker, current or legacy, in any order. */
	static String stripMarkers(const String& name) {
		String out = name;
		bool changed = true;

		while (changed) {
			changed = false;

			if (endsWith(out, rangeSuffix())) {
				out = out.subString(0, out.length() - rangeSuffix().length());
				changed = true;
			}

			if (endsWith(out, legacySuffix())) {
				out = out.subString(0, out.length() - legacySuffix().length());
				changed = true;
			}
		}

		return out;
	}

	/** Self-heal for companions this session is NOT currently tracking.
	 * If baseNames() holds an entry the marker on the name is ours and
	 * legitimate, so it is left alone; otherwise any marker present is a
	 * leftover from a previous boot and is stripped. Runs on every tick
	 * for every companion, which is cheap (a string compare on the tail)
	 * and writes only when something actually changed. Matches this
	 * project's established "self-heal on every summon/tick" convention. */
	static void healOrphanedSuffixes(CompanionObject* comp, uint64 id) {
		if (comp == nullptr || baseNames().contains(id)) {
			return;
		}

		String name = comp->getCustomObjectName().toString();
		String cleaned = stripMarkers(name);

		if (cleaned != name) {
			comp->setCustomObjectName(cleaned, true);
		}
	}

	static void applySuffix(CompanionObject* comp, uint64 id) {
		if (comp == nullptr || baseNames().contains(id)) {
			return; // already suffixed -- idempotent, never double-apply
		}

		// 2026-08-01: strip before capturing. The map-only guard above is
		// NOT sufficient on its own -- the map is in-memory and the name is
		// persisted, so after a restart this would otherwise capture an
		// already-suffixed name as the "base" and stack another marker on
		// top, permanently, once per restart.
		String base = stripMarkers(comp->getCustomObjectName().toString());

		baseNames().put(id, base);
		comp->setCustomObjectName(base + rangeSuffix(), true);
	}

	static void removeSuffix(CompanionObject* comp, uint64 id) {
		if (!baseNames().contains(id)) {
			return;
		}

		if (comp != nullptr) {
			comp->setCustomObjectName(baseNames().get(id), true);
		}

		baseNames().drop(id);
	}

	static void clearIndicator(CompanionObject* comp, uint64 id) {
		removeSuffix(comp, id);
		inRangeStates().drop(id);
		pulseCountdowns().drop(id);
	}

	/** Private-only-to-owner flytext pulse -- see file header for why this
	 * MUST be called on `owner`, not on the companion, to actually render
	 * (companions have no client to route isPrivate=true through). New
	 * companion.stf key "crafting_range_flytext" -- see file header's
	 * STF DEPENDENCY note (needs regen + `r reloadstrings`). */
	static void pulse(CreatureObject* owner) {
		if (owner == nullptr) {
			return;
		}

		owner->showFlyText("companion", "crafting_range_flytext", 0, 200, 255, true);
	}

};

#endif // COMPANIONCRAFTINGRANGEINDICATOR_H_
