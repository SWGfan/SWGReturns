#!/usr/bin/env python3
"""
Phase 7.21A
Audit the New Weapon Component System.

READ ONLY.
Does not modify your source tree.
"""

from pathlib import Path
import os

ROOT = Path.home() / "StarDust-2" / "MMOCoreORB"

if not ROOT.exists():
    print("ERROR: MMOCoreORB not found.")
    exit(1)

COMP_DIR = ROOT / "bin/scripts/object/custom_content/draft_schematic/weapon/component"

if not COMP_DIR.exists():
    print("Missing:", COMP_DIR)
    exit(1)

print("=" * 80)
print(" NEW WEAPON COMPONENT AUDIT")
print("=" * 80)

schematics = sorted(COMP_DIR.glob("new_weapon_comp_*.lua"))

if not schematics:
    print("No new_weapon_comp schematics found.")
    exit()

missing = []

for lua in schematics:

    name = lua.stem

    print()
    print("=" * 80)
    print(name)
    print("=" * 80)

    text = lua.read_text(errors="ignore")

    #
    # Shared Draft Template
    #

    shared = None

    for line in text.splitlines():
        if "_shared_" in line and ":new" in line:
            shared = line.split("=")[1].split(":")[0].strip()
            break

    if shared:
        print("Shared Parent :", shared)
    else:
        print("!! Missing shared parent declaration")
        missing.append((name, "shared parent"))

    #
    # ObjectTemplate registration
    #

    if "ObjectTemplates:addTemplate" in text:
        print("ObjectTemplate : OK")
    else:
        print("!! Missing ObjectTemplate registration")
        missing.append((name, "ObjectTemplate"))

    #
    # IFF
    #

    iff = None

    for line in text.splitlines():

        if "ObjectTemplates:addTemplate" in line:

            s = line.find('"')
            e = line.rfind('"')

            iff = line[s + 1:e]

            break

    if iff:

        print("IFF :", iff)

    #
    # Tangible guess
    #

    tangible = iff.replace(
        "draft_schematic",
        "tangible"
    ) if iff else None

    if tangible:
        tangible_path = ROOT / "bin/scripts" / tangible.replace("object/", "")

        if tangible_path.exists():
            print("Tangible : OK")
        else:
            print("!! Tangible missing")
            missing.append((name, "tangible"))

    #
    # Search registrations
    #

    registrations = []

    for path, dirs, files in os.walk(ROOT / "bin/scripts"):

        for f in files:

            if not f.endswith(".lua"):
                continue

            full = Path(path) / f

            if full == lua:
                continue

            try:
                txt = full.read_text(errors="ignore")
            except:
                continue

            if name in txt:
                registrations.append(full.relative_to(ROOT))

    if registrations:

        print("Registrations:")

        for r in registrations:
            print("   ", r)

    else:

        print("!! No registrations found")
        missing.append((name, "registration"))

print()
print("=" * 80)
print("SUMMARY")
print("=" * 80)

if not missing:
    print("Everything appears registered.")
else:

    for m in missing:
        print(m[0], "->", m[1])

print()
print("Audit Complete.")
