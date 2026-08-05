# Companion client patch

`companion_patch.tre` — the one custom client archive this server uses.
**3,071,267 bytes, 16 records.** Generated 2026-08-04 21:48:59.

See `companion_patch.manifest.txt` for exactly what is inside; GitHub cannot
render a `.tre`, so the manifest is the only way to inspect it without
downloading.

## What it does

Adds the Companion Handler profession's client-side content: skill tables,
command definitions, string tables, the companion control device template, and
the icon styles its commands use.

## Installing it (players)

1. Put `companion_patch.tre` in your SWG client folder, beside the other
   `.tre` files.
2. Open `swgemu_live.cfg`, find the `[SharedFile]` section, and add a line:

   ```
   searchTree_00_29=companion_patch.tre
   ```

3. **Fully close and relaunch the client.** It caches TRE contents, so
   reconnecting is not enough.

### The three things that go wrong

**Priority.** The client loads the archive with the HIGHEST `searchTree_XX_NN`
number first, and the first archive holding a path wins. This patch must be
above every archive it overrides. Pick a number higher than any already in the
file — and check `maxSearchPriority` at the top of `[SharedFile]`: an entry
above that cap is **silently ignored**, the game loads normally, and nothing
anywhere logs a reason.

**Content packs.** If your client has a content pack (aftermath, etc.), this
patch is built against it. Installing it over a *different* pack will revert
that pack's version of any file listed in the manifest.

**Loose files beat archives.** <!-- LOOSE_UI_STYLES_README_2026_08_04 -->
The client reads loose files under the game folder in preference to *any* `.tre`.
If a file exists both loosely and in an archive, the loose copy wins and the
archive copy is never consulted. See the next section — on a SWG Returns install
this is not hypothetical.

## The icons need a second step

**If the companion commands show the right names but all share one generic
icon, this is why.**

SWG Returns ships a loose `ui/ui_styles.inc` in the client folder, and a loose
file beats any archive. The icon styles inside `companion_patch.tre` are
therefore never read, and every companion command falls back to the same default
glyph.

What makes this expensive to diagnose is that nothing looks wrong. The archive
loads. Its command tables and string tables work — the commands appear in the
Command Browser with their proper names, which *proves* the archive is being
read. Only the icons fail, and they fail silently.

The fix is to add the companion styles to the loose file as well:

```bash
python3 docs/companion_system/tools/patch_loose_ui_styles.py
```

It clones each companion style from that file's own base entries (so the art
matches your client build, not ours), writes a timestamped backup first, and is
safe to re-run. Then **fully relaunch the client**.

⚠️ **The launcher owns that file.** A client update can overwrite
`ui/ui_styles.inc` and take the companion styles with it. If the icons ever
revert to the generic glyph, re-run the script above — nothing is broken, the
file was simply replaced.

*How this was found, in case you hit something similar: a stock icon (`assist`)
was deliberately repointed at different pixels inside the TRE and the client
fully relaunched. It still drew its original icon — proving the client had never
read our copy of the file. Worth remembering as a technique: when every check
says the data is correct, change something you can SEE and find out whether
anything is reading it at all.*

## Installing it (server operators)

Put it in your `TrePath` and list it **first** in `TreFiles`. Order is the
whole point — the first matching archive wins, so a patch that is not first
does nothing at all.

Start once with `reloadstrings` after installing, or item names render as raw
`.iff` paths and flytext shows unresolved STF keys.

## Rebuilding

From `docs/companion_system/tools/`:

```bash
python3 build_companion_content.py
python3 build_tre_patch.py          # prints ARCHIVE VERIFIED OK
```

then publish it here with `swggenesis_menu.py tre_publish`.
