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
BASE_TRE = os.path.join(TRE_DIR, "aftermath_1.tre")

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

# Companion System (2026-08-07, "full combat tree ability coverage" pass, per
# live bug report: "how come im not seeing any of my companion's attack
# icons?"). The original 36 above only covered the 11 profession MASTER-tier
# skill.iff COMMANDS entries (the "badge-gated" set). Every ORDINARY tier
# skill in the same 11 professions' Accuracy/Speed/Ability/Support/weapon-
# tier I-IV branches -- i.e. almost everything a companion actually trains
# via normal xp, since master tier requires the whole tree first -- was never
# scanned at all. Confirmed by direct skills.iff COMMANDS extraction across
# every in-scope profession/starter prefix (combat_marksman/carbine/rifle/
# pistol/brawler/1hsword/2hsword/polearm/unarmed/bountyhunter/smuggler/
# commando, science_doctor/medic, outdoors_squadleader/scout, social_
# entertainer): 144 additional real, dispatchable ability tokens found (each
# verified to have a real command_table.iff row to clone, same discipline as
# the original 36/25). 2 excluded from that raw count: "formup" (collides
# with the existing hand-authored baseline /companionformup order command --
# a completely different feature, not an ability clone) and
# "registerWithLocation" (real command exists, but it registers a location
# on the planetary map -- a one-time character-progression side effect of
# learning Doctor novice, not a sensible companion combat ability).
_NEW_COMPANION_ABILITY_NAMES_2026_08_07 = [
    "actionShot1", "actionShot2", "aim", "berserk2", "bodyShot1",
    "bodyShot2", "bodyShot3", "boostmorale", "burstShot1", "burstShot2",
    "chargeShot1", "chargeShot2", "cripplingShot", "cureDisease", "curePoison",
    "dazzle", "disarmingShot1", "disarmingShot2", "diveShot", "doubleTap",
    "dragIncapacitatedPlayer", "extinguishFire", "fanShot", "feignDeath", "firstAid",
    "forage", "forceOfWill", "fullAutoArea1", "fullAutoArea2", "fullAutoSingle1",
    "fullAutoSingle2", "headShot1", "headShot2", "healEnhance", "healState",
    "healthShot1", "healthShot2", "intimidate2", "kipUpShot", "lastDitch",
    "legShot1", "legShot2", "legShot3", "lowBlow", "maskscent",
    "meditate", "melee1hBlindHit1", "melee1hBlindHit2", "melee1hBodyHit1", "melee1hBodyHit2",
    "melee1hBodyHit3", "melee1hDizzyHit1", "melee1hDizzyHit2", "melee1hHealthHit1", "melee1hHealthHit2",
    "melee1hHit1", "melee1hHit2", "melee1hHit3", "melee1hLunge2", "melee1hScatterHit1",
    "melee1hScatterHit2", "melee1hSpinAttack1", "melee1hSpinAttack2", "melee2hArea1", "melee2hArea2",
    "melee2hArea3", "melee2hHeadHit1", "melee2hHeadHit2", "melee2hHeadHit3", "melee2hHit1",
    "melee2hHit2", "melee2hHit3", "melee2hLunge2", "melee2hMindHit1", "melee2hMindHit2",
    "melee2hSpinAttack1", "melee2hSpinAttack2", "melee2hSweep1", "melee2hSweep2", "mindShot1",
    "multiTargetPistolShot", "overChargeShot2", "panicShot", "pistolMeleeDefense1", "pistolMeleeDefense2",
    "pointBlankArea2", "pointBlankSingle2", "polearmActionHit1", "polearmActionHit2", "polearmArea1",
    "polearmArea2", "polearmHit1", "polearmHit2", "polearmHit3", "polearmLegHit1",
    "polearmLegHit2", "polearmLegHit3", "polearmLunge2", "polearmSpinAttack1", "polearmSpinAttack2",
    "polearmStun1", "polearmStun2", "polearmSweep1", "polearmSweep2", "powerBoost",
    "quickHeal", "rally", "retreat", "revivePlayer", "rollShot",
    "scatterShot1", "scatterShot2", "steadyaim", "stoppingShot", "suppressionFire1",
    "suppressionFire2", "takeCover", "threatenShot", "tumbleToKneeling", "tumbleToProne",
    "tumbleToStanding", "unarmedBlind1", "unarmedBodyHit1", "unarmedCombo1", "unarmedCombo2",
    "unarmedDizzy1", "unarmedHeadHit1", "unarmedHit1", "unarmedHit2", "unarmedHit3",
    "unarmedKnockdown1", "unarmedKnockdown2", "unarmedLegHit1", "unarmedLunge2", "unarmedSpinAttack1",
    "unarmedSpinAttack2", "unarmedStun1", "volleyFire", "warcry2", "warningShot",
    "wildShot1", "wildShot2",
]
_COMPANION_ABILITY_NAMES = _COMPANION_ABILITY_NAMES + _NEW_COMPANION_ABILITY_NAMES_2026_08_07
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
    "companionguard": "defend",
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
    "companionreturn": "callRetreat",
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

# ICON_COLLISION_FIX_2026_08_04 -- two entries above were changed because they
# resolved to pixels another companion command was already drawing:
#   companionguard  was "assist", whose SourceRect is IDENTICAL to "pethigh"
#                   (the icon /hpet uses) in aftermath's palette -> now "defend"
#   companionreturn was "areaTrack", the same style /companionpatrol uses
#                   -> now "callRetreat"
# Both replacements were verified present in aftermath_1.tre's palette and
# unused by any other companion command. When adding a NEW hand-picked mapping,
# check the chosen style's SourceRect against every other entry here first --
# distinct style NAMES do not guarantee distinct PIXELS.
# Companion System (2026-08-07) -- same class of gap as berserk1 above:
# these three abilities have no ImageStyle anywhere in the real palette
# (the stock command shows the client's own default icon too, same as
# berserk1), discovered by actually running the clone pass below and
# reading its own "WARNING: no base style ... -- skipped" output rather
# than guessing. Each mapped to the closest thematically-related real icon
# that DOES exist and is already proven safe (self-clone precedent, not a
# hand-picked shared-chrome pick like the original 6 baseline commands, so
# the ICON_COLLISION_FIX_2026_08_04 risk class doesn't apply the same way):
#   berserk2 (combat_brawler_master, upgraded aggro shout)  -> warcry2
#     (the tier-2 sibling ability, already confirmed to have its own real
#     icon -- same reasoning berserk1->warcry1 already used one row up)
#   rally (outdoors_squadleader_support_01, squad buff)     -> boostmorale
#     (another squad-leader buff ability with a real icon, closest
#     available thematic match)
#   takeCover (combat_marksman_rifle_02, defensive posture)  -> tumbleToProne
#     (closest real icon to "get low/defensive" available)
_NO_REAL_ICON_OVERRIDES_2026_08_07 = {
    "berserk1": "warcry1",
    "berserk2": "warcry2",
    "rally": "boostmorale",
    "takeCover": "tumbleToProne",
}

for a in _COMPANION_ABILITY_NAMES + _STARTER_ABILITY_NAMES:
    MAPPING["companion" + a] = _NO_REAL_ICON_OVERRIDES_2026_08_07.get(a, a)

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
