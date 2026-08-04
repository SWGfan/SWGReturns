-- Companion follow-and-fight tree.  Added 2026-08-04 by the Companion port.
--
-- Identical to stock `stationarynoleash` EXCEPT the combat mover:
--   stationarynoleash uses CombatMove          = createClass(CombatMoveBase, Interrupt)
--   this uses            CombatMoveCreaturePet = createClass(CombatMoveBase, CreaturePetInterrupt)
--
-- Why that one substitution matters: CombatMoveBase overrides doAction only
-- (movebase.lua:112-114) and therefore inherits MoveBase:checkConditions --
-- the ONLY place in the AI system that calls shouldRetreat(256) then
-- leash().  So a companion on stationarynoleash fires once, the attack
-- branch runs, and leash() clears its follow object and sends it home.
--
-- CombatMoveCreaturePet (combatmove.lua) overrides checkConditions WITHOUT
-- the leash check, and its terminate() calls restoreFollowObject() when the
-- behaviour fails -- which is exactly "resume following the owner once the
-- fight ends".
--
-- Stock `stationarynoleash` is left untouched: quest encounters use it, and
-- editing it would change wild mob behaviour server-wide.

companionfollow = {
	{"root", "CompositeDefault", "none", SELECTORBEHAVIOR},
	{"attack", "CompositeDefault", "root", SEQUENCEBEHAVIOR},
	{"attack0", "GetTarget", "attack", BEHAVIOR},
	{"attack1", "SelectWeapon", "attack", BEHAVIOR},
	{"attack2", "SelectAttack", "attack", BEHAVIOR},
	{"attack3", "CombatMoveCreaturePet", "attack", BEHAVIOR},
	{"idle0", "MoveCreaturePet", "root", BEHAVIOR},
	{"idle1", "WaitDefault", "root", BEHAVIOR},
}

addAiTemplate("companionfollow", companionfollow)
