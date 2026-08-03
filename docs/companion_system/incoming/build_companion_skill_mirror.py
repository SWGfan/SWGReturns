#!/usr/bin/env python3
"""
Companion System (2026-07-27, "companion skill mirror" pass).

Mirrors real player commands into owner-callable "companion_<base>" commands
so that every ability a companion knows shows up in the owner's COMMAND
BROWSER with a real name, a real description, and the SAME ICON as the base
command. One run appends, per base command:

  1. a command_table.iff row named "companion_<base>", cloned from the base
     command's own row exactly the way build_command_table_rows.py's
     make_companion_ability_command() clones the 61 existing companion
     ability rows (same override set: hooks cleared, tempScript cleared,
     commandGroup/disabled/godLevel zeroed, stringId cleared, visible bumped
     0->2, everything else -- locomotion/state mask, targetType,
     addToCombatQueue, defaultTime, priority -- inherited verbatim from the
     proven real row), with
         characterAbility = "companion_<base.lower()>"
     -- the distinct owner-side gate string the server grants/revokes via
     CompanionSkillTrainer::trainSkill()/untrainSkill() exactly like the
     existing companion_<ability> gates (PlayerObject::hasAbility() path,
     see build_command_table_rows.py's 2026-07-13 "macro list" comment).

  2. a string/en/cmd_n.stf entry (the file the command browser + skills
     "Commands and Abilities Granted" panel resolve names through,
     case-insensitively -- see build_companion_content.py's
     build_cmd_n_stf()):
         "<base display name> (Companion)"

  3. a string/en/cmd_d.stf entry (the matching stock DESCRIPTION table --
     same fallback-on-miss pattern already proven for stat_n/stat_d/cmd_n):
         "<base description>

          COMPANION ACTION: you call it -- your companion performs it."

  4. patched/ui_styles.inc ImageStyle clones (same mechanism/output format
     as build_ui_styles_patch.py: clone the base command's ImageStyle block
     after EACH occurrence so both palette copies -- plain + ui_shader_add --
     get one, plus a lowercase-name duplicate to cover either
     case-sensitivity behavior) so companion_<base> renders the base
     command's icon. command_table.iff has NO icon column (75-col schema);
     the icon is purely a client-side style-name match.

INPUTS (existing tools/ conventions -- run from the tools/ dir):
  extracted/command_table.iff   771-row stock table (as build_command_table_rows.py)
  patched/command_table.iff     PREFERRED if present (build_command_table_rows.py's
                                848-row output) so mirror rows stack on top of the
                                16 baseline + 61 ability rows already shipped
  extracted/cmd_n.stf           stock names table (as build_companion_content.py)
  patched/cmd_n.stf             PREFERRED if present (build_companion_content.py output)
  extracted/cmd_d.stf           stock descriptions table. NOT previously extracted by
                                this project -- if missing, this tool auto-extracts it
                                from the client TRE dir (same candidate list as
                                build_ui_styles_patch.py) and saves it to extracted/
                                for reproducibility. Manual equivalent:
                                  python3 - <<'EOF'
                                  from tre_reader import TreArchive
                                  d = TreArchive("/mnt/c/Companion/tre/patch_14_00.tre").extract("string/en/cmd_d.stf")
                                  open("extracted/cmd_d.stf","wb").write(d)
                                  EOF
  patched/ui_styles.inc         PREFERRED if present (build_ui_styles_patch.py output);
                                else ui/ui_styles.inc is pulled straight from the TRE.
  extracted/skills.iff          only needed for --all (stock 1068-row table).

RUN ORDER: this tool must run AFTER build_command_table_rows.py,
build_companion_content.py and build_ui_styles_patch.py (each of those
rebuilds its patched/ file from base inputs and would DROP this tool's
appended content if run afterwards), and BEFORE build_tre_patch.py.

OUTPUTS (all under patched/, ready for build_tre_patch.py):
  patched/command_table.iff, patched/cmd_n.stf, patched/cmd_d.stf,
  patched/ui_styles.inc

NOTE for build_tre_patch.py: cmd_d.stf is a NEW packed file -- add
    ("string/en/cmd_d.stf", "cmd_d.stf"),
to its FILES list (one line, next to the existing cmd_n.stf entry).

PHASING / ROW-COUNT CAP: an in-game row cap for command_table.iff is
UNVERIFIED. 771 -> 848 rows (the current build_command_table_rows.py
output) is proven in-game; nothing beyond that is. Therefore this tool is
phased: the default base-command list is PHASE1_BASE_COMMANDS below (~20
common combat specials; edit the list there, or pass --commands-file
PHASE1_COMMANDS.txt), --limit N truncates any list, and the tool REFUSES to
append more than MAX_ROWS_WITHOUT_FORCE (100) new rows in one run unless
--force is given. Grow in phases and verify the client still logs in and
renders the command browser after each phase.

IDEMPOTENT / APPEND-ONLY: every base command whose mirror already exists
(either this tool's "companion_<base>" spelling or the pre-existing
"companion<base>" ability-macro spelling from build_command_table_rows.py)
is skipped; STF add() updates-in-place on re-run; ui_styles clones are
skipped when the style name already resolves. Re-running is a no-op that
produces byte-identical outputs. No stock row/entry/style is ever modified
or removed.

USAGE:
  python3 build_companion_skill_mirror.py                    # PHASE1 list
  python3 build_companion_skill_mirror.py --commands-file PHASE2_COMMANDS.txt
  python3 build_companion_skill_mirror.py --all              # every skills.iff-granted command
  python3 build_companion_skill_mirror.py --all --prefix combat_rifleman
  python3 build_companion_skill_mirror.py --limit 10 --dry-run
"""
import sys, os, re, glob, struct, argparse
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from iff_datatable import DataTable
from stf_codec import StfTable
from tre_reader import TreArchive

