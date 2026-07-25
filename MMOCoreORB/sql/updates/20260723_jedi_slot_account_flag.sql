-- Adds the persistent "bonus Jedi character slot" flag to accounts, used by
-- the holocron grind's silent final holocron to grant an extra character
-- creation slot without converting the player's existing character.
ALTER TABLE `swgemu`.`accounts` ADD COLUMN `has_unlocked_jedi_slot` TINYINT(1) NOT NULL DEFAULT '0' AFTER `salt`;
