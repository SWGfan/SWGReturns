#!/usr/bin/env python3
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
from tre_writer import build_tre
from tre_reader import TreArchive
from core3_hashcode import hash_code

PATCHED = "patched"

FILES = [
    ("datatables/skill/skills.iff", "skills.iff"),
    ("datatables/skill/xp_limits.iff", "xp_limits.iff"),
    ("string/en/exp_n.stf", "exp_n.stf"),
    ("string/en/skl_n.stf", "skl_n.stf"),
    ("string/en/skl_t.stf", "skl_t.stf"),
    ("string/en/skl_d.stf", "skl_d.stf"),
    ("string/en/mob/creature_names.stf", "creature_names.stf"),
    ("string/en/companion.stf", "companion.stf"),
    ("string/en/skill_teacher.stf", "skill_teacher.stf"),
    ("datatables/command/command_table.iff", "command_table.iff"),
    ("string/en/stat_n.stf", "stat_n.stf"),
    ("string/en/stat_d.stf", "stat_d.stf"),
    ("string/en/cmd_n.stf", "cmd_n.stf"),
    # Companion System (2026-07-15, "macro/command icons" -- see
    # build_ui_styles_patch.py): companion* commands' hotbar/browser icons.
    ("ui/ui_styles.inc", "ui_styles.inc"),
    # Companion System (2026-07-15, "increase player inventory space" -- see
    # build_inventory_patch.py): client-side capacity 80 -> 150, matching
    # character_inventory.lua's server-side raise.
    ("object/tangible/inventory/shared_character_inventory.iff", "shared_character_inventory.iff"),
    # Companion System (2026-07-15, "datapad device should show a human
    # model" -- see build_device_template_patch.py): custom intangible
    # client template (3PO clone repointed at appearance/hum_m.sat).
    ("object/intangible/companion/shared_companion_control_device.iff", "shared_companion_control_device.iff"),
]

entries = []
for trePath, localName in FILES:
    with open(os.path.join(PATCHED, localName), "rb") as f:
        data = f.read()
    entries.append((trePath, data))
    print(f"packing {trePath} ({len(data)} bytes, hash={hash_code(trePath)})")

archiveBytes = build_tre(entries)
outPath = "companion_patch.tre"
with open(outPath, "wb") as f:
    f.write(archiveBytes)
print(f"\nwrote {outPath} ({len(archiveBytes)} bytes, {len(entries)} records)")

arc = TreArchive(outPath)
assert arc.totalRecords == len(entries)

checksums = [r.checksum for r in arc.records]
print("FileBlock sorted ascending by checksum:", checksums == sorted(checksums))

ok = True
for trePath, data in entries:
    got = arc.extract(trePath)
    match = got == data
    ok = ok and match
    print(f"  verify {trePath}: match={match}")
print("ARCHIVE VERIFIED OK" if ok else "ARCHIVE VERIFICATION FAILED")