EXTRACTED = "extracted"
PATCHED = "patched"

# Same client-TRE-folder candidate list as build_ui_styles_patch.py
# (2026-07-17: the folder moves between environments; Debian sees /mnt/c).
_TRE_DIR_CANDIDATES = [
    "/sessions/inspiring-lucid-noether/mnt/Companion/tre",
    "/sessions/elegant-fervent-carson/mnt/Companion/tre",
    "/mnt/c/Companion/tre",
    "C:\\Companion\\tre",
]
TRE_DIR = next((p for p in _TRE_DIR_CANDIDATES if os.path.isdir(p)), _TRE_DIR_CANDIDATES[0])

MAX_ROWS_WITHOUT_FORCE = 100  # unverified cap; 771->848 is the proven ceiling so far

# ---------------------------------------------------------------------------
# >>> EDIT PHASE LISTS HERE <<<
#
# PHASE1_BASE_COMMANDS is the default work list: ~20 common combat specials
# (rifle/pistol/carbine/unarmed/1h/2h/polearm), chosen from real stock
# command_table.iff command names while deliberately EXCLUDING every base
# command already mirrored by build_command_table_rows.py's 61
# companion<Ability> rows (headShot3, warcry1, intimidate1, berserk1,
# mindShot2, torsoShot, *Lunge1, pointBlank*1, overchargeShot1, ...).
# Names are validated against the live table at runtime -- an unknown name
# is a per-command warning + skip, never a crash -- so this list is safe to
# edit freely. Later phases: put names in a text file (one per line, '#'
# comments allowed) and pass --commands-file.
# ---------------------------------------------------------------------------
PHASE1_BASE_COMMANDS = [
    # rifle (headShot3 already mirrored)
    "headShot1", "headShot2", "mindShot1", "fullAutoSingle1",
    # carbine
    "bodyShot1", "bodyShot2", "bodyShot3",
    # pistol
    "legShot1", "legShot2", "legShot3",
    # aggro shouts, tier-2 (tier-1 warcry1/intimidate1/berserk1 already mirrored)
    "warcry2", "intimidate2", "berserk2",
    # unarmed / melee 1h / melee 2h / polearm, tier-2 lunges
    # (all four *Lunge1 already mirrored)
    "unarmedLunge2", "melee1hLunge2", "melee2hLunge2", "polearmLunge2",
    # carbine/pistol tier-2 (tier-1 already mirrored)
    "pointBlankSingle2", "pointBlankArea2", "overchargeShot2",
]

