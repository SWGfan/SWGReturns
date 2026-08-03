#!/usr/bin/env python3
"""
Companion System (2026-07-15, "increase player inventory space" -- see
NOTES.md). Patches the CLIENT's copy of
object/tangible/inventory/shared_character_inventory.iff so the inventory
window's capacity display/limit matches the server-side raise to 150
(character_inventory.lua). The stock value (80) lives in a fixed-size XXXX
property chunk: 'containerVolumeLimit\\0' + 01 20 + int32 LE -- patching the
int in place changes no chunk sizes, so no IFF structural fixups are needed.
Source copy: patch_13_00.tre (the latest archive carrying this file).
"""
import sys, os, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tre_reader import TreArchive

NEW_LIMIT = 150
SRC_TRE = "/sessions/elegant-fervent-carson/mnt/Companion/tre/patch_13_00.tre"
TARGET = "object/tangible/inventory/shared_character_inventory.iff"

arc = TreArchive(SRC_TRE)
data = bytearray(arc.extract(TARGET))

idx = data.find(b"containerVolumeLimit\x00")
assert idx >= 0, "containerVolumeLimit property not found"
payload = idx + len(b"containerVolumeLimit\x00")
flag, enc = data[payload], data[payload + 1]
assert (flag, enc) == (0x01, 0x20), f"unexpected payload encoding {flag:#x} {enc:#x}"
old = struct.unpack_from("<i", data, payload + 2)[0]
assert old == 80, f"unexpected stock value {old}"
struct.pack_into("<i", data, payload + 2, NEW_LIMIT)

dst = os.path.join(os.path.dirname(os.path.abspath(__file__)), "patched", "shared_character_inventory.iff")
with open(dst, "wb") as f:
    f.write(data)
print(f"patched containerVolumeLimit {old} -> {NEW_LIMIT}, wrote {dst} ({len(data)} bytes)")
