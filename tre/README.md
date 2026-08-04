# Companion client patch

`companion_patch.tre` — the one custom client archive this server uses.
**3,121,943 bytes, 16 records.** Generated 2026-08-04 14:46:00.

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

### The two things that go wrong

**Priority.** The client loads the archive with the HIGHEST `searchTree_XX_NN`
number first, and the first archive holding a path wins. This patch must be
above every archive it overrides. Pick a number higher than any already in the
file — and check `maxSearchPriority` at the top of `[SharedFile]`: an entry
above that cap is **silently ignored**, the game loads normally, and nothing
anywhere logs a reason.

**Content packs.** If your client has a content pack (aftermath, etc.), this
patch is built against it. Installing it over a *different* pack will revert
that pack's version of any file listed in the manifest.

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
