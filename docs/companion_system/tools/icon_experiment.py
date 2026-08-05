#!/usr/bin/env python3
"""
icon_experiment.py -- stop theorising about the companion icons and get one
unambiguous fact.

FIVE THEORIES HAVE NOW FAILED
-----------------------------
  1. ui_styles.inc built against base SWG instead of aftermath  -- disproved
     (aftermath adds exactly one style, ours preserves it perfectly)
  2. two hand-picked mappings collided on identical pixels      -- was TRUE and
     fixed, but was not the cause
  3. cloned source SourceRects had moved in aftermath           -- disproved
     (all 65 clones match their source exactly)
  4. our styles were missing from a third palette               -- disproved
     (the third run is 11 nine-slice frame names)
  5. the client keys the icon off characterAbility              -- disproved
     (ability-named styles added, icons unchanged)

Every one of those was reasoned from the file and every one was consistent with
the file. The file is not the problem, or at least not the part of it anyone has
been looking at.

THE EXPERIMENT
--------------
Change a style that is KNOWN to work -- `assist`, which draws a correct, distinct
hand icon in Nick's Command Browser -- to point at completely different pixels.
Then look at Assist in game.

    Assist's icon CHANGES  -> the client IS reading our ui_styles.inc. The data
                              path is fine and the problem is purely which NAME
                              the client looks up for a companion command. Next
                              step is to enumerate candidate names, not to keep
                              re-verifying the file.

    Assist's icon is the   -> the client is NOT reading our ui_styles.inc at all,
    SAME                      despite companion_patch.tre being first in the
                              search order and despite command_table.iff and
                              cmd_n.stf from the SAME archive demonstrably
                              working. Every icon change made so far has been
                              landing in a file nothing reads, and the whole
                              approach needs replacing.

Either answer kills half the remaining possibilities. That is worth one relaunch.

This is a DELIBERATE, TEMPORARY change to a stock icon. run with --revert to put
it back.
"""
import os
import re
import sys
import shutil

HERE = os.path.dirname(os.path.abspath(__file__))
PATCHED = os.path.join(HERE, "patched", "ui_styles.inc")

# assist normally draws ('ui_rebel_icons', '26,140,50,164').
# Point it at burstRun's pixels instead -- a completely different, unmistakable
# glyph that already renders correctly in the same list, so a null result cannot
# be blamed on a bad rectangle.
TARGET_STYLE = "assist"
EXPERIMENT_RECT = "301,3,325,27"          # burstRun
ORIGINAL_RECT = "26,140,50,164"


def swap(path, want_from, want_to):
    src = open(path, encoding="latin-1").read()

    pat = re.compile(
        r"(<ImageStyle\s*\n\s*Name='%s'\s*\n\s*Source='[^']*'\s*\n\s*SourceRect=')([^']*)(')"
        % re.escape(TARGET_STYLE))

    hits = pat.findall(src)
    if not hits:
        print("could not find the '%s' style -- aborting." % TARGET_STYLE)
        return 1

    print("found %d copies of '%s' (one per palette)" % (len(hits), TARGET_STYLE))
    for _, rect, _ in hits:
        print("   current SourceRect = %s" % rect)

    if all(r == want_to for _, r, _ in hits):
        print("already set to %s -- nothing to do." % want_to)
        return 0

    out = pat.sub(lambda m: m.group(1) + want_to + m.group(3), src)
    shutil.copy2(path, path + ".bak-iconexp")
    open(path, "w", encoding="latin-1").write(out)
    print("set '%s' SourceRect -> %s" % (TARGET_STYLE, want_to))
    return 0


def main():
    revert = "--revert" in sys.argv
    path = PATCHED

    if not os.path.exists(path):
        print("MISSING: %s" % path)
        print("Run build_ui_styles_patch.py first.")
        return 1

    if revert:
        print("REVERTING the experiment -- putting Assist's real icon back.")
        return swap(path, EXPERIMENT_RECT, ORIGINAL_RECT)

    print("EXPERIMENT: repointing the 'assist' icon at burstRun's pixels.")
    print("")
    print("Assist currently draws a correct, distinct hand icon in your Command")
    print("Browser, so it is proof that stock icons resolve. If it CHANGES after a")
    print("relaunch, the client is reading our file and the problem is only which")
    print("name it looks up. If it does NOT change, the client is not reading our")
    print("ui_styles.inc at all and every icon fix so far has been landing in a")
    print("file nothing reads.")
    print("")
    return swap(path, ORIGINAL_RECT, EXPERIMENT_RECT)


if __name__ == "__main__":
    sys.exit(main())
