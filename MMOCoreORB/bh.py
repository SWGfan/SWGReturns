#!/usr/bin/env python3

from pathlib import Path
import shutil

patches = [
(
"src/server/zone/managers/combat/CombatManager.cpp",
'''if (attacker->hasSkill("force_title_jedi_rank_03"))
                                VisibilityManager::instance()->increaseVisibility(attacker, data.getCommand()->getVisMod()); // Give visibility''',
'''if (playerObject != nullptr && playerObject->isJedi())
                                VisibilityManager::instance()->increaseVisibility(attacker, data.getCommand()->getVisMod()); // Give visibility'''
),

(
"src/server/zone/objects/player/PlayerObjectImplementation.cpp",
'''if (playerCreature->hasSkill("force_title_jedi_rank_02")) {''',
'''if (isJedi()) {'''
)

]

patched = 0

for filename, old, new in patches:

    p = Path(filename)

    if not p.exists():
        print("Missing:", filename)
        continue

    text = p.read_text()

    if old not in text:
        print("Already patched or pattern not found:", filename)
        continue

    backup = Path(str(p) + ".jedi_bh.bak")

    if not backup.exists():
        shutil.copy2(p, backup)

    text = text.replace(old, new)

    p.write_text(text)

    print("Patched:", filename)
    patched += 1

print()
print("Applied", patched, "patch(es)")
