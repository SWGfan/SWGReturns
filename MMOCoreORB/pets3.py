#!/usr/bin/env python3

from pathlib import Path
import shutil
import re

CPP = Path("src/server/zone/objects/creature/CreatureObjectImplementation.cpp")

if not CPP.exists():
    print("ERROR: Cannot find:")
    print(CPP)
    exit(1)

BACKUP = CPP.with_suffix(".cpp.phase709a.bak")

if not BACKUP.exists():
    shutil.copy2(CPP, BACKUP)
    print("Created backup:", BACKUP)

text = CPP.read_text(encoding="utf-8")

pattern = re.compile(
    r'if\s*\(\s*object->isPet\(\)\s*\)\s*\{.*?return\s+isAttackableBy\(owner\);\s*\n\s*\}',
    re.DOTALL
)

replacement = r'''if (object->isPet()) {

                        // ====================================================
                        // Phase 7.09a
                        // Pets may attack any AI creature.
                        // PvP against players still follows owner's rules.
                        // ====================================================

                        if (!isPlayerCreature())
                                return true;

                        ManagedReference<CreatureObject*> owner = object->getLinkedCreature().get();

                        if (owner == nullptr)
                                return false;

                        ManagedReference<PetControlDevice*> controlDevice =
                                object->getControlDevice().get().castTo<PetControlDevice*>();

                        if (controlDevice != nullptr) {
                                ManagedReference<SceneObject*> lastCommander =
                                        controlDevice->getLastCommander().get();

                                if (lastCommander != nullptr &&
                                    lastCommander != owner &&
                                    lastCommander->isCreatureObject()) {

                                        return isAttackableBy(lastCommander->asCreatureObject());
                                }
                        }

                        return isAttackableBy(owner);
                }'''

newtext, count = pattern.subn(replacement, text, count=1)

if count == 0:
    print("ERROR: Could not find pet block.")
    exit(1)

CPP.write_text(newtext, encoding="utf-8")

print()
print("=" * 72)
print("Phase 7.09a complete")
print("=" * 72)
print("Blocks patched:", count)
print()
print("Rebuild:")
print("cd build")
print("make -j$(nproc)")
