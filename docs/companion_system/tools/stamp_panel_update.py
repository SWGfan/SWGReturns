#!/usr/bin/env python3
"""
stamp_panel_update.py -- set the "why" shown in the control panel's footer.

    python3 stamp_panel_update.py "nightly backup schedule + push target picker"

The panel's footer reads:

    Updated 2026-08-04 21:52:10   --   <this note>

The TIME is not stored anywhere; the panel reads its own file's mtime, so it
cannot claim to be newer than it is. This script only sets the reason, which no
filesystem can work out on its own.

Call it at the end of any job that changes the panel. One line, and the footer
never goes stale:

    python3 docs/companion_system/tools/stamp_panel_update.py "what changed"
"""
import os
import re
import sys

PANEL = "/mnt/d/SWGGenesis/SWGGenesisControlPanel.ahk"
SHORTCUT_COPY = "/mnt/c/Users/nickw/Downloads/SWGGenesisControlPanel_v3.ahk"


def main():
    if len(sys.argv) < 2:
        print('Usage: stamp_panel_update.py "short reason for the change"')
        return 1

    note = " ".join(sys.argv[1:]).strip().replace('"', "'")
    if len(note) > 110:
        note = note[:107] + "..."

    path = os.environ.get("PANEL_PATH", PANEL)

    if not os.path.exists(path):
        print("MISSING: %s" % path)
        return 1

    src = open(path, encoding="utf-8", errors="replace").read()

    if "PANEL_UPDATE_NOTE" not in src:
        print("panel has no footer yet -- run patch_panel_footer.py first.")
        return 1

    new_src, n = re.subn(r'PANEL_UPDATE_NOTE\s*:=\s*"[^"]*"',
                         'PANEL_UPDATE_NOTE := "%s"' % note, src, count=1)

    if n != 1:
        print("could not find the PANEL_UPDATE_NOTE assignment.")
        return 1

    open(path, "w", encoding="utf-8").write(new_src)
    print('footer note set: "%s"' % note)

    # Keep the copy the desktop shortcut launches in step -- three divergent
    # copies of this file have already caused one round of "where are the new
    # buttons?" today.
    if os.path.exists(os.path.dirname(SHORTCUT_COPY)):
        try:
            import shutil
            shutil.copy2(path, SHORTCUT_COPY)
            print("synced to the file the desktop shortcut launches")
        except OSError as e:
            print("(could not sync the shortcut copy: %s)" % e)

    return 0


if __name__ == "__main__":
    sys.exit(main())
