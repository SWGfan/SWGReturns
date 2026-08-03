#!/usr/bin/env python3
"""
Generic SOE/EA "DTII" IFF DataTable codec, reverse-engineered empirically
from real Core3/SWG client bytes (datatables/skill/xp_limits.iff and
datatables/skill/skills.iff extracted from patch_11_03.tre / patch_14_00.tre).

Format (all chunk length fields are big-endian uint32; all payload data
inside chunks is little-endian):

  FORM <len> "DTII"
    FORM <len> "0001"
      COLS <len>  numColumns:int32LE, then numColumns x null-terminated column names
      TYPE <len>  numColumns x null-terminated type descriptors
                  ('s'=string, 'i'=int32, 'f'=float32, 'b'=byte/bool,
                   'e(name=val,...)[default]'=enum-as-int32)
      ROWS <len>  rowCount:int32LE, then rowCount rows; each row has one
                  field per column in COLS order: 's' -> null-terminated
                  string, everything else -> 4 raw bytes (int32/float32/byte
                  all occupy a full 4-byte slot in this format).

Verified by round-tripping real 273KB skills.iff byte-for-byte (see
verify_roundtrip()).
"""
import struct


def read_chunk_header(data, pos):
    tag = data[pos:pos + 4]
    length = struct.unpack_from(">I", data, pos + 4)[0]
    return tag, length, pos + 8


class DataTable:
    def __init__(self):
        self.columns = []       # list of (name, typeDesc)
        self.rows = []          # list of list-of-values (str or bytes(4))

    @staticmethod
    def parse(data):
        dt = DataTable()

        tag, length, pos = read_chunk_header(data, 0)
        assert tag == b"FORM", tag
        outer_end = pos + length
        formType = data[pos:pos + 4]
        assert formType == b"DTII", formType
        pos += 4

        tag, length, pos = read_chunk_header(data, pos)
        assert tag == b"FORM", tag
        inner_end = pos + length
        versionTag = data[pos:pos + 4]
        pos += 4

        while pos < inner_end:
            tag, length, chunkDataStart = read_chunk_header(data, pos)
            chunkData = data[chunkDataStart:chunkDataStart + length]

            if tag == b"COLS":
                (numCols,) = struct.unpack_from("<I", chunkData, 0)
                off = 4
                names = []
                for i in range(numCols):
                    end = chunkData.index(b"\x00", off)
                    names.append(chunkData[off:end].decode("ascii"))
                    off = end + 1
                dt._colNames = names

            elif tag == b"TYPE":
                off = 0
                types = []
                for i in range(len(dt._colNames)):
                    end = chunkData.index(b"\x00", off)
                    types.append(chunkData[off:end].decode("ascii"))
                    off = end + 1
                dt.columns = list(zip(dt._colNames, types))

            elif tag == b"ROWS":
                (rowCount,) = struct.unpack_from("<I", chunkData, 0)
                off = 4
                baseTypes = [t[0] for (_, t) in dt.columns]  # first char
                for r in range(rowCount):
                    row = []
                    for bt in baseTypes:
                        if bt == "s":
                            end = chunkData.index(b"\x00", off)
                            row.append(chunkData[off:end].decode("ascii", errors="replace"))
                            off += (end - off) + 1
                        else:
                            row.append(chunkData[off:off + 4])
                            off += 4
                    dt.rows.append(row)
                assert off == length, f"ROWS parse mismatch: consumed {off}, expected {length}"

            pos = chunkDataStart + length

        assert pos == inner_end, f"inner FORM mismatch: {pos} != {inner_end}"
        return dt, outer_end

    def serialize_rows_chunk(self):
        baseTypes = [t[0] for (_, t) in self.columns]
        out = bytearray()
        out += struct.pack("<I", len(self.rows))
        for row in self.rows:
            for bt, val in zip(baseTypes, row):
                if bt == "s":
                    out += val.encode("ascii") + b"\x00"
                else:
                    assert len(val) == 4
                    out += val
        return bytes(out)

    def serialize_cols_chunk(self):
        out = bytearray()
        out += struct.pack("<I", len(self.columns))
        for name, _ in self.columns:
            out += name.encode("ascii") + b"\x00"
        return bytes(out)

    def serialize_type_chunk(self):
        out = bytearray()
        for _, t in self.columns:
            out += t.encode("ascii") + b"\x00"
        return bytes(out)

    def serialize(self):
        cols = self.serialize_cols_chunk()
        types = self.serialize_type_chunk()
        rows = self.serialize_rows_chunk()

        def chunk(tag, data):
            return tag + struct.pack(">I", len(data)) + data

        inner = b"0001" + chunk(b"COLS", cols) + chunk(b"TYPE", types) + chunk(b"ROWS", rows)
        inner_chunk = chunk(b"FORM", inner)
        outer = b"DTII" + inner_chunk
        return chunk(b"FORM", outer)


def verify_roundtrip(path):
    with open(path, "rb") as f:
        original = f.read()
    dt, end = DataTable.parse(original)
    assert end == len(original), f"trailing bytes: parsed {end} of {len(original)}"
    rebuilt = dt.serialize()
    ok = rebuilt == original
    print(f"{path}: parsed {len(dt.rows)} rows, {len(dt.columns)} columns. Round-trip identical: {ok}")
    if not ok:
        print(f"  original len={len(original)} rebuilt len={len(rebuilt)}")
        for i in range(min(len(original), len(rebuilt))):
            if original[i] != rebuilt[i]:
                print(f"  first diff at byte {i}: orig={original[i]:02x} rebuilt={rebuilt[i]:02x}")
                break
    return dt, ok


if __name__ == "__main__":
    import sys
    verify_roundtrip(sys.argv[1])
