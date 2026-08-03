-- Companion System (2026-07-15, "companion stops following / leashes back
-- home" fix -- see docs/companion_system/NOTES.md).
--
-- The companion used to run the generic wild-mobile trees (default.lua),
-- which are built for creatures that live at a fixed homeLocation: their
-- aware/idle logic leashes the creature back home whenever it strays too
-- far, and their movement runs at walkSpeed when out of combat. That is
-- exactly wrong for a player-owned companion: once the owner moved any
-- real distance, the companion got yanked back to its summon spot ("no
-- longer follows", "walks too slowly to keep up" -- both live reports).
--
-- These trees are modeled on ai/pet.lua (the real Creature Handler pet
-- trees, the proven design for "creature that follows an owner"), with
-- every PetControlDevice-dependent leaf (CheckPetCommand, PetReturn,
-- RestorePetPatrols -- all hard-gated on isPet(), always false for a
-- companion) replaced by CheckCompanionState, a dedicated leaf gating on
-- CompanionObject::getCompanionState() (see Checks.h/.cpp, AiMap.h).
-- State constants (CompanionObject.idl): FOLLOW = 1, PATROL = 2, STAY = 3.
--
-- Only the movement trio (AWARE / IDLE / MOVE) is overridden -- the
-- ROOT/TARGET/ATTACK/EQUIP/HEAL slots keep the default trees the companion
-- already uses successfully for combat/threat interception (AiMap's
-- customMap lookup falls back per-slot to the bitmask lookup). Wired up
-- via templates.lua's customMap ("companion") and
-- CompanionControlDeviceImplementation::spawnObject()'s setCustomAiMap().

-- AWARE: out of combat, not STAYing, not PATROLling -> follow the owner at
-- a run. Mirrors awarePet minus the PetReturn branch.
awareCompanion = {
	{id="910000001",	name="Sequence",	pid="none"},
	{id="910000002",	name="Not",	pid="910000001"},
	{id="910000003",	name="If",	pid="910000002"},
	{id="910000004",	name="CheckIsInCombat",	pid="910000003"},
	{id="910000005",	name="Not",	pid="910000001"},
	{id="910000006",	name="If",	pid="910000005"},
	{id="910000007",	name="CheckCompanionState",	pid="910000006",	args={condition=3}},
	{id="910000008",	name="Not",	pid="910000001"},
	{id="910000009",	name="If",	pid="910000008"},
	{id="910000010",	name="CheckMovementState",	pid="910000009",	args={condition=PATROLLING}},
	{id="910000011",	name="SetMovementState",	pid="910000001",	args={state=FOLLOWING}},
	{id="910000012",	name="WriteBlackboard",	pid="910000001",	args={key="moveMode", val=RUN}},
	{id="910000013",	name="TreeSocket",	pid="910000001",	args={slot=MOVE}}}
addAiTemplate("awareCompanion", awareCompanion)

-- IDLE: three branches, mirroring idlePet.
--   1. PATROLLING -> walk the player-set patrol route.
--   2. STAY (companionState 3) -> hold position; Leash anchors to the
--      homeLocation CompanionStayCommand sets at the stay spot, so after a
--      combat retreat the companion returns there, at a run.
--   3. Otherwise -> finish any pending movement at a run.
idleCompanion = {
	{id="920000001",	name="Selector",	pid="none"},
	{id="920000002",	name="Sequence",	pid="920000001"},
	{id="920000003",	name="If",	pid="920000002"},
	{id="920000004",	name="CheckMovementState",	pid="920000003",	args={condition=PATROLLING}},
	-- moveMode RUN (2026-07-15, taxi live feedback: PATROLLING is also the
	-- taxi's movement state, and WALK pace made the ride crawl -- companions
	-- now run their patrols too, matching their run-everywhere behavior).
	{id="920000005",	name="WriteBlackboard",	pid="920000002",	args={key="moveMode", val=RUN}},
	{id="920000006",	name="TreeSocket",	pid="920000002",	args={slot=MOVE}},
	{id="920000007",	name="Wait",	pid="920000002",	args={durationMax=10.0, durationMin=5.0}},
	{id="920000008",	name="Sequence",	pid="920000001"},
	{id="920000009",	name="If",	pid="920000008"},
	{id="920000010",	name="CheckCompanionState",	pid="920000009",	args={condition=3}},
	{id="920000011",	name="WriteBlackboard",	pid="920000008",	args={key="moveMode", val=RUN}},
	{id="920000012",	name="Leash",	pid="920000008"},
	{id="920000013",	name="TreeSocket",	pid="920000008",	args={slot=MOVE}},
	{id="920000014",	name="Sequence",	pid="920000001"},
	{id="920000015",	name="If",	pid="920000014"},
	{id="920000016",	name="CheckDestination",	pid="920000015",	args={condition=0.0}},
	{id="920000017",	name="WriteBlackboard",	pid="920000014",	args={key="moveMode", val=RUN}},
	{id="920000018",	name="AlwaysSucceed",	pid="920000014"},
	{id="920000019",	name="TreeSocket",	pid="920000018",	args={slot=MOVE}}}
addAiTemplate("idleCompanion", idleCompanion)

-- MOVE: mirrors movePet minus its owner-out-of-range/PetReturn branch
-- (PetReturn is a no-op FAILURE for a companion anyway).
moveCompanion = {
	{id="930000001",	name="Selector",	pid="none"},
	{id="930000002",	name="Not",	pid="930000001"},
	{id="930000003",	name="Sequence",	pid="930000002"},
	{id="930000004",	name="If",	pid="930000003"},
	{id="930000005",	name="CheckPosture",	pid="930000004",	args={condition=UPRIGHT}},
	{id="930000006",	name="If",	pid="930000003"},
	{id="930000007",	name="CheckDestination",	pid="930000006",	args={condition=0.0}},
	{id="930000008",	name="FindNextPosition",	pid="930000001"}}
addAiTemplate("moveCompanion", moveCompanion)
