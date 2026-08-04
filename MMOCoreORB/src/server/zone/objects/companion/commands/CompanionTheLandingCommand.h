/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-30) -- "The Landing", per approved design doc
	(see claude/design-... in the project's Claude workspace, applied here
	verbatim): a scripted 2-minute companion performance/skirmish-reveal
	show. A mock Rebel supply-drop-under-fire re-enactment using the
	player's own currently-summoned companions (5-8 assumed typical, but
	this iterates however many are actually out, exactly like every other
	multi-companion command in this project -- see
	CompanionAttackCommand.h's resolveActiveCompanions()), that resolves
	into a reveal/celebration rather than a real fight.

	Task-based state machine, same shape as CompanionFireworksShow.h /
	CompanionCraftTheater.h (a phase counter + a state Object holding
	companion/prop ids, driven by Core::getTaskManager()->scheduleTask(...)
	lambdas that re-invoke the next step after a delay, each one
	re-resolving its ManagedReference/Reference targets, null-checking, and
	Locker-ing before touching anything -- copied faithfully per this
	project's own documented crash lesson about skipping that pattern in
	scheduled lambdas).

	Because the whole point is a FIXED 2:00 runtime with specific beat
	timestamps (0:00-0:10, 0:10-0:25, 0:25-1:10, 1:10-1:40, 1:40-2:00), this
	is driven by fixed scheduleTask delays rather than a distance-polling
	loop (unlike CompanionFireworksShow's "keep re-checking distance every
	400ms" walk style) -- movement is still issued via the same
	setFollowObject(nullptr)/clearPatrolPoints()/addPatrolPoint()/
	setMovementState(AiAgent::PATROLLING) mechanism, it's just not gated on
	confirming arrival before advancing the beat, since this is a staged
	show, not a precision-blocking sequence.

	STATE-SUSPENSION: every cast companion's companionState is set to
	CompanionObject::THEATER for the WHOLE show (not toggled per beat) --
	this new constant is being added to CompanionObject.idl in a parallel
	patch (public static final int THEATER = 6; right after GUARD = 5;) and
	is only REFERENCED here, never redefined. THEATER suspends the
	background auto-heal/flee/self-buff/idle-emote/attack-intercept ticks
	for the duration; it does not gate movement (setMovementState/
	PatrolPoint is a separate mechanism -- confirmed via direct source read
	of CompanionObjectImplementation.cpp: the only companionState-gated
	branches found are ATTACK and FOLLOW checks elsewhere, nothing that
	would block PATROLLING movement while companionState == THEATER).

	RESTORE ON FINALE: mirrors CompanionObjectImplementation.cpp's own
	runSweepStep()/endSweep() restoration exactly (same
	getStandingOrder()-based STAY / GUARD / FOLLOW branches, since that's
	the project's real, only precedent for "what state does a companion
	return to when a scripted interruption ends" -- no separate
	restoreStandingPosture() helper exists in this codebase to call
	instead, confirmed via repo-wide grep). Posture is recorded per-cast-
	member before the show starts and restored at the finale.

	PROP TEMPLATES: confirmed real, already-used-elsewhere decorative
	tangible templates (none invented) --
	  - Supply crate: object/tangible/camp/camp_crate_s1.iff (the
	    project's own companion-camp crate prop, see
	    MMOCoreORB/bin/scripts/object/tangible/camp/objects.lua).
	  - Fallback marker: object/tangible/lair/base/objective_banner_generic_2.iff
	    (the stock game's own NPC-camp "objective banner" marker prop --
	    already used across dozens of npc_theater lair scripts as exactly
	    this kind of in-world position marker).
	  - Resolution banner: object/tangible/furniture/all/frn_all_banner_rebel.iff
	    (a real Rebel banner furniture prop -- thematically exact for a
	    Rebel supply-drop reveal's raised banner beat).

	MUZZLE-FLASH COVER FIRE: reuses FireworkObject::launch() exactly as
	CompanionFireworksShow.h calls it (same crouch/posture handling around
	the call -- launch() itself applies a CROUCHED posture on its own
	delayed launch event, so this show force-stands cast members back
	UPRIGHT at the resolution/finale beats the same way FireworksShow
	re-asserts UPRIGHT after its own finale launch). This is opportunistic:
	only cast members who happen to be carrying a real FireworkObject fire
	one during the cover-fire beats (same collectFireworks() scan
	CompanionFireworksShow.h uses, copied here) -- a companion with none
	on hand simply skips that beat's visual with no error. The design
	brief doesn't specify sourcing/crafting new firework stock for this
	show (unlike the original fireworks show's elaborate borrow/craft
	fallback), so no such fallback was built here; flagged for Nick in
	case guaranteed muzzle-flash on every cast member turns out to matter.

	Header-only (all methods in-class => implicitly inline; no new .cpp).
*/

#ifndef COMPANIONTHELANDINGCOMMAND_H_
#define COMPANIONTHELANDINGCOMMAND_H_

#include "server/zone/objects/creature/commands/QueueCommand.h"
#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/objects/tangible/TangibleObject.h"
#include "server/zone/objects/tangible/firework/FireworkObject.h"
#include "server/zone/objects/creature/ai/PatrolPoint.h"
#include "server/zone/objects/player/sui/messagebox/SuiMessageBox.h"
#include "server/zone/objects/player/sui/SuiWindowType.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/managers/skill/Performance.h" // PerformanceType::DANCE, confirmed real (Performance.h:13)
#include "templates/params/creature/CreaturePosture.h"
#include "server/chat/ChatManager.h"
#include "server/zone/Zone.h"
#include "server/zone/ZoneServer.h"
#include "templates/manager/TemplateManager.h"
#include "server/zone/managers/creature/CreatureManager.h"
#include "server/zone/objects/creature/ai/AiAgent.h"

// ---- Prop templates -- confirmed real existing decorative tangibles, see
// file header for exactly where each was confirmed in the live source. ----
#define THELANDING_CRATE_TEMPLATE  "object/tangible/camp/camp_crate_s1.iff"
#define THELANDING_MARKER_TEMPLATE "object/tangible/lair/base/objective_banner_generic_2.iff"
#define THELANDING_BANNER_TEMPLATE "object/tangible/furniture/all/frn_all_banner_rebel.iff"
#define THELANDING_DIRECTOR_DISGUISE_TEMPLATE "object/mobile/mouse_droid.iff"

// THELANDING_FILLER_CAST_2026_07_30 -- per Nick: "some players might not have enough
// companions to make the play work with only 1 companion... spawn in
// npc and creatures to help with the play." "rebel_recruiter" is a
// real, already-spawned-elsewhere-in-this-codebase mobile template
// (confirmed live precedent: PersonnelPerkZoneComponent.cpp) -- a
// dressed Rebel humanoid, thematically exact for this show's Rebel
// supply-drop theming (matches THELANDING_BANNER_TEMPLATE above).
// Target cast size matches this project's own "up to 5 companions
// per player" cap, so the show always looks fully staffed regardless
// of how many real companions the player actually brought.
#define THELANDING_FILLER_TEMPLATE_NAME "rebel_recruiter"
#define THELANDING_TARGET_CAST_SIZE 5

/** Per-companion pre-show snapshot, restored at the finale. */
class CompanionTheLandingCastMember : public Object {
public:
	uint64 companionID = 0;
	int preShowPosture = CreaturePosture::UPRIGHT;

	// THELANDING_FILLER_CAST_2026_07_30 -- true for a temporary spawned filler NPC (see
	// spawnFillerCast()), false for a real companion. A filler has no
	// pre-show state to restore -- finishShow() despawns it
	// unconditionally instead of running the real-companion restoration
	// path, which needs an actual CompanionObject.
	bool isFiller = false;
};

class CompanionTheLandingState : public Object {
public:
	uint64 ownerID = 0;
	uint64 directorID = 0;
	uint32 directorPreShowClientCRC = 0;
	bool directorDisguised = false;
	Vector<Reference<CompanionTheLandingCastMember*> > castMembers;

	uint64 cratePropID = 0;
	uint64 markerPropID = 0;
	uint64 bannerPropID = 0;

	// Stage marks, computed once at show start from the owner's position/
	// facing at that moment (same "fixed short distance in front of the
	// owner" math shape CompanionCraftTheater::spawnFactoryProp() and
	// CompanionFireworksShow's spot-picking already use).
	float centerX = 0, centerY = 0;
	float crateX = 0, crateY = 0;
	float markerX = 0, markerY = 0;
	float lineAX = 0, lineAY = 0; // near line's center point
	float lineBX = 0, lineBY = 0; // far line's center point
	float fwdX = 0, fwdY = 0;     // owner forward unit vector
	float perpX = 0, perpY = 0;   // perpendicular unit vector

	int skirmishBeat = 0; // 0-4, drives the sub-beats inside 0:25-1:10

	// GO-button confirm (2026-07-30, per Nick): true from the moment
	// promptGo() shows the owner the SuiMessageBox until either the GO
	// button is clicked (CompanionTheLandingGoSuiCallback::run()) or the
	// pending-confirm watchdog aborts the show first. Guards against a
	// stale/duplicate SUI response or watchdog tick doing anything once
	// one of those two has already fired.
	bool awaitingGoConfirm = false;
};

class CompanionTheLandingShow {
public:

	/** Copied verbatim from CompanionFireworksShow.h's identical helper. */
	static void say(CompanionObject* companion, const String& text) {
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

	/** Copied from CompanionFireworksShow.h's collectFireworks() -- scans a
	 * companion's own body + inventory for real FireworkObject items. */
	static void collectFireworks(CompanionObject* companion, Vector<uint64>& out) {
		if (companion == nullptr) {
			return;
		}

		auto scan = [&out](SceneObject* container) {
			if (container == nullptr) {
				return;
			}

			for (int i = 0; i < container->getContainerObjectsSize(); ++i) {
				ManagedReference<SceneObject*> obj = container->getContainerObject(i);

				if (obj != nullptr && obj->isFireworkObject()) {
					out.add(obj->getObjectID());
				}
			}
		};

		scan(companion);
		scan(companion->getSlottedObject("inventory"));
	}

	/** Walks `member` toward (x, y) using the same mechanism
	 * CompanionFireworksShow.h/CompanionCraftTheater.h already use --
	 * companionState is deliberately left at THEATER throughout (not
	 * toggled to PATROL per leg the way those two older files do, since
	 * THEATER didn't exist yet when they were written -- see file header).
	 * THELANDING_FILLER_CAST_2026_07_30: takes AiAgent* (CompanionObject's own base
	 * class) instead of CompanionObject* so this same call works
	 * unchanged for a filler cast member -- every existing call site
	 * still compiles as-is (CompanionObject* upcasts to AiAgent*
	 * implicitly). @pre { member locked } */
	static void walkTo(AiAgent* member, Zone* zone, float x, float y) {
		if (member == nullptr || zone == nullptr) {
			return;
		}

		member->setFollowObject(nullptr);
		member->clearPatrolPoints();
		PatrolPoint point(x, zone->getHeight(x, y), y);
		// genesis port: setMovementState() -> setFollowState(); genesis
		// setFollowState() calls clearPatrolPoints(), so the state must be
		// set BEFORE the point is queued (see PetPatrolCommand.h).
		member->setFollowState(AiAgent::PATROLLING);
		member->addPatrolPoint(point);
	}

	/** Mirrors CompanionObjectImplementation.cpp's runSweepStep()/endSweep()
	 * standingOrder-based restoration exactly (the project's only real
	 * precedent for "what does a companion return to after a scripted
	 * interruption ends" -- no separate restoreStandingPosture() helper
	 * exists to call instead, confirmed via repo-wide grep). @pre {
	 * companion locked } */
	static void restoreStanding(CompanionObject* companion, CreatureObject* owner) {
		if (companion == nullptr || companion->isDead() || companion->getZone() == nullptr) {
			return;
		}

		int standing = companion->getStandingOrder();

		if (standing == CompanionObject::STAY) {
			companion->setCompanionState(CompanionObject::STAY);
			companion->setFollowObject(nullptr);
			companion->setOblivious();
		} else if (standing == CompanionObject::GUARD) {
			CreatureObject* guardTarget = companion->getGuardTarget().get();

			if (guardTarget != nullptr && guardTarget->getZone() != nullptr) {
				companion->setCompanionState(CompanionObject::GUARD);
				companion->setFollowObject(guardTarget);
				companion->setFollowState(AiAgent::FOLLOWING); // genesis port: was setMovementState()
			} else {
				companion->setCompanionState(CompanionObject::FOLLOW);
				companion->setFollowObject(owner);
				companion->setFollowState(AiAgent::FOLLOWING); // genesis port: was setMovementState()
			}
		} else {
			companion->setCompanionState(CompanionObject::FOLLOW);

			CreatureObject* escortTarget = companion->getEscortTarget().get();

			if (escortTarget != nullptr && escortTarget != owner && escortTarget->getZone() != nullptr) {
				companion->setFollowObject(escortTarget);
			} else {
				companion->setFollowObject(owner);
			}

			companion->setFollowState(AiAgent::FOLLOWING); // genesis port: was setMovementState()
		}
	}

	/** Spawns a prop at (x, y) using the exact createObject/
	 * initializePosition/transferObject sequence
	 * CompanionCraftTheater::spawnFactoryProp() already uses. Returns the
	 * new object's id, or 0 on failure. */
	static uint64 spawnProp(ZoneServer* zoneServer, Zone* zone, const char* templatePath, float x, float y) {
		if (zoneServer == nullptr || zone == nullptr) {
			return 0;
		}

		ManagedReference<SceneObject*> prop = zoneServer->createObject(String(templatePath).hashCode(), 0);

		if (prop == nullptr) {
			return 0;
		}

		Locker propLocker(prop);

		float z = zone->getHeight(x, y);
		prop->initializePosition(x, z, y);
		zone->transferObject(prop, -1, true);

		return prop->getObjectID();
	}

	static void despawnProp(ZoneServer* zoneServer, uint64 objectID) {
		if (zoneServer == nullptr || objectID == 0) {
			return;
		}

		ManagedReference<SceneObject*> obj = zoneServer->getObject(objectID);

		if (obj == nullptr) {
			return;
		}

		Locker locker(obj);
		obj->destroyObjectFromWorld(true);
	}

	/** THELANDING_FILLER_CAST_2026_07_30 -- spawns `fillerCount` temporary, invulnerable
	 * filler NPCs (real AiAgent instances, same spawnCreatureWithAi()
	 * mechanism every other AI-driven mob spawn in this project uses --
	 * confirmed real "rebel_recruiter" mobile template, already spawned
	 * this exact way elsewhere in this codebase) and adds each as a
	 * full castMembers entry with isFiller = true. This is FULL CAST
	 * INTEGRATION per Nick's explicit choice -- fillers run through
	 * every beat (walkTo/setPosture/doAnimation/faceObject) exactly like
	 * a real companion, since AiAgent is CompanionObject's own base
	 * class and every shared beat call already resolves generically
	 * against an AiAgent* (see each beat's own "member" resolve). Not
	 * persisted to the DB; despawned unconditionally in finishShow()
	 * rather than restored (they have no pre-show state). Placed
	 * alternating along the two skirmish lines, spaced past the real
	 * roster's own line spacing so they don't overlap -- exact initial
	 * position doesn't matter much beyond that, since beatSkirmishSetup()
	 * walks every cast member (real + filler alike) to a freshly
	 * computed formation spot at 0:25 anyway. @pre { nothing locked --
	 * this locks each spawned NPC itself } */
	static void spawnFillerCast(ZoneServer* zoneServer, Zone* zone, Reference<CompanionTheLandingState*> state, int fillerCount) {
		if (zoneServer == nullptr || zone == nullptr || state == nullptr || fillerCount <= 0) {
			return;
		}

		CreatureManager* creatureManager = zone->getCreatureManager();

		if (creatureManager == nullptr) {
			return;
		}

		uint32 templateCRC = STRING_HASHCODE(THELANDING_FILLER_TEMPLATE_NAME);

		for (int i = 0; i < fillerCount; ++i) {
			bool inLineA = (i % 2) == 0;
			float baseX = inLineA ? state->lineAX : state->lineBX;
			float baseY = inLineA ? state->lineAY : state->lineBY;
			float spread = ((float) (i / 2) + 2.f) * 3.f; // past the real roster's own line spacing

			float fx = baseX + state->perpX * (inLineA ? spread : -spread);
			float fy = baseY + state->perpY * (inLineA ? spread : -spread);
			float fz = zone->getHeight(fx, fy);

			ManagedReference<CreatureObject*> fillerCreo = creatureManager->spawnCreatureWithAi(templateCRC, fx, fz, fy, 0, false);

			if (fillerCreo == nullptr) {
				continue;
			}

			ManagedReference<AiAgent*> filler = fillerCreo.castTo<AiAgent*>();

			if (filler == nullptr) {
				continue;
			}

			Locker fLocker(filler);

			Reference<CompanionTheLandingCastMember*> member = new CompanionTheLandingCastMember();
			member->companionID = filler->getObjectID();
			member->isFiller = true;
			state->castMembers.add(member);
		}
	}

	static void scheduleStep(int phase, ZoneServer* zoneServer, Reference<CreatureObject*> ownerRef, Reference<CompanionTheLandingState*> state, int delayMs) {
		Core::getTaskManager()->scheduleTask([phase, zoneServer, ownerRef, state] () {
			runPhase(phase, zoneServer, ownerRef, state);
		}, "CompanionTheLandingStepLambda", delayMs);
	}

	/** Builds and sends the owner a GO-button SuiMessageBox instead of
	 * letting phase 1 (beatCrateConverge -- the 0:10-0:25 crate-converge
	 * beat) fire automatically on a timer. Defined out-of-class further
	 * down this file (see the #include right after this class's closing
	 * brace) because it needs CompanionTheLandingGoSuiCallback, which in
	 * turn needs THIS class fully defined first -- see that header's own
	 * ORDERING note for why the include has to sit exactly there. */
	static void promptGo(ZoneServer* zoneServer, CreatureObject* owner, Reference<CompanionTheLandingState*> state);

	static void scheduleGoWatchdog(ZoneServer* zoneServer, Reference<CreatureObject*> ownerRef, Reference<CompanionTheLandingState*> state, int delayMs) {
		Core::getTaskManager()->scheduleTask([zoneServer, ownerRef, state] () {
			goWatchdogTick(zoneServer, ownerRef, state);
		}, "CompanionTheLandingGoWatchdogLambda", delayMs);
	}

	/** Re-checks the exact same abort conditions runPhase() already uses
	 * (owner gone/zoned-out/in-combat, or every cast member dead/gone)
	 * while the GO confirm box is pending -- never auto-advances the show,
	 * only aborts it cleanly if the owner can never click GO (logged off,
	 * companion died/went into combat, etc.). Self-reschedules every 5s
	 * for as long as awaitingGoConfirm stays true. */
	static void goWatchdogTick(ZoneServer* zoneServer, Reference<CreatureObject*> ownerRef, Reference<CompanionTheLandingState*> state) {
		if (state == nullptr || !state->awaitingGoConfirm) {
			return; // GO already clicked, or already aborted -- stop polling.
		}

		CreatureObject* owner = ownerRef.get();

		if (owner == nullptr) {
			return; // matches runPhase()'s own existing precedent: nothing safe to do.
		}

		if (owner->getZone() == nullptr || owner->isInCombat()) {
			abortPendingGo(zoneServer, owner, state);
			return;
		}

		Zone* zone = owner->getZone();
		int aliveCount = 0;

		// THELANDING_FILLER_CAST_2026_07_30 -- generic CreatureObject* check (not
		// CompanionObject*-specific) so a filler cast member counts as
		// "alive" too; isDead()/isInCombat()/getZone() are all
		// CreatureObject-level and apply identically to a real companion
		// (CompanionObject IS-A CreatureObject) and a filler AiAgent (also
		// IS-A CreatureObject).
		for (int i = 0; i < state->castMembers.size(); ++i) {
			ManagedReference<SceneObject*> obj = zoneServer->getObject(state->castMembers.get(i).get()->companionID);
			CreatureObject* member = obj != nullptr ? obj.castTo<CreatureObject*>().get() : nullptr;

			if (member != nullptr && member->getZone() == zone && !member->isDead() && !member->isInCombat()) {
				++aliveCount;
			}
		}

		if (aliveCount == 0) {
			abortPendingGo(zoneServer, owner, state);
			return;
		}

		scheduleGoWatchdog(zoneServer, ownerRef, state, 5000);
	}

	/** Cleanly ends the show early because the pending GO confirm can
	 * never be resolved (see goWatchdogTick()) -- best-effort closes the
	 * SuiMessageBox if it's still open, then reuses finishShow() exactly
	 * like every other early-abort path in this file already does. */
	static void abortPendingGo(ZoneServer* zoneServer, CreatureObject* owner, Reference<CompanionTheLandingState*> state) {
		state->awaitingGoConfirm = false;

		if (owner != nullptr) {
			ManagedReference<PlayerObject*> ghost = owner->getPlayerObject();

			if (ghost != nullptr) {
				ghost->closeSuiWindowType(SuiWindowType::COMPANION_THEATER_GO_CONFIRM);
			}
		}

		finishShow(zoneServer, owner, state);
	}

	/** @pre { player and every resolved companion crosslocked to the
	 * player at call time (matches CompanionCraftTheater::begin()'s own
	 * documented precondition) } */
	static void start(CreatureObject* owner, Vector<ManagedReference<CompanionObject*> >& companions) {
		if (owner == nullptr || owner->getZone() == nullptr || companions.size() == 0) {
			return;
		}

		// OVERLAPPING_THEATER_SHOWS_FIX_2026_07_30 -- per Nick, live freeze report: two
		// overlapping /companionthelanding runs on the same roster
		// deadlocked the server (two concurrent multi-companion beat
		// sequences fighting over the same companion locks in different
		// orders). Arms a generous 10-minute defensive ceiling on the
		// OWNER (not per-companion -- this must block ANY new show for
		// this player regardless of which companions they pick)
		// immediately; finishShow() clears it the instant the show
		// ACTUALLY finishes (the real signal -- this ceiling is only a
		// safety net in case finishShow() somehow never fires). Every
		// show-start entry point (doQueueCommand() below,
		// CompanionDialogMenuSuiCallback.h's Fireworks case, and
		// CompanionTheaterModeSuiCallback.h) checks this same flag
		// before ever reaching here.
		owner->updateCooldownTimer("companion_theater_mode_busy", 600000);

		Zone* zone = owner->getZone();
		ZoneServer* zoneServer = owner->getZoneServer();

		if (zoneServer == nullptr) {
			return;
		}

		Reference<CompanionTheLandingState*> state = new CompanionTheLandingState();
		state->ownerID = owner->getObjectID();
		state->directorID = companions.get(0)->getObjectID();

		// Owner forward/perpendicular unit vectors + stage marks, computed
		// ONCE here from the owner's position/facing at show start (same
		// angle-to-unit-vector shape CompanionCraftTheater::
		// spawnFactoryProp() already uses).
		float angle = owner->getDirectionAngle() * (float) (M_PI / 180.0);
		state->fwdX = sin(angle);
		state->fwdY = cos(angle);
		state->perpX = state->fwdY;
		state->perpY = -state->fwdX;

		float ox = owner->getPositionX();
		float oy = owner->getPositionY();

		state->crateX = ox + state->fwdX * 12.f;
		state->crateY = oy + state->fwdY * 12.f;

		// Skirmish center 15m out; the two lines sit 17.5m apart straddling
		// it along the forward axis (within the design's 15-20m spec).
		float skirmishCenterX = ox + state->fwdX * 15.f;
		float skirmishCenterY = oy + state->fwdY * 15.f;
		state->centerX = skirmishCenterX;
		state->centerY = skirmishCenterY;

		const float halfGap = 8.75f; // lines end up 17.5m apart
		state->lineAX = skirmishCenterX - state->fwdX * halfGap;
		state->lineAY = skirmishCenterY - state->fwdY * halfGap;
		state->lineBX = skirmishCenterX + state->fwdX * halfGap;
		state->lineBY = skirmishCenterY + state->fwdY * halfGap;

		// Fallback marker: 10m further back behind line A (i.e. further
		// from line B, along the negative forward axis).
		state->markerX = state->lineAX - state->fwdX * 10.f;
		state->markerY = state->lineAY - state->fwdY * 10.f;

		// Suspend background ticks + snapshot posture for every cast
		// member, per the design's state-suspension spec.
		for (int i = 0; i < companions.size(); ++i) {
			CompanionObject* companion = companions.get(i);

			if (companion == nullptr || companion->isDead() || companion->getZone() != zone) {
				continue;
			}

			Locker clocker(companion, owner);

			Reference<CompanionTheLandingCastMember*> member = new CompanionTheLandingCastMember();
			member->companionID = companion->getObjectID();
			member->preShowPosture = companion->getPosture();
			state->castMembers.add(member);

			if (companion->isTaxiActive()) {
				companion->stopTaxiRide(false);
			}

			companion->setFollowObject(nullptr);
			companion->clearPatrolPoints();
			companion->setCompanionState(CompanionObject::THEATER);
			companion->updateCooldownTimer("companion_thelanding", 150000);
		}

		if (state->castMembers.size() == 0) {
			return;
		}

		// THELANDING_FILLER_CAST_2026_07_30 -- per Nick: "some players might not have
		// enough companions to make the play work with only 1
		// companion... spawn in npc and creatures to help with the play."
		// Tops the visible cast up to a fixed target size using temporary
		// filler NPCs (see spawnFillerCast()) that run through the exact
		// same beats as a real companion. Computed from the REAL roster
		// size only (before any fillers are added).
		int fillerNeeded = THELANDING_TARGET_CAST_SIZE - state->castMembers.size();

		if (fillerNeeded > 0) {
			spawnFillerCast(zoneServer, zone, state, fillerNeeded);
		}

		ManagedReference<SceneObject*> directorObj = zoneServer->getObject(state->directorID);
		CompanionObject* director = directorObj != nullptr ? directorObj.castTo<CompanionObject*>().get() : nullptr;

		// Cosmetic-only disguise (2026-07-30, "The Landing" director
		// mouse-droid pass): re-stamp the director's CLIENT template CRC so
		// it visually renders as a mouse droid to everyone nearby for the
		// rest of the show -- the object underneath is untouched, still the
		// same real CompanionObject the whole time (same AI/inventory/
		// everything). Exact same setClientObjectCRC() + TemplateManager::
		// instance()->getTemplate() mechanism CompanionObjectImplementation.cpp's
		// startTaxiRide() uses to make a taxi driver LOOK like the mimicked
		// vehicle. The pre-show CRC is snapshotted first (rather than
		// re-deriving the normal humanoid CRC by hand) because no shared
		// "restore appearance to default" helper exists in this codebase to
		// call at the finale instead -- see finishShow()'s matching restore.
		if (director != nullptr) {
			Locker directorDisguiseLocker(director, owner);

			state->directorPreShowClientCRC = director->getClientObjectCRC();

			SharedObjectTemplate* mouseDroidTemplateData = TemplateManager::instance()->getTemplate(STRING_HASHCODE(THELANDING_DIRECTOR_DISGUISE_TEMPLATE));

			if (mouseDroidTemplateData != nullptr) {
				director->setClientObjectCRC(mouseDroidTemplateData->getClientObjectCRC());
				state->directorDisguised = true;
			}
		}

		say(director, "Gather round -- we've got something for you.");

		// GO-button confirm (2026-07-30, per Nick): phase 1
		// (beatCrateConverge, the 0:10-0:25 crate-converge beat) no longer
		// fires automatically on a 10s timer -- promptGo() shows the owner
		// a SuiMessageBox and only schedules phase 1 once they click GO
		// (see CompanionTheLandingGoSuiCallback.h).
		promptGo(zoneServer, owner, state);
	}

	/** Single dispatcher for every scheduled beat -- keeps the whole
	 * fixed-timeline shape in one place rather than one function per
	 * phase number. Each phase re-resolves the owner + every cast member
	 * fresh (never trusts a raw pointer across the delay), matching this
	 * project's documented scheduled-lambda safety rules. */
	static void runPhase(int phase, ZoneServer* zoneServer, Reference<CreatureObject*> ownerRef, Reference<CompanionTheLandingState*> state) {
		CreatureObject* owner = ownerRef.get();

		if (owner == nullptr || state == nullptr || zoneServer == nullptr) {
			return;
		}

		Zone* zone = owner->getZone();

		if (zone == nullptr || owner->isInCombat()) {
			finishShow(zoneServer, owner, state);
			return;
		}

		// If every cast member has died/despawned/gone into combat since
		// the last beat, cut the show short rather than talking to an
		// empty field. THELANDING_FILLER_CAST_2026_07_30: generic CreatureObject*
		// check (not CompanionObject*-specific) so a filler cast member
		// counts as "alive" too -- see goWatchdogTick()'s matching comment.
		int aliveCount = 0;

		for (int i = 0; i < state->castMembers.size(); ++i) {
			ManagedReference<SceneObject*> obj = zoneServer->getObject(state->castMembers.get(i).get()->companionID);
			CreatureObject* member = obj != nullptr ? obj.castTo<CreatureObject*>().get() : nullptr;

			if (member != nullptr && member->getZone() == zone && !member->isDead() && !member->isInCombat()) {
				++aliveCount;
			}
		}

		if (aliveCount == 0) {
			finishShow(zoneServer, owner, state);
			return;
		}

		switch (phase) {
		case 1:
			beatCrateConverge(zoneServer, owner, state);
			break;
		case 2:
			beatSkirmishSetup(zoneServer, owner, state);
			break;
		case 3:
			runSkirmishSubBeat(zoneServer, ownerRef, state);
			break;
		case 4:
			beatResolution(zoneServer, owner, state);
			break;
		case 5:
			beatResolutionPerform(zoneServer, owner, state);
			break;
		case 6:
			beatFinale(zoneServer, owner, state);
			break;
		case 7:
			finishShow(zoneServer, owner, state);
			break;
		default:
			finishShow(zoneServer, owner, state);
			break;
		}
	}

	// ---- 0:10-0:25 -- supply crate spawns, 2-3 companions converge -------
	static void beatCrateConverge(ZoneServer* zoneServer, CreatureObject* owner, Reference<CompanionTheLandingState*> state) {
		Zone* zone = owner->getZone();

		state->cratePropID = spawnProp(zoneServer, zone, THELANDING_CRATE_TEMPLATE, state->crateX, state->crateY);

		// Up to 3 (at least 2 if the roster allows) converge on the crate,
		// each given a DISTINCT arrival angle around it so they visually
		// spread in rather than stack on the same tile.
		int convergeCount = Math::min(3, state->castMembers.size());
		const float radius = 2.5f;

		for (int i = 0; i < convergeCount; ++i) {
			ManagedReference<SceneObject*> obj = zoneServer->getObject(state->castMembers.get(i).get()->companionID);
			AiAgent* member = obj != nullptr ? obj.castTo<AiAgent*>().get() : nullptr;

			if (member == nullptr || member->getZone() != zone || member->isDead()) {
				continue;
			}

			Locker clocker(member, owner);

			float memberAngle = (2.0f * (float) M_PI * i) / (float) convergeCount;
			float tx = state->crateX + sin(memberAngle) * radius;
			float ty = state->crateY + cos(memberAngle) * radius;

			walkTo(member, zone, tx, ty);
		}

		// Reaction-beat animation on arrival -- fixed-timer, not
		// arrival-polled (see file header). "bow" per design spec (safest
		// of the two confirmed choices).
		ManagedReference<CreatureObject*> ownerRef2 = owner;

		Core::getTaskManager()->scheduleTask([zoneServer, ownerRef2, state] () {
			CreatureObject* o = ownerRef2.get();

			if (o == nullptr) {
				return;
			}

			int convergeCount2 = Math::min(3, state->castMembers.size());

			for (int i = 0; i < convergeCount2; ++i) {
				ManagedReference<SceneObject*> obj = zoneServer->getObject(state->castMembers.get(i).get()->companionID);
				AiAgent* member = obj != nullptr ? obj.castTo<AiAgent*>().get() : nullptr;

				if (member == nullptr || member->isDead()) {
					continue;
				}

				Locker clocker(member);
				member->doAnimation("bow");
			}
		}, "CompanionTheLandingCrateArrivalLambda", 8000);

		Reference<CreatureObject*> ownerRef = owner;
		scheduleStep(2, zoneServer, ownerRef, state, 15000); // 0:10 -> 0:25
	}

	// ---- 0:25 -- form up into two facing lines, kick off the skirmish ----
	static void beatSkirmishSetup(ZoneServer* zoneServer, CreatureObject* owner, Reference<CompanionTheLandingState*> state) {
		Zone* zone = owner->getZone();

		state->markerPropID = spawnProp(zoneServer, zone, THELANDING_MARKER_TEMPLATE, state->markerX, state->markerY);

		int total = state->castMembers.size();
		int halfA = (total + 1) / 2; // roster split roughly in half

		for (int i = 0; i < total; ++i) {
			ManagedReference<SceneObject*> obj = zoneServer->getObject(state->castMembers.get(i).get()->companionID);
			AiAgent* member = obj != nullptr ? obj.castTo<AiAgent*>().get() : nullptr;

			if (member == nullptr || member->getZone() != zone || member->isDead()) {
				continue;
			}

			Locker clocker(member, owner);

			bool inLineA = i < halfA;
			int indexInLine = inLineA ? i : (i - halfA);
			int lineSize = inLineA ? halfA : (total - halfA);
			float spread = ((float) indexInLine - ((float) (lineSize - 1)) / 2.0f) * 3.f; // 3m spacing

			float baseX = inLineA ? state->lineAX : state->lineBX;
			float baseY = inLineA ? state->lineAY : state->lineBY;

			float tx = baseX + state->perpX * spread;
			float ty = baseY + state->perpY * spread;

			walkTo(member, zone, tx, ty);
		}

		// "Weapon-ready" beat -- "bow" stand-in per design spec (no
		// confirmed real "ready pose" animation string found).
		for (int i = 0; i < total; ++i) {
			ManagedReference<SceneObject*> obj = zoneServer->getObject(state->castMembers.get(i).get()->companionID);
			AiAgent* member = obj != nullptr ? obj.castTo<AiAgent*>().get() : nullptr;

			if (member == nullptr || member->isDead()) {
				continue;
			}

			Locker clocker(member);
			member->doAnimation("bow");
		}

		state->skirmishBeat = 0;
		Reference<CreatureObject*> ownerRef = owner;
		scheduleStep(3, zoneServer, ownerRef, state, 9000);
	}

	// ---- 0:25-1:10 -- 5 sub-beats (9s each = 45s) of cover-fire / kneel /
	// fallback, then hand off to the resolution beat. -----------------------
	static void runSkirmishSubBeat(ZoneServer* zoneServer, Reference<CreatureObject*> ownerRef, Reference<CompanionTheLandingState*> state) {
		CreatureObject* owner = ownerRef.get();

		if (owner == nullptr) {
			return;
		}

		Zone* zone = owner->getZone();
		int total = state->castMembers.size();
		int beat = state->skirmishBeat;

		for (int i = 0; i < total; ++i) {
			ManagedReference<SceneObject*> obj = zoneServer->getObject(state->castMembers.get(i).get()->companionID);
			AiAgent* member = obj != nullptr ? obj.castTo<AiAgent*>().get() : nullptr;

			if (member == nullptr || member->getZone() != zone || member->isDead()) {
				continue;
			}

			Locker clocker(member, owner);

			// Staggered cover-fire: alternate which half of the roster
			// "fires" each odd sub-beat, muzzle-flash stand-in via
			// FireworkObject::launch() -- exact same call
			// CompanionFireworksShow.h uses, opportunistic (see file
			// header). THELANDING_FILLER_CAST_2026_07_30: real companions only -- the
			// CompanionObject* cast below is always null for a filler cast
			// member (it isn't one), and collectFireworks() already no-ops
			// on a null companion, so this naturally does nothing for a
			// filler without any extra isFiller check needed.
			if ((beat == 1 || beat == 3) && (i % 2 == (beat == 1 ? 0 : 1))) {
				CompanionObject* companion = obj.castTo<CompanionObject*>().get();
				Vector<uint64> fireworks;
				collectFireworks(companion, fireworks);

				if (fireworks.size() > 0) {
					ManagedReference<SceneObject*> fireworkObj = zoneServer->getObject(fireworks.get(0));
					FireworkObject* firework = fireworkObj != nullptr ? fireworkObj.castTo<FireworkObject*>().get() : nullptr;

					if (firework != nullptr) {
						Locker flocker(firework, companion);
						firework->launch(companion, 10);
					}
				}
			}

			// Staggered kneel/stagger posture (never real "death") so the
			// field stays visually full for the whole runtime.
			if (beat == 1 && (i % 3 == 0)) {
				member->setPosture(CreaturePosture::CROUCHED, true, true);
			} else if (beat == 3 && (i % 3 == 0)) {
				member->setPosture(CreaturePosture::UPRIGHT, true, true);
			}

			// 1-2 cast members (first two of line A, indices 0/1) fall back
			// to the marker prop partway through, on sub-beat 2.
			if (beat == 2 && i < 2) {
				walkTo(member, zone, state->markerX, state->markerY);
			}
		}

		++state->skirmishBeat;

		if (state->skirmishBeat > 4) {
			Reference<CreatureObject*> ownerRef2 = owner;
			scheduleStep(4, zoneServer, ownerRef2, state, 9000); // final sub-beat -> resolution
		} else {
			Reference<CreatureObject*> ownerRef2 = owner;
			scheduleStep(3, zoneServer, ownerRef2, state, 9000);
		}
	}

	// ---- 1:10 -- lines converge toward center, banner raised -------------
	static void beatResolution(ZoneServer* zoneServer, CreatureObject* owner, Reference<CompanionTheLandingState*> state) {
		Zone* zone = owner->getZone();

		state->bannerPropID = spawnProp(zoneServer, zone, THELANDING_BANNER_TEMPLATE, state->centerX, state->centerY);

		int total = state->castMembers.size();
		const float radius = 3.0f;

		for (int i = 0; i < total; ++i) {
			ManagedReference<SceneObject*> obj = zoneServer->getObject(state->castMembers.get(i).get()->companionID);
			AiAgent* member = obj != nullptr ? obj.castTo<AiAgent*>().get() : nullptr;

			if (member == nullptr || member->getZone() != zone || member->isDead()) {
				continue;
			}

			Locker clocker(member, owner);

			// Stand everyone back up first -- launch()'s delayed event may
			// have left some crouched (same re-assert FireworksShow does).
			member->setPosture(CreaturePosture::UPRIGHT, true, true);

			float memberAngle = (2.0f * (float) M_PI * i) / (float) Math::max(1, total);
			float tx = state->centerX + sin(memberAngle) * radius;
			float ty = state->centerY + cos(memberAngle) * radius;

			walkTo(member, zone, tx, ty);
			member->doAnimation("bow"); // holster/relax beat
		}

		Reference<CreatureObject*> ownerRef = owner;
		scheduleStep(5, zoneServer, ownerRef, state, 15000); // give 15s to converge before the group performance
	}

	// ---- 1:25ish -- group performance combo (confirmed real combo, see
	// CampDeploymentManager.cpp:1402-1404) --------------------------------
	static void beatResolutionPerform(ZoneServer* zoneServer, CreatureObject* owner, Reference<CompanionTheLandingState*> state) {
		Zone* zone = owner->getZone();
		int total = state->castMembers.size();

		for (int i = 0; i < total; ++i) {
			ManagedReference<SceneObject*> obj = zoneServer->getObject(state->castMembers.get(i).get()->companionID);
			AiAgent* member = obj != nullptr ? obj.castTo<AiAgent*>().get() : nullptr;

			if (member == nullptr || member->getZone() != zone || member->isDead()) {
				continue;
			}

			Locker clocker(member, owner);

			if (member->getPosture() != CreaturePosture::UPRIGHT) {
				member->setPosture(CreaturePosture::UPRIGHT, true, true);
			}

			// genesis port: dropped ->setPerformanceType(PerformanceType::DANCE, true) -- genesis's
			// CreatureObject.idl has no performanceType field (only performanceAnimation /
			// performanceCounter). setPerformanceAnimation() is genesis's real dance-visual
			// API (EntertainingSessionImplementation::sendEntertainingUpdate()) and already
			// carries this beat on its own.
			member->setPerformanceAnimation("exotic4", true);
			member->doAnimation("skill_action_1");
		}

		Reference<CreatureObject*> ownerRef = owner;
		scheduleStep(6, zoneServer, ownerRef, state, 15000); // rest of the 1:10-1:40 window
	}

	// ---- 1:40 -- finale: face the player, one beat, closing line ---------
	static void beatFinale(ZoneServer* zoneServer, CreatureObject* owner, Reference<CompanionTheLandingState*> state) {
		Zone* zone = owner->getZone();
		int total = state->castMembers.size();

		for (int i = 0; i < total; ++i) {
			ManagedReference<SceneObject*> obj = zoneServer->getObject(state->castMembers.get(i).get()->companionID);
			AiAgent* member = obj != nullptr ? obj.castTo<AiAgent*>().get() : nullptr;

			if (member == nullptr || member->getZone() != zone || member->isDead()) {
				continue;
			}

			Locker clocker(member, owner);

			// End the dance performance mode before the closing pose.
			// genesis port: dropped ->setPerformanceType(0, true) -- genesis's
			// CreatureObject.idl has no performanceType field (only performanceAnimation /
			// performanceCounter). setPerformanceAnimation() is genesis's real dance-visual
			// API (EntertainingSessionImplementation::sendEntertainingUpdate()) and already
			// carries this beat on its own.
			member->setPerformanceAnimation("", true);

			member->faceObject(owner, true);
			member->doAnimation("bow");
		}

		ManagedReference<SceneObject*> directorObj = zoneServer->getObject(state->directorID);
		CompanionObject* director = directorObj != nullptr ? directorObj.castTo<CompanionObject*>().get() : nullptr;

		say(director, "That's the show -- thanks for watching.");

		Reference<CreatureObject*> ownerRef = owner;
		scheduleStep(7, zoneServer, ownerRef, state, 20000); // 1:40 -> 2:00, let the pose/banner land before cleanup
	}

	/** Despawns all 3 tracked props and restores every cast member's
	 * pre-show posture + standingOrder-based state. Safe to call from any
	 * phase as an early-abort as well as the normal 2:00 finish. */
	static void finishShow(ZoneServer* zoneServer, CreatureObject* owner, Reference<CompanionTheLandingState*> state) {
		// OVERLAPPING_THEATER_SHOWS_FIX_2026_07_30 -- the real "show is done" signal;
		// clears the busy flag armed in start() regardless of how long
		// the GO-wait + full playback actually took (this is the single
		// choke point every abort/finish path already funnels through,
		// per this function's own doc comment).
		if (owner != nullptr) {
			owner->updateCooldownTimer("companion_theater_mode_busy", 0);
		}

		despawnProp(zoneServer, state->cratePropID);
		despawnProp(zoneServer, state->markerPropID);
		despawnProp(zoneServer, state->bannerPropID);

		for (int i = 0; i < state->castMembers.size(); ++i) {
			CompanionTheLandingCastMember* member = state->castMembers.get(i).get();
			ManagedReference<SceneObject*> obj = zoneServer->getObject(member->companionID);

			if (obj == nullptr) {
				continue;
			}

			// THELANDING_FILLER_CAST_2026_07_30 -- a filler has no pre-show state to
			// restore (spawned solely for this run) -- clean it up
			// unconditionally here instead of the real-companion
			// restoration path below, which needs an actual CompanionObject
			// (standingOrder, companionState, etc. -- none of which a
			// filler AiAgent has). Not persisted to the DB in the first
			// place (see spawnFillerCast()), but destroyed from both world
			// and database here anyway to guarantee zero residue, matching
			// this project's own BountyHunterDroid.cpp precedent for a
			// genuinely temporary spawned creature.
			if (member->isFiller) {
				Locker fLocker(obj);
				obj->destroyObjectFromWorld(true);
				obj->destroyObjectFromDatabase(true);
				continue;
			}

			CompanionObject* companion = obj.castTo<CompanionObject*>().get();

			if (companion == nullptr || companion->isDead()) {
				continue;
			}

			Locker clocker(companion, owner);

			// THEATER_STRANDING_FIX_2026_07_30 -- companionState is cleared back to
			// FOLLOW unconditionally, BEFORE the zone-null check below,
			// because it's a plain field write that needs no valid zone.
			// Previously a cast member whose zone happened to be
			// transiently null right when the show ended (a zoning race,
			// not death) was skipped ENTIRELY here, permanently stranding
			// it at companionState == THEATER -- runKeepUpTick() bails out
			// of every background sub-tick (medic auto-care included)
			// forever while THEATER, and both CompanionFollowCommand.h's
			// and FormationManager::arrangeFollowers()'s own gather loops
			// also require getZone() != nullptr, so a zone-null companion
			// was invisible to ordinary Follow/Formup too -- nothing left
			// could ever pull it out of THEATER again. Now, the moment its
			// zone comes back, it's already off THEATER and reachable by
			// ordinary Follow/Formup again.
			companion->setCompanionState(CompanionObject::FOLLOW);

			if (companion->getZone() == nullptr) {
				continue;
			}

			int posture = member->preShowPosture;

			if (posture < 0 || posture > CreaturePosture::DEAD) {
				posture = CreaturePosture::UPRIGHT;
			}

			companion->setPosture(posture, true, true);
			// genesis port: dropped ->setPerformanceType(0, true) -- genesis's
			// CreatureObject.idl has no performanceType field (only performanceAnimation /
			// performanceCounter). setPerformanceAnimation() is genesis's real dance-visual
			// API (EntertainingSessionImplementation::sendEntertainingUpdate()) and already
			// carries this beat on its own.
			companion->setPerformanceAnimation("", true);
			restoreStanding(companion, owner);

			// Mouse-droid disguise restore (2026-07-30): mirrors the exact same
			// restore-on-any-exit-path rigor as the posture/companionState
			// restore above -- finishShow() is the single choke point called
			// from every abort path this show has (combat interruption,
			// all-cast-dead early cutoff, and the normal 2:00 finish -- see
			// runPhase()), so the director's real appearance is never left
			// stuck as a mouse droid regardless of how the show ends.
			if (state->directorDisguised && member->companionID == state->directorID) {
				companion->setClientObjectCRC(state->directorPreShowClientCRC);
				state->directorDisguised = false;
			}
		}
	}

};

// ORDERING: must be included only here, after CompanionTheLandingState
// and CompanionTheLandingShow are both fully defined above -- see the
// header's own ORDERING note for why (it deliberately does not include
// this file back, to avoid a circular include).
#include "server/zone/managers/companion/callbacks/CompanionTheLandingGoSuiCallback.h"

inline void CompanionTheLandingShow::promptGo(ZoneServer* zoneServer, CreatureObject* owner, Reference<CompanionTheLandingState*> state) {
	if (owner == nullptr || zoneServer == nullptr || state == nullptr) {
		return;
	}

	if (!owner->isPlayerCreature() || owner->getPlayerObject() == nullptr) {
		// No player ghost to pop a SUI box on -- fall back to the old
		// automatic behavior rather than stranding the cast in THEATER
		// forever waiting for a confirmation that can never arrive.
		Reference<CreatureObject*> ownerRef = owner;
		scheduleStep(1, zoneServer, ownerRef, state, 10000);
		return;
	}

	state->awaitingGoConfirm = true;

	ManagedReference<SuiMessageBox*> suiBox = new SuiMessageBox(owner, SuiWindowType::COMPANION_THEATER_GO_CONFIRM);
	suiBox->setPromptTitle("The Landing -=COMPANION=- : Ready?");
	suiBox->setPromptText("Everyone's in position. Press GO when you're ready to start the show.");
	suiBox->setCancelButton(false, "");
	suiBox->setOkButton(true, "GO");
	suiBox->setCallback(new CompanionTheLandingGoSuiCallback(zoneServer, owner->getObjectID(), state.get()));

	owner->getPlayerObject()->addSuiBox(suiBox);
	owner->sendMessage(suiBox->generateMessage());

	Reference<CreatureObject*> ownerRef = owner;
	scheduleGoWatchdog(zoneServer, ownerRef, state, 5000);
}

class CompanionTheLandingCommand : public QueueCommand {
public:

	CompanionTheLandingCommand(const String& name, ZoneProcessServer* server)
		: QueueCommand(name, server) {

	}

	/** Duplicated (rather than shared) from CompanionAttackCommand.h's
	 * identical helper -- this project deliberately keeps each command
	 * file self-contained rather than sharing this scan. Resolves EVERY
	 * summoned, living companion linked to the player, matching every
	 * other Companion*Command in this codebase. */
	void resolveActiveCompanions(CreatureObject* player, Vector<ManagedReference<CompanionObject*> >& companions) const {
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

			CompanionObject* companion = device->getCompanionObject();

			if (companion == nullptr || companion->getZone() == nullptr) {
				continue;
			}

			if (companion->getLinkedCreature().get() != player) {
				continue;
			}

			companions.add(companion);
		}
	}

	int doQueueCommand(CreatureObject* creature, const uint64& target, const UnicodeString& arguments) const {
		if (!checkStateMask(creature)) {
			return INVALIDSTATE;
		}

		if (!checkInvalidLocomotions(creature)) {
			return INVALIDLOCOMOTION;
		}

		if (creature->isInCombat()) {
			creature->sendSystemMessage("Not while there's a fight going on!");
			return GENERALERROR;
		}

		// OVERLAPPING_THEATER_SHOWS_FIX_2026_07_30 -- refuse a new show while one is
		// already active for this owner (see start()'s matching arm and
		// finishShow()'s matching clear).
		if (!creature->checkCooldownRecovery("companion_theater_mode_busy")) {
			creature->sendSystemMessage("A theater show is already in progress -- try again once it's finished.");
			return GENERALERROR;
		}

		Vector<ManagedReference<CompanionObject*> > companions;
		resolveActiveCompanions(creature, companions);

		if (companions.size() == 0) {
			creature->sendSystemMessage("@companion:no_active_companion"); // You have no active companion.
			return GENERALERROR;
		}

		for (int i = 0; i < companions.size(); ++i) {
			CompanionObject* companion = companions.get(i);

			if (companion == nullptr) {
				continue;
			}

			if (!companion->checkCooldownRecovery("companion_thelanding")) {
				creature->sendSystemMessage("Your companions are still recovering from the last show.");
				return GENERALERROR;
			}

			if (companion->isInCombat()) {
				creature->sendSystemMessage("Not while there's a fight going on!");
				return GENERALERROR;
			}
		}

		CompanionTheLandingShow::start(creature, companions);

		return SUCCESS;
	}
};

#endif // COMPANIONTHELANDINGCOMMAND_H_
