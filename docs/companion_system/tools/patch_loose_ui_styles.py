#!/usr/bin/env python3
"""
patch_loose_ui_styles.py -- put the companion icon styles where the client
ACTUALLY reads them: the loose ui/ui_styles.inc on disk, not the TRE.

THE ANSWER, AFTER FIVE WRONG THEORIES
-------------------------------------
The experiment settled it. `assist` was repointed at different pixels inside
companion_patch.tre, the client was fully relaunched, and Assist still drew its
usual hand. The client never read our file.

Then this turned up:

    D:\\Launcher\\newreturnbenserver\\ui\\ui_styles.inc     1,326,220 bytes

A LOOSE file, installed by the SWG Returns launcher on 3 August. SWG reads loose
files from the game directory in preference to TRE archives, so the UI subsystem
has been reading that one the whole time. Checked directly: it holds 1,019
styles, `assist` at its real 26,140,50,164, and NOT ONE companion entry.

That single fact explains every observation from today at once:

  * `string/en/cmd_n.stf` has no loose override, so it IS read from our TRE --
    which is why the command NAMES render correctly ("Companion Command: Attack")
  * `ui/ui_styles.inc` HAS a loose override, so our TRE copy is ignored entirely
    -- which is why every companion icon falls back to the same default
  * and why the palette verified perfect five different ways: it WAS perfect.
    It was just in a file nothing opens.

WHAT THIS DOES
--------------
Clones each companion style into the loose file, taking the Source and
SourceRect from the LOOSE file's own copy of the base style rather than from our
TRE build -- so the icons match what this client actually renders, not what a
different UI build contained. The cloned block is inserted immediately after the
style it copies, preserving that file's exact formatting.

For every companion command it adds up to three names, covering each way the
client might do the lookup:
    companionattack     (command name)
    companionattack     lowercase, if it differs
    companion_attack    the characterAbility string

A backup is written first. Idempotent: re-running with an unchanged
MAPPING is a no-op.

BUG FIX (2026-08-07, live report: "2 icons still using the old icons" --
Companion Guard and Companion Return were showing a generic default,
confirmed to actually be cloned from animalControl/guildMove, styles that
predate even the pre-2026-08-04 assist/areaTrack picks): the original
"already present -> skip" check only asked whether a name existed at all,
never whether it still pointed at the CURRENT MAPPING's base style. Once
build_ui_styles_patch.py's MAPPING changes for a name already cloned into
this loose file, re-running this script silently left the OLD pixels in
place forever -- a real, previously-unnoticed staleness bug, not a one-off.
Now compares each existing entry's SourceRect against the current target
and refreshes it in place if they differ, instead of only ever appending
brand-new names.

⚠ The SWG Returns launcher owns this file and may overwrite it on a client
update. If the companion icons ever revert, re-run this.
"""
import os
import re
import sys
import shutil
import datetime
import importlib.util

LOOSE = "/mnt/d/Launcher/newreturnbenserver/ui/ui_styles.inc"
TOOLS = "/mnt/d/SWGGenesis/docs/companion_system/tools"

# Companion System (2026-08-08, "icon coverage drift" fix) -- this used to be
# a second, hand-maintained copy of build_ui_styles_patch.py's ability lists,
# and it silently fell out of sync: the 2026-08-07 "full combat tree ability
# coverage" pass (142 new abilities -- actionShot1, aim, berserk2, bodyShot1,
# etc.) added those names to build_ui_styles_patch.py's own
# _NEW_COMPANION_ABILITY_NAMES_2026_08_07 list, but nobody updated this
# file's separate copy, so every one of those 142 commands got a blank icon
# in-game (confirmed live -- Action Shot 1/2, Aim, Berserk 2, Body Shot 1,
# etc. all showed empty icon boxes) even though the command itself worked
# fine. ABILITY_NAMES is now READ from build_ui_styles_patch.py at runtime
# (same pattern read_mapping() already uses for the 16 order-command
# mappings, just below), so this file can never drift from the real list
# again -- add a new ability there and this script picks it up automatically.
def read_ability_names():
    """_COMPANION_ABILITY_NAMES + _STARTER_ABILITY_NAMES, read live from
    build_ui_styles_patch.py so this file has no separate list to go stale."""
    path = os.path.join(TOOLS, "build_ui_styles_patch.py")
    try:
        src = open(path, encoding="utf-8", errors="replace").read()
    except OSError:
        print("  (could not read build_ui_styles_patch.py -- ability icons skipped)")
        return []
    names = []
    for varname in ("_COMPANION_ABILITY_NAMES", "_NEW_COMPANION_ABILITY_NAMES_2026_08_07",
                     "_STARTER_ABILITY_NAMES"):
        m = re.search(r"^" + varname + r"\s*=\s*(?:_COMPANION_ABILITY_NAMES\s*\+\s*)?\[(.*?)^\]",
                       src, re.S | re.M)
        if not m:
            continue
        names.extend(re.findall(r'"([^"]+)"', m.group(1)))
    # de-dupe, preserve first-seen order (the +=  reassignment of
    # _COMPANION_ABILITY_NAMES means a naive scan could otherwise double-count)
    seen = set()
    out = []
    for n in names:
        if n not in seen:
            seen.add(n)
            out.append(n)
    return out


ABILITY_NAMES = None  # populated by main() via read_ability_names(), once TOOLS is known to be valid

