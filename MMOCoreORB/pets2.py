#!/usr/bin/env python3

from pathlib import Path
import re

files = [
    "src/server/zone/objects/creature/CreatureObjectImplementation.cpp",
    "src/server/zone/objects/creature/ai/AiAgentImplementation.cpp",
]

patterns = [
    r'if\s*\(\s*pcd\s*!=\s*nullptr\s*&&\s*pcd->getPetType\(\)\s*==\s*PetManager::FACTIONPET\s*&&\s*object->isNeutral\(\)\s*\)\s*\{\s*return\s+false;\s*\}',
    r'if\s*\(\s*pcd\s*!=\s*nullptr\s*&&\s*pcd->getPetType\(\)\s*==\s*PetManager::FACTIONPET\s*&&\s*isNeutral\(\)\s*\)\s*\{\s*return\s+false;\s*\}',
    r'if\s*\(\s*pcd\s*!=\s*nullptr\s*&&\s*pcd->getPetType\(\)\s*==\s*PetManager::FACTIONPET\s*&&\s*object->isNeutral\(\)\s*\)\s*return\s+false;',
    r'if\s*\(\s*pcd\s*!=\s*nullptr\s*&&\s*pcd->getPetType\(\)\s*==\s*PetManager::FACTIONPET\s*&&\s*isNeutral\(\)\s*\)\s*return\s+false;',
]

for filename in files:
    path = Path(filename)

    if not path.exists():
        print(f"Missing: {filename}")
        continue

    text = path.read_text()

    original = text
    count = 0

    for pat in patterns:
        text, n = re.subn(
            pat,
            "// PATCH: faction pets may attack neutral targets",
            text,
            flags=re.MULTILINE | re.DOTALL,
        )
        count += n

    if count:
        backup = path.with_suffix(path.suffix + ".bak")
        backup.write_text(original)
        path.write_text(text)
        print(f"{filename}: patched {count} occurrence(s)")
    else:
        print(f"{filename}: nothing matched")

print("Done.")
