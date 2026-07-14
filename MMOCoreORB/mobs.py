#!/usr/bin/env python3

from pathlib import Path
import shutil
import re

ROOT = Path("bin/scripts/mobile")

LEVEL_RE = re.compile(r'^(\s*level\s*=\s*)(\d+)', re.MULTILINE)
CHANCE_RE = re.compile(r'^(\s*chanceHit\s*=\s*)([\d.]+)', re.MULTILINE)
DMIN_RE = re.compile(r'^(\s*damageMin\s*=\s*)(\d+)', re.MULTILINE)
DMAX_RE = re.compile(r'^(\s*damageMax\s*=\s*)(\d+)', re.MULTILINE)


def get_scale(level):
    if level >= 300:
        return 0.70, 0.70

    if level >= 250:
        return 0.75, 0.75

    if level >= 225:
        return 0.80, 0.80

    if level >= 200:
        return 0.85, 0.90

    return None


patched = 0

for lua in ROOT.rglob("*.lua"):

    try:
        text = lua.read_text(encoding="utf-8")
    except Exception:
        continue

    m = LEVEL_RE.search(text)

    if not m:
        continue

    level = int(m.group(2))

    scales = get_scale(level)

    if scales is None:
        continue

    damage_scale, chance_scale = scales

    backup = Path(str(lua) + ".returnsbalance.bak")

    if not backup.exists():
        shutil.copy2(lua, backup)

    original = text

    def chance(match):
        value = round(float(match.group(2)) * chance_scale, 2)
        return match.group(1) + str(value)

    def dmin(match):
        value = int(round(int(match.group(2)) * damage_scale))
        return match.group(1) + str(value)

    def dmax(match):
        value = int(round(int(match.group(2)) * damage_scale))
        return match.group(1) + str(value)

    text = CHANCE_RE.sub(chance, text, count=1)
    text = DMIN_RE.sub(dmin, text, count=1)
    text = DMAX_RE.sub(dmax, text, count=1)

    if text != original:
        lua.write_text(text, encoding="utf-8")
        patched += 1
        print(f"[PATCHED] {lua} (Level {level})")

print()
print("=" * 60)
print(f"Finished! {patched} mobiles rebalanced.")
print("=" * 60)
