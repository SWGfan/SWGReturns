-- Conversations
includeFile("conversations.lua")

-- Dress Groups - Must be loaded before mobiles
includeFile("dressgroup/serverobjects.lua") 

-- Creatures
includeFile("corellia/serverobjects.lua")
includeFile("dantooine/serverobjects.lua")
includeFile("dathomir/serverobjects.lua")
includeFile("endor/serverobjects.lua")
includeFile("event/serverobjects.lua")
includeFile("herald/serverobjects.lua")
includeFile("lok/serverobjects.lua")
includeFile("misc/serverobjects.lua")
includeFile("naboo/serverobjects.lua")
includeFile("pet/serverobjects.lua")
includeFile("quest/serverobjects.lua")
includeFile("rori/serverobjects.lua")
includeFile("space/serverobjects.lua")
includeFile("talus/serverobjects.lua")
includeFile("tatooine/serverobjects.lua")
includeFile("thug/serverobjects.lua")
includeFile("townsperson/serverobjects.lua")
includeFile("tutorial/serverobjects.lua")
includeFile("yavin4/serverobjects.lua")

includeFile("faction/serverobjects.lua")
includeFile("dungeon/serverobjects.lua") 

-- Weapons
includeFile("weapon/serverobjects.lua") 

-- Spawn Groups
includeFile("spawn/serverobjects.lua")

-- Trainer
includeFile("trainer/serverobjects.lua")

-- Mission
includeFile("mission/serverobjects.lua")

-- Lairs
includeFile("lair/serverobjects.lua")

-- Outfits
includeFile("outfits/serverobjects.lua")

-- Custom content - Loads last to allow for overrides
includeFile("../custom_scripts/mobile/serverobjects.lua")
includeFile("custom/serverobjects.lua")

-- === Custom Sith/Wampa set ===
-- Shared (must be first)
includeFile("shared/_boss_attacks.lua")

-- Generics
includeFile("generics/sith_acolyte.lua")
includeFile("generics/sith_lord.lua")
includeFile("generics/sith_boss.lua")
includeFile("generics/wampa_alpha.lua")

-- Legendaries
includeFile("legendaries/darth_revan.lua")
includeFile("legendaries/bastila_shan.lua")
includeFile("legendaries/darth_malak.lua")
includeFile("legendaries/emperor_palpatine.lua")
includeFile("legendaries/darth_vader.lua")
includeFile("legendaries/luke_skywalker_rotj.lua")
includeFile("legendaries/hk47.lua")
-- === End custom set ===
