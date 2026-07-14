#!/usr/bin/env python3

from pathlib import Path
import shutil
import re
import sys

FILE = Path("src/server/zone/managers/resource/ResourceManager.idl")

if not FILE.exists():
    print("Couldn't find ResourceManager.idl")
    sys.exit(1)

backup = FILE.with_suffix(".idl.resource100k.bak")

if not backup.exists():
    shutil.copy2(FILE, backup)

text = FILE.read_text()

old = "public static final int RESOURCE_DEED_QUANTITY = 10000;"
new = "public static final int RESOURCE_DEED_QUANTITY = 100000;"

if new in text:
    print("Already patched.")
    sys.exit(0)

if old not in text:
    print("Couldn't locate RESOURCE_DEED_QUANTITY.")
    sys.exit(1)

text = text.replace(old, new)

FILE.write_text(text)

print()
print("========================================")
print(" Resource Crates = 100,000")
print("========================================")
print("Updated:")
print(FILE)
print()
print("Backup:")
print(backup)
print()
print("Now rebuild the server.")
