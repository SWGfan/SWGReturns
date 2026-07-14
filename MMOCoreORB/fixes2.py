#!/usr/bin/env python3

import re
from pathlib import Path

CPP = Path("MMOCoreORB/src/server/zone/objects/player/PlayerObjectImplementation.cpp")

text = CPP.read_text(encoding="utf-8")

#
# Fix increaseFactionStanding()
#
text = re.sub(
    r'//float newAmount = currentAmount \+ amount;',
    r'float newAmount = currentAmount + amount;',
    text
)

text = re.sub(
    r'if \(!factionStandingList\.isPvpFaction\(factionName\)\)\s*\n\s*newAmount = newAmount;\s*\n\s*else if \(player->getFaction\(\) == factionName\.hashCode\(\)\)\s*\n\s*newAmount = newAmount;.*?\n\s*else\s*\n\s*newAmount = newAmount;',
    r'// Unlimited faction points (cap removed)',
    text,
    flags=re.S
)

#
# Fix decreaseFactionStanding()
#
text = re.sub(
    r'if \(factionStandingList\.isPvpFaction\(factionName\)\) \{\s*\n\s*if \(player->getFaction\(\) == factionName\.hashCode\(\)\)\s*\n\s*newAmount = newAmount;.*?\n\s*else\s*\n\s*newAmount = newAmount;\s*\n\s*\}',
    r'// Unlimited faction points (cap removed)',
    text,
    flags=re.S
)

#
# Fix setFactionStanding()
#
text = re.sub(
    r'newAmount = newAmount;\s*\n\s*if \(factionStandingList\.isPvpFaction\(factionName\)\) \{\s*\n\s*if \(player->getFaction\(\) == factionName\.hashCode\(\)\)\s*\n\s*newAmount = newAmount;.*?\n\s*else\s*\n\s*newAmount = newAmount;\s*\n\s*\}',
    r'// Unlimited faction points (cap removed)',
    text,
    flags=re.S
)

CPP.write_text(text, encoding="utf-8")

print("Faction caps removed successfully.")
