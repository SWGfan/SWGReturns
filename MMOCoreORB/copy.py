#!/usr/bin/env python3
"""
Phase 7.21.2
Import missing Advanced Weaponsmith registrations from Flurry.
"""

from pathlib import Path
import shutil
import re

FLURRY = Path.home() / "SWGFlurry" / "MMOCoreORB"
STARDUST = Path.home() / "StarDust-2" / "MMOCoreORB"

FILES = [
    "bin/scripts/loot/items.lua",
    "bin/scripts/object/custom_content/draft_schematic/weapon/component/objects.lua",
    "bin/scripts/object/custom_content/draft_schematic/weapon/component/serverobjects.lua",
]

COMPONENTS = [
    "blaster_pistol_barrel_advanced",
    "blaster_rifle_barrel_advanced",
    "blaster_power_handler_advanced",
    "projectile_pistol_barrel_advanced",
    "projectile_rifle_barrel_advanced",
    "projectile_feed_mechanism_advanced",
    "scope_weapon_advanced",
    "stock_advanced",
]


def backup(path):
    bak = path.with_suffix(path.suffix + ".phase7212.bak")
    if not bak.exists():
        shutil.copy2(path, bak)


def extract_entries(text):
    entries = []

    for line in text.splitlines():
        lower = line.lower()

        for comp in COMPONENTS:
            if comp in lower:
                entries.append(line.rstrip())

    return entries


for rel in FILES:

    src = FLURRY / rel
    dst = STARDUST / rel

    if not src.exists():
        print(f"[SKIP] Missing in Flurry: {rel}")
        continue

    if not dst.exists():
        print(f"[SKIP] Missing in Stardust: {rel}")
        continue

    backup(dst)

    src_text = src.read_text(errors="ignore")
    dst_text = dst.read_text(errors="ignore")

    additions = []

    for line in extract_entries(src_text):

        if line.strip() == "":
            continue

        if line not in dst_text:
            additions.append(line)

    if not additions:
        print(f"[OK] {rel} already complete.")
        continue

    with open(dst, "a", encoding="utf-8") as f:
        f.write("\n")
        f.write("-- Phase 7.21.2 Imported from Flurry\n")

        for line in additions:
            f.write(line + "\n")

    print(f"[UPDATED] {rel}")
    print(f"          Added {len(additions)} registration(s).")

print()
print("Done.")
print("Rebuild scripts if necessary.")
