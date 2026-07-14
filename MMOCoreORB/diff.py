#!/usr/bin/env python3
"""
Phase 8.27
Core3 / Stardust Crafting Difference Auditor

Compares your source tree against a clean SWGEmu Core3 checkout.
Ignores comments and whitespace.
Reports only functional differences.

Author: ChatGPT
"""

from pathlib import Path
import difflib
import re

##########################################################################
# CHANGE THESE
##########################################################################

CORE3 = Path("/home/ubuntu/Core3/MMOCoreORB")
SERVER = Path("/home/ubuntu/StarDust-2/MMOCoreORB")

##########################################################################

INCLUDE = [
    "src/server/zone/managers/crafting",
    "src/server/zone/managers/loot",
    "src/server/zone/objects/manufactureschematic",
    "src/server/zone/objects/tangible/weapon",
    "src/server/zone/objects/tangible/component",
    "bin/scripts/object/weapon",
    "bin/scripts/object/tangible/component/weapon",
    "bin/scripts/object/draft_schematic/weapon",
]

EXTENSIONS = {
    ".cpp",
    ".h",
    ".lua",
    ".idl",
}

COMMENT_CPP = re.compile(
    r"//.*?$|/\*.*?\*/",
    re.M | re.S
)

COMMENT_LUA = re.compile(
    r"--\[\[.*?\]\]|--.*?$",
    re.M | re.S
)


def clean(path):
    text = path.read_text(errors="ignore")

    if path.suffix in [".cpp", ".h", ".idl"]:
        text = COMMENT_CPP.sub("", text)

    if path.suffix == ".lua":
        text = COMMENT_LUA.sub("", text)

    lines = []

    for line in text.splitlines():

        line = line.strip()

        if not line:
            continue

        lines.append(line)

    return lines


report = []

for folder in INCLUDE:

    sdir = SERVER / folder
    cdir = CORE3 / folder

    if not sdir.exists():
        continue

    for sf in sdir.rglob("*"):

        if sf.suffix not in EXTENSIONS:
            continue

        rel = sf.relative_to(SERVER)

        cf = CORE3 / rel

        if not cf.exists():
            report.append((
                "NEW FILE",
                rel
            ))
            continue

        try:
            a = clean(cf)
            b = clean(sf)
        except Exception:
            continue

        if a == b:
            continue

        diff = list(
            difflib.unified_diff(
                a,
                b,
                lineterm=""
            )
        )

        changes = sum(
            1
            for d in diff
            if d.startswith("+")
            or d.startswith("-")
        )

        report.append((
            changes,
            rel
        ))

report.sort(
    key=lambda x:
    (0 if x[0] == "NEW FILE" else -int(x[0]))
)

outfile = SERVER / "phase827_core3_diff.txt"

with outfile.open("w") as f:

    f.write("=" * 72 + "\n")
    f.write("Phase 8.27 Core3 Difference Report\n")
    f.write("=" * 72 + "\n\n")

    for item in report:

        if item[0] == "NEW FILE":
            f.write(f"[NEW] {item[1]}\n")
        else:
            f.write(
                f"{item[0]:4d} changes  {item[1]}\n"
            )

print("=" * 72)
print("Done.")
print(outfile)
print("=" * 72)
