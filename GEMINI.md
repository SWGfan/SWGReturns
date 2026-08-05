# SWGGenesis — project context

You are working on a private SWGEmu **Core3** server fork with a large custom
"Companion System" bolted on. This file is loaded automatically. Read it before
touching anything.

The owner is Nick. This is a fan project with no income — be efficient with
time and tokens. Prefer one correct action over three exploratory ones.

---

## Layout

| path | what it is |
|---|---|
| `/mnt/d/SWGGenesis` | the repo (Windows `D:\SWGGenesis`, reached through WSL) |
| `MMOCoreORB/src/server/zone/objects/companion/` | the Companion System — most custom work happens here |
| `MMOCoreORB/bin/scripts/custom_scripts/` | this fork's own Lua; hooked in from `scripts/screenplays/screenplays.lua` |
| `MMOCoreORB/build/unix` | build directory |
| `MMOCoreORB/bin/core3` | the server binary |
| `docs/companion_system/NOTES.md` | permanent engineering record — append findings here |
| `docs/companion_system/tools/` | `tre_reader.py`, `build_tre_patch.py`, and friends |
| `/mnt/d/Launcher/newreturnbenserver` | the **game client** the players actually run |
| `/mnt/c/Users/nickw/Downloads` | where Nick's patch scripts land |

## Server control

Never start or stop the server by hand — go through the menu:

```bash
python3 /mnt/d/SWGGenesis/swggenesis_menu.py status
python3 /mnt/d/SWGGenesis/swggenesis_menu.py shutdown
python3 /mnt/d/SWGGenesis/swggenesis_menu.py start
python3 /mnt/d/SWGGenesis/swggenesis_menu.py console_errors
python3 /mnt/d/SWGGenesis/swggenesis_menu.py backup_all
```

Valid actions are `status`, `start`, `shutdown`, `backup_all`, `console_errors`,
`remote_set`. `save` and `stop` are **not** actions — a job using them fails.

There is also a Windows control panel (`SWGGenesisControlPanel.ahk`) that shows
build progress, a colour pulse and an ETA. If you change what the panel reports
on, run `stamp_panel_update.py` so the footer's "last updated" line stays true.

---

## Rules that were learned the hard way

**Never edit `src/autogen`.** It is generated from the `.idl` files. Change the
`.idl`, not the output.

**Builds must `tee`.** `make | tail` hides all progress and makes a hung build
indistinguishable from a slow one:

```bash
cd /mnt/d/SWGGenesis/MMOCoreORB/build/unix
make -j$(nproc) core3 2>&1 | tee /tmp/build.log | tail -25
```

A full build is roughly 30–45 minutes. To tell a live build from a dead one:
`pgrep -c cc1plus`, or look for object files newer than two minutes.

**Patch with anchored Python scripts, not by hand.** The established pattern:
back up to `<file>.bak-<timestamp>`, require every anchor to match **exactly
once**, bail out if not, and stay idempotent via a marker constant like
`COMPANION_SOMETHING_FIX_2026_08_05`. Build anchors from the file **verbatim** —
one job failed because its anchors were built from `grep -vE '^\s*//'` output,
which had silently stripped the comments.

**Every fix gets a marker comment** naming what was wrong and why, not just what
changed. The codebase is searched by marker constantly.

**Check that a file is actually referenced before patching it.** Two builds were
spent editing `buffTerminalMenuComponent.lua`, which turned out to be an orphan
no object pointed at. One grep would have caught it.

**A failed pattern seen three times is a defect class, not a coincidence.** The
missing `activateMovementEvent()` call was found three separate times because
the first was treated as a one-off.

## Client-side assets

**Loose files beat `.tre` archives.** The client reads loose files on disk before
consulting any archive, for every asset type — UI, icons, sound. No
`swgemu_live.cfg` change is needed; there is no `searchPath_*` key in any config.

⚠ The SWG Returns launcher owns the client folder and can overwrite loose files
on a client update. If icons or audio ever revert, re-run the patch script.
`ui/ui_styles.inc` is the known case.

**A `.snd` file contains no audio.** It is a ~160-byte IFF wrapper holding a path
string to a `sample/*.wav` or `music/*.mp3` plus playback parameters. Voice audio
is PCM mono 22050 Hz 16-bit. `snd_tool.py` reads and writes them.

**`showFlyText()` takes an STF key, never runtime text.** Spatial chat
(`spatialChat(pNpc, "...")`) *does* take raw text and needs no TRE regen.

## Engine notes worth not rediscovering

- `activateMovementEvent()` is the only thing that makes an `AiAgent` move. Several
  calls to it are commented out in `AiAgent.idl`. Any new code that spawns,
  seats, mounts or repositions a companion probably needs one.
- `MovePetBase:checkConditions` requires `posture == UPRIGHT`, so a seated
  companion can never move on its own until something stands it up.
- `enqueueCommand()` honours the command queue and cooldowns;
  `activateCommand()` bypasses both.
- `AbilityList::add()` has no duplicate check in this fork.
- The server cannot position a SUI window. Chain boxes from callbacks instead.
- `playMusicMessage` is per-client and is a **silent no-op** on an NPC.
  `playEffect` broadcasts and is positional.

---

## Working style Nick expects

- **Finish with copy-paste commands for his Debian terminal.** He runs them; if
  you can run them yourself, say plainly what you ran and what it returned.
- **When something needs a decision, give options with a recommended pick and
  why** — not a menu with no opinion.
- **After significant changes, remind him to refresh backups.**
- Keep explanations short. He is technical but busy, and reads the summary first.
- Say when you are unsure. A confident wrong diagnosis costs him a 40-minute
  build.

## Before you finish

- Did the build actually return `rc=0`? Check, don't assume.
- Did you leave the server running?
- Is anything worth appending to `docs/companion_system/NOTES.md`?
- Is there anything uncommitted that should be? (Clear a stale
  `.git/index.lock` first if one is left over.)

## Standing policy

Push only to Nick's own GitHub (swgfan). **Never push to another server's
repository** — if they want these changes they can pull them.