BLOCK = re.compile(
    r"[ \t]*<ImageStyle\s*\n"
    r"[ \t]*Name='([^']*)'\s*\n"
    r"[ \t]*Source='[^']*'\s*\n"
    r"[ \t]*SourceRect='[^']*'\s*\n"
    r"[ \t]*/>\n")


def read_mapping():
    """All MAPPING entries -- the hand-picked order-command mappings AND the
    per-ability entries build_ui_styles_patch.py computes at import time
    (icon-name defaults, plus the _NO_REAL_ICON_OVERRIDES_2026_08_07
    substitutes for abilities with no real icon in the palette, e.g.
    berserk1 -> warcry1).

    Companion System (2026-08-08, batch 31 fix) -- this used to regex-slice
    only the STATIC "key": "value" pairs written literally inside the
    `MAPPING = {...}` block, which silently missed every entry added
    afterward by `for a in ...: MAPPING["companion"+a] = ...get(a, a)`.
    Confirmed live: berserk1/berserk2/rally/takeCover were reported "no base
    style in this file" even though their intended overrides (warcry1,
    warcry2, boostmorale, tumbleToProne) DO exist in the loose file -- the
    override was computed correctly in build_ui_styles_patch.py but this
    function never saw it. Now imports the module directly so the fully
    -computed MAPPING (including runtime overrides) is read, not a static
    text slice that goes stale the moment anyone adds a dynamic override.
    """
    path = os.path.join(TOOLS, "build_ui_styles_patch.py")
    try:
        spec = importlib.util.spec_from_file_location("_build_ui_styles_patch", path)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        return dict(getattr(mod, "MAPPING", {}))
    except Exception as exc:
        print("  (could not import build_ui_styles_patch.py for MAPPING -- %s)" % exc)
        return {}


def main():
    loose = sys.argv[1] if len(sys.argv) > 1 else LOOSE

    if not os.path.exists(loose):
        print("MISSING: %s" % loose)
        print("That path is where the client actually reads its UI styles from.")
        return 1

    text = open(loose, encoding="latin-1").read()

    # name -> the exact source block, so clones keep this file's own formatting
    blocks = {}
    for m in BLOCK.finditer(text):
        blocks.setdefault(m.group(1).lower(), (m.group(0), m.group(1)))

    print("loose file: %d bytes, %d styles" % (len(text), len(blocks)))

    global ABILITY_NAMES
    ABILITY_NAMES = read_ability_names()
    print("ability list: %d names read from build_ui_styles_patch.py" % len(ABILITY_NAMES))

    wanted = {}                                    # newName -> baseName
    for ab in ABILITY_NAMES:
        wanted["companion" + ab] = ab
    for cmd, base in read_mapping().items():       # MAPPING overrides win
        wanted[cmd] = base

    def source_rect(block_text):
        m = re.search(r"SourceRect='([^']*)'", block_text)
        return m.group(1) if m else None

    have = set(blocks)
    added, updated, missing_base, already = [], [], [], 0
    out = text

    for new_name, base in sorted(wanted.items()):
        if base.lower() not in have:
            missing_base.append((new_name, base))
            continue

        block, real_base_name = blocks[base.lower()]
        target_rect = source_rect(block)

        # every name the client might plausibly look up
        variants = [new_name, new_name.lower()]
        if new_name.lower().startswith("companion") and not new_name.lower().startswith("companion_"):
            variants.append("companion_" + new_name[len("companion"):].lower())
        elif new_name.lower() in ("jenkins", "hpet"):
            variants.append("companion_" + new_name.lower())

        clones = ""
        for v in dict.fromkeys(variants):          # de-dupe, keep order
            vlow = v.lower()

            if vlow in have:
                existing_block, existing_real_name = blocks[vlow]

                if source_rect(existing_block) == target_rect:
                    already += 1
                    continue

                # STALE -- this name was cloned from a base style that has
                # since changed in MAPPING (e.g. a collision fix). Refresh
                # it in place rather than leaving the old pixels forever.
                refreshed = block.replace("Name='%s'" % real_base_name, "Name='%s'" % existing_real_name, 1)
                out = out.replace(existing_block, refreshed, 1)
                blocks[vlow] = (refreshed, existing_real_name)
                updated.append(v)
                continue

            new_block = block.replace("Name='%s'" % real_base_name, "Name='%s'" % v, 1)
            clones += new_block
            have.add(vlow)
            blocks[vlow] = (new_block, v)
            added.append(v)

        if clones:
            out = out.replace(block, block + clones, 1)

    if not added and not updated:
        print("nothing to add or refresh -- already patched.")
        return 0

    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    backup = loose + ".bak-" + stamp
    shutil.copy2(loose, backup)
    open(loose, "w", encoding="latin-1").write(out)

    print("")
    print("added %d style names, refreshed %d stale ones (%d already present)" % (len(added), len(updated), already))
    if updated:
        print("REFRESHED (were pointing at an old/removed MAPPING base style):")
        for n in updated[:8]:
            print("    %-26s" % n)
    if missing_base:
        print("SKIPPED %d with no base style in this file:" % len(missing_base))
        for n, b in missing_base[:8]:
            print("    %-26s <- %s" % (n, b))
    print("")
    print("%d -> %d bytes" % (len(text), len(out)))
    print("backup: %s" % os.path.basename(backup))
    print("")
    print("Fully relaunch the client. The icons live here, NOT in the TRE.")
    print("NOTE: the SWG Returns launcher owns this file and may overwrite it on")
    print("      a client update -- if the icons ever revert, re-run this.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
