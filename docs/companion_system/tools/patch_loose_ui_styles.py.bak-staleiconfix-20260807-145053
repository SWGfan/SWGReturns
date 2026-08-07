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

A backup is written first. Idempotent: re-running is a no-op.

⚠ The SWG Returns launcher owns this file and may overwrite it on a client
update. If the companion icons ever revert, re-run this.
"""
import os
import re
import sys
import shutil
import datetime

LOOSE = "/mnt/d/Launcher/newreturnbenserver/ui/ui_styles.inc"
TOOLS = "/mnt/d/SWGGenesis/docs/companion_system/tools"

# Ability mirrors: companion<Name> clones <Name>.
ABILITY_NAMES = [
    "applyDisease", "applyPoison", "bleedingShot", "concealShot", "confusionShot",
    "eyeShot", "fastBlast", "fireAcidCone1", "fireAcidCone2", "fireAcidSingle1",
    "fireAcidSingle2", "fireLightningCone1", "fireLightningCone2",
    "fireLightningSingle1", "fireLightningSingle2", "flameCone1", "flameCone2",
    "flameSingle1", "flameSingle2", "flurryShot1", "flurryShot2", "flushingShot1",
    "flushingShot2", "headShot3", "healMind", "knockdownFire", "mindShot2",
    "sniperShot", "sprayShot", "startleShot1", "startleShot2", "strafeShot1",
    "strafeShot2", "surpriseShot", "torsoShot", "underHandShot",
    "healDamage", "healWound", "tendWound", "tendDamage", "diagnose",
    "medicalForage", "harvestCorpse", "startDance", "stopDance",
    "startMusic", "stopMusic", "sample", "survey", "warcry1",
    "intimidate1", "berserk1", "taunt", "polearmLunge1", "unarmedLunge1",
    "melee1hLunge1", "melee2hLunge1", "centerOfBeing", "pointBlankArea1",
    "pointBlankSingle1", "overchargeShot1",
]

BLOCK = re.compile(
    r"[ \t]*<ImageStyle\s*\n"
    r"[ \t]*Name='([^']*)'\s*\n"
    r"[ \t]*Source='[^']*'\s*\n"
    r"[ \t]*SourceRect='[^']*'\s*\n"
    r"[ \t]*/>\n")


def read_mapping():
    """The 16 hand-picked order-command mappings, read from the generator."""
    path = os.path.join(TOOLS, "build_ui_styles_patch.py")
    try:
        src = open(path, encoding="utf-8", errors="replace").read()
    except OSError:
        print("  (could not read build_ui_styles_patch.py -- hand-picked mappings skipped)")
        return {}
    m = re.search(r"^MAPPING\s*=\s*\{(.*?)^\}", src, re.S | re.M)
    if not m:
        return {}
    return dict(re.findall(r"\"([^\"]+)\"\s*:\s*\"([^\"]+)\"", m.group(1)))


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

    wanted = {}                                    # newName -> baseName
    for cmd, base in read_mapping().items():
        wanted[cmd] = base
    for ab in ABILITY_NAMES:
        wanted["companion" + ab] = ab

    have = set(blocks)
    added, missing_base, already = [], [], 0
    out = text

    for new_name, base in sorted(wanted.items()):
        if base.lower() not in have:
            missing_base.append((new_name, base))
            continue

        block, real_base_name = blocks[base.lower()]

        # every name the client might plausibly look up
        variants = [new_name, new_name.lower()]
        if new_name.lower().startswith("companion") and not new_name.lower().startswith("companion_"):
            variants.append("companion_" + new_name[len("companion"):].lower())
        elif new_name.lower() in ("jenkins", "hpet"):
            variants.append("companion_" + new_name.lower())

        clones = ""
        for v in dict.fromkeys(variants):          # de-dupe, keep order
            if v.lower() in have:
                already += 1
                continue
            clones += block.replace("Name='%s'" % real_base_name, "Name='%s'" % v, 1)
            have.add(v.lower())
            added.append(v)

        if clones:
            out = out.replace(block, block + clones, 1)

    if not added:
        print("nothing to add -- already patched.")
        return 0

    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    backup = loose + ".bak-" + stamp
    shutil.copy2(loose, backup)
    open(loose, "w", encoding="latin-1").write(out)

    print("")
    print("added %d style names (%d already present)" % (len(added), already))
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
