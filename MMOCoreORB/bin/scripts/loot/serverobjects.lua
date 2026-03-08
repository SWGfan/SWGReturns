-- Loot System Server Objects
-- Merged: Base + Custom Loot
-- Load order: Items first, then Groups

print("Loading loot system...")

-- Load all loot items
includeFile("items.lua")

-- Load all loot groups  
includeFile("groups.lua")

print("Loot system loaded: Items and Groups merged successfully")
