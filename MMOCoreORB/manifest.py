#!/usr/bin/env python3
"""
===========================================================
Phase 7.22 Part 1
Flurry -> Stardust Weapon Component Manifest Builder
===========================================================

READ ONLY

Builds a complete inventory of all weapon component related
files in Flurry and Stardust.

Outputs:

    phase722_manifest.json
    phase722_report.txt

Nothing is modified.
"""

from pathlib import Path
import hashlib
import json

# ------------------------------------------------------------------

FLURRY = Path.home() / "SWGFlurry" / "MMOCoreORB"
STARDUST = Path.home() / "StarDust-2" / "MMOCoreORB"

OUT_JSON = Path("phase722_manifest.json")
OUT_REPORT = Path("phase722_report.txt")

SEARCH_DIRS = [

    "bin/scripts/object/draft_schematic/weapon/component",
    "bin/scripts/object/custom_content/draft_schematic/weapon/component",

    "bin/scripts/object/tangible/component/weapon",

    "bin/scripts/loot/items/component_loot",
    "bin/scripts/loot/groups/component_loot",

    "bin/scripts/managers/crafting",

    "bin/scripts/object/draft_schematic/weapon",
    "bin/scripts/object/custom_content/draft_schematic/weapon",
]

# ------------------------------------------------------------------

KEYWORDS = {

    "Advanced Weapon Components":[

        "_advanced",
        "advanced"
    ],

    "Exceptional Components":[

        "_exceptional",
        "exceptional"
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

    "Scopes":[

        "scope"
    ],

    "Stocks":[

        "stock"
    ],

    "Gas Cartridges":[

        "gas_cartridge"
    ],

    "Elemental Components":[

        "elemental"
    ],

    "Projectile Components":[

        "projectile"
    ],

    "Blaster Components":[

        "blaster"
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
    ]
}

# ------------------------------------------------------------------

def sha1(path):

    h = hashlib.sha1()

    with open(path,"rb") as f:

        while True:

            d = f.read(65536)

            if not d:
                break

            h.update(d)

    return h.hexdigest()

# ------------------------------------------------------------------

def scan(root):

    data = {}

    for directory in SEARCH_DIRS:

        folder = root / directory

        if not folder.exists():
            continue

        for file in folder.rglob("*"):

            if not file.is_file():
                continue

            rel = str(file.relative_to(root))

            data[rel] = {

                "sha1":sha1(file),
                "size":file.stat().st_size
            }

    return data

# ------------------------------------------------------------------

print("Scanning Flurry...")

flurry = scan(FLURRY)

print("Scanning Stardust...")

stardust = scan(STARDUST)

manifest = {

    "flurry":flurry,
    "stardust":stardust
}

OUT_JSON.write_text(json.dumps(manifest,indent=4))

missing=[]
different=[]
same=[]

for file in sorted(flurry):

    if file not in stardust:

        missing.append(file)

    elif flurry[file]["sha1"] != stardust[file]["sha1"]:

        different.append(file)

    else:

        same.append(file)

report=[]

report.append("="*72)
report.append("Phase 7.22 Part 1")
report.append("="*72)
report.append("")
report.append(f"Flurry Files   : {len(flurry)}")
report.append(f"Stardust Files : {len(stardust)}")
report.append("")
report.append(f"Same Files     : {len(same)}")
report.append(f"Different      : {len(different)}")
report.append(f"Missing        : {len(missing)}")
report.append("")

for category,words in KEYWORDS.items():

    report.append("="*72)
    report.append(category)
    report.append("="*72)

    count=0

    for f in missing:

        lower=f.lower()

        if any(w in lower for w in words):

            report.append(f)

            count+=1

    if count==0:

        report.append("(none)")

    report.append("")

report.append("="*72)
report.append("ALL DIFFERENT FILES")
report.append("="*72)

for f in different:

    report.append(f)

OUT_REPORT.write_text("\n".join(report))

print()
print("="*72)
print("Manifest written to:")
print(" ",OUT_JSON)
print()
print("Report written to:")
print(" ",OUT_REPORT)
print("="*72)
print()
print("Summary")
print("-------")
print("Same      :",len(same))
print("Different :",len(different))
print("Missing   :",len(missing))
