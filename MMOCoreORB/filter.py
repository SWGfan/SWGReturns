#!/usr/bin/env python3
"""
=========================================================
Phase 7.22 Part 1.5

Filter the manifest to ONLY weapon component related files.

Input:
    phase722_manifest.json

Output:
    phase722_weapon_manifest.json
    phase722_weapon_report.txt

READ ONLY
=========================================================
"""

from pathlib import Path
import json

MANIFEST = Path("phase722_manifest.json")

if not MANIFEST.exists():
    print("Missing phase722_manifest.json")
    exit(1)

manifest = json.loads(MANIFEST.read_text())

flurry = manifest["flurry"]
stardust = manifest["stardust"]

missing = []

for f in sorted(flurry):

    if f not in stardust:
        missing.append(f)

#####################################################################

KEYWORDS = {

    "Advanced Components":[

        "_advanced",
        "advanced"
    ],

    "Exceptional Components":[

        "_exceptional",
        "exceptional"
    ],

    "Blaster Components":[

        "blaster"
    ],

    "Projectile Components":[

        "projectile"
    ],

    "Power Handlers":[

        "power_handler"
    ],

    "Feed Mechanisms":[

        "feed"
    ],

    "Scopes":[

        "scope"
    ],

    "Stocks":[

        "stock"
    ],

    "Reinforcement Cores":[

        "reinforcement"
    ],

    "Sword Cores":[

        "sword_core"
    ],

    "Vibro Units":[

        "vibro"
    ],

    "Gas Cartridges":[

        "gas_cartridge"
    ],

    "Elemental Components":[

        "elemental"
    ],

    "Bio Components":[

        "biological",
        "chemical",
        "liquid",
        "solid"
    ],

    "Rare Components":[

        "nightsister",
        "rancor"
    ],

    "Loot":[

        "loot"
    ],

    "Crafting":[

        "schematic",
        "crafting"
    ],

    "Tangibles":[

        "tangible"
    ],

    "Draft Schematics":[

        "draft_schematic"
    ],

    "Shared Objects":[

        "shared"
    ]
}

#####################################################################

filtered = {}

used = set()

for category, words in KEYWORDS.items():

    filtered[category] = []

    for f in missing:

        l = f.lower()

        if any(w in l for w in words):

            filtered[category].append(f)
            used.add(f)

#####################################################################

unknown = []

for f in missing:

    if f not in used:
        unknown.append(f)

filtered["Unknown"] = unknown

#####################################################################

Path("phase722_weapon_manifest.json").write_text(
    json.dumps(filtered, indent=4)
)

report = []

report.append("=" * 70)
report.append("PHASE 7.22 WEAPON COMPONENT FILTER")
report.append("=" * 70)
report.append("")

total = 0

for category in filtered:

    report.append(category)
    report.append("-" * len(category))

    report.append(f"Count: {len(filtered[category])}")
    report.append("")

    for f in filtered[category]:
        report.append(f)

    report.append("")
    total += len(filtered[category])

report.append("=" * 70)
report.append(f"Total Classified Entries: {total}")
report.append(f"Original Missing Files : {len(missing)}")
report.append("=" * 70)

Path("phase722_weapon_report.txt").write_text(
    "\n".join(report)
)

print()
print("=" * 70)
print("Weapon manifest written:")
print("  phase722_weapon_manifest.json")
print()
print("Weapon report written:")
print("  phase722_weapon_report.txt")
print("=" * 70)
