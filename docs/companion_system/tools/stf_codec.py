#!/usr/bin/env python3
"""
Generic .stf string-table codec, reverse-engineered empirically from real
Core3/SWG client string/en/*.stf files (exp_n.stf, skl_n.stf,
mob/creature_names.stf) extracted from patch_12_00.tre / patch_14_00.tre.

Layout:
  u16 magic = 0xabcd
  u16 zero
  u32 fieldA = (entryCount + 1) * 256 + 1      <- derived/verified formula
  u32 fieldB = entryCount * 256                <- derived/verified formula
  u8  zero
  VALUE TABLE: entryCount x { u32 index(1-based), u32 flags (almost always
                               0xFFFFFFFF but NOT a strict sentinel -- real
                               creature_names.stf has a trailing run of 148
                               entries with flags=1, presumably written by a
                               different/later tool version; must be
                               preserved verbatim, not assumed constant),
                               u32 strLenChars, strLenChars*2 bytes UTF16LE }
  KEY TABLE:   entryCount x { u32 index(1-based), u32 strLenBytes,
                               strLenBytes bytes ASCII (no flags field, no
                               null terminator) }

Value table and key table are ordered INDEPENDENTLY:
  - value table is in original insertion order, index == position (1-based)
  - key table is sorted lexicographically by key name, each entry's index
    field points back to the matching value-table position
(discovered empirically: skl_n.stf's key table is alphabetically sorted
while its value table is in definition/insertion order; the two only
happen to coincide for small files like exp_n.stf).

Verified byte-for-byte round-trip on all three real sample files:
exp_n.stf (73 entries), skl_n.stf (1481 entries), creature_names.stf
(2876 entries, including the flags=1 tail run).
"""
import struct


class StfTable:
    def __init__(self):
        self.version = 1
        self.nextIndex = 1
        self.values = []  # list of (idx:int, flags:int, value:str), VALUE TABLE file order
        self.keys = []     # list of (idx:int, key:str), KEY TABLE file order

    @staticmethod
    def parse(data):
        # Real STF header layout (13 bytes):
        #   [0..3]  u32 magic 0x0000ABCD
        #   [4]     u8  version
        #   [5..8]  u32 nextIndex     -- next free value index, NOT count+1
        #   [9..12] u32 numEntries
        #
        # This used to read u32s at offsets 4 and 8 and assert
        #   fieldA == (declaredCount + 1) * 256 + 1
        # Those reads straddle the version/nextIndex and nextIndex/numEntries
        # boundaries, which is why both values appeared multiplied by 256, and
        # the "formula" only held when nextIndex happened to equal count+1.
        # It does not: aftermath_1.tre's exp_n.stf has nextIndex=74 with 75
        # entries (indices are reused), and the assert rejected the file
        # outright. Read the real fields instead.
        assert data[0:2] == b"\xcd\xab", "bad STF magic"
        assert data[2:4] == b"\x00\x00"
        version = data[4]
        (nextIndex,) = struct.unpack_from("<I", data, 5)
        (declaredCount,) = struct.unpack_from("<I", data, 9)

        pos = 13
        values = []
        for i in range(declaredCount):
            (idx,) = struct.unpack_from("<I", data, pos)
            (flags,) = struct.unpack_from("<I", data, pos + 4)
            (strLen,) = struct.unpack_from("<I", data, pos + 8)
            strStart = pos + 12
            strBytes = data[strStart:strStart + strLen * 2]
            value = strBytes.decode("utf-16-le")
            pos = strStart + strLen * 2
            values.append((idx, flags, value))

        keys = []
        for i in range(declaredCount):
            (idx,) = struct.unpack_from("<I", data, pos)
            (strLen,) = struct.unpack_from("<I", data, pos + 4)
            strStart = pos + 8
            keyBytes = data[strStart:strStart + strLen]
            key = keyBytes.decode("ascii")
            pos = strStart + strLen
            keys.append((idx, key))

        assert pos == len(data), f"trailing bytes: consumed {pos} of {len(data)}"

        table = StfTable()
        table.values = values
        table.keys = keys
        table.version = version
        table.nextIndex = nextIndex
        return table

    def serialize(self):
        count = len(self.values)
        assert count == len(self.keys)

        # PRESERVE nextIndex exactly as parsed. Do not recompute it: these
        # files are not densely indexed (aftermath's cmd_n.stf carries 1677
        # entries with nextIndex=1651), so any formula derived from the entry
        # count rewrites the field and breaks byte-for-byte round-trip. add()
        # is responsible for raising it when it allocates a new index.
        nextIndex = getattr(self, "nextIndex", len(self.values) + 1)

        out = bytearray()
        out += b"\xcd\xab\x00\x00"
        out += bytes([getattr(self, "version", 1)])
        out += struct.pack("<I", nextIndex)
        out += struct.pack("<I", count)

        for idx, flags, value in self.values:
            out += struct.pack("<I", idx)
            out += struct.pack("<I", flags)
            out += struct.pack("<I", len(value))
            out += value.encode("utf-16-le")

        for idx, key in self.keys:
            out += struct.pack("<I", idx)
            out += struct.pack("<I", len(key))
            out += key.encode("ascii")

        return bytes(out)

    def as_dict(self):
        keyByIdx = dict((idx, k) for idx, k in self.keys)
        return {keyByIdx[idx]: v for idx, _flags, v in self.values if idx in keyByIdx}

    def add(self, key, value, flags=0xFFFFFFFF):
        """Appends a new entry to the value table (next sequential index) and
        inserts the matching key entry at its correct alphabetically-sorted
        position in the key table. If key already exists, updates its value
        in place instead (preserving its original flags/index)."""
        keyByIdx = dict((idx, k) for idx, k in self.keys)
        for idx, k in list(keyByIdx.items()):
            if k == key:
                self.values = [
                    (i, f, (value if i == idx else v)) for i, f, v in self.values
                ]
                return

        nextIdx = (max((i for i, _f, _v in self.values), default=0)) + 1
        self.values.append((nextIdx, flags, value))
        # keep the header's nextIndex above every index now in use
        if getattr(self, "nextIndex", 0) <= nextIdx:
            self.nextIndex = nextIdx + 1

        insertAt = len(self.keys)
        for pos, (i, k) in enumerate(self.keys):
            if key < k:
                insertAt = pos
                break
        self.keys.insert(insertAt, (nextIdx, key))


def verify_roundtrip(path):
    with open(path, "rb") as f:
        original = f.read()
    table = StfTable.parse(original)
    rebuilt = table.serialize()
    ok = rebuilt == original
    print(path + ": parsed " + str(len(table.values)) + " entries. Round-trip identical: " + str(ok))
    if not ok:
        print("  original len=" + str(len(original)) + " rebuilt len=" + str(len(rebuilt)))
        n = min(len(original), len(rebuilt))
        for i in range(n):
            if original[i] != rebuilt[i]:
                print("  first diff at byte " + str(i) + ": orig=" + hex(original[i]) + " rebuilt=" + hex(rebuilt[i]))
                break
    return table, ok


if __name__ == "__main__":
    import sys
    verify_roundtrip(sys.argv[1])
