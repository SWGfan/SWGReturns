-- Jedi command-browser repair.
--
-- Commands are registered in bin/scripts/commands/commands.lua, but several
-- skill boxes did not grant them. This migration is safe to run more than
-- once: each command is appended only when it is absent.

-- One-handed lightsaber finishers and defensive Force abilities.
UPDATE skills
SET commands = CONCAT_WS(',', NULLIF(TRIM(BOTH ',' FROM commands), ''),
  IF(FIND_IN_SET('saber1hComboHit1', commands) = 0, 'saber1hComboHit1', NULL),
  IF(FIND_IN_SET('saber1hComboHit2', commands) = 0, 'saber1hComboHit2', NULL),
  IF(FIND_IN_SET('saber1hComboHit3', commands) = 0, 'saber1hComboHit3', NULL),
  IF(FIND_IN_SET('saber1hFlurry2', commands) = 0, 'saber1hFlurry2', NULL),
  IF(FIND_IN_SET('saber1hHeadHit1', commands) = 0, 'saber1hHeadHit1', NULL),
  IF(FIND_IN_SET('saber1hHeadHit2', commands) = 0, 'saber1hHeadHit2', NULL),
  IF(FIND_IN_SET('saber1hHeadHit3', commands) = 0, 'saber1hHeadHit3', NULL),
  IF(FIND_IN_SET('forceMeditate', commands) = 0, 'forceMeditate', NULL),
  IF(FIND_IN_SET('forceResistBleeding', commands) = 0, 'forceResistBleeding', NULL),
  IF(FIND_IN_SET('forceResistStates', commands) = 0, 'forceResistStates', NULL))
WHERE name = 'jedi_padawan_master';

-- Two-handed lightsaber finishers, shared by both Light and Dark Journeymen.
UPDATE skills
SET commands = CONCAT_WS(',', NULLIF(TRIM(BOTH ',' FROM commands), ''),
  IF(FIND_IN_SET('saber2hSweep1', commands) = 0, 'saber2hSweep1', NULL),
  IF(FIND_IN_SET('saber2hSweep2', commands) = 0, 'saber2hSweep2', NULL),
  IF(FIND_IN_SET('saber2hSweep3', commands) = 0, 'saber2hSweep3', NULL),
  IF(FIND_IN_SET('saber2hPhantom', commands) = 0, 'saber2hPhantom', NULL))
WHERE name IN ('jedi_light_side_journeyman_master', 'jedi_dark_side_journeyman_master');

-- Double-bladed lightsaber finishers, shared by both Light and Dark Masters.
UPDATE skills
SET commands = CONCAT_WS(',', NULLIF(TRIM(BOTH ',' FROM commands), ''),
  IF(FIND_IN_SET('saberPolearmLegHit1', commands) = 0, 'saberPolearmLegHit1', NULL),
  IF(FIND_IN_SET('saberPolearmLegHit2', commands) = 0, 'saberPolearmLegHit2', NULL),
  IF(FIND_IN_SET('saberPolearmLegHit3', commands) = 0, 'saberPolearmLegHit3', NULL),
  IF(FIND_IN_SET('saberPolearmSpinAttack2', commands) = 0, 'saberPolearmSpinAttack2', NULL),
  IF(FIND_IN_SET('saberPolearmSpinAttack3', commands) = 0, 'saberPolearmSpinAttack3', NULL),
  IF(FIND_IN_SET('forceWeaken1', commands) = 0, 'forceWeaken1', NULL))
WHERE name IN ('jedi_light_side_master_master', 'jedi_dark_side_master_master');

-- Sentinel / FRS: do not grant Force Run 1 a second time. Upgrade that box to
-- Force Run 2 and expose the final Force knockdown.
UPDATE skills
SET commands = CONCAT_WS(',',
  NULLIF(TRIM(BOTH ',' FROM REPLACE(commands, 'forceRun1', '')), ''),
  IF(FIND_IN_SET('forceRun2', commands) = 0, 'forceRun2', NULL),
  IF(FIND_IN_SET('forceKnockdown3', commands) = 0, 'forceKnockdown3', NULL))
WHERE name IN ('force_rank_light_rank_01', 'force_rank_dark_rank_01');
