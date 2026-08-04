#!/usr/bin/env python3
"""
Companion System (2026-07-15, "macro/command icons" -- see NOTES.md).

The client resolves hotbar/command-browser/macro icons BY NAME against the
ImageStyle palette in ui/ui_styles.inc (confirmed two ways: the user's own
macros.txt stores an icon as the bare style name, e.g. "follow", and the
extracted patch_14_00.tre ui_styles.inc carries e.g.
    <ImageStyle Name='follow' Source='ui_rebel_icons' SourceRect='393,94,417,118'/>
in both its plain and ui_shader_add palette copies). command_table.iff has
NO icon column (75-column schema, confirmed by the earlier macro-icon
investigation) -- the icon is purely a client-side name match, which is why
every companion* command shows the generic default icon: no style with its
name exists.

This tool extracts the latest ui/ui_styles.inc (patch_14_00.tre), clones the
matching real command's ImageStyle entry for every companion command --
inserted immediately after each occurrence of the base entry so BOTH palette
copies (plain + ui_shader_add) get one -- and writes patched/ui_styles.inc
for build_tre_patch.py to pack. A lowercase-name duplicate is added whenever
the command name has any uppercase letters, covering either case-sensitivity
behavior in the client's style lookup.
"""
import sys, os, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tre_reader import TreArchive

# 2026-07-17: the client TRE folder moves between environments (each Cowork
# sandbox mounts it at a different session path; Debian sees it via /mnt/c).
# Try the known candidates in order instead of hardcoding one session's path.
_TRE_DIR_CANDIDATES = [
    # Companion System (2026-07-25, "Jenkins" pass) -- confirmed live via
    # device-bridge listing: Nick's real client install is C:\SWGEmu,
    # i.e. /mnt/c/SWGEmu from WSL. Kept first since it's the verified path;
    # the rest are left as fallbacks in case this runs from a different box.
    os.environ.get("SWG_TRE_DIR", "/mnt/d/Launcher/newreturnbenserver"),
    "/mnt/d/Launcher/newreturnbenserver",   # genesis port: aftermath content
    "/mnt/c/SWGEmu",
    "/sessions/inspiring-lucid-noether/mnt/Companion/tre",
    "/sessions/elegant-fervent-carson/mnt/Companion/tre",
    "/mnt/c/Companion/tre",
    "C:\\Companion\\tre",
]
TRE_DIR = next((p for p in _TRE_DIR_CANDIDATES if os.path.isdir(p)), _TRE_DIR_CANDIDATES[0])
BASE_TRE = os.path.join(TRE_DIR, "patch_14_00.tre")

_COMPANION_ABILITY_NAMES = [
    "applyDisease", "applyPoison", "bleedingShot", "concealShot", "confusionShot",
    "eyeShot", "fastBlast", "fireAcidCone1", "fireAcidCone2", "fireAcidSingle1",
    "fireAcidSingle2", "fireLightningCone1", "fireLightningCone2",
    "fireLightningSingle1", "fireLightningSingle2", "flameCone1", "flameCone2",
    "flameSingle1", "flameSingle2", "flurryShot1", "flurryShot2", "flushingShot1",
    "flushingShot2", "headShot3", "healMind", "knockdownFire", "mindShot2",
    "sniperShot", "sprayShot", "startleShot1", "startleShot2", "strafeShot1",
    "strafeShot2", "surpriseShot", "torsoShot", "underHandShot",
]
_STARTER_ABILITY_NAMES = [
    "healDamage", "healWound", "tendWound", "tendDamage", "diagnose",
    "medicalForage", "harvestCorpse", "startDance", "stopDance",
    "startMusic", "stopMusic", "sample", "survey", "warcry1",
    "intimidate1", "berserk1", "taunt", "polearmLunge1", "unarmedLunge1",
    "melee1hLunge1", "melee2hLunge1", "centerOfBeing", "pointBlankArea1",
    "pointBlankSingle1", "overchargeShot1",
]

