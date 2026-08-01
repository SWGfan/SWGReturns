-- WESTAR-34: equippable at Master Bounty Hunter
-- Adds cert_pistol_bounty_hunter to combat_bountyhunter_master skill_commands.
-- Already applied directly via mysql on 2026-07-29.
UPDATE skills
SET skill_commands = CONCAT(skill_commands, chr(44) || chr(99) || chr(101) || chr(114) || chr(116) || chr(95) || chr(112) || chr(105) || chr(115) || chr(116) || chr(111) || chr(108) || chr(95) || chr(98) || chr(111) || chr(117) || chr(110) || chr(116) || chr(121) || chr(95) || chr(104) || chr(117) || chr(110) || chr(116) || chr(101) || chr(114))
WHERE skill_name = chr(99) || chr(111) || chr(109) || chr(98) || chr(97) || chr(116) || chr(95) || chr(98) || chr(111) || chr(117) || chr(110) || chr(116) || chr(121) || chr(104) || chr(117) || chr(110) || chr(116) || chr(101) || chr(114) || chr(95) || chr(109) || chr(97) || chr(115) || chr(116) || chr(101) || chr(114)
AND FIND_IN_SET(chr(99) || chr(101) || chr(114) || chr(116) || chr(95) || chr(112) || chr(105) || chr(115) || chr(116) || chr(111) || chr(108) || chr(95) || chr(98) || chr(111) || chr(117) || chr(110) || chr(116) || chr(121) || chr(95) || chr(104) || chr(117) || chr(110) || chr(116) || chr(101) || chr(114), skill_commands) = 0;
