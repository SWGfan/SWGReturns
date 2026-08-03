# CLAUDE ONBOARDING — Read This First On A New Computer

You are joining Nick's SWGEmu Core3 "Companion" project mid-flight. Everything
you need to know is in files, not in any chat history. Read in this order:

1. **This file** — how the project runs.
2. **NOTES.md** (this folder) — the complete engineering log. Every feature,
   bug, root cause, and fix, chronological. The last ~10 entries are the
   current state.
3. **HANDOFF.md** (this folder) — coordination state between chats and
   standing warnings. Skim the last few sections.
4. **RESTORE_INSTRUCTIONS.md** (in the backup zip / C:\MasterCompanionServerBackUp
   on the main computer) — full rebuild-from-zero guide for a new machine.

## What this project is

A custom "Companion Handler" profession for a private SWGEmu server ("JFF"):
players own humanoid companion NPCs (up to 5) that fight, equip real
weapons/armor/clothing with visual rendering, carry inventory, drive taxi
vehicles, set up real camps, auto-loot battlefields, harvest resources, and
(planned) doctor/entertainer buff routines. Built on Core3 (SWGEmu server)
with a custom client TRE patch.

## The multi-chat system (IMPORTANT)

Nick runs several Claude chats in parallel, each with a role:
- **"Companion"** — main build chat (compiles, heavy coding)
- **"c3r"** — research-only: reads code, writes findings to NOTES.md, never edits source
- **"AI Voice"** — research-only: AI voice integration research
- **Cowork desktop chat(s)** — coding + files + backups (this document's author)

Chats CANNOT read each other's conversations. ALL knowledge transfer happens
through NOTES.md and HANDOFF.md. Rules every chat follows:
- **Append to the notes, never rewrite them.**
- Log every significant change/finding in NOTES.md so other chats stay informed.
- When you finish thinking, give Nick copy-paste commands for his WSL/Debian terminal.
- When something needs a decision, give Nick options with a recommendation and why.
- After significant changes, remind Nick to refresh his backups.

## Machine setup (main computer — ADAPT PATHS on a new one)

- **Edit surface**: `C:\Companion\Core3` (Windows) — the master copy, what Claude edits.
- **Build copy**: `~/workspace/Core3` in WSL — synced from the edit surface, never edited directly.
- **Every rebuild** (Nick runs in WSL):
  `rsync -av --delete --exclude='autogen/' /mnt/c/Companion/Core3/MMOCoreORB/src/ ~/workspace/Core3/MMOCoreORB/src/ && rsync -av /mnt/c/Companion/Core3/MMOCoreORB/bin/scripts/ ~/workspace/Core3/MMOCoreORB/bin/scripts/ && cd ~/workspace/Core3/MMOCoreORB/build/unix/ninja-debug && ninja`
  (config changes additionally need: `cp /mnt/c/Companion/Core3/MMOCoreORB/bin/conf/config.lua ~/workspace/Core3/MMOCoreORB/bin/conf/config.lua`)
- **Run server**: `cd ~/workspace/Core3/MMOCoreORB/bin && gdb ./core3` then `r`.
- **Client**: `C:\SWGEmu` (SWGEmu.exe + swgemu_live.cfg with companion_patch.tre at top priority).
- **Server TREs**: `C:\Companion\tre` (config.lua TrePath), companion_patch.tre first in TreFiles.

## Iron rules (each one was learned the hard way — details in NOTES.md)

1. **NEVER sync or edit `src/autogen/`** — regenerated from .idl by the build; syncing corrupts it.
2. **NEVER run git commands from a Claude sandbox against this repo** — it corrupted .git/config once. Nick runs all git in his own WSL terminal.
3. **Exactly ONE custom TRE**: `companion_patch.tre` — updates REPLACE it (server copy, client copy, and github.com/SWGfan/CompanionTREs). Never create a second .tre.
4. **The server caches all STF strings in a database at first boot** — after changing string files, boot once with `r reloadstrings`.
5. **Client-authoritative walls** (cannot be fixed server-side): the client ignores server movement of the player's own mount; /follow can't be forced; client collision can't be disabled. Don't burn time retrying these.
6. **New .cpp files need a cmake reconfigure; new headers don't** — prefer header-only additions or put method bodies in existing .cpp files.
7. idl edits to EXISTING files regen automatically (mtime-gated) — safe.

## Backups

- **GitHub (private)**: https://github.com/SWGfan/Companion — full repo, branch `main`
  (local branch is `unstable`; push with `git push backup HEAD:main`).
- **TRE distribution**: https://github.com/SWGfan/CompanionTREs (public) — the
  player launcher downloads companion_patch.tre from here.
- **Local zip**: C:\MasterCompanionServerBackUp\CompanionServer_Backup_*.zip
  (+ loose RESTORE_INSTRUCTIONS.md) on the main computer.
- **Player installer**: repo `Launcher/` folder (Install SWGEmu JFF.bat).

## Getting a NEW computer to this state

1. Clone the private repo: `git clone https://github.com/SWGfan/Companion.git Core3`
   into the equivalent of C:\Companion (or unzip the backup zip).
2. Follow RESTORE_INSTRUCTIONS.md (in the zip; its steps map 1:1 to any machine).
3. Connect the equivalent folders to your Claude chats (Core3 parent folder,
   client folder), and point the new chats at this file first.
4. Nick's plan of record: the OTHER computer will host the PUBLIC server;
   this one stays the dev machine. Public-server extras: bind public
   interface, port-forward login 44453 + zone ports, set the launcher's
   $LoginHost to the public IP/domain, run the server as a service.

## Where work stands

Don't trust this section to stay current — read the END of NOTES.md. As of
2026-07-18: everything through "keep-up monitor" is coded; a big build/test
pass was in progress; phases 2-4 of the wild-camp-and-buff plan (doctor
buffs, entertainer dance, companion-to-companion fetch) are designed but not
started; backlog list lives at the end of the 2026-07-15 backup entry.
