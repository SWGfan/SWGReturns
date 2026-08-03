#!/usr/bin/env python3
"""
TRE archive reader, ported from Core3's MMOCoreORB/src/tre3/TreeFile.{h,cpp},
TreeFileRecord.h, TreeArchive.h (version '0005' format only -- verified
empirically against real archive bytes: magic 'TREE' + version '0005' are
stored as the reversed ASCII strings "EERT"/"5000" in the file due to
multi-char literal + little-endian storage).
"""
import struct
import zlib
import sys
import os

class TreeFileRecord:
    __slots__ = ("checksum", "uncompressedSize", "fileOffset", "compressionType", "compressedSize", "nameOffset", "path")

    def __init__(self, data, offset):
        (self.checksum, self.uncompressedSize, self.fileOffset,
         self.compressionType, self.compressedSize, self.nameOffset) = struct.unpack_from("<IIIIII", data, offset)
        self.path = None

class TreArchive:
    def __init__(self, path):
        self.path = path
        with open(path, "rb") as f:
            self.data = f.read()

        magic = self.data[0:4]
        version = self.data[4:8]
        if magic != b"EERT":
            raise ValueError(f"{path}: bad magic {magic}")
        if version != b"5000":
            raise ValueError(f"{path}: unsupported version {version} (only '0005' supported)")

        (self.totalRecords, self.dataOffset) = struct.unpack_from("<II", self.data, 8)
        (fb_compType, fb_compSize) = struct.unpack_from("<II", self.data, 16)
        (nb_compType, nb_compSize, nb_uncompSize) = struct.unpack_from("<III", self.data, 24)

        self.fb_compType, self.fb_compSize = fb_compType, fb_compSize
        self.nb_compType, self.nb_compSize, self.nb_uncompSize = nb_compType, nb_compSize, nb_uncompSize

        # File (record) block begins at absolute offset self.dataOffset
        pos = self.dataOffset
        fb_uncompSize = 24 * self.totalRecords
        if fb_compType == 2:
            raw = zlib.decompress(self.data[pos:pos + fb_compSize])
        else:
            raw = self.data[pos:pos + fb_uncompSize]
        pos += fb_compSize if fb_compType == 2 else fb_uncompSize

        assert len(raw) == fb_uncompSize, f"file block size mismatch {len(raw)} != {fb_uncompSize}"

        self.records = []
        for i in range(self.totalRecords):
            rec = TreeFileRecord(raw, i * 24)
            self.records.append(rec)

        # Name block follows immediately
        if nb_compType == 2:
            nameRaw = zlib.decompress(self.data[pos:pos + nb_compSize])
        else:
            nameRaw = self.data[pos:pos + nb_uncompSize]
        pos += nb_compSize if nb_compType == 2 else nb_uncompSize

        assert len(nameRaw) == nb_uncompSize, f"name block size mismatch {len(nameRaw)} != {nb_uncompSize}"

        self.nameBlock = nameRaw

        for rec in self.records:
            end = nameRaw.index(b"\x00", rec.nameOffset)
            rec.path = nameRaw[rec.nameOffset:end].decode("ascii", errors="replace")

        self.byPath = {r.path: r for r in self.records}

    def extract(self, path):
        rec = self.byPath.get(path)
        if rec is None:
            return None
        if rec.compressionType == 2:
            blob = self.data[rec.fileOffset:rec.fileOffset + rec.compressedSize]
            return zlib.decompress(blob, bufsize=rec.uncompressedSize)
        else:
            # Uncompressed records: empirically, compressedSize is stored as 0
            # for compressionType != 2 (verified against real archive bytes --
            # e.g. patch_07.tre's appearance/*.apt entries all have
            # compressedSize=0 while uncompressedSize is the real byte count).
            # The previous implementation sliced by compressedSize first, which
            # silently truncated every uncompressed record to zero bytes.
            return self.data[rec.fileOffset:rec.fileOffset + rec.uncompressedSize]

    def list_paths(self):
        return list(self.byPath.keys())


if __name__ == "__main__":
    path = sys.argv[1]
    arc = TreArchive(path)
    print(f"{path}: version=0005 totalRecords={arc.totalRecords}")
    for p in sorted(arc.list_paths())[:20]:
        print(" ", p)
