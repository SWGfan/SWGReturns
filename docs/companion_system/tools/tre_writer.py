#!/usr/bin/env python3
"""
TRE archive writer, companion to tre_reader.py. Produces version '0005'
archives readable by Core3's MMOCoreORB/src/tre3/TreeFile.cpp.

CRITICAL, empirically discovered (see core3_hashcode.py): "checksum" is NOT
a content checksum -- it is Core3's own String::hashCode(path), i.e. a hash
of the record's PATH, used as a lookup key. Verified 808/808 exact matches
against the real bottom.tre. All three sampled real archives (bottom.tre,
patch_12_00.tre, patch_14_00.tre) have their FileBlock records sorted
STRICTLY ASCENDING by this checksum/hash value -- almost certainly so a
binary-search-by-hash lookup works, which Core3's own server-side reader
happens not to depend on (it re-sorts into a container by name after
loading) but the real game client's native reader may well require directly.
This writer computes checksum = hashCode(path) and sorts FileBlock (and
MD5Sums, generated in lockstep) by that value before writing, to match.

Header is exactly 36 bytes: magic "TREE" (stored reversed as "EERT"),
version "0005" (stored reversed as "5000"), totalRecords:u32LE,
dataOffset:u32LE, fileBlock{compressionType:u32LE, compressedSize:u32LE},
nameBlock{compressionType:u32LE, compressedSize:u32LE, uncompressedSize:u32LE}.
Bytes [36, dataOffset) hold concatenated raw content blobs; FileBlock (24
bytes/record) begins at dataOffset, then NameBlock, then MD5Sums (16 raw
bytes/record, unvalidated by Core3's own reader).
"""
import struct
import hashlib
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from core3_hashcode import hash_code


def build_tre(entries):
    """entries: list of (path:str, data:bytes). Content/name blocks are
    written in the given order; FileBlock/MD5Sums are re-sorted ascending by
    hash_code(path) to match the real archive layout (each record still
    points at the correct content/name offsets via fileOffset/nameOffset,
    so content/name order doesn't itself need to match).
    Returns the full archive bytes."""
    totalRecords = len(entries)

    content = bytearray()
    fileOffsets = []
    for path, data in entries:
        fileOffsets.append(36 + len(content))
        content += data

    dataOffset = 36 + len(content)

    nameBlock = bytearray()
    nameOffsets = []
    for path, data in entries:
        nameOffsets.append(len(nameBlock))
        nameBlock += path.encode("ascii") + b"\x00"
    nameBlock = bytes(nameBlock)

    records = []
    for i, (path, data) in enumerate(entries):
        checksum = hash_code(path)
        uncompressedSize = len(data)
        fileOffset = fileOffsets[i]
        compressionType = 0
        compressedSize = len(data)
        nameOffset = nameOffsets[i]
        recordBytes = struct.pack(
            "<IIIIII",
            checksum, uncompressedSize, fileOffset,
            compressionType, compressedSize, nameOffset,
        )
        md5Bytes = hashlib.md5(data).digest()
        records.append((checksum, recordBytes, md5Bytes))

    records.sort(key=lambda r: r[0])
    assert len(set(r[0] for r in records)) == totalRecords, "duplicate path hash collision"

    fileBlock = b"".join(r[1] for r in records)
    assert len(fileBlock) == 24 * totalRecords

    md5Block = b"".join(r[2] for r in records)

    header = bytearray()
    header += b"EERT"
    header += b"5000"
    header += struct.pack("<II", totalRecords, dataOffset)
    header += struct.pack("<II", 0, len(fileBlock))
    header += struct.pack("<III", 0, len(nameBlock), len(nameBlock))
    assert len(header) == 36

    out = bytearray()
    out += header
    out += content
    out += fileBlock
    out += nameBlock
    out += md5Block
    return bytes(out)


if __name__ == "__main__":
    import sys, os
    sys.path.insert(0, os.path.dirname(__file__))
    from tre_reader import TreArchive

    testEntries = [
        ("foo/bar.txt", b"hello world" * 5),
        ("foo/baz.dat", bytes(range(256)) * 3),
        ("string/en/companion.stf", b"\xcd\xab\x00\x00fakeheaderdata"),
    ]
    archiveBytes = build_tre(testEntries)
    testPath = "/tmp/_tre_writer3_selftest.tre"
    with open(testPath, "wb") as f:
        f.write(archiveBytes)

    arc = TreArchive(testPath)
    ok = True
    for path, data in testEntries:
        got = arc.extract(path)
        match = got == data
        ok = ok and match
        print(f"{path}: extracted {len(got) if got else 'None'} bytes, match={match}")
    print("ALL OK" if ok else "MISMATCH DETECTED")
