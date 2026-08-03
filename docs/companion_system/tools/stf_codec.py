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
        self.values = []  # list of (idx:int, flags:int, value:str), VALUE TABLE file order
        self.keys = []     # list of (idx:int, key:str), KEY TABLE file order

    @staticmethod
    def parse(data):
        assert data[0:2] == b"\xcd\xab", "bad STF magic"
        assert data[2:4] == b"\x00\x00"
        (fieldA,) = struct.unpack_from("<I", data, 4)
        (fieldB,) = struct.unpack_from("<I", data, 8)
        assert data[12] == 0
        assert fieldB % 256 == 0, f"unexpected fieldB {fieldB}"
        declaredCount = fieldB // 256
        assert fieldA == (declaredCount + 1) * 256 + 1, f"fieldA/fieldB mismatch: {fieldA} vs {fieldB}"

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
        return table

    def serialize(self):
        count = len(self.values)
        assert count == len(self.keys)
        fieldA = (count + 1) * 256 + 1
        fieldB = count * 256

        out = bytearray()
        out += b"\xcd\xab\x00\x00"
        out += struct.pack("<I", fieldA)
        out += struct.pack("<I", fieldB)
        out += b"\x00"

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
