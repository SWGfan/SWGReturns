/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System -- spec 4E ("Ranger Utility Integration: Portable Camps
	& Tents"). Modeled closely on
	server/zone/objects/tangible/components/CampKitMenuComponent.cpp (the
	real handheld-camp-kit placement flow), but sources the tent item from the
	companion's own private inventory container instead of a
	player-carried CampKit, and is gated on the companion having advanced
	through the Ranger/Scout skill trees rather than the owner's "camp"
	skill mod.
*/

#ifndef CAMPDEPLOYMENTMANAGER_H_
#define CAMPDEPLOYMENTMANAGER_H_

#include "engine/engine.h"

namespace server {
namespace zone {
	class Zone;

namespace objects {
namespace creature {
	class CreatureObject;
}
namespace companion {
	class CompanionObject;
}
namespace tangible {
	class TangibleObject;
}
}
}
}

using namespace server::zone::objects::creature;
using namespace server::zone::objects::companion;
using namespace server::zone::objects::tangible;

namespace server {
namespace zone {
namespace managers {
namespace companion {

class CampDeploymentManager : public Singleton<CampDeploymentManager>, public Logger, public Object {

public:
	CampDeploymentManager();

	/** Spec 4E entry point, reachable via /hpet camp or the dialogue menu.
	 * 2026-07-18 second revision (live feedback: "let me choose the tent,
	 * and require the CORRECT resources"): now opens the tent picker
	 * (CompanionCampChoiceSuiCallback) listing every tier the companion's
	 * training allows, each marked carried or craft-with-recipe. */
	void deployCamp(CreatureObject* owner, CompanionObject* companion);

	/** Deploys or crafts one specific tier (index into the internal
	 * CAMP_KIT_TIERS table) -- the picker's dispatch target. Carried kit of
	 * that tier -> full placement flow; no kit -> recipe-checked crafting
	 * theater (missing materials itemized in spatial chat). */
	void deployCampTier(CreatureObject* owner, CompanionObject* companion, int tierIndex);

	/** Phase 1 (2026-07-18): the ranger strikes the owner's deployed camp
	 * (same teardown as the camp terminal's own Disband option). */
	void packUpCamp(CreatureObject* owner, CompanionObject* companion);

	/** Camp life (2026-07-20, per user request): starts a recurring
	 * ambiance loop for the owner's deployed camp -- idle companions sit
	 * and sheath their weapons; an entertainer companion auto-dances
	 * exotic4 with a flourish every 3s and buffs the owner + companions in
	 * the camp. Idempotent (guarded so only one loop runs per owner); the
	 * loop self-terminates when the owner no longer has a camp. */
	void startCampAmbiance(CreatureObject* owner);
	void runCampAmbianceTick(uint64 ownerID);

	// Companion System (2026-07-29, "Entertainer Dance/Watch" -- per Nick's
	// dance-radial request): a Dance radial on an Entertainer-trained
	// companion makes it dance exotic4 + flourish every 3s while every
	// OTHER summoned companion watches (faces it + unequips its weapon),
	// until the owner stops watching -- three independent ways: the "Stop
	// Dance" radial, auto-stop on range/line-of-sight loss, or the
	// /companionenddance command. On stop every watcher re-equips its
	// weapon, gets the real dance-mind PerformanceBuff (reuses
	// applyDanceBuff() below), and says so via companionSay() (the same
	// local helper the camp-ambiance feature above already uses). One
	// session per OWNER, same one-loop-per-owner shape as
	// startCampAmbiance()/activeCampAmbiance above.
	/** Starts (or, if the SAME entertainer is already dancing for this
	 * owner, no-ops with a message; if a DIFFERENT entertainer is already
	 * dancing for this owner, refuses with a message) an entertainer
	 * dance/watch session. @pre owner already locked by this thread (same
	 * discipline as changeIntoCampClothes()/restoreArmorFromCamp() below --
	 * every existing radial-handler/command call site already satisfies
	 * this, see NOTES.md). */
	void startEntertainerDanceWatch(CreatureObject* owner, CompanionObject* entertainer);

	/** Recurring 3s tick: re-issues the dance+flourish, re-faces every
	 * still-valid watcher, and checks the ongoing auto-stop conditions
	 * (owner/entertainer combat, entertainer gone, range, line-of-sight). */
	void runEntertainerDanceWatchTick(uint64 ownerID);

	/** Ends the owner's active dance/watch session (idempotent -- no-ops if
	 * none is active): stops the entertainer's dance state, re-equips every
	 * watcher's stored weapon, applies the dance-mind buff, and has each
	 * watcher say so in spatial chat. Reused directly by the "Stop Dance"
	 * radial, the /companionenddance command, and every internal auto-stop
	 * path (combat, range, line-of-sight, entertainer/owner gone). @pre
	 * owner already locked by this thread IF owner is still resolvable --
	 * every call site (the tick above, the radial handler, the chat
	 * command) already satisfies this per the existing framework
	 * convention documented at every other Companion*Command.h/radial
	 * handler in this codebase. */
	void stopEntertainerDanceWatch(uint64 ownerID);

	/** True if `entertainer` is the companion currently dancing in owner's
	 * active session (radial gating: shows "Stop Dance" instead of
	 * "Dance"). */
	bool isEntertainerDancing(CreatureObject* owner, CompanionObject* entertainer) const;

	/** True if owner has ANY active dance/watch session right now (radial
	 * gating: hides "Dance" on every other entertainer-trained companion
	 * while one is already dancing; also used by /companionenddance to
	 * report "nobody is dancing" cleanly). */
	bool hasActiveDanceSession(CreatureObject* owner) const;

	// Companion System (2026-07-29, "Play Music" -- per Nick: "we need
	// a musician as well", the Musician-trained analog of the
	// Entertainer Dance/Watch feature above). Shares the exact same
	// one-session-per-owner watch loop (runEntertainerDanceWatchTick()),
	// stop path (stopEntertainerDanceWatch()), and session dedupe
	// (activeEntertainerDance/hasActiveDanceSession() above) as Dance --
	// starting Music while a Dance session (or another Music session) is
	// already active for this owner is refused the same way a second
	// Dance would be, and vice versa. Only the performance type +
	// default animation literal differ (see entertainerPerformanceMode
	// below); the real per-instrument-audio animation lookup a live
	// player's /startmusic uses (PerformanceManager::
	// getInstrumentAnimation()) is intentionally NOT reproduced here --
	// companions don't carry a real Instrument object today (confirmed:
	// zero Instrument references anywhere in the companion source), so
	// this always uses "music_3", the single most common real
	// instrument-animation literal across this server's Performance
	// rows (slitherhorn/fizz/fanfar/kloohorn/traz all resolve to it) --
	// same "one deliberately-hardcoded good default" choice already made
	// for Dance's "exotic4".
	void startEntertainerMusicWatch(CreatureObject* owner, CompanionObject* entertainer);

	/** True if `entertainer` is the companion currently PLAYING MUSIC in
	 * owner's active session (radial gating: shows "Stop Music" instead
	 * of "Play Music"). Mirrors isEntertainerDancing() above but checks
	 * entertainerPerformanceMode == MUSIC instead of DANCE. */
	bool isEntertainerPlayingMusic(CreatureObject* owner, CompanionObject* entertainer) const;

private:
	/** Applies the real dance-mind PerformanceBuff to one target (owner or
	 * companion). `strength` is a FRACTION (0.0-1.25 = 0%-125%, clamped
	 * here), NOT a raw percent -- confirmed via
	 * PerformanceBuffImplementation::activate()'s
	 * `round(strength * baseHAM(MIND))` formula. `duration` is in seconds.
	 * Skips if an existing buff is already at least this strong. Buff
	 * accrual redesign (2026-07-29): the defaults (1.25f/300) preserve
	 * the camp-ambiance loop's original always-max-strength,
	 * continuously-refreshed behavior (its call sites pass no args) --
	 * the entertainer dance/watch session below passes its own accrued
	 * strength/duration explicitly instead of relying on these
	 * defaults. */
	void applyDanceBuff(CreatureObject* target, float strength = 1.25f, int duration = 300) const;

	/** "Play Music" (2026-07-29): applies the real TWO music
	 * PerformanceBuffs (performance_enhance_music_focus +
	 * performance_enhance_music_willpower,
	 * PerformanceBuffType::MUSIC_FOCUS/MUSIC_WILLPOWER) -- confirmed via
	 * direct read of the real EntertainingSessionImplementation::
	 * activateEntertainerBuff()'s PerformanceType::MUSIC case, which is
	 * genuinely a different buff shape than Dance's single mind buff,
	 * not a reskin. Skips a buff individually if already at least this
	 * strong (same per-buff guard applyDanceBuff() above uses). See
	 * applyDanceBuff() above for the strength/duration parameter
	 * semantics and default rationale. */
	void applyMusicBuff(CreatureObject* target, float strength = 1.25f, int duration = 300) const;

public:

	/** Number of tiers / metadata accessors for the picker UI. */
	int getTierCount() const;
	String describeTierForPicker(CompanionObject* companion, int tierIndex) const;
	bool isTierWithinTraining(CompanionObject* companion, int tierIndex) const;

	/** Swaps a companion into camp clothes (unequip armor -> equip clothes)
	 * if it carries clothes; records removed armor. @pre companion locked.
	 * Made public 2026-08-11 (per Nick: "the companion is not ... taking
	 * off armor or weapon when inside a cantina") so
	 * CompanionObjectImplementation.cpp's own cantina-ambiance tick can
	 * call it directly -- same call shape/precondition as every existing
	 * caller inside this class's own camp-ambiance tick, none of which
	 * were touched. */
	void changeIntoCampClothes(CompanionObject* companion, CreatureObject* owner);

	/** Restores the armor unequipped by changeIntoCampClothes(). @pre
	 * companion locked. Made public 2026-08-11, see changeIntoCampClothes()
	 * above for why. */
	void restoreArmorFromCamp(CompanionObject* companion, CreatureObject* owner);

private:
	/** Owners with a camp-ambiance loop currently running (dedupe guard).
	 * 2026-07-20. */
	SortedVector<uint64> activeCampAmbiance;

	/** Camp attire swap (2026-07-20): companionID -> the armor piece object
	 * IDs that were unequipped when it changed into camp clothes, to be
	 * re-equipped when it leaves the camp. Empty entry = swapped but had no
	 * armor. Absent = not currently in camp clothes. */
	VectorMap<uint64, Vector<uint64> > campAttireRemovedArmor;

	// Companion System (2026-07-29, "Entertainer Dance/Watch") -- one
	// session per owner, same one-loop-per-owner dedupe shape as
	// activeCampAmbiance above. Object IDs only (never live references),
	// same "store the ID, re-resolve via ZoneServer::getObject() each time"
	// idiom campAttireRemovedArmor above already uses -- so a watcher/
	// weapon that gets sold, traded, destroyed, or stored mid-dance is
	// skipped instead of leaving a dangling reference.
	/** Owners with an active entertainer dance/watch session right now
	 * (dedupe guard -- one session per owner). */
	SortedVector<uint64> activeEntertainerDance;

	/** ownerID -> the dancing entertainer companion's object ID for that
	 * session. */
	VectorMap<uint64, uint64> entertainerDanceEntertainer;

	/** ownerID -> the watcher companion object IDs for that session (every
	 * OTHER summoned companion at watch-start, excluding the entertainer
	 * itself). */
	VectorMap<uint64, Vector<uint64> > entertainerDanceWatchers;

	/** ownerID -> the weapon object ID each watcher above had equipped at
	 * watch-start, same index order as entertainerDanceWatchers' list (0 =
	 * was unarmed) -- re-equipped from this on stop. */
	VectorMap<uint64, Vector<uint64> > entertainerDanceWatcherWeapons;

	/** ownerID -> the active performance type for that session
	 * (PerformanceType::DANCE or PerformanceType::MUSIC) -- "Play Music"
	 * (2026-07-29). Missing entry defaults to DANCE everywhere it's
	 * read, matching this session's original (pre-Music) behavior
	 * exactly. */
	VectorMap<uint64, int> entertainerPerformanceMode;

	/** Buff accrual redesign (2026-07-29): objectID (the owner OR any
	 * watcher companion) -> seconds accrued watching so far THIS session,
	 * incremented by the tick interval (~3s) each tick that ID is
	 * actively watching (not in combat/dead), capped at 7210 (the real
	 * system's own 120min+10s cap). Matches the real
	 * EntertainingSessionImplementation model: banked silently while
	 * watching, no buff exists until stopEntertainerDanceWatch()
	 * constructs the real buff(s) from whatever accrued -- nothing is
	 * applied mid-session anymore. Reset to 0 for the owner + every
	 * watcher at the start of a new session
	 * (startEntertainerDanceWatch()/startEntertainerMusicWatch()); each
	 * entry is read once and dropped in stopEntertainerDanceWatch(). */
	VectorMap<uint64, int> entertainerWatchAccrual;

	/** Every summoned, living companion linked to owner (includes the
	 * entertainer itself -- caller excludes it) -- same datapad-scan shape
	 * duplicated in every Companion*Command.h and
	 * CompanionMenuComponent.cpp per this project's own per-file-copy
	 * convention. */
	void resolveActiveCompanionsForDance(CreatureObject* owner, Vector<ManagedReference<CompanionObject*> >& out) const;

	bool companionHasRangerOrScoutTraining(CompanionObject* companion) const;

	/** Highest camp tier the companion's training allows, expressed as the
	 * camp structure's skillRequired ceiling: ranger training = 100 (all
	 * six tiers incl. luxury), scout-only = 50 (up to High Quality). */
	int companionCampSkillCap(CompanionObject* companion) const;

	/** The placement flow (checks + placeCamp + terminal + active area),
	 * consuming the given carried kit on success. */
	void deployCampFromKit(CreatureObject* owner, CompanionObject* companion, TangibleObject* kit, int tierIndex);

	/** Recipe-checked crafting theater for one tier: requires a crafting
	 * tool and the tier's exact resource classes/amounts (hide/bone/metal)
	 * across the companion's bag; misses are itemized in spatial chat; on
	 * completion the exact consumption is announced, the chime plays, and
	 * the tier deploys. */
	void craftCampKit(CreatureObject* owner, CompanionObject* companion, int tierIndex);

	/** Phase 4 ("companion-to-companion fetch", 2026-07-18): when crafting
	 * materials (or the tool) are missing, scan the owner's OTHER summoned
	 * companions for them -- if one has any, the two companions walk toward
	 * each other, trade the goods, HIGH-FIVE, and the craft retries
	 * automatically. Returns true if a fetch was started (the caller should
	 * then skip its plain "I'm missing X" call-out). */
	bool startMaterialFetch(CreatureObject* owner, CompanionObject* ranger, int tierIndex, bool needsTool);

	/**
	 * Terrain slope check at (x, y): samples Zone::getHeight() (the real
	 * engine primitive -- there is no Zone::getInclineHeight() in this
	 * codebase, see NOTES.md, "Terrain slope API") at the center and at four
	 * cardinal offsets, and rejects placement if the steepest delta exceeds
	 * maxSlope (in world units per meter).
	 */
	bool isSlopeAcceptable(Zone* zone, float x, float y, float maxSlope) const;

};

}
}
}
}

using namespace server::zone::managers::companion;

#endif // CAMPDEPLOYMENTMANAGER_H_