# When a base command has no ImageStyle of its own anywhere in the palette,
# clone this style instead (build_ui_styles_patch.py precedent: real
# berserk1 has NO style -- the real command shows the default icon too --
# and warcry1 is the closest aggro-shout visual).
ICON_FALLBACKS = {
    "berserk1": "warcry1",
    "berserk2": "warcry1",
}

COMPANION_NOTE = "COMPANION ACTION: you call it -- your companion performs it."


def _prefer_patched(name):
    """Existing-convention input resolution: build on top of the sibling
    builder's patched/ output when present, else the stock extracted/ copy."""
    for d in (PATCHED, EXTRACTED):
        p = os.path.join(d, name)
        if os.path.isfile(p):
            return p
    raise SystemExit(
        f"ERROR: {name} not found in {PATCHED}/ or {EXTRACTED}/ -- extract it "
        f"from the client TREs first (see module docstring)."
    )


def _extract_from_tre(trePath_inside, saveTo=None):
    """Pull one file from the highest-priority client TRE that carries it
    (higher patch number wins, same priority rule the client uses)."""
    if not os.path.isdir(TRE_DIR):
        return None
    for tre in sorted(glob.glob(os.path.join(TRE_DIR, "*.tre")), reverse=True):
        try:
            data = TreArchive(tre).extract(trePath_inside)
        except (ValueError, AssertionError):
            continue
        if data is not None:
            print(f"extracted {trePath_inside} from {os.path.basename(tre)} ({len(data)} bytes)")
            if saveTo:
                os.makedirs(os.path.dirname(saveTo) or ".", exist_ok=True)
                with open(saveTo, "wb") as f:
                    f.write(data)
            return data
    return None


def _pretty(name):
    """headShot1 -> 'Head Shot 1' (fallback display name when the stock
    cmd_n.stf has no entry for the base command)."""
    s = re.sub(r"([a-z])([A-Z])", r"\1 \2", name)
    s = re.sub(r"([A-Za-z])(\d)", r"\1 \2", s)
    return " ".join(w[:1].upper() + w[1:] for w in s.replace("_", " ").split())


