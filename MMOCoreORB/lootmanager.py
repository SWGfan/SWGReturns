#!/usr/bin/env python3
"""
fix_lootmanager.py
Fixes all compilation errors in LootManagerImplementation.cpp caused by
the Flurry codebase having a different API than StarDust-2.

Fixes applied:
  1. Stub applyRandomJediRobeMods  (LootItemTemplate methods missing)
  2. Stub applyRandomBackpackMods  (LootItemTemplate methods missing)
  3. Replace getMinimumLevel/getMaximumLevel with 0
  4. Remove usesRandomJediRobeMods/usesRandomBackpackMods from setSkillMods
  5. Add TransactionLog& trx to: createLoot(AiAgent), createLoot(String),
     createLootFromCollection, createLootSet
  6. Update internal loot calls to pass trx
  7. Add createLootAttachment + createNamedLoot to LootManager.idl
  8. Delete stale autogen files so idlc regenerates them

Usage:
  python3 fix_lootmanager.py <stardust_root>

Example:
  python3 fix_lootmanager.py /home/ubuntu/StarDust-2/MMOCoreORB
"""

import sys, os, shutil, datetime, re

if len(sys.argv) < 2:
    print(__doc__); sys.exit(1)

root = sys.argv[1].rstrip("/")
CPP  = f"{root}/src/server/zone/managers/loot/LootManagerImplementation.cpp"
IDL  = f"{root}/src/server/zone/managers/loot/LootManager.idl"
AUTOGEN_H   = f"{root}/src/autogen/server/zone/managers/loot/LootManager.h"
AUTOGEN_CPP = f"{root}/src/autogen/server/zone/managers/loot/LootManager.cpp"

for p in (CPP, IDL):
    if not os.path.isfile(p):
        sys.exit(f"ERROR: not found: {p}")

ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
shutil.copy2(CPP, f"{CPP}.lootfix_{ts}.bak")
shutil.copy2(IDL, f"{IDL}.lootfix_{ts}.bak")
print(f"Backed up .cpp and .idl")
print()

with open(CPP) as f:
    src = f.read()

changes = 0

def fix(src, old, new, label):
    if old not in src:
        print(f"  WARN ({label}): anchor not found")
        return src, 0
    result = src.replace(old, new, 1)
    print(f"  PATCHED: {label}")
    return result, 1

# ── 1. Stub applyRandomJediRobeMods ──────────────────────────────────────────
old1 = (
    '\tvoid applyRandomJediRobeMods(TangibleObject* object, const LootItemTemplate* templateObject) {\n'
    '\t\tif (!templateObject->usesRandomJediRobeMods() || !object->isWearableObject())\n'
    '\t\t\treturn;\n'
    '\n'
    '\t\tapplyRandomWearableModsFromPool(\n'
    '\t\t\tobject,\n'
    '\t\t\tjediRobeSkillMods,\n'
    '\t\t\tjediRobeSkillModCount,\n'
    '\t\t\ttemplateObject->getMinJediRobeMods(),\n'
    '\t\t\ttemplateObject->getMaxJediRobeMods(),\n'
    '\t\t\ttemplateObject->getMinJediRobeModValue(),\n'
    '\t\t\ttemplateObject->getMaxJediRobeModValue());\n'
    '\t}'
)
new1 = (
    '\tvoid applyRandomJediRobeMods(TangibleObject* object, const LootItemTemplate* templateObject) {\n'
    '\t\t// Flurry feature: jedi robe mods not supported in StarDust LootItemTemplate\n'
    '\t}'
)
src, n = fix(src, old1, new1, "stub applyRandomJediRobeMods"); changes += n

# ── 2. Stub applyRandomBackpackMods ──────────────────────────────────────────
old2 = (
    '\tvoid applyRandomBackpackMods(TangibleObject* object, const LootItemTemplate* templateObject) {\n'
    '\t\tif (!templateObject->usesRandomBackpackMods() || !object->isWearableObject())\n'
    '\t\t\treturn;\n'
    '\n'
    '\t\tint modPoolCount = 0;\n'
    '\t\tconst char** modPool = getBackpackModPool(templateObject->getBackpackModTheme(), modPoolCount);\n'
    '\n'
    '\t\tapplyRandomWearableModsFromPool(\n'
    '\t\t\tobject,\n'
    '\t\t\tmodPool,\n'
    '\t\t\tmodPoolCount,\n'
    '\t\t\ttemplateObject->getMinBackpackMods(),\n'
    '\t\t\ttemplateObject->getMaxBackpackMods(),\n'
    '\t\t\ttemplateObject->getMinBackpackModValue(),\n'
    '\t\t\ttemplateObject->getMaxBackpackModValue());\n'
    '\t}'
)
new2 = (
    '\tvoid applyRandomBackpackMods(TangibleObject* object, const LootItemTemplate* templateObject) {\n'
    '\t\t// Flurry feature: backpack mods not supported in StarDust LootItemTemplate\n'
    '\t}'
)
src, n = fix(src, old2, new2, "stub applyRandomBackpackMods"); changes += n

