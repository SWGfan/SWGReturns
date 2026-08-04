#!/usr/bin/env python3
"""
add_ability_named_styles.py -- the companion command icons, take five.

WHAT IS ALREADY PROVEN (2026-08-04, all verified against the live archives)
--------------------------------------------------------------------------
  * the client loads companion_patch.tre FIRST (searchTree_00_29, highest)
  * ui/ui_styles.inc exists only in aftermath_1.tre and older patches, so
    aftermath was the right base
  * ours: 0 aftermath styles missing, 0 altered
  * all 75 companion styles are present, correctly named, in BOTH real
    palettes, with the intended Source and SourceRect, sitting immediately
    beside the style they clone
  * the third "run" in the file is only 11 unique nine-slice frame names
    (n/s/e/w/nw/...), not command icons -- nothing missing there either

And yet every companion command in the Command Browser draws one identical
fallback icon. So the lookup key is not the command name.

THE ACTUAL RULE
---------------
Compare the rows that DO show correct icons against the ones that do not:

    command      characterAbility     style named after command?   icon?
    assist       (empty)              yes                          correct
    burstRun     (empty)              yes                          correct
    cityBan      (empty)              yes ("cityban", lowercased)  correct
    cityPardon   (empty)              yes ("citypardon")           correct
    mount        (empty)              yes                          correct
    tame         (empty)              yes                          correct
    companion*   companion_<x>        yes                          FALLBACK

Every command that resolves has an EMPTY characterAbility. Every companion
command has one. The client keys the icon off `characterAbility` when it is
set, and falls back to the command name only when it is empty -- and no style
named `companion_follow`, `companion_attack`, ... exists, so all 75 land on the
same default.

This also explains a note the original tool author left behind without
realising what it meant: "berserk1 has NO real style anywhere in the palette
(the real command shows the default icon too)". berserk1's characterAbility is
`berserk1`, there is no style by that name, and so the STOCK command shows the
default icon as well. Same rule, observed from the other side.

Also worth recording: there are no Creature-Handler pet-command icons to copy.
The only pet-ish styles in the whole palette are mount, tame, hpet, pethigh,
petlow, emboldenPets and enragePets -- there is no petFollow/petAttack/petStay
style, because those commands never had bespoke icons either.

WHAT THIS DOES
--------------
For every existing `companion<Rest>` ImageStyle, emit a sibling named
`companion_<rest>` (lowercased) with the same Source and SourceRect -- i.e. a
style named after the characterAbility the command is actually gated on. Also
covers `jenkins` -> `companion_jenkins`.

Purely additive: no existing entry is touched, so it cannot regress anything
already correct. If the rule above is right, the icons resolve. If it is wrong,
the file carries some unused styles and nothing changes.

Run AFTER build_ui_styles_patch.py and BEFORE build_tre_patch.py.
Idempotent: re-running is a no-op.
"""
import os
import re
import shutil
import sys
import datetime

PATCHED = os.path.join(os.path.dirname(os.path.abspath(__file__)), "patched", "ui_styles.inc")

ENTRY = re.compile(
    r"([ \t]*)<ImageStyle\s*\n"
    r"([ \t]*)Name='(companion[^']*|jenkins)'\s*\n"
    r"([ \t]*)Source='([^']*)'\s*\n"
    r"([ \t]*)SourceRect='([^']*)'\s*\n"
    r"([ \t]*)/>\n"
)


def ability_name(style_name):
    if style_name.lower() == "jenkins":
        return "companion_jenkins"
    rest = style_name[len("companion"):]
    if not rest or rest.startswith("_"):
        return None
    return "companion_" + rest.lower()


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else PATCHED

    if not os.path.exists(path):
        print("MISSING: %s" % path)
        return 1

    src = open(path, encoding="latin-1").read()

    have = set(n.lower() for n in re.findall(r"<ImageStyle\s*\n\s*Name='([^']*)'", src))
    have |= set(n.lower() for n in re.findall(r"<ImageStyle\s+Name='([^']*)'", src))

    added = []

    def repl(m):
        indent, i2, name, i3, source, i4, rect, i5 = m.groups()
        ab = ability_name(name)
        if ab is None or ab in have or ab in [a.lower() for a in added]:
            return m.group(0)
        added.append(ab)
        clone = (
            "%s<ImageStyle\n%sName='%s'\n%sSource='%s'\n%sSourceRect='%s'\n%s/>\n"
            % (indent, i2, ab, i3, source, i4, rect, i5)
        )
        return m.group(0) + clone

    out = ENTRY.sub(repl, src)

    if not added:
        print("nothing to add -- already patched (or no companion styles found).")
        return 0

    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    shutil.copy2(path, path + ".bak-" + stamp)
    open(path, "w", encoding="latin-1").write(out)

    print("added %d ability-named styles (backup .bak-%s)" % (len(added), stamp))
    for a in sorted(set(added))[:12]:
        print("   " + a)
    if len(set(added)) > 12:
        print("   ... and %d more" % (len(set(added)) - 12))
    print()
    print("%d -> %d bytes" % (len(src), len(out)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