def load_base_commands(args):
    """Resolve the ordered, de-duplicated base-command work list."""
    if args.all:
        # skills.iff COMMANDS column is the authoritative skill-box ->
        # commands-granted mapping (the same source build_command_table_rows.py's
        # 36+25 ability lists were curated from). Same filtering discipline as
        # documented there: split on ',', strip the '+argument' convention
        # (startDance+basic), drop private_*/cert_* progression/cert tokens.
        path = os.path.join(EXTRACTED, "skills.iff")
        if not os.path.isfile(path):
            raise SystemExit(f"ERROR: --all needs {path} (stock 1068-row table).")
        dt, _ = DataTable.parse(open(path, "rb").read())
        colIndex = {name: i for i, (name, _t) in enumerate(dt.columns)}
        cmds, seen = [], set()
        for row in dt.rows:
            if args.prefix and not row[colIndex["NAME"]].startswith(args.prefix):
                continue
            for entry in row[colIndex["COMMANDS"]].split(","):
                base = entry.split("+")[0].strip()
                if not base or base.startswith(("private_", "cert_")):
                    continue
                if base.lower() not in seen:
                    seen.add(base.lower())
                    cmds.append(base)
        return cmds
    if args.commands_file:
        cmds, seen = [], set()
        for line in open(args.commands_file):
            base = line.split("#")[0].strip()
            if base and base.lower() not in seen:
                seen.add(base.lower())
                cmds.append(base)
        return cmds
    return list(PHASE1_BASE_COMMANDS)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    ap.add_argument("--all", action="store_true",
                    help="mirror every skills.iff-granted real command instead of the PHASE1 list")
    ap.add_argument("--prefix", default=None,
                    help="with --all: only skills.iff rows whose NAME starts with this (e.g. combat_rifleman)")
    ap.add_argument("--commands-file", default=None,
                    help="read the base-command list from a file (one per line, '#' comments)")
    ap.add_argument("--limit", type=int, default=None,
                    help="truncate the work list to the first N base commands (phase sizing)")
    ap.add_argument("--force", action="store_true",
                    help=f"allow appending more than {MAX_ROWS_WITHOUT_FORCE} rows in one run "
                         f"(row-count cap beyond 848 total is UNVERIFIED in-game)")
    ap.add_argument("--no-desc", action="store_true",
                    help="skip cmd_d.stf descriptions (only if the stock cmd_d.stf truly can't be obtained)")
    ap.add_argument("--dry-run", action="store_true",
                    help="report what would be appended, write nothing")
    args = ap.parse_args()

    os.makedirs(PATCHED, exist_ok=True)

    # ----- load command_table (prefer the sibling builder's patched output) --
    ctPath = _prefer_patched("command_table.iff")
    ctData = open(ctPath, "rb").read()
    dt, end = DataTable.parse(ctData)
    assert end == len(ctData)
    assert len(dt.columns) == 75, len(dt.columns)
    assert len(dt.rows) >= 771, len(dt.rows)
    baseRowCount = len(dt.rows)
    colnames = [c[0] for c in dt.columns]
    baseTypes = [t[0] for (_, t) in dt.columns]
    colIndex = {name: i for i, name in enumerate(colnames)}
    rowByLower = {r[0].lower(): r for r in dt.rows}

    def enc(colname, pyval):
        bt = baseTypes[colIndex[colname]]
        if bt == "s":
            return pyval
        elif bt == "f":
            return struct.pack("<f", float(pyval))
        else:  # 'i', 'e', 'b', 'h' -- all 4-byte int slots in this format
            return struct.pack("<i", int(pyval))

    def set_col(row, colname, pyval):
        row[colIndex[colname]] = enc(colname, pyval)

    # ----- load STF tables ---------------------------------------------------
    cmdN = StfTable.parse(open(_prefer_patched("cmd_n.stf"), "rb").read())
    cmdNDict = cmdN.as_dict()
    cmdNLower = {k.lower(): v for k, v in cmdNDict.items()}

    cmdD, cmdDLower = None, {}
    if not args.no_desc:
        cmdDPath = None
        for d in (PATCHED, EXTRACTED):
            p = os.path.join(d, "cmd_d.stf")
            if os.path.isfile(p):
                cmdDPath = p
                break
        if cmdDPath:
            cmdDBytes = open(cmdDPath, "rb").read()
        else:
            cmdDBytes = _extract_from_tre("string/en/cmd_d.stf",
                                          saveTo=os.path.join(EXTRACTED, "cmd_d.stf"))
        if cmdDBytes is None:
            raise SystemExit(
                "ERROR: no extracted/cmd_d.stf and no client TRE dir found to "
                "auto-extract it from (see module docstring for a manual "
                "one-liner). Descriptions REPLACE the whole stock file in the "
                "patch TRE, so building cmd_d.stf from scratch would wipe every "
                "stock command description -- refusing. Use --no-desc to skip."
            )
        cmdD = StfTable.parse(cmdDBytes)
        cmdDLower = {k.lower(): v for k, v in cmdD.as_dict().items()}

    # ----- load ui_styles (prefer build_ui_styles_patch.py's patched output) -
    uiPath = None
    for d in (PATCHED, EXTRACTED):
        p = os.path.join(d, "ui_styles.inc")
        if os.path.isfile(p):
            uiPath = p
            break
    if uiPath:
        uiText = open(uiPath, "rb").read().decode("latin-1")
    else:
        uiBytes = _extract_from_tre("ui/ui_styles.inc")
        if uiBytes is None:
            raise SystemExit("ERROR: no patched/ or extracted/ ui_styles.inc and no "
                             "client TRE dir found -- run build_ui_styles_patch.py first.")
        uiText = uiBytes.decode("latin-1")

    # Index every ImageStyle block by name, case-insensitive (identical
    # mechanism to build_ui_styles_patch.py).
    blockRe = re.compile(r"<ImageStyle\b[^>]*?/>", re.S)
    styleOcc = {}
    for m in blockRe.finditer(uiText):
        nm = re.search(r"Name='([^']+)'", m.group(0))
        if nm:
            styleOcc.setdefault(nm.group(1).lower(), []).append((m.start(), m.end(), m.group(0)))

    # ----- resolve work list -------------------------------------------------
    bases = load_base_commands(args)
    if args.limit is not None:
        bases = bases[:args.limit]

    new_rows, stf_adds, insertions, skipped, warnings = [], [], [], [], []

    for base in bases:
        baseRow = rowByLower.get(base.lower())
        if baseRow is None:
            warnings.append(f"WARNING: no command_table row named '{base}' -- skipped")
            continue
        base = baseRow[0]  # canonical spelling from the table
        mirrorName = "companion_" + base
        # Idempotence / dedupe: skip if mirrored under either naming scheme
        # (this tool's companion_<base>, or build_command_table_rows.py's
        # pre-existing companion<base> ability-macro spelling).
        for existing in (mirrorName, "companion" + base):
            if existing.lower() in rowByLower:
                skipped.append(f"{base} (already mirrored as '{rowByLower[existing.lower()][0]}')")
                mirrorName = None
                break
        if mirrorName is None:
            continue

        # -- command_table row: identical override set to
        #    build_command_table_rows.py's make_companion_ability_command() --
        row = list(baseRow)
        set_col(row, "commandName", mirrorName)
        set_col(row, "scriptHook", "")
        set_col(row, "failScriptHook", "")
        set_col(row, "cppHook", "")
        set_col(row, "failCppHook", "")
        set_col(row, "characterAbility", "companion_" + base.lower())
        set_col(row, "tempScript", "")
        set_col(row, "stringId", "")
        set_col(row, "commandGroup", 0)
        set_col(row, "disabled", 0)
        set_col(row, "godLevel", 0)
        if struct.unpack("<i", row[colIndex["visible"]])[0] == 0:
            set_col(row, "visible", 2)  # force hotbar-draggable, leave nonzero values alone
        new_rows.append(row)
        rowByLower[mirrorName.lower()] = row

        # -- STF name + description ------------------------------------------
        baseDisplay = cmdNLower.get(base.lower())
        if baseDisplay is None:
            baseDisplay = _pretty(base)
            warnings.append(f"note: no stock cmd_n entry for '{base}' -- using synthesized "
                            f"display name '{baseDisplay}'")
        display = baseDisplay + " (Companion)"
        baseDesc = cmdDLower.get(base.lower(), "")
        desc = (baseDesc + "\n\n" + COMPANION_NOTE) if baseDesc else COMPANION_NOTE
        if cmdD is not None and not baseDesc:
            warnings.append(f"note: no stock cmd_d entry for '{base}' -- companion note only")
        # cmd_n/cmd_d resolution is case-insensitive per prior research, but
        # keys are cheap: register the exact spelling plus a lowercase
        # duplicate (same both-cases hedge build_ui_styles_patch.py uses).
        for key in dict.fromkeys([mirrorName, mirrorName.lower()]):
            stf_adds.append((key, display, desc))

        # -- icon: clone the base command's ImageStyle under the mirror name --
        styleBase = base
        occ = styleOcc.get(styleBase.lower())
        if not occ and base in ICON_FALLBACKS:
            styleBase = ICON_FALLBACKS[base]
            occ = styleOcc.get(styleBase.lower())
        if not occ:
            warnings.append(f"WARNING: no ImageStyle '{base}' (or fallback) in palette -- "
                            f"{mirrorName} will show the default icon")
        elif mirrorName.lower() in styleOcc:
            pass  # already patched in on a previous run -- idempotent skip
        else:
            for s, e, blk in occ:  # after EACH occurrence: plain + ui_shader_add palettes
                clones = []
                for variant in dict.fromkeys([mirrorName, mirrorName.lower()]):
                    clones.append("\n\t\t\t\t" +
                                  re.sub(r"Name='[^']+'", f"Name='{variant}'", blk, count=1))
                insertions.append((e, "".join(clones)))

    # ----- cap gate ----------------------------------------------------------
    if len(new_rows) > MAX_ROWS_WITHOUT_FORCE and not args.force:
        raise SystemExit(
            f"REFUSING: {len(new_rows)} new rows requested in one run, cap-guard is "
            f"{MAX_ROWS_WITHOUT_FORCE}. The client's real command_table row limit is "
            f"UNVERIFIED -- only {baseRowCount} rows (stock 771 + existing companion "
            f"rows) are proven in-game. Split into phases with --limit / "
            f"--commands-file, verify each in-game, or re-run with --force."
        )

    # ----- summary -----------------------------------------------------------
    for w in warnings:
        print(w)
    if skipped:
        print(f"skipped {len(skipped)} already-mirrored: " + ", ".join(skipped))
    print(f"\ncommand_table.iff: {baseRowCount} rows -> {baseRowCount + len(new_rows)} "
          f"({len(new_rows)} appended)")
    print(f"cap check: {baseRowCount + len(new_rows)} total vs proven-in-game 848; "
          f"anything above is UNVERIFIED -- test login + command browser after this phase")
    for r in new_rows:
        print(f"  + {r[0]}  (ability gate: companion_{r[0][len('companion_'):].lower()})")

    if args.dry_run:
        print("\n--dry-run: nothing written")
        return

    if not new_rows and not stf_adds and not insertions:
        print("nothing to do (all requested commands already mirrored)")

    # ----- write command_table.iff ------------------------------------------
    dt.rows.extend(new_rows)
    rebuilt = dt.serialize()
    outCT = os.path.join(PATCHED, "command_table.iff")
    with open(outCT, "wb") as f:
        f.write(rebuilt)
    # post-write re-parse sanity check (same discipline as build_command_table_rows.py)
    dt2, end2 = DataTable.parse(rebuilt)
    assert end2 == len(rebuilt) and len(dt2.rows) == baseRowCount + len(new_rows)
    names2 = [r[0] for r in dt2.rows]
    for r in new_rows:
        assert names2.count(r[0]) == 1, r[0]
    print(f"wrote {outCT}: {len(dt2.rows)} rows, {len(rebuilt)} bytes (re-parse OK)")

    # ----- write cmd_n.stf / cmd_d.stf --------------------------------------
    for key, display, _desc in stf_adds:
        cmdN.add(key, display)
    outN = os.path.join(PATCHED, "cmd_n.stf")
    with open(outN, "wb") as f:
        f.write(cmdN.serialize())
    print(f"wrote {outN}: {len(cmdN.values)} entries")

    if cmdD is not None:
        for key, _display, desc in stf_adds:
            cmdD.add(key, desc)
        outD = os.path.join(PATCHED, "cmd_d.stf")
        with open(outD, "wb") as f:
            f.write(cmdD.serialize())
        print(f"wrote {outD}: {len(cmdD.values)} entries")
        print('REMINDER: add ("string/en/cmd_d.stf", "cmd_d.stf"), to build_tre_patch.py FILES')

    # ----- write ui_styles.inc (back-to-front so offsets stay valid) --------
    insertions.sort(key=lambda t: t[0], reverse=True)
    out = uiText
    for pos, ins in insertions:
        out = out[:pos] + ins + out[pos:]
    outUI = os.path.join(PATCHED, "ui_styles.inc")
    with open(outUI, "wb") as f:
        f.write(out.encode("latin-1"))
    print(f"wrote {outUI}: {len(insertions)} insertion points, {len(out.encode('latin-1'))} bytes")

    # sanity: every appended row's icon + name resolvable
    outNames = set(n.lower() for n in re.findall(r"<ImageStyle\s+Name='([^']+)'", out))
    missingIcons = [r[0] for r in new_rows if r[0].lower() not in outNames]
    print("rows without an icon style (default icon):", missingIcons if missingIcons else "none")
    print("\nNEXT: rebuild companion_patch.tre with build_tre_patch.py, then grant the "
          "printed companion_<base> ability strings server-side (CompanionSkillTrainer).")


if __name__ == "__main__":
    main()