# ── 3. Replace getMinimumLevel/getMaximumLevel ────────────────────────────────
src, n = fix(src,
    '\tint minLevel = templateObject->getMinimumLevel();\n'
    '\tint maxLevel = templateObject->getMaximumLevel();',
    '\tint minLevel = 0; // Flurry: getMinimumLevel() not in StarDust LootItemTemplate\n'
    '\tint maxLevel = 0; // Flurry: getMaximumLevel() not in StarDust LootItemTemplate',
    "replace getMinimumLevel/getMaximumLevel with 0"
)
changes += n

# ── 4. Remove usesRandomJediRobeMods/usesRandomBackpackMods from setSkillMods ─
src, n = fix(src,
    'if (!templateObject->usesRandomJediRobeMods() && !templateObject->usesRandomBackpackMods() && System::random(skillModChance / modSqr) == 0) {',
    'if (System::random(skillModChance / modSqr) == 0) {',
    "remove usesRandomJediRobeMods/usesRandomBackpackMods from setSkillMods"
)
changes += n

# ── 5a. createLoot(SceneObject*, AiAgent*) signature + internal calls ─────────
src, n = fix(src,
    'bool LootManagerImplementation::createLoot(SceneObject* container, AiAgent* creature) {',
    'bool LootManagerImplementation::createLoot(TransactionLog& trx, SceneObject* container, AiAgent* creature) {',
    "createLoot(AiAgent) signature"
)
changes += n

# internal createLoot calls inside createLoot(AiAgent)
src = src.replace(
    '\t\t\t\tcreateLoot(container, "rarelootsystem", creatureLevel, false);',
    '\t\t\t\tcreateLoot(trx, container, "rarelootsystem", creatureLevel, false);')
src = src.replace(
    '\t\t\t\tcreateLoot(container, "lootcollectiontierdiamonds", creatureLevel, false);',
    '\t\t\t\tcreateLoot(trx, container, "lootcollectiontierdiamonds", creatureLevel, false);')
src = src.replace(
    '\treturn createLootFromCollection(container, lootCollection, creature->getLevel());',
    '\treturn createLootFromCollection(trx, container, lootCollection, creature->getLevel());')

# ── 5b. createLootFromCollection signature ────────────────────────────────────
src, n = fix(src,
    'bool LootManagerImplementation::createLootFromCollection(SceneObject* container, const LootGroupCollection* lootCollection, int level) {',
    'bool LootManagerImplementation::createLootFromCollection(TransactionLog& trx, SceneObject* container, const LootGroupCollection* lootCollection, int level) {',
    "createLootFromCollection signature"
)
changes += n

# internal createLoot calls inside createLootFromCollection
src = src.replace(
    '\t\t\tcreateLoot(container, lootGroup, level, false);',
    '\t\t\tcreateLoot(trx, container, lootGroup, level, false);')

# ── 5c. createLoot(SceneObject*, String, int, bool) signature ─────────────────
src, n = fix(src,
    'bool LootManagerImplementation::createLoot(SceneObject* container, const String& lootGroup, int level, bool maxCondition) {',
    'bool LootManagerImplementation::createLoot(TransactionLog& trx, SceneObject* container, const String& lootGroup, int level, bool maxCondition) {',
    "createLoot(String) signature"
)
changes += n

# ── 5d. createLootSet signature ───────────────────────────────────────────────
src, n = fix(src,
    'bool LootManagerImplementation::createLootSet(SceneObject* container, const String& lootGroup, int level, bool maxCondition, int setSize) {',
    'bool LootManagerImplementation::createLootSet(TransactionLog& trx, SceneObject* container, const String& lootGroup, int level, bool maxCondition, int setSize) {',
    "createLootSet signature"
)
changes += n

with open(CPP, 'w') as f:
    f.write(src)
print()

# ── 6. Add createLootAttachment + createNamedLoot to LootManager.idl ──────────
print("=== Patching LootManager.idl ===")
with open(IDL) as f:
    idl = f.read()

# Add after createLootObject declaration
new_decls = (
    '\n\t// Flurry additions: createLootAttachment and createNamedLoot\n'
    '\tpublic native TangibleObject createLootAttachment(LootItemTemplate templateObject, final string modName, int value);\n'
    '\tpublic native boolean createNamedLoot(SceneObject container, final string lootGroup, final string name, int level, boolean maxCondition);\n'
)

anchor = 'public native TangibleObject createLootObject(final LootItemTemplate templateObject, int level, boolean maxCondition = false);'
if 'createLootAttachment' in idl:
    print("  SKIP: declarations already in IDL")
elif anchor in idl:
    idl = idl.replace(anchor, anchor + new_decls)
    with open(IDL, 'w') as f:
        f.write(idl)
    print("  PATCHED: added createLootAttachment + createNamedLoot to LootManager.idl")
else:
    print("  WARN: IDL anchor not found — add these manually to LootManager.idl:")
    print("    " + new_decls.strip())

# ── 7. Delete stale autogen so idlc regenerates ───────────────────────────────
print()
print("=== Clearing stale autogen files ===")
for f in (AUTOGEN_H, AUTOGEN_CPP):
    if os.path.isfile(f):
        os.remove(f)
        print(f"  Deleted: {f}")
    else:
        print(f"  Already gone: {f}")

print()
print(f"=== Summary: {changes} C++ patches applied ===")
print("Recompile with: cd <build_dir> && make -j4")
