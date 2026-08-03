#!/usr/bin/env python3
"""
Companion System (2026-07-15, "datapad device should show a human model" --
see NOTES.md). No humanoid intangible client template ships with the game
(224 scanned; closest is the 3PO protocol droid), so this crafts a NEW one:
object/intangible/companion/shared_companion_control_device.iff -- a clone
of shared_3po_protocol_droid.iff with its appearanceFilename repointed at
the human male body (appearance/hum_m.sat, the same body players and the
companion actor itself use). The datapad preview then shows a human.
NOTE: the preview is a STATIC template model -- mirroring the companion's
live gear/clothes per-instance is not possible (client templates are fixed
per object type), documented limitation.

IFF surgery: the appearanceFilename XXXX chunk shrinks, so every ancestor
FORM's big-endian size covering it is adjusted by the delta.
"""
import sys, os, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tre_reader import TreArchive

SRC_TRE = "/sessions/elegant-fervent-carson/mnt/Companion/tre/patch_11_02.tre"
SRC = "object/intangible/pet/shared_3po_protocol_droid.iff"
NEW_APPEARANCE = b"appearance/hum_m.sat\x00"

arc = TreArchive(SRC_TRE)
data = bytearray(arc.extract(SRC))
print("source size:", len(data))

marker = b"appearanceFilename\x00\x01"
idx = data.find(marker)
assert idx >= 0
# chunk header is 8 bytes before the property name
chunkHdr = data.rfind(b"XXXX", 0, idx)
assert chunkHdr == idx - 8, f"unexpected chunk layout {chunkHdr} vs {idx}"
oldSize = struct.unpack_from(">I", data, chunkHdr + 4)[0]
payloadStart = idx + len(marker)
oldAppearanceEnd = data.index(b"\x00", payloadStart) + 1
oldAppearance = bytes(data[payloadStart:oldAppearanceEnd])
print("old appearance:", oldAppearance)

newChunkPayload = marker[len(b"XXXX")+0:] if False else None
newSize = len(b"appearanceFilename\x00\x01") + len(NEW_APPEARANCE)
delta = newSize - oldSize
print("chunk size:", oldSize, "->", newSize, "delta:", delta)

# Collect ancestor FORM offsets covering the chunk BEFORE mutating.
ancestors = []
def walk(buf, start, end, depth=0):
    pos = start
    while pos + 8 <= end:
        tag = bytes(buf[pos:pos+4])
        size = struct.unpack_from(">I", buf, pos+4)[0]
        payload = (pos+8, pos+8+size)
        if tag == b"FORM":
            if payload[0] <= chunkHdr < payload[1]:
                ancestors.append(pos)
            walk(buf, payload[0]+4, payload[1], depth+1)  # +4 skips the FORM subtype
        pos = payload[1]

walk(data, 0, len(data))
print("ancestor FORMs at offsets:", ancestors)
assert len(ancestors) >= 2

# Build the new file: swap the appearance string, then fix sizes.
newChunk = b"XXXX" + struct.pack(">I", newSize) + b"appearanceFilename\x00\x01" + NEW_APPEARANCE
out = bytearray()
out += data[:chunkHdr]
out += newChunk
out += data[chunkHdr + 8 + oldSize:]

for off in ancestors:
    cur = struct.unpack_from(">I", out, off+4)[0]
    struct.pack_into(">I", out, off+4, cur + delta)

print("new size:", len(out))
assert len(out) == len(data) + delta

dst = os.path.join(os.path.dirname(os.path.abspath(__file__)), "patched", "shared_companion_control_device.iff")
with open(dst, "wb") as f:
    f.write(out)
print("wrote", dst)

# sanity re-walk of the output
def verify(buf, start, end):
    pos = start
    while pos + 8 <= end:
        tag = bytes(buf[pos:pos+4])
        size = struct.unpack_from(">I", buf, pos+4)[0]
        nxt = pos + 8 + size
        assert nxt <= end, f"chunk {tag} at {pos} overruns"
        if tag == b"FORM":
            verify(buf, pos+12, nxt)
        pos = nxt
    assert pos == end, f"trailing bytes: {pos} != {end}"
verify(out, 0, len(out))
print("IFF structure verified OK")