# newCommandName -> base style name to clone. Baseline order commands get
# hand-picked matches (the user explicitly wants /companionfollow to carry
# the real follow icon); every ability macro clones its real counterpart.
# berserk1 has NO real style anywhere in the palette (the real command shows
# the default icon too) -> warcry1 is the closest aggro-shout visual.
MAPPING = {
    "companionfollow": "follow",
    "companionstay": "stopFollow",
    "companionpatrol": "areaTrack",
    "companionstore": "store",
    "companionattack": "attack",
    "companionformup": "formup",
    "hpet": "pethigh",
    # Companion System (2026-07-17, "pet command port" pass) -- icons for
    # the seven ported pet-order equivalents, each cloning an existing
    # palette style verified present in patched/ui_styles.inc:
    #   guard -> assist (helping-hand shield visual)
    #   followother -> combatTarget (see 2026-07-25 fix note below)
    #   rangedattack -> overchargeShot1 (ranged blast)
    #   specialone -> chargeShot1 (see 2026-07-25 fix note below)
    #   specialtwo -> defaultAttack
    #   group -> group
    #   friend -> consent (the grant-consent handshake glyph)
    #
    # BUG FIX (2026-07-25, live report: "icons are wrong on follow other,
    # jenkins, special attack one" -- all rendering blank/default). Root
    # cause, confirmed by cross-checking every ImageStyle definition in a
    # live extraction of patch_14_00.tre's ui_styles.inc: real hotbar/
    # command icons need BOTH a plain copy AND a matching ui_shader_add
    # copy at the same SourceRect, with no Size override (small square).
    # The three original picks each failed one of those:
    #   - CMD_uiFollowTarget: HUD overlay icon, Size='128,64' (oversized,
    #     non-square), and only has the ui_shader_add copy -- no plain
    #     copy at all. Never a valid command-icon candidate.
    #   - animalAttack: only has a plain copy, no ui_shader_add copy.
    #   - callRetreat (Jenkins' original pick, previous pass): same gap --
    #     no ui_shader_add copy, which is *also* why it silently fell back
    #     to looking like "assist" instead of erroring.
    # Replaced with combatTarget / chargeShot1 / setwarcry respectively --
    # each independently verified to have exactly one plain + one
    # ui_shader_add definition at the same SourceRect, no Size override.
    "companionguard": "assist",
    "companionfollowother": "combatTarget",
    "companionrangedattack": "overchargeShot1",
    "companionspecialone": "chargeShot1",
    "companionspecialtwo": "defaultAttack",
    "companiongroup": "group",
    "companionfriend": "consent",
    # Companion System (2026-07-20, "massive battlefield" pass, per user
    # request) -- return -> areaTrack (same "walk back to a marked spot"
    # visual already used for patrol; no dedicated "rally point" style
    # exists in the palette).
    "companionreturn": "areaTrack",
    # Companion System (2026-07-25, "Jenkins" pass, per user request "i also
    # want a icon macro i can use") -- went through two wrong picks before
    # this one: "callRetreat" (no ui_shader_add copy -- silently fell back
    # to looking like "assist"), then "shuttle" (a map-marker style, only
    # has a plain copy, no ui_shader_add copy either -- same class of bug,
    # see the dual-copy fix note above companionfollowother/specialone).
    # "setwarcry" is verified to have both copies at a matching SourceRect,
    # and fits the theme (a war cry before charging in).
    "jenkins": "setwarcry",
}
for a in _COMPANION_ABILITY_NAMES + _STARTER_ABILITY_NAMES:
    MAPPING["companion" + a] = "warcry1" if a == "berserk1" else a

def main():
    arc = TreArchive(BASE_TRE)
    data = arc.extract("ui/ui_styles.inc")
    text = data.decode("latin-1")

    # Index every ImageStyle block by name (case-insensitive).
    blockRe = re.compile(r"<ImageStyle\b[^>]*?/>", re.S)
    blocks = []
    for m in blockRe.finditer(text):
        nm = re.search(r"Name='([^']+)'", m.group(0))
        if nm:
            blocks.append((nm.group(1), m.start(), m.end(), m.group(0)))

    byLower = {}
    for name, s, e, blk in blocks:
        byLower.setdefault(name.lower(), []).append((s, e, blk))

    insertions = []  # (position, textToInsert)
    added = 0
    for newName, baseName in sorted(MAPPING.items()):
        occ = byLower.get(baseName.lower())
        if not occ:
            print(f"WARNING: no base style '{baseName}' for {newName} -- skipped")
            continue
        for s, e, blk in occ:
            clones = []
            for variant in {newName, newName.lower()}:
                clone = re.sub(r"Name='[^']+'", f"Name='{variant}'", blk, count=1)
                clones.append("\n\t\t\t\t" + clone)
            insertions.append((e, "".join(clones)))
            added += len(clones)

    # apply insertions back-to-front so offsets stay valid
    insertions.sort(key=lambda t: t[0], reverse=True)
    out = text
    for pos, ins in insertions:
        out = out[:pos] + ins + out[pos:]

    outBytes = out.encode("latin-1")
    dst = os.path.join(os.path.dirname(os.path.abspath(__file__)), "patched", "ui_styles.inc")
    with open(dst, "wb") as f:
        f.write(outBytes)
    print(f"base entries: {len(blocks)}, cloned entries added: {added}")
    print(f"wrote {dst} ({len(outBytes)} bytes, was {len(data)})")

    # sanity: every mapped command now resolvable
    outNames = set(n.lower() for n in re.findall(r"<ImageStyle\s+Name='([^']+)'", out))
    missing = [k for k in MAPPING if k.lower() not in outNames]
    print("missing after patch:", missing if missing else "none")

if __name__ == "__main__":
    main()
