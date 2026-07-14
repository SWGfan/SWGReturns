#!/usr/bin/env python3

from pathlib import Path
import re

ROOT = Path("bin/scripts/object/draft_schematic/clothing")

if not ROOT.exists():
    print("Cannot find", ROOT)
    raise SystemExit(1)

issues = []

for lua in sorted(ROOT.glob("*.lua")):

    text = lua.read_text(errors="ignore")

    #
    # object name
    #
    obj = re.search(
        r'^(object_draft_schematic_clothing_[A-Za-z0-9_]+)\s*=',
        text,
        re.MULTILINE)

    #
    # shared parent
    #
    shared = re.search(
        r'(object_draft_schematic_clothing_shared_[A-Za-z0-9_]+):new',
        text)

    #
    # iff path
    #
    iff = re.search(
        r'"(object/draft_schematic/[^"]+\.iff)"',
        text)

    if obj is None:
        issues.append((lua.name, "Missing object declaration"))
        continue

    if shared is None:
        issues.append((lua.name, "Missing shared template"))
    else:
        objname = obj.group(1).replace(
            "object_draft_schematic_clothing_",
            "")

        sharedname = shared.group(1).replace(
            "object_draft_schematic_clothing_shared_",
            "")

        if objname != sharedname:
            issues.append((
                lua.name,
                f"Shared mismatch: {sharedname}"
            ))

    if iff is None:
        issues.append((lua.name, "Missing IFF path"))
    else:
        path = iff.group(1)

        if "draft_schematic/clothing/" not in path:
            issues.append((
                lua.name,
                "Bad clothing path"
            ))

        expected = obj.group(1).replace(
            "object_draft_schematic_",
            "").replace("_", "/")

        if lua.stem not in path:
            issues.append((
                lua.name,
                f"IFF mismatch: {path}"
            ))

print("=" * 72)
print("Phase 7.11b Clothing Integrity Scan")
print("=" * 72)
print()

if not issues:
    print("No obvious problems found.")
else:
    print("Problems found:", len(issues))
    print()

    for file, problem in issues:
        print(f"{file}: {problem}")

    with open("phase711b_clothing_report.txt", "w") as fp:
        for file, problem in issues:
            fp.write(f"{file}: {problem}\n")

    print()
    print("Report written to phase711b_clothing_report.txt")
