# Handoff -- read this first in a new chat

This project's real knowledge base is `docs/companion_system/NOTES.md` (detailed
decision-by-decision history, every bug found and fixed, exact root causes,
build recipes) and `docs/CODEBASE_GUIDE.md` (20-section reference on how this
codebase works generally -- commands, SUI, radials, skills/XP, combat, loot,
missions, guilds, vehicles, chat, crafting, etc.). Read both before doing
anything else in a new session. This file is just the "where we left off"
pointer.

## Next up (2026-07-16, corrected same day): build LIVE conversational companion voice -- part (B), NOT pre-baked TTS

**Correction**: this section originally scoped part (A) (pre-generated/
pre-baked `.snd` clips for fixed lines) as the first build. The user
explicitly rejected that after seeing it -- **no pre-recorded audio,
wants live/dynamic conversation instead.** Part (A) is shelved indefinitely
(not deleted as an idea, just not what's being built next -- the research
for it stays in NOTES.md in case it's revisited later for background
ambient barks or similar, where "pre-written" is actually fine).

**What's actually wanted: part (B), full live conversation** -- a player
can talk to a companion and get a dynamically-generated (LLM) response,
spoken back live, not a fixed pre-written line. Per the AI Voice chat's
research (see the "AI voice research" NOTES.md entry for full sourcing),
the hard constraint driving the whole design: the 2003 client can only
play audio files it already has (`playMusicMessage()` plays existing
named `.snd` files -- it cannot receive streamed or newly-generated audio
at all). So this **cannot** be built as a client/server protocol feature
the way every other companion feature so far has been -- it has to be a
**separate desktop companion app** running alongside the game, not a
Core3/.tre change. This is a different kind of build than anything this
project has done yet -- worth treating as its own component, not a patch
to the C++ server.

Concrete shape, per the research (adjust as needed, this is a starting
design not a locked spec):

1. **New app or extend the existing AHK Launcher** -- the
   SWGEmu-Core3-Server-Launcher (AutoHotkey v2) already exists as a
   companion app the user runs alongside the game; either add a "Talk to
   companion" feature to it, or spawn a separate Python/Node backend
   process it manages (AHK itself can't reasonably do the audio pipeline
   below -- it would shell out to a real backend).
2. **Pipeline**: push-to-talk hotkey -> speech-to-text (local Whisper
   recommended: free, private, fast enough) -> LLM call with a
   per-companion personality/system prompt (Claude or GPT API for
   quality, or a self-hosted Ollama model for zero marginal cost) ->
   streaming text-to-speech (ElevenLabs Flash for low latency if quality
   matters, or local Piper for zero cost) -> audio output, mixed with
   game audio via a virtual audio cable (e.g. VB-Cable) so it's heard
   alongside the game without touching the client.
3. **Situational awareness without touching the client**: SWG's client
   writes a local plain-text chat log file -- a file-watcher tailing that
   log can feed recent in-game events/chat into the LLM's context so
   responses feel aware of what's happening, without any memory-reading
   or process injection (safe, read-only, same approach the
   `swglogparser` community tool already uses).
4. **Prior art to reference, not build from scratch**: `Mantella` (Skyrim
   mod, same STT->LLM->TTS-for-an-NPC pattern, closest analog found),
   `Pipecat` (open-source framework specifically for building this kind
   of streaming pipeline), `Open-LLM-VTuber` (reference for swappable
   STT/LLM/TTS backends). AzerothCore's `mod-ollama-chat` /
   `mod-llm-chatter` and ServUO's `uo-llm-npc` are the closest "LLM NPC on
   a legacy MMO emulator" prior art, though those solve TEXT chat
   server-side, not live voice through an external app -- useful for the
   "keep the LLM out of the simulation loop, fail-open, hardcoded action
   allowlist" safety pattern, less useful for the audio half specifically.
5. **Expect real per-conversation cost** if using cloud STT/LLM/TTS (this
   is the first genuinely metered, ongoing-cost feature in this project --
   everything else so far has been free/one-time). Self-hosted
   (Whisper+Ollama+Piper) avoids that entirely at the cost of setup effort
   and needing real hardware (a GPU helps a lot for acceptable latency).

Realistic round-trip latency: ~0.5-1.5s with a good streamed cloud stack,
which is fine for a "live NPC" feel; fully local is hardware-dependent and
can be much slower (8-25s) on weak hardware -- worth benchmarking early
before committing to an all-local stack.

This is a bigger, more open-ended build than anything else in this
project so far (new app, new dependencies, ongoing cost, first
LLM-in-the-loop feature) -- whichever chat picks this up should probably
start with a small proof-of-concept (one companion, one hardcoded
personality, cloud APIs for speed of iteration) before investing in the
full local/self-hosted version.

**UPDATE (2026-07-16, later): full concrete spec now written up, still
research only.** The AI Voice chat ran a much deeper pass (16 total
research agents across two batches) and has a build-ready recommended
stack in the "Live conversational companion: concrete architecture spec"
entry in NOTES.md -- read that in full before starting, it answers most
of the open questions this section originally raised. Short version for
orientation: **STT** = faster-whisper (local) or Deepgram Nova-3 (cloud);
**LLM** = Claude Haiku 4.5 with prompt caching on the character system
prompt; **TTS** = ElevenLabs Flash (best) or Kokoro-TTS (free/local);
**client integration** = AutoHotkey v2 frontend (extends the existing
Launcher) calling a local Python HTTP backend, plain WASAPI audio
playback (no virtual cable needed -- confirmed unnecessary); **turn-taking**
= push-to-talk for v1, not always-listening; **build order** = prototype
first with Player2/Elefant AI alone (free, zero setup) to validate the
plumbing, then swap in the real stack, then add the safety layer
(prompt-injection guarding, per-player cost caps, content moderation --
NOT optional, do before any public rollout), then packaging
(PyInstaller + Ahk2Exe + Inno Setup installer). Cost is real and ongoing
at this stack (~$0.015/exchange, ~$18-2,000+/mo depending on usage
scale) -- this is the first metered/ongoing-cost feature this project
would ship, budget accordingly.

Per the user directly: **the AI Voice chat will never build this itself**
-- it stays research-only permanently. Whichever of Companion or Fable
the user picks to actually build it should treat NOTES.md's spec entry
as the starting brief, not re-derive the research.

**SHELVED (2026-07-16, later still): user doesn't want to buy a GPU right
now, so the whole live-voice feature is on hold** -- not cancelled, just
not being built next. Don't pick this up unless the user explicitly
revisits it (e.g. after acquiring a GPU for other reasons, or deciding
cloud/BYOK cost is acceptable after all). The spec below stays valid for
whenever that happens.

**FINAL DECISION (2026-07-16, later): SELF-HOSTED, not cloud APIs.**
User wasn't expecting an ongoing cost (fair, everything else in this
project has been free/one-time) and chose self-hosted over cloud/BYOK/
shelving once shown the options. Stack is now: local `faster-whisper`
(STT) + local **Ollama** LLM (not Claude/GPT) + local **Kokoro-TTS**/Piper
(TTS) -- $0 marginal cost forever, but needs a real GPU (16-24GB VRAM
class, ~$800-2,000) **separate from and in addition to** the game
server's own hardware (the earlier "no GPU needed" server-sizing advice
was specifically for the C++ Core3 simulation server -- AI voice
inference is a different workload with different hardware needs). Known
real risk flagged in NOTES.md: stock Ollama serves requests
**sequentially** -- fine for 1 player, degrades hard (2s -> 45s+ reported)
past ~5 simultaneous conversations -- look at vLLM or a real
concurrent-serving setup before assuming this scales. Full detail and
build order in the NOTES.md "DECISION" entry right after the architecture
spec.

## Multi-chat coordination protocol

Separate chats in this project can't message each other directly -- this
file (plus NOTES.md and CODEBASE_GUIDE.md) is the only channel, relayed by
the user copy-pasting between chats. To avoid two chats researching the same
thing at once, or one chat editing a file mid-build in another:

1. **Check "Active work claims" below before starting any non-trivial
   research or edit.** If your intended area is already claimed by another
   chat, pick something else or wait.
2. **Add a claim line before you start**, in this format:
   `- [chat role/topic] started <what you're about to do> -- <rough time/context, e.g. "while the store-command build was running">`
3. **Remove your claim when done**, and fold anything reusable into the
   right file (see below) rather than leaving it only in this claims list.
4. **Where new knowledge goes**, so it compounds instead of getting
   re-derived every session:
   - General "how does subsystem X work" findings, useful beyond this one
     feature -> `docs/CODEBASE_GUIDE.md` (new numbered section, or extend
     an existing one).
   - Companion-system-specific decisions, bugs, fixes, build state ->
     `docs/companion_system/NOTES.md` (new dated section).
   - Current overall status / what to do next -> this file (HANDOFF.md),
     kept short, pointing at the two files above for detail.
5. **Don't edit live source files from a research-only chat** while another
   chat is mid-build/mid-test on the same repo -- read-only until the
   active build-fix chat confirms a stable state.

### Chat roles (as of 2026-07-14, per the user)

Three named chats currently work this repo, each with a distinct job. Every
chat should still follow the coordination protocol above (check claims,
read NOTES.md/HANDOFF.md/CODEBASE_GUIDE.md first, log what you did) --
this section just clarifies who's *supposed* to be doing what, so nobody
duplicates another chat's role by accident.

- **"c3r"** -- this chat (the one whose log entries you're reading right
  now). Research-only, Sonnet-based. Reads the codebase, writes findings
  into NOTES.md/CODEBASE_GUIDE.md/HANDOFF.md, never edits live source
  (.cpp/.h/.idl/.lua/.py build scripts). Good for grounding a feature
  request in real code before anyone builds it, and for parallel-agent
  research sweeps.
- **"Companion"** -- the live build/ops chat. This is where the user
  asks for the server to actually be built, rebuilt, and run, and where
  they ask in-the-moment questions about whatever they're doing at that
  exact time (often triggered by something just observed in-game, like a
  live bug). Edits source directly, runs the build/deploy loop.
  (Historically there have been two sessions literally named "Companion"
  -- both play this same role; check "Active work claims" for which one
  is currently active if it matters.)
  - **"Fable"** -- also builds source directly (same as "Companion"), but
  is additionally the escalation/deep-dive chat: **the user has stated
  Fable is currently the most capable/smartest of the three.** When a
  research question or a build problem is too hard for "c3r" or
  "Companion" to work out, it goes to Fable. Fable's own NOTES.md entries
  (e.g. the "Overnight batch"/"Evening batch" dated entries in this repo)
  should generally be treated as the most authoritative/most-vetted
  findings among the three chats when they conflict with something an
  earlier c3r research pass concluded -- see the companion-group-
  membership correction as a concrete example (c3r's own earlier research
  wrongly concluded companions could never be real group members; Fable
  had already built the real fix by the time c3r re-checked).
- **"AI Voice"** -- a fourth chat, research-only like c3r (never edits
  live source), scoped specifically to AI voice/audio integration for
  companions. Found the delivery mechanism is `playMusicMessage()` +
  `broadcastChatMessage()`, and that this is a two-tier problem: (1)
  TTS for existing/future scripted companion lines -- small, concrete,
  the only real gap is nobody's produced a `.snd`-format audio file
  yet; (2) real-time conversational NPCs -- a much bigger, genuinely
  new subsystem (first LLM-in-the-loop feature this project would
  have), needs an external companion app since the 2003 client can't
  run any SDK. Full write-up in the "AI voice research" dated entry in
  NOTES.md -- read that before starting any voice-related work so it
  isn't re-researched from scratch.
- **All four read and write the same three files** (NOTES.md,
  CODEBASE_GUIDE.md, HANDOFF.md) -- that's the entire coordination
  channel between them, there is no other way for these chats to talk to
  each other. If you're any of these chats picking this file up
  cold, assume the others are active in parallel and may have changed
  something since your last read -- re-`Grep`/`Read` fresh before editing,
  don't trust a cached view (see the stale-mount/stale-Read gotchas
  documented elsewhere in this file and in NOTES.md).

### False alarm, 2026-07-13: CODEBASE_GUIDE.md was never actually missing sections

A new research-only-chat session (continuing this same chat after a context
compaction) briefly believed `CODEBASE_GUIDE.md` had lost sections 40-70 --
both the `Read` tool (at a specific offset/tail check) and the bash-tool
mount showed the file ending cleanly after section 39. Acting on that, it
started re-appending reconstructed versions of sections 40-46 from this
file's own pass-summary bullets. A follow-up check with the `Grep` tool
(not `Read`, not bash) revealed the real sections 40-70 were there all
along, further down the file (~7600 lines total, not ~4200) -- both the
earlier `Read` calls and the bash mount were serving **stale/truncated
cached content**, exactly the "recurring gotcha" this file already
documents further down (stale bash-mount reads), except this time it fooled
`Read` too, on a file this large. The duplicate sections 40-46 this chat
had just inserted were found and removed immediately; `CODEBASE_GUIDE.md`
is confirmed clean, 1-70, no dupes, no gaps, verified via `Grep` with
pattern `^## \d+\.` across the whole file.

**Lesson for future chats**: on a large doc file, don't trust a single
`Read` at a guessed offset (or bash `tail`/`grep`) to confirm true EOF or
section count -- use the `Grep` tool (pattern `^## \d+\.`, no head limit)
for a full, reliable structural listing before concluding content is
missing or corrupted. This file's existing advice to distrust the bash
mount was correct but incomplete -- `Read` itself can also serve stale
content on a large enough file.

### Active work claims

- [research-only chat] finished the seventh batch (sections 70-72:
  skill training, group invite/kick/disband, entertainer performance
  formulas), then ran an eighth batch of 8 topics **in parallel via 8
  subagents** at the user's explicit request, adding sections 73-80:
  overt/covert faction status, bazaar cross-vendor search, NPC AI
  behavior tree/aggro/patrol, food/drink consumable buffs, player city
  government, loot table roll mechanics, combat action/command queue
  internals, waypoint/datapad/quest journal. A ninth 8-topic parallel
  batch is also done, adding sections 81-88: Jedi/lightsaber crafting,
  survey tool/harvester concentration/tier mechanics, weather/
  environment system, HAM stat pool allocation, squad/ops group
  (confirmed absent), chat moderation/profanity filter/mute, character
  deletion/transfer, crafting station vs. handheld tool. A tenth
  8-topic parallel batch is also done, adding sections 89-96: ship
  component installation/weapon energy management, structure
  maintenance-droid automation, COD mail (confirmed absent)/secure
  trade edge cases, static world content loading (terrain/POI/
  dungeons), emote/social action system, pet/vehicle/vendor store-
  summon persistence, Doctor profession buffs/poison-disease-as-
  weapon, server console/remote admin tooling. Continuing to cycle
  further 8-topic parallel batches per the user's "keep going,
  stepping out for an hour" instruction, adding a new dated pass-
  summary paragraph after each batch. No companion source files
  touched, no conflict expected with any claim below.
  **Second thing worth flagging** (not a bug fix, a research finding
  with real backlog relevance): section 94 found the real, shipped
  `PetControlDevice`/vehicle system has the **same** "native setter
  never calls the real persistence primitive" gap this project already
  found and patched for `CompanionObject` (see the persistence-gap
  entries earlier in this file) -- a real pet's vitality/HAM setters
  are plain native mutators with no dirty-mark call either, just less
  consequential in practice since an idle stored pet has little state
  to lose. This is a good sanity-check data point: this project's own
  persistence fix was addressing a real, systemic engine-wide pattern,
  not an over-cautious companion-only worry. Worth remembering if a
  player ever reports a *real* (non-companion) pet losing vitality/HAM
  across a restart -- the root cause is now already traced, no need to
  re-diagnose.
  An eleventh 8-topic parallel batch is also done, adding sections
  97-104: GCW crackdown system, city militia, vehicle/mount terrain &
  speed tiers, GCW military rank system/faction recruiter, bounty
  posting mechanics (confirmed automatic, not player-initiated),
  structure condition repair (confirmed tied to maintenance, no repair
  kits exist), structure/harvester placement spacing (confirmed
  footprint-only, no minimum distance), new player tutorial system.
  `CODEBASE_GUIDE.md` is now 104 sections. A twelfth 8-topic parallel
  batch is also done, adding sections 105-112: combat defenses (dodge/
  parry/block/counterattack), encumbrance system, GCW score/planetary
  faction control, GCW base turret combat AI, schematic acquisition
  (reverse engineering/loot schematics), faction perk (`private_*`)
  skill mod catalog, guild hall/guild structures (confirmed not a
  distinct object type), combat state effects (stun/knockdown/blind/
  dizzy/intimidate/incapacitation). `CODEBASE_GUIDE.md` is now 112
  sections. A thirteenth 8-topic parallel batch is also done, adding
  sections 113-120: skill point budget/profession point economy,
  damage type/armor resistance system, weapon range mechanics, solo/
  non-grouped corpse looting permissions, GCW base placement/deed
  system, player command macro system (confirmed client-side only),
  vehicle deed acquisition, storage crates/house cell connectivity.
  `CODEBASE_GUIDE.md` is now 120 sections. A fourteenth 8-topic parallel
  batch is also done, adding sections 121-128: Jedi holocron item
  mechanic, vendor barker/advertisement system, Personal Shield
  Generator (PSG) mechanics, new player "theme park" quest chains,
  structure relocation (confirmed absent, redeed-and-rebuild only),
  group mission sharing (confirmed individual acceptance, shared
  reward only), personal PvP rating/kill tracking, Jedi Padawan/mentor
  system (confirmed absent). `CODEBASE_GUIDE.md` is now 128 sections.
  A fifteenth 8-topic parallel batch is also done, adding sections
  129-136: static NPC shopkeeper vendors, structure ownership transfer,
  cloning facility binding, multi-seat/capital ships, NPC conversation
  branching/conditional dialogue, city-vs-city GCW conquest (confirmed
  absent), hit-location/called-shot combat system (a genuine
  correction/expansion to section 114 -- armor mitigation turns out to
  be per-body-location as well as per-damage-type, a real system this
  guide had missed until this pass), faction HQ building perk catalog
  by tier. `CODEBASE_GUIDE.md` is now 136 sections. At this point
  coverage is very deep and new genuinely-uncovered topics are getting
  harder to find -- future batches should expect a higher proportion
  of "confirmed absent" or corrections-to-existing-sections results
  rather than wholly new material. Continuing to cycle further 8-topic
  parallel batches per the user's "keep going" instruction.
  A sixteenth 8-topic parallel batch is also done, adding sections
  137-144: minefield mechanics, covert detector installations, faction
  building registration mechanic (`isPlayerRegisteredWithin`), combat
  while mounted on a vehicle (confirmed blocked), account ban/
  suspension system, XP conversion between skill types (confirmed
  narrow/one-way only, no general conversion), real estate/structure-
  for-sale browsing (confirmed absent), faction uniforms (confirmed
  purely cosmetic). `CODEBASE_GUIDE.md` is now 144 sections. A
  seventeenth 8-topic parallel batch is also done, adding sections
  145-152: ranged weapon ammo/power pack consumption (confirmed
  absent -- personal weapons have no ammo system, only ships do, per
  section 89), faction NPC guard aggression near GCW bases (real,
  reuses section 73's OVERT/TEF machinery), creature corpse tissue/
  hide/bone harvesting (separate mechanic from loot rolls, draws from
  live resource-spawn state), combat-state blocking of shuttle/travel/
  clone commands (gate lives in `command_table.iff` data, not C++ --
  worth remembering when grepping for combat guards), enemy structure
  sabotage (confirmed absent outside GCW siege -- civilian structures
  are categorically unattackable), pet/creature death and revival
  (creature pets are forgiving -- incapacitate-then-recover is the
  norm, true permadeath is the edge case), /tell and friends-list
  internals (no offline queuing for /tell, unlike mail; friends list
  is real server-push), and space station docking/NPC space
  encounters (docking permission hook is stubbed/non-functional;
  space spawning and space missions are fully parallel systems to
  their ground equivalents). `CODEBASE_GUIDE.md` is now 152 sections.
  An eighteenth 8-topic parallel batch is also done, adding sections
  153-160: weapon/armor repair kit system (real, crafter-gated 5-charge
  tools -- sharply contrasts with section 102's confirmed-absent
  structure repair kits), creature trap/capture mechanic (no physical
  trap item -- pure `/tame` command + roll, baby-creatures-only),
  guild permission bitmask catalog (8 flat flags, no treasury bit at
  all, confirming section 111), house/structure ban list & visitor
  access (`StructurePermissionList`, fully separate from guild
  permissions but interoperates via guild-ID list entries), bazaar/
  vendor sales fee stacking (up to 3 deductions on a bazaar sale: flat
  listing fee + city sales tax baked into price + nothing extra for
  buyer), buff reapplication rules (engine default is destroy-and-
  replace, not refresh -- four distinct caller-side override patterns
  found, including a new pairing of doctor enhance buffs with
  entertainer buffs' "reject if weaker" rule not previously noted in
  section 95), droid pet command/behavior (combat is opt-in via module/
  species, death has zero recovery path -- confirms and hardens
  section 150), resource/item decay and spoilage (confirmed absent for
  raw resources and food -- the engine's time-based decay pattern
  exists but is scoped to exactly one special quest item,
  `FsCsObject`). `CODEBASE_GUIDE.md` is now 160 sections. Continuing
  to cycle further 8-topic parallel batches per the user's "keep
  going" instruction.
  **Second thing worth a second pair of eyes**: while researching
  bazaar/vendor fee stacking (section 157), a subagent flagged an
  inline comment in `AuctionManagerImplementation.cpp` (around lines
  2291-2300, in `expireAuction()`'s payout block) dated 2026-07-13,
  describing a fix for a real bug where bid-won auctions transferred
  the item and notified both parties but never actually paid the
  seller. Unlike the CombatManager.cpp comment flagged earlier (still
  below), this one reads like a normal engineering fix-note and fits
  the established pattern of other specialist chats leaving dated
  commit-style comments (see the companion-container-fix and
  build-fix chat entries above) -- not treated as suspicious, just
  noted here in case an economy/auction-focused chat wants to verify
  the fix against NOTES.md.
  A nineteenth 8-topic parallel batch is also done, adding sections
  161-168: personal bank/credit storage (confirmed strictly
  per-character, never account-shared, 2B cap per pool, no fee), item
  no-trade/soulbound flags (real and consistently enforced across
  trade/vendor/bazaar, but opt-in per template -- the new-player
  vehicle coupon is a live counterexample of an unbound "unique"
  item), structure paint/coloring (confirmed absent -- a genuine gap,
  not just undocumented, unlike most other absent findings which have
  workarounds), vendor NPC "mannequin" appearance (confirmed server-
  randomized cosmetic only, no player-facing merchandising display),
  auction bid war/proxy bidding (real eBay-style proxy system, no
  anti-snipe extension, no bid retraction), city specialization bonus
  catalog (2 of 8 specializations -- Clone Lab and Entertainment
  District -- confirmed to have zero code consumers, i.e. selectable
  but mechanically inert), Jedi visibility/unlock mechanic
  (deterministic 5-category badge threshold under village progression,
  not XP-based or random like classic SWG), death penalty/XP loss
  (confirmed Jedi-only XP loss on death, zero penalty for any other
  profession beyond standard healable wounds, no clone-sickness debuff
  exists). `CODEBASE_GUIDE.md` is now 168 sections. A twentieth
  8-topic parallel batch is also done, adding sections 169-176:
  species/race mechanical differences (real, non-cosmetic -- affects
  starting HAM, XP-gain rate via a previously-undocumented per-species
  multiplier, 4 innate species-gated commands, and Imperial faction
  vendor pricing), character title/surname system (renaming is
  GM-only, no player rename token exists; only one true title field,
  guild/GCW/mayor titles are separate client-composed fields), group
  finder/LFG (confirmed passive self-tag + `/who` search filter only,
  no real matchmaking, and the matchmaking-ID commands are dead
  stubs), GCW score/standing decay (confirmed absent entirely -- both
  are pure accumulate/snapshot with no time-based erosion), vehicle/
  ship vanity naming (ships real and free; ground vehicles confirmed
  to have zero renaming capability, a genuine feature gap), duel
  rematch/cooldown (no cooldown exists at all; duel kills DO feed the
  Elo pvpRating same as open-world PvP, only GCW standing is
  duel-exempt), GCW recruiter bribe mechanic (flat credit-to-standing
  converter, double-gated by Smuggler skill + OVERT status, no
  cooldown beyond a soft rank-cap ceiling), crafted-item salvage
  (confirmed absent -- crafted items can't even reach the junk
  dealer's payout, a genuine economic dead-end with zero return on
  disposal in any form). `CODEBASE_GUIDE.md` is now 176 sections.
  A twenty-first 8-topic parallel batch is also done, adding sections
  177-184: veteran reward system catalog (real day-threshold ladder,
  confirms/corrects earlier findings -- Anti-Decay Kit is specifically
  the 360-day reward, space yacht deed is specifically the 180-day
  reward), OVERT/COVERT resignation mechanic (full faction/rank reset
  but standing-preserving, 5 or 15 min timer depending on config),
  faction standing delegate/transfer + Cries of Alderaan bonus (both
  confirmed live -- delegation has an economic tax but zero cooldown;
  CoA is a hidden automatic +10% standing-gain bonus for the weekly
  quest-chain-determined "winning" faction, separate from GCW base
  score), vehicle/droid custom color dye (shared engine, but droid
  recolor is skill-gated via Droid Engineer while vehicle recolor
  isn't -- both are finite-charge consumables unlike unlimited armor
  dye), lightsaber crystal color mechanic (color = socket state, all
  12 base colors equally common by design, no rarity tiers despite
  classic-SWG lore expectations, legacy unique color names are dead
  content), recall/instant-travel-home item (confirmed absent --
  genuine clean gap, no scaffolding to extend), GM invisibility/
  teleport admin tools (three genuinely separate flags -- invisible,
  PvP-invulnerable, damage-invulnerable -- plus a recurring
  `hasGodMode()`-bypass-permission-stub pattern also seen in section
  152's docking stub), structure maintenance grace period/eviction
  (hard-coded 30-day back-maintenance destruction threshold,
  harvesters/factories/turrets get zero grace unlike player housing,
  city tax delinquency has no separate eviction path -- fully
  subsumed into ordinary maintenance decay). `CODEBASE_GUIDE.md` is
  now 184 sections. A twenty-second 8-topic parallel batch is also
  done, adding sections 185-192: no-PvP/no-combat safe zones (real,
  fully wired `NOCOMBATAREA`/`NODUELAREA` ActiveArea flags, but zero
  current world content actually uses them -- a one-line Lua addition
  would activate a safe zone anywhere), player container lock/
  combination system (confirmed absent -- the one `locked` boolean on
  Container is a loot-crate slicing gimmick with dead/commented-out
  enforcement code, not a player feature), ground escort missions
  (real via the Screenplay system, `quest_tasks/escort.lua`, but
  MissionTypes::ESCORT is confirmed dead/unused in the terminal system
  -- third confirmed case of this ground/space split pattern after
  section 152's space missions), AFK status (confirmed purely
  self-toggled, no server-side inactivity detection or idle-kick
  exists anywhere), linkdead disconnect handling (real ~3-minute
  vulnerable limbo window, seamless reconnection, no XP/credit penalty
  on timeout), Doctor/medic group heal (real but is a crafted-item AOE
  property with no group-membership requirement and zero per-target
  cost scaling -- strictly more efficient than single-target spam),
  Jedi robe crafting (confirmed NOT player-craftable -- pure reward/
  promotion content with real Force-pool stat bonuses, unlike
  cosmetic-only faction uniforms), structure no-build zones around
  POIs (three independent stacking mechanisms: region flag, per-
  template radius, flat 150-unit POI check, all structure-type-
  agnostic). `CODEBASE_GUIDE.md` is now 192 sections.
  A twenty-third 8-topic parallel batch is also
  done, adding sections 193-200: world dynamic/random events (only
  one true holiday event exists -- Life Day, boot-time-gated not a
  real scheduler; rampage/faction-raid mechanic confirmed completely
  absent), Jedi-specific bounty hunting (player bounty targets are
  exclusively and unconditionally Jedi, airtight across every code
  path; a third TEF-bypass channel `hasBhTef()` confirmed alongside
  duel and crackdown TEF), dual wielding (confirmed absent by design
  -- single weapon field on CreatureObject, equip is always a swap
  never a simultaneous hold), vehicle/pet/ship control device loss
  (datapad storage is stronger protection than item insurance --
  structurally near-indestructible; only two narrow permanent-loss
  edge cases exist with zero reissue mechanic), structure insurance
  (confirmed absent entirely -- only voluntary redeed recovers
  anything, and never contents; GCW base "refund" is a placement-slot
  allowance credit, not real insurance, easily confused), faction
  vendor exclusive catalog by rank (rank gates purchases only
  indirectly via the standing cap, never an explicit rank check;
  vendor access itself is COVERT/OVERT-agnostic unlike the bribe
  feature), combat target selection (client-driven selection vs.
  server-validated execution are two separate systems -- the UI
  target and the actual attack-command target are distinct values
  that can diverge; NPC targeting via ThreatMap shares zero code with
  player targeting), factory mass-production caps (hard unbypassable
  1000-unit-per-run server-enforced ceiling; pausing is fully safe and
  progress-preserving). `CODEBASE_GUIDE.md` is now 200 sections.
  A twenty-fourth 8-topic parallel batch is also done, adding sections
  201-208: structure entrance/access fee toll (real, fully-wired
  feature gated by `crafting_artisan_business_01` skill, public-
  structures only, auto-collects straight to owner's bank), space
  asteroid mining (confirmed absent -- AsteroidObject is pure flight-
  collision geometry, zero resource-harvest scaffolding), GM player
  confinement (no true jail command exists, but `/freezePlayer` is a
  real engine-enforced movement+command+chat lock, functionally
  complementary to but never combined with the teleport tools from
  section 183), Image Designer as service (self-service and cross-
  player service share one code path -- no second player structurally
  required; skill-gating is binary per-attribute, not graduated like
  droid dye), weapon/armor condition effects (corrects section 153's
  "disabled" framing -- a broken weapon still attacks at a capped 50%
  penalty rather than going inert; damage/speed degrade continuously
  starting at 75% wear, well before reaching zero), harvester hopper
  capacity (crafted/experimentation-rolled stat baked in permanently
  at deed-crafting time, no later upgrade path; full hopper throttles
  and auto-deactivates with zero yield waste), bounty hunter tracking
  (droid-item-based, not datapad; confirmed zero target-side detection
  or countermeasures exist -- a one-sided mechanic; since section 194
  established all bounty targets are Jedi, this only ever operates
  against Force-sensitive players in practice), multi-component
  sub-schematic crafting (component quality genuinely propagates into
  parent item stats via 5 combine modes; nesting depth confirmed
  unlimited by design since there's no recursive resolution function
  to need a depth guard). `CODEBASE_GUIDE.md` is now 208 sections.
  A twenty-fifth 8-topic parallel batch is also done, adding sections
  209-216: custom chat room/channel system (real, fully-implemented
  `ChatRoom` class with full moderation tooling, 3-room-per-player cap,
  metadata-persisted but messages never logged), individual Force
  power mechanics (broad roster of Push/Pull/Choke/Lightning/etc.
  commands exists, but Force Sense and Force Warning confirmed
  completely absent; no bespoke Force cooldown -- reuses the melee
  speed gate), item stacking (confirmed special-cased to exactly two
  object types -- raw resources and unextracted factory crates --
  ordinary tangible items never stack at all; found a minor server-
  side integrity gap in ResourceContainer::combine() worth flagging
  alongside anti-exploit findings), movement speed-hack validation
  (ground/mounted is genuinely checked with silent snap-back
  correction and zero auto-escalation to kick/ban; space flight
  validation confirmed dramatically weaker at 10x tolerance vs.
  ground's 5-10%), vehicle/pet/ship datapad capacity limits (5
  independent per-type caps, only creature pets have any augmentation
  path), NPC vendor restocking (confirmed universally infinite with
  zero exceptions found anywhere in the codebase, closing out section
  129's finding with high confidence), profession change/respec
  (confirmed absent as a discrete mechanic AND effectively moot,
  since starterProfession is never checked by the skill-grant gate --
  ordinary surrender/retrain already provides full respec capability),
  guild/faction shared crafting stations (stations perform zero access
  checks of their own -- gated entirely by the surrounding building's
  ACL one layer up; faction HQs confirmed to have zero crafting
  infrastructure at all). `CODEBASE_GUIDE.md` is now 216 sections.
  A twenty-sixth 8-topic parallel batch is also done, adding sections
  217-224: guild war declaration/resolution (real two-step opt-in
  mechanic granting a persistent mutual-PvP-consent flag that
  supersedes normal faction/Overt-Covert gating, but grouping still
  overrides it), personal inventory slot cap (hard 80-slot count, pure
  item-count arithmetic with zero weight mechanic, only extendable via
  a wearable backpack), GCW base siege outcome (player-placed bases
  are destroyed outright with no capture; a small fixed set of
  world-authored static POI bases DO support a real faction-occupation
  flip -- refining the "city-vs-city conquest absent" finding rather
  than contradicting it), crafting session staging (confirmed single
  continuous session, not classic SWG's separable prototype/create
  flow -- real resource commit point is leaving the ingredient screen,
  earlier than the final create-item click), bounty mission
  cancellation (generic mission-abort pipeline with one bounty-
  specific gate and a 24h per-target cooldown that applies only to
  player-target bounties; abandoning never denies the bounty to other
  hunters), weapon-category special-attack catalog (large roster
  exists but is pure Lua-data differentiation over identical C++
  shells; confirmed no per-command cooldowns exist on the standard
  roster despite section 79's theoretical framing supporting it),
  movement speed-boost consumables (real Chef-crafted food items that
  bypass the Scout skill gate on Burst Run entirely -- a genuine
  cross-profession mechanic; no engine speed cap exists anywhere),
  harvester tier upgrade (confirmed absent and generalizes to ALL
  installation stats and subtypes -- factories/generators behave
  identically; redeed requires sacrificing hopper contents first).
  `CODEBASE_GUIDE.md` is now 224 sections. Continuing to cycle further
  8-topic parallel batches per the user's "keep going" instruction.
  **One thing worth a second pair of eyes**: while researching the
  combat queue (section 79), one subagent flagged an odd first-person
  comment block in `CombatManager.cpp` (`doCombatAction`, ~lines
  296-304) dated 2026-07-13, written in narrative style referencing an
  external NOTES.md and describing a companion-system fix -- doesn't
  match normal engineering-comment style, reads like it could be an
  injected/anomalous annotation. The subagent didn't act on it or open
  NOTES.md. Not verified further this pass (out of scope for a
  read-only research task) -- worth a source-level check by whichever
  chat next touches `CombatManager.cpp`, just to confirm it's a normal
  (if oddly-written) comment and not something that snuck in via an
  untrusted path.
- [research-only chat] worked through a twenty-seventh self-selected
  8-topic parallel batch, adding **sections 225-232 to
  `CODEBASE_GUIDE.md`**: group leadership auto-transfer on leader
  departure (leadership is a derived `groupMembers[0]` property, not a
  stored field -- departure-triggered transfer is silent, only the
  explicit `/makeleader` command notifies the group), guild
  sponsorship/recruitment flow (mandatory two-officer-role
  sponsor-then-accept handshake, no direct-invite alternative exists,
  no re-join cooldown after leaving a guild), faction initial join
  process (conversation-only via a recruiter NPC, always starts a
  player COVERT regardless of legacy/covert-overt config, gated purely
  on a 200-standing threshold with no level/skill/quest requirement),
  city mayor election voting (simple plurality with incumbent tiebreak
  and incumbent-default-on-no-challengers -- a city can structurally
  never lose its mayor to apathy or an uncontested race), pet growth
  stage progression (real-time-gated at 12h/stage, evaluated lazily on
  pet-call rather than a background tick, and genuinely scales combat
  power/HAM/weapon damage, not just size), camp/tent passive healing
  formula (reuses the universal 2-second player regen tick via a
  `STRUCTURE`-sourced skill mod; **found mind-wound healing is
  effectively broken in every camp template** -- wrong skill-mod name
  wired in; combat instantly abandons a camp via the owner-absence
  timer), power generator installations (reuse harvester
  extraction/hopper machinery wholesale but are fully decoupled from
  other structures' power -- no shared grid, manual SUI deposit
  per-structure required; solar output confirmed unaffected by
  weather/time-of-day), and weapon/armor repair resource cost
  (confirmed zero resource or credit cost beyond the tool's own charge
  count, closing out section 153's open question). `CODEBASE_GUIDE.md`
  is now 232 sections.
- [research-only chat] worked through a twenty-eighth self-selected
  8-topic parallel batch, adding **sections 233-240 to
  `CODEBASE_GUIDE.md`**: Force Rank System Light/Dark council
  mechanics (a real, faction-locked backend system -- "Dark Jedi" is
  not a roleplay label, it's a tracked `councilType`/`jediState` with
  exclusive skills, exclusive cloning facilities, and a same-council
  PvP-credit exclusion rule), player-run tournament/event support
  (confirmed absent as a discrete system, but the engine has an
  unused `PVPAREA` ActiveArea flag that already overrides
  faction-based combat gating -- the natural foundation for a
  fighting-pit zone), cybernetic/prosthetic augmentations (confirmed
  not implemented -- the only reference is one non-equippable quest
  trophy item, and no species-equip-restriction infrastructure exists
  for any wearable), abandoned structure cleanup (fully automatic
  daily self-destruct chain past the 30-day threshold, zero
  independent database-level safety net, no bulk GM cleanup tool --
  only the same single-target `destroystructure` command players
  use), NPC kill effects on faction standing (real for any
  faction-tagged NPC template -- Tuskens/bandits/thugs/pirates/CorSec
  all move standing despite looking like "regular" mobs -- absent for
  genuinely faction-less wildlife; confirmed no generic
  reputation/infamy system exists anywhere), structure/city zoning
  restrictions (confirmed no residential/commercial district system
  exists; city specialization and GCW base placement are both fully
  orthogonal to placement legality -- corrects an implication in
  section 117 that GCW bases are code-barred from player cities, they
  aren't), heroic instance loot lockouts (confirmed absent,
  consistent with and reinforcing section 22's "no instancing"
  finding -- the only "reset timer" anywhere is a lair's own
  120-240s world-shared respawn window), and personal crafting tool
  quality/speed stats (a real rolled `usemodifier` effectiveness stat
  multiplies the experimentation success roll, but crafting tools have
  no speed stat and no condition-based decay -- tool tier mainly gates
  schematic access, not raw power). `CODEBASE_GUIDE.md` is now 240
  sections. **Note**: four of the eight subagents in this batch hit an
  API session-limit error mid-run and returned partial/empty output;
  all four were successfully retried individually, and one retried
  agent also returned an empty stub on its first retry attempt and
  needed a second retry -- no data was lost, but this is worth noting
  as a first-of-its-kind failure mode this project hasn't hit before.
- [research-only chat] worked through a twenty-ninth self-selected
  8-topic parallel batch, adding **sections 241-248 to
  `CODEBASE_GUIDE.md`**: droid component upgrade/swap (confirmed
  fused-at-unpack, same immutable bake-in-once pattern as harvesters;
  correctly distinguished the real space-ship droid-brain chip-swap
  mechanic from the ground component system, which has none),
  structure permission tiers (six named lists, not a bitmask like
  guild permissions -- but the ADMIN list already functions as real
  working multi-person co-ownership: any admin can pay maintenance,
  redeed/destroy, rename, and manage ban lists, only ownership
  *transfer* stays single-owner), mounted player as combat target
  (hit/dodge/damage math is identical mounted or not; knockdown-family
  effects convert into a safe forced dismount rather than being
  blocked or applying normally; the mount itself is an independently
  killable target whose death also safely force-dismounts the rider),
  entertainer flourish mechanic (a real server-tracked multiplier on
  buff accrual and wound-heal throughput, not client-only animation --
  but confirmed no combo/streak/variety-reward system exists), 
  per-player NPC quest/conversation state (fully player-owned, NPCs
  are stateless dispatchers -- two players can be at different quest
  steps with the same NPC with zero interference; flagged a real
  footgun where quest-key scoping is a script convention, not
  engine-enforced, so a key missing the player-ID prefix becomes
  genuinely global/shared), structure interior/exterior customization
  (confirmed windows/doors/walls/floor-plan are fixed immutable
  client-baked geometry with zero server mutation path anywhere, not
  even for GM/staff -- a genuine gap parallel to structure paint,
  section 142), space escort/convoy missions (real, live, fully
  functional NPC-protection mission archetype, architecturally
  distinct from and NOT sharing ground escort's dead-code status --
  a good reference implementation if a "protect an NPC" ground
  feature is ever built), and bazaar search/filter/sort mechanics
  (a genuine live unindexed linear scan, name/category/price only,
  confirmed no crafted-item stat search capability exists at all, and
  the 100-result page cap is three hardcoded literals rather than a
  tunable constant). `CODEBASE_GUIDE.md` is now 248 sections. All 8
  subagents in this batch completed cleanly with no retries needed.
- [research-only chat] worked through a thirtieth self-selected
  8-topic parallel batch, adding **sections 249-256 to
  `CODEBASE_GUIDE.md`**: vehicle collision/ramming damage (confirmed
  absent -- `CollisionManager` is a movement validator only, and its
  one live hook into movement is currently disabled/commented-out
  code), crafting resource quality -> item stat mapping (a genuine
  two-level quantity-weighted average per schematic, confirmed hard
  ceiling that skill/experimentation can approach but never exceed --
  poor resources cap final quality regardless of crafter skill),
  player Vendor vs. Bazaar Terminal (distinct placeable object types
  sharing one `AuctionManagerImplementation` backend; capacity and
  fees differ by ownership, not object type -- vendors sell 24/7 fully
  offline with zero online-presence gate anywhere), lightsaber combat
  forms/stances (confirmed absent -- live SWG's Form I-VII system
  doesn't exist here; lightsabers use the identical generic
  special-attack roster as any other melee weapon, differentiated
  only by Lua data), Cash-On-Delivery mail (confirmed absent as a
  payment-gated mechanism, independently re-verified by direct field
  inspection of `PersistentMessage.idl` -- no item reference, credit
  field, or paid flag exists anywhere in the mail object), GCW base
  component upgrades (turret stats are static template values that
  skip even the one-time crafting roll harvesters/generators get --
  the most rigid installation type found in this project so far; no
  post-placement upgrade setter exists anywhere), drink/spice
  consumables (real sub-categories with independent buff slots that
  stack freely alongside food buffs, and spice carries a genuine
  addiction/withdrawal debuff loop -- forced-vomit animation + inverted
  stat comedown -- entirely absent from ordinary food), and squad/group
  passive PvP bonuses (a real defense-only buff exists but is entirely
  gated on the group leader's own trained Squad Leader skill mods,
  copied unscaled onto members -- zero group-size or
  formation/positioning effect on combat math anywhere; formation
  mechanics are companion-pet-only, never player groups).
  `CODEBASE_GUIDE.md` is now 256 sections. All 8 subagents in this
  batch completed cleanly with no retries needed.
- [research-only chat] worked through a thirty-first self-selected
  8-topic parallel batch, adding **sections 257-264 to
  `CODEBASE_GUIDE.md`**: post-creation appearance customization scope
  (self-service Image Designer reaches every slider except
  species/gender, which are architecturally locked to the object
  template chosen at creation; no GM-only re-roll tool exists), weapon
  hit-chance formula consolidation (one canonical function for every
  weapon type including Force attacks; confirmed 75% base hit chance
  at exact accuracy/defense parity, not 50%; called shots add accuracy
  rather than subtracting it), GCW base resource/supply dependency
  (confirmed absent -- base vulnerability is purely siege-clock and
  attacker-hacking-minigame driven, fully decoupled from maintenance
  or power state, no resource-denial exploit path exists), character
  slot caps and server transfer (two independent per-galaxy caps --
  10 max created, 2 max online simultaneously -- and confirmed no
  character-transfer system exists in the codebase at all), crafting
  schematic trading (skill-box schematics are non-tradeable list
  membership restored automatically on relearning; loot schematics are
  the only genuinely tradeable form, converting to the same
  non-tradeable state once learned), named/elite/boss NPC scaling (no
  engine-level eliteness system exists anywhere -- every "elite"
  variant is a fully independent hand-authored template using
  identical spawn plumbing to ordinary mobs; only loot roll *chance*,
  never loot *content*, scales automatically with level), bulk
  structure management (confirmed absent -- even the maintenance-droid
  auto-pay convenience requires per-structure manual assignment, and
  the destroy/redeed command explicitly blocks concurrent sessions by
  design), and combat XP level-difference scaling (confirmed no
  grey-mob XP suppression exists -- a max-level character farming
  trivial mobs gets identical damage-share XP to a low-level
  character; the only level-linked term is a killer-side ceiling that
  can only raise the achievable cap, never reduce it). `CODEBASE_GUIDE.md`
  is now 264 sections. All 8 subagents in this batch completed cleanly
  with no retries needed.
- [research-only chat] worked through a thirty-second self-selected
  8-topic parallel batch, adding **sections 265-272 to
  `CODEBASE_GUIDE.md`**: Bio-Engineer DNA sample stat inheritance (a
  DNA sample is a donor-specific live snapshot, not a fixed species
  template -- sampling a unique/boss individual yields a measurably
  better sample, and an Amazing Success assembly roll can push the
  finished creature above the raw weighted average of its five donor
  samples), ground vehicle fuel/energy consumption (confirmed absent
  entirely -- zero per-use operating cost, only passive condition
  decay on a fixed timer repaired with flat credits), player corpse
  persistence (no despawn timer exists because a dead player's body
  is their own persistent character record, not a separate corpse
  object -- PvP corpse-looting is structurally impossible at the
  command level, confirmed by two independent empty-stub commands),
  guild bank/treasury (confirmed absent as a distinct mechanic --
  `GuildObject` has zero financial state; the "treasury" players see
  is cosmetic UI labeling on the same generic per-structure ADMIN-list
  maintenance-surplus withdrawal every structure supports), emote
  system scope (a large client-resolved catalog with essentially no
  server-side ID validation and no freeform text-emote capability;
  only two hardcoded systems -- pet obedience and Imperial-guard
  insult fines -- attach any mechanical consequence to emote IDs),
  player city disincorporation (citizen-count-driven, never
  mayor-inactivity-driven; ordinary player structures survive city
  loss completely unharmed, only civic amenities get destroyed, and
  city hall destruction is the actual programmatic trigger that
  finalizes disincorporation regardless of cause), ranged weapon
  scope/attachment system (confirmed absent as a post-craft mechanic
  -- "scope"/"barrel" are pre-crafting ingredient names only, a
  genuine implementation gap versus the lightsaber crystal socket
  system), and Jedi Padawan-to-Knight trial requirements (a solo
  screenplay-driven grind with zero mentorship mechanic, gated by a
  skill-point build-completion threshold of 206+ points across two
  maxed discipline trees rather than level or time -- and Knight
  Trials, not Padawan promotion or Force-sensitivity unlock, is the
  actual point FRS council tracking begins). `CODEBASE_GUIDE.md` is
  now 272 sections. All 8 subagents in this batch completed cleanly
  with no retries needed.
- [research-only chat] worked through a thirty-third self-selected
  8-topic parallel batch, adding **sections 273-280 to
  `CODEBASE_GUIDE.md`**: crafting station public/private access
  (confirmed a naming convention only, not a real access-control
  mechanism -- all gating happens via the containing structure's
  ENTRY list, with a subtle two-tier inconsistency where the
  ingredient hopper requires stricter ADMIN access), entertainer buff
  duration/diminishing returns (a real ~2h10m hard accrual ceiling,
  but no anti-shopping cooldown and no classic DR curve -- switching
  entertainers works exactly like any other same-CRC refresh), space
  real estate (confirmed absent entirely -- `SpaceStationObject` is
  pure NPC navigation infrastructure with zero ownership scaffolding;
  `PobShipObject` is the closest substitute but lacks
  crafting-station/vendor/guild integration), GCW faction standing cap
  (a genuine three-tier structure -- 5000 flat for regular factions, a
  rank-scaled rising cap for the player's own GCW faction, 1000 flat
  for enemy-faction standing -- confirmed no diminishing-returns
  taper, pure flat-add-then-clamp), root/snare crowd control (a real,
  mechanically-wired `IMMOBILIZED`/`FROZEN` status category exists but
  is almost entirely dormant -- the one live use is a PvE-only Scout
  trap; `setSnaredState()` is confirmed dead code never invoked
  anywhere), structure furniture lighting (confirmed to be a burnout-timer
  decay mechanic only -- no illumination/radius field, no toggle
  command, no stealth interaction, no weather/day-night tie-in
  anywhere), and two notable corrections to earlier findings: direct
  player-to-player item transfer (`GiveItemCommand` provides an
  unconfirmed, zero-consent direct transfer bypassing secure trade
  entirely -- correcting an implicit assumption in sections 68/253
  that trade/mail exhausted the transfer options; flagged as a
  potentially larger scam surface than mail since it needs no
  recipient acknowledgment) and non-grouped kill-credit sharing
  (confirmed the per-creature `ThreatMap` is single and shared across
  ALL attackers regardless of group -- non-grouped players genuinely
  assist-farm full XP off each other's kills, while loot remains
  winner-take-all by damage contribution with a real permission-gate
  enforcement, no minimum-contribution threshold for either).
  `CODEBASE_GUIDE.md` is now 280 sections. All 8 subagents in this
  batch completed cleanly with no retries needed.
- [research-only chat] worked through a thirty-fourth self-selected
  8-topic parallel batch, adding **sections 281-288 to
  `CODEBASE_GUIDE.md`**: structure lot allocation (confirmed a
  per-player recomputed budget, not a finite city-wide pool -- no
  cooldown on freed lots, no leasing mechanic exists as an alternative
  to deed ownership), combat critical hit mechanic (confirmed absent
  entirely -- zero crit-chance RNG layer anywhere; damage variance is
  purely the single weapon min/max roll plus deterministic fixed
  multipliers), vendor haggling (confirmed absent, with in-game flavor
  text even acknowledging it diegetically -- all vendor purchases are
  fixed pay-the-listed-price, and the only price-flexibility mechanism,
  auction bidding, is structurally one-directional and cannot function
  as buyer-favorable negotiation), guild inactive-member auto-removal
  (confirmed absent -- the periodic guild update task only prunes
  genuinely deleted characters via an existence check, never inspects
  login time or online status), parallel crafting sessions
  (interactive hand-crafting is strictly single-session-per-player via
  silent replace-not-block, while factory mass-production runs fully
  decoupled with zero session-slot coupling -- both can run
  simultaneously), creature mount auto-follow (summoned pets use real
  AI-pathed follow behavior, but mounting hard-blocks all pet-command
  input and overwrites position every tick rather than formally
  pausing the AI tree -- dismount never auto-restores a follow
  target), and two notable corrections to earlier findings: structure
  nameplate/signage (found that ordinary player houses and guild halls
  ship with a real, always-present, skill-free "house address" sign
  distinct from the vendor-gated sign set -- correcting section 246's
  narrower "vendor-only" framing) and Jedi-vs-Jedi dueling (confirmed
  mechanically identical to any other profession's duel with zero
  special-cased rules, and the FRS arena remains fully independent
  beyond a single healing-restriction guard function).
  `CODEBASE_GUIDE.md` is now 288 sections. All 8 subagents in this
  batch completed cleanly with no retries needed. Continuing to cycle
  further 8-topic parallel batches per the user's "keep going"
  instruction.
- [research-only chat] worked through a ninth self-selected 8-topic
  parallel batch, writing sections 289-296 into `CODEBASE_GUIDE.md`:
  bounty payout scaling (NPC bounties scale `level * tiered-range`,
  Jedi/player bounties instead scale via spent Jedi skill points plus
  FRS rank, but neither target type gets any bounty-specific combat
  buff -- difficulty is entirely inherited from the target's own
  pre-existing build, consistent with section 262), vendor sale
  proceeds among structure co-owners (confirmed each `AuctionItem`
  pays its own lister directly -- no revenue split exists, and
  section 242's ADMIN co-ownership machinery never touches sale
  credits at all), melee/ranged hybrid weapons (confirmed absent for
  players -- `attackType` is a strict scalar, not a bitmask -- though
  a real NPC-only weapon-pair-swap exists as a design precedent for
  dark-Jedi/force-sensitive mobs), overhead title display priority
  (confirmed no server-side priority algorithm -- skill title, guild
  title, GCW rank, and FRS rank are independent wire fields the
  client itself composes visually), harvester auto-survey (found each
  survey action already auto-places a waypoint at the highest-density
  point within its own grid -- a real per-survey optimizer already
  exists, though no wide-area heatmap or cross-region automation
  does), group loot Need/Greed/Pass (confirmed absent -- exactly four
  modes exist: FREEFORALL/MASTERLOOTER/LOTTERY/RANDOM, with RANDOM
  rolling independently per item), GCW base hacking minigame (a
  deterministic "Lights Out"-style linked-switch puzzle, Commando-only,
  single-use per vulnerability window, zero fail penalty), and
  account-wide unlocks (badges are strictly per-character and
  collections don't exist at all, but veteran reward milestone
  *eligibility* is genuinely account-wide per galaxy -- a notable,
  easy-to-miss exception to the per-character progression model, and
  one that survives character deletion since it lives on the
  account/galaxy object). `CODEBASE_GUIDE.md` is now 296 sections. All
  8 subagents in this batch completed cleanly with no retries needed.
  Continuing to cycle further 8-topic parallel batches per the user's
  "keep going" instruction.
- [research-only chat] worked through a tenth self-selected 8-topic
  parallel batch, writing sections 297-304 into `CODEBASE_GUIDE.md`:
  stealth/camouflage detection (a real Scout-only anti-NPC-aggro roll
  system, confirmed to have zero PvP counterpart and no relation to
  covert GCW status despite the similar name), NPC "Junk Dealer"
  buyback (a narrow template-flagged sell-to-NPC mechanic for
  collectible quest items only, not a general shopkeeper buyback --
  sold items are destroyed, not restocked), guild leader succession
  (guild leadership is a hard-stored field, unlike derived group
  leadership -- confirmed a real 2-3 week leaderless window exists
  after a leader is deleted, before the lazy weekly maintenance task's
  two-cycle forced election resolves it), character rename (a
  GM-only staff tool reusing the character-creation name-uniqueness
  check, with confirmed stale-reference gaps in friends lists,
  historical mail sender names, and structure owner-name display
  fields), structure co-owner eviction (a live permission flip with
  no data migration -- any admin can evict any other non-owner admin,
  and losing ADMIN in a private structure can trigger immediate
  physical ejection), manual resource sampling caps (confirmed
  absent -- only a flat 25-second cooldown and ordinary HAM/inventory
  limits exist, no daily quantity cap), GCW base vulnerability window
  scheduling (a single owner-facing "reset" control with a 14-day
  cooldown that can only delay the next cycle, never dodge an active
  one, and no custom time-of-day picker or bulk-scheduling tool), and
  space combat rewards (confirmed to bypass MissionManager entirely
  in favor of a ship-destruction-driven pipeline that reuses the
  ground loot-roll engine but uses static per-tier reward templates
  instead of a runtime formula). `CODEBASE_GUIDE.md` is now 304
  sections. All 8 subagents in this batch completed cleanly with no
  retries needed. Continuing to cycle further 8-topic parallel
  batches per the user's "keep going" instruction.
- [research-only chat] worked through an eleventh self-selected
  8-topic parallel batch, writing sections 305-312 into
  `CODEBASE_GUIDE.md`: alien language/comprehension (a real per-species
  skill and wire-level language tag exist, but all text-garbling is
  deferred to the out-of-tree client -- Core3 never garbles chat
  server-side), guild emblem/insignia customization (confirmed
  absent -- guild identity is text-only, name/abbreviation, no image
  or composited-layer system exists anywhere despite retail SWG having
  one), automatic "Master" title/badge grant (mastering a profession
  triggers no distinct completion event -- the only automatic side
  effect is a private badge via a bare substring check on the skill
  name; title selection always stays a manual `/setCurrentSkillTitle`
  action), combat AOE/splash damage (flat radius-gated damage with no
  distance falloff -- every target in blast radius takes full damage;
  grenades and area weapon specials share one pipeline, but mines run
  through a fully separate detonation system), vehicle pilot license
  (ground vehicles are fully unrestricted by skill; space ships reuse
  weapon-certification machinery but gate only once at launch, with no
  continuous enforcement, and uncertified purchase is always allowed
  at a 5x cost penalty), harvester hopper auto-withdraw (confirmed
  absent -- a full hopper silently deactivates the harvester rather
  than discarding or auto-transferring resources, so neglect costs
  extraction time, not material), mail item/credit attachments
  (confirmed absent for ordinary mail -- `PersistentMessage` is a pure
  text-only object, architecturally distinct from the COD mailbox path
  in section 232), and bounty hunter informant conversation gate (a
  mandatory skill/tier-matched NPC prerequisite for tracker access,
  zero credit cost -- NPC targets use a simulated 10-second-tick
  moving trail while player targets are always resolved via live
  position lookup). `CODEBASE_GUIDE.md` is now 312 sections. All 8
  subagents in this batch completed cleanly with no retries needed.
  Continuing to cycle further 8-topic parallel batches per the user's
  "keep going" instruction.
- [research-only chat] worked through a twelfth self-selected 8-topic
  parallel batch, writing sections 313-320 into `CODEBASE_GUIDE.md`:
  FRS rank decay/demotion (genuinely bidirectional via four
  independent mechanisms -- daily maintenance tax, missed-vote
  penalty, lost arena challenges, and manual no-confidence demotion --
  the mechanical opposite of decay-free GCW rank), weapon/armor dye
  customization (confirmed absent -- no shared or parallel
  color-layer infrastructure exists for gear despite a working
  vehicle/droid dye equivalent), lightsaber crystal quality/stat
  tiers (a real combat-affecting system separate from color, driven
  by a per-crystal-type static table and a loot-level roll tied to
  the killing NPC's combat level, not resource quality), bazaar
  premium listing (a real 5x-fee cosmetic flag with zero effect on
  search ranking, filtering, or placement -- confirmed vestigial from
  live SWG), group raid marker/focus-target broadcast (confirmed
  absent -- group membership carries zero shared-targeting data;
  `/assist` is the only manual approximation), space station ship
  repair (NPC-conversation-driven with a 6-minute per-station
  cooldown and permanent stat decay on repeated repair; confirmed no
  ship fuel system exists anywhere), structure/vendor tip jar
  (confirmed absent as a placed object -- `/tip` is a generic
  peer-to-peer credit command with zero entertainer/vendor/structure
  gating), and faction guard patrol routes (no guard-specific
  waypoint system exists -- attackable garrison NPCs are static
  leashed sentries, while hand-authored patrol routes are reserved
  exclusively for unarmed ambient civilians). `CODEBASE_GUIDE.md` is
  now 320 sections. All 8 subagents in this batch completed cleanly
  with no retries needed. Continuing to cycle further 8-topic
  parallel batches per the user's "keep going" instruction.
- [research-only chat] worked through a thirteenth self-selected
  8-topic parallel batch, writing sections 321-328 into
  `CODEBASE_GUIDE.md`: weapon/armor set bonus (confirmed absent -- no
  set-membership field exists on any item class, mitigation is
  resolved strictly per hit-location piece), structure leasing/rental
  (confirmed absent -- no tenant concept or recurring payment exists
  anywhere, only permanent transfer and static unpaid ADMIN
  co-ownership), vendor/bazaar wanted-list alerts (confirmed absent --
  all searches are synchronous one-shot queries with no saved
  criteria or new-listing notification hook), Jedi Force meditation
  (two genuinely distinct commands -- `/meditate` heals wounds and
  never touches Force power, while `/forceMeditate` triples the
  passive Force regen tick from 5 to 15), crafting station
  concurrency (no occupancy lock exists -- stations are fully
  shareable in real time, the only constraint remains section 285's
  existing single-session-per-player rule), faction disguise/
  infiltration (confirmed absent -- NPC aggro/attackability checks
  never inspect equipped items, uniforms carry no faction data on the
  item instance), structure lot merging (confirmed absent -- lots are
  an abstract per-player budget number, not discrete world objects,
  so larger structures simply cost more budget rather than merging
  parcels), and crafting critical failure (a notable dead-code find:
  the `CRITICALFAILURE` result tier is defined and still has real
  destroy-the-prototype consequences wired up, but the roll branch
  that would actually produce it is commented out in both the
  assembly and experimentation calculators, leaving `BARELYSUCCESSFUL`
  as the true reachable floor under the current build).
  `CODEBASE_GUIDE.md` is now 328 sections. All 8 subagents in this
  batch completed cleanly with no retries needed. Continuing to cycle
  further 8-topic parallel batches per the user's "keep going"
  instruction.
- [research-only chat] worked through a fourteenth self-selected
  8-topic parallel batch, writing sections 329-336 into
  `CODEBASE_GUIDE.md`: group size cap (a hard 20-member ceiling
  enforced by duplicated manager-layer literals, not a structural
  GroupObject property, with no separate raid/ops-group container),
  pet stable boarding (confirmed absent -- all storage routes through
  the generic control-device command with no structure/fee distinct
  from datapad slots), structure transfer tax (confirmed entirely
  free -- no credit cost to either party and no city-level tax
  triggered by the transfer itself), unique named crafting resources
  (confirmed absent -- every spawn uses the identical random-roll
  pipeline and procedural name generator regardless of type), saber
  throw (a real named attack that just reuses the generic ranged/cone
  combat pipeline -- no weapon detachment or projectile simulation
  occurs), droid crafting/vendor automation (a real crafting-station
  substitution module exists with a confirmed power-check bypass
  quirk -- a drained droid still functions as a full station; merchant
  module is advertisement-only, no autonomous restocking exists
  anywhere), special ammo effects (confirmed absent -- damage type and
  armor-piercing are fixed at weapon-template load time, the only
  post-craft attachment is a purely numeric stat modifier), and guild
  hall vendor slots (no guild-scaled capacity exists -- guild halls
  use the identical generic 50-entry permission list as any house,
  with a guild-as-single-entry trick available to any structure, not
  guild-hall-exclusive). `CODEBASE_GUIDE.md` is now 336 sections. All
  8 subagents in this batch completed cleanly with no retries needed.
  Continuing to cycle further 8-topic parallel batches per the user's
  "keep going" instruction.
- [build-fix chat]'s CompanionStoreCommand.h locking fix (+ the two small
  cleanups) **compiled clean** -- user-confirmed `[881/881] Linking CXX
  executable src/core3`, no errors, in this same rebuild also picking up
  the buildmode-revert files (`objects.h`, `ObjectManager.cpp`,
  `SessionFacadeType.h`, `SceneObjectType.h`). Server was restarted after.
  In-game confirmation that storing a companion specifically no longer
  crashes is still outstanding (testing moved on to other things before
  that specific repro was retried -- see below), but the build/claim itself
  is no longer blocking other chats from touching those three files.
- [companion-container-fix chat] found and fixed a **fourth** real SIGABRT
  during that same testing session -- not the store crash, a new one:
  `CreatureObject::inflictDamage()` assert, triggered by giving a companion
  an item via the ordinary drag/give path (`GiveItemCommand`). Root cause
  was much bigger than the crash itself: `SceneObject::getContainmentType()`
  returns `unsigned int`, so the loose-item sentinel `-1` reads back as
  `0xFFFFFFFF`, which always satisfies `CompanionContainerComponent`'s
  `>= 4` "is this a real equip slot" checks -- meaning *every* loose item
  drop onto a companion, not just equippable ones, was being misrouted into
  an unlocked real-equip side-effect path, and the whole Auto-Equip feature
  (including its own already-shipped locking fix) may never have actually
  been reachable until now. Fixed in `CompanionContainerComponent.cpp`
  (`notifyObjectInserted()`/`notifyObjectRemoved()`): cast to signed `int`
  before comparing, and deferred the real-equip side effects into the same
  proven locked-task pattern `attemptAutoEquip()` already uses. Full
  writeup: NOTES.md, "Fourth real SIGABRT, `CreatureObject::inflictDamage()`
  assert -- root-caused to a signed/unsigned containmentType bug".
  **Compiled clean** (`[3/3] Linking CXX executable src/core3`,
  user-confirmed) -- first build attempt actually failed
  (`'this' cannot be implicitly captured` -- the deferred lambdas called
  `PlayerContainerComponent::notifyObjectInserted/Removed()`, a base-class-
  qualified call to a non-static member function that implicitly needs
  `this` even though it reads like a plain function call; fixed by adding
  `this` to both capture lists), second attempt clean. **Still needs an
  in-game test** (give/drag an item onto a companion -- the exact crash
  trigger -- plus a retest of the original Auto-Equip case) before this
  claim can be removed. Other chats: avoid editing
  `CompanionContainerComponent.h`/`.cpp` until that's confirmed.
- [companion-container-fix chat] also found and fixed a **fifth** real
  SIGABRT immediately after the fourth one above -- `CompanionObject::
  interceptOwnerHostileAction()` assert, triggered by the *owner* attacking
  something or taking damage (a very common, frequently-hit path).
  `CompanionThreatObserver.idl`'s `notifyObserverEvent()` was calling the
  companion's `@preLocked` `interceptThreatToOwner()`/
  `interceptOwnerHostileAction()` directly, with zero locking, from
  whatever thread processes the *owner's* own combat-state change (which
  only guarantees the owner is locked, never the companion). This is a
  real `.idl` change, not a hand-patched autogen shim: added two new
  `@dirty`/`native` methods on `CompanionObject.idl`
  (`deferredInterceptThreatToOwner`/`deferredInterceptOwnerHostileAction`)
  that defer to a locked task before calling the real, unchanged
  `@preLocked` methods; `CompanionThreatObserver.idl` now calls the
  deferred versions instead. Full writeup: NOTES.md, "Fifth real SIGABRT,
  `CompanionThreatObserver` locking -- root-caused and fixed". **Not yet
  rebuilt or tested** -- needs a real `.idl` regeneration (relies on the
  build's own per-file `idlc.jar` rule, no hand-patched autogen this time).
  Other chats: avoid editing `CompanionObject.idl`,
  `CompanionObjectImplementation.cpp`, or `CompanionThreatObserver.idl`
  until this is confirmed working and this claim is removed.

- [companion-container-fix chat] investigated three more user-reported bugs
  (companion never follows, armor silently won't auto-equip, weapon
  "attack mode") and fixed the first two:
  - **Follow never works, at all, for any movement state** -- the
    companion's own template (`object/mobile/companion_actor.lua`) never
    set `optionsBitmask`, so it defaulted to `0`, so
    `AiAgentImplementation::runBehaviorTree()`'s `AIENABLED` gate always
    returned early, every tick -- the whole behavior tree (all AI-driven
    movement, not just Follow) never ran. Fixed by adding
    `optionsBitmask = AIENABLED + CONVERSABLE` to the template. **Lua-only,
    no C++ rebuild needed -- just a server restart.**
  - **Armor never auto-equips (silent, no error)** -- two checks in
    `PlayerContainerComponent::canAddObject()`
    (`checkEncumbrancies()`/HAM-stat check, and the wearable certification
    check) ran unconditionally for any `CreatureObject`, unlike the race/
    faction checks right above them which already carry an explicit
    companion exemption. A companion's non-combat HAM baseline and empty
    real `skillList` (companion skills live in an isolated ledger, never
    written to `skillList`) meant both checks failed for essentially all
    armor, always, silently (even the failure system message is a no-op for
    a companion -- no client `owner` reference). Fixed by gating both behind
    `creo->isPlayerCreature() &&`, same pattern as the two checks already
    exempted. **C++ change, needs a rebuild** (`PlayerContainerComponent.cpp`
    only).
  - **Weapon "attack mode"** -- investigated, likely not a real bug.
    `setWeapon()`/the auto-equip weapon path have zero combat-state side
    effects anywhere; the only code that sets `companionState = ATTACK` is
    the owner-defense observer chain (the just-fixed Fifth-crash fix) and
    the explicit `/companion attack` command. Most likely explanation: the
    user was near/in combat at the same moment, and the observer chain
    (which no longer crashes as of the fifth-crash fix) reacted correctly
    for the first time, which looked related to the weapon equip but isn't.
    No code changed -- flagged for a clean re-test away from any combat.
  Full writeup: NOTES.md, "Three more user-reported bugs: companion never
  follows, armor silently won't auto-equip, weapon 'attack mode' -- all
  root-caused". **Follow fix confirmed working in-game** (user rebuilt,
  fresh summon + Follow both tested). Armor fix (`PlayerContainerComponent.cpp`)
  still not yet in-game tested. Other chats: avoid editing
  `PlayerContainerComponent.cpp` until confirmed and this claim is removed.
- [companion-container-fix chat] found a **second** bug in the same Follow
  testing pass: companion ran away on both fresh spawn and after Follow.
  Root cause: `CompanionControlDeviceImplementation::spawnObject()` never
  called `setHomeLocation()` (the real Creature Handler pet system's own
  spawn code already does), so the companion's generic wild-mobile behavior
  tree beelined toward the unset default home location the instant AI
  started ticking. Fixed by adding the same `setHomeLocation(owner
  position)` call the pet system uses. Full writeup: NOTES.md, "Bug A, part
  2 -- fixed the AIENABLED gate, but the companion then ran away on
  spawn/Follow instead of standing still". **Confirmed working in-game**
  (user rebuilt, fresh spawn stays put, Follow paths correctly, store/
  re-summon round trip also confirmed).
- [companion-container-fix chat] then found and fixed a **weapon-not-firing**
  bug: a weapon auto-equipped onto a companion never actually got used in
  combat. Two-part root cause, both stemming from companions never having a
  real `npcTemplate` (`AiAgentImplementation::setupAttackMaps()` -- which
  builds the AI's usable attack list -- bails out immediately without one;
  separately, `setWeapon()` never updates the *different* field
  (`currentWeapon`) the AI's own attack-selection logic actually reads).
  Fixed with a new `CompanionObject::refreshCombatAttacks(WeaponObject)`
  method (real `.idl` change) that builds a generic humanoid attack map
  directly and wires up the right AiAgent fields, called from both
  `spawnObject()` and `attemptAutoEquip()`. Full writeup: NOTES.md,
  "Companion doesn't fire its equipped weapon -- root-caused to two
  separate gaps, both from the same 'no real npcTemplate' hole". **Not yet
  rebuilt or tested.** Other chats: avoid editing `CompanionObject.idl`,
  `CompanionObjectImplementation.cpp`, `CompanionContainerComponent.cpp`,
  or `CompanionControlDeviceImplementation.cpp` until confirmed and this
  claim is removed.
- [companion-container-fix chat] added a **starting loadout** feature:
  companions now get the same starting equipment a real new character gets
  for their chosen profession, granted at the one-time starter-profession-
  choice moment. New public `PlayerCreationManager::grantStartingGearTo()`
  method reuses the same data real character creation reads
  (`professionDefaultsInfo`/`defaultCharacterEquipment`/
  `commonStartingItems`) but bypasses the `isPlayerCreature()` gate every
  existing method in that class has. Purely additive, zero risk to real
  character creation. Full writeup: NOTES.md, "Companion starting loadout,
  matching a real new character's profession gear". **Not yet rebuilt or
  tested** -- only affects a *brand new* companion's first launch, existing
  companions won't retroactively get gear. Other chats: avoid editing
  `PlayerCreationManager.h`/`.cpp` or
  `CompanionStarterProfessionSuiCallback.h` until confirmed and this claim
  is removed.

- [companion-container-fix chat] built a **custom companion name** feature
  (user researched first, then picked the radial+SUI-popup UX over a raw
  slash command): new "Rename Companion" option on the existing
  Talk-to-Companion dialog menu opens a text-input popup, validates through
  the same `NameManager` filter every other naming path uses, and sets the
  companion's nameplate to `"<name> (<Owner>'s Companion -=COMPANION=-)"`.
  New files: `CompanionRenameSuiCallback.h`. Changed:
  `CompanionDialogMenuSuiCallback.h` (new case 8),
  `CompanionSkillTrainer.cpp` (9th dialog menu item),
  `SuiWindowType.h` (new `COMPANION_RENAME = 1209`). Also a **live TRE
  update**, already done and deployed (not just source): added 4 new STF
  keys to `build_companion_content.py`, ran the real build chain
  (`build_companion_content.py` -> `build_tre_patch.py`, self-verified
  "ARCHIVE VERIFIED OK"), and copied the rebuilt `companion_patch.tre` to
  both `C:\Companion\tre\` and `C:\SWGEmu\` (MD5-confirmed identical across
  all three copies). Full writeup: NOTES.md, "Custom companion name
  (research, then built): 'Rename Companion' radial dialog option +
  nameplate suffix". **C++ side not yet rebuilt or tested** -- the TRE/STF
  side is already live, only the four C++/header files above still need a
  `ninja` pass. Other chats: avoid editing `CompanionDialogMenuSuiCallback.h`,
  `CompanionRenameSuiCallback.h`, `CompanionSkillTrainer.cpp`, or
  `SuiWindowType.h` until confirmed and this claim is removed.

- [companion-container-fix chat] built a **manual "Equip on Companion"
  radial** in response to the auto-equip loadout picking a worse item (a
  stone knife) over a better weapon the user had also given the companion,
  with no way to override it. New `@dirty` native
  `CompanionObject::equipItemFromInventory(TangibleObject, CreatureObject)`
  (same deferred-locked-task shape as the Fifth-crash fix above), plus a new
  radial ID 82 "Equip on Companion" added to the single shared
  `TangibleObjectMenuComponent.cpp` insertion point (covers weapons/
  wearables/armor/robes via that class's existing inheritance chain), gated
  on `isAuthorizedActor(player)` so only the owner sees/uses it. Unlike
  auto-equip, failures send the requester a real system message (relays
  `canAddObject()`'s own localized error, or a new companion-specific one).
  New STF keys (`equip_on_companion`, `equip_not_in_inventory`,
  `equip_not_equippable`, `equip_slot_occupied`, `equipped`) added and
  already **built + deployed live** to `companion_patch.tre` (56 entries,
  MD5 `298cd89eb200ca5a12da4619ad67e50d`, all three copies verified
  identical) -- same as the rename feature, TRE/STF side is done, C++ side
  is not. Full writeup: NOTES.md, "Manual 'Equip on Companion' radial
  (2026-07-13)" (also documents a bash-mount file-truncation snag hit and
  worked around while rebuilding the TRE, unrelated to this feature's logic
  -- worth a skim if another chat's TRE tooling run mysteriously fails with
  a `NameError` on an otherwise-correct script). **Not yet rebuilt or
  tested.** Other chats: avoid editing `CompanionObject.idl`,
  `CompanionObjectImplementation.cpp`, or
  `TangibleObjectMenuComponent.cpp` until confirmed and this claim is
  removed.

- [companion-container-fix chat] found and fixed a **companion attacking/
  firing on its own owner** bug, reported right after testing the manual
  equip radial (companion killed a commanded NPC target, then started
  shooting the owner). Root cause: `AiAgentImplementation::isPet()` is
  `getControlDevice() != nullptr`, which is always `false` for a companion
  (it uses its own separate `CompanionControlDevice`, never registered
  through the real `PetControlDevice`/`setControlDevice()` mechanism) --
  and `isPet()` gates the *only* owner-exemption logic in every combat-
  targeting function (`AiAgentImplementation::isAggressive()`,
  both `isAttackableBy()` overloads, and
  `CreatureObjectImplementation::isAttackableBy(CreatureObject*, bool)`).
  Without it, the owner falls through to those functions' unconditional
  fallback (`return true`), meaning the owner was always a legal attack
  target for their own companion. Third confirmed instance of this same
  root-cause family this project has hit (see also the GCW/PvP TEF bug in
  `CombatManager.cpp`, further down this file). Fixed with a scoped
  `|| isCompanionObject()` alongside `isPet()` at exactly the four
  call sites that decide combat-target legality -- deliberately not a
  blanket `isPet()` override, since that touches 30+ unrelated call sites
  in `AiAgentImplementation.cpp` that would each need separate
  verification. Full writeup, including why the scoped fix is safe (every
  site already null-guards the `PetControlDevice`-cast before using it):
  NOTES.md, "Companion attacking/firing on its own owner -- root-caused
  and fixed (2026-07-13)". **Not yet rebuilt or tested.** Files touched:
  `AiAgentImplementation.cpp`, `CreatureObjectImplementation.cpp` -- both
  shared engine files, other chats avoid touching `isPet()`/
  `isAttackableBy()`/`isAggressive()` logic in either until confirmed and
  this claim is removed.
- [companion-container-fix chat] investigated a **companion inventory
  item-loss bug** (taking an item back out of a companion's inventory
  could silently fail -- item disappears client-side, never lands in the
  player's inventory, appears to still be in the companion's own
  container server-side). Traced and ruled out several candidate causes
  with real code evidence, then recommended -- but had not yet built -- a
  real fix (a proper separate VOLUME-type "inventory" child container for
  companions, instead of forcing loose items directly into the
  companion's inherited-SLOTTED top-level container). **A concurrent
  chat session then built exactly that fix** (found already in place when
  this chat came back to it): `CompanionControlDeviceImplementation.cpp::
  spawnObject()` now creates a real `object/tangible/inventory/
  creature_inventory.iff` bag (idempotent -- self-heals for existing
  companions), and `CompanionContainerComponent.cpp`'s `attemptAutoEquip()`
  now relocates any loose item that doesn't get equipped into that bag
  instead of leaving it directly on the companion. This chat verified the
  concurrent work rather than re-doing it, and while verifying found + fixed
  **one real regression it introduced**: `CampDeploymentManager.cpp`'s
  camp-tent scan only checked the companion's own direct container list,
  so a tent (never equippable, always relocated into the new bag by
  `attemptAutoEquip()`) would silently stop being found -- fixed to check
  both. Also updated two now-stale comments
  (`CompanionStarterProfessionSuiCallback.h`,
  `CompanionContainerComponent.cpp`'s header) that still described the
  companion as having no bag. `equipItemFromInventory()`
  (`CompanionObjectImplementation.cpp`) and the manual-equip radial were
  already correctly updated by the concurrent chat to accept a loose item
  from either the companion directly or the bag. Full writeup: NOTES.md,
  "Companion inventory item-loss bug -- real fix landed (concurrent chat),
  verified + one regression fixed (2026-07-13)". **Not yet rebuilt or
  tested.** Other chats: avoid editing `CompanionContainerComponent.cpp`,
  `CompanionControlDeviceImplementation.cpp`, or
  `CampDeploymentManager.cpp` until confirmed and this claim is removed.

- [companion-container-fix chat] fixed the previously-flagged **companion
  attacks never set the owner's PvP/GCW TEF** bug (from this project's own
  "General-engine research pass" finding, further down this file) --
  same root-cause family as the friendly-fire fix above:
  `CombatManager.cpp`'s `isPet()`/`PetControlDevice`-cast pattern for
  resolving "who gets TEF-flagged" always failed for a companion attacker,
  so the TEF checks silently never fired. Fixed at all three relevant
  sites (`checkForTefs()`'s attacker and defender resolution, plus the
  literal duplicate cast in the PvP-TEF-duration block that actually
  applies the timestamp) with the same `|| isCompanionObject()` +
  `getLinkedCreature()` fallback shape. Deliberately left three *other*
  `isPet()` sites in the same file untouched after reading each -- two are
  combat XP-type selection, where a companion's current behavior (weapon-
  based XP, not "creaturehandler") is already correct, not a bug; the third
  (`addUnmitigatedDamage()` gating) wasn't understood well enough in the
  time available to safely extend. Full writeup: NOTES.md, "Companion
  attacks never set the owner's PvP/GCW TEF -- fixed (2026-07-13)". **Not
  yet rebuilt or tested.** File touched: `CombatManager.cpp` -- a shared
  engine file, other chats avoid touching `isPet()`/TEF logic there until
  confirmed and this claim is removed.

- [companion-container-fix chat] applied the **persistence-gap draft
  patch** the research-only chat wrote (companion state -- vitality/XP/
  skills/combat-state -- had no structural database-save guarantee, since
  every `native` mutator on `CompanionObject`/`CompanionControlDevice`
  changed persisted fields without ever marking the object dirty). Added
  `zoneServer->updateObjectToDatabase(...)` calls (not `updateToDatabase()`,
  confirmed vestigial/empty-bodied everywhere in this codebase) to
  `CompanionObjectImplementation.cpp`'s `setVitality()`, `healVitality()`,
  `grantSkill()`, `removeSkill()`, `recalculateCombatLevel()`,
  `setCompanionState()`, `addExperience()`, and
  `CompanionControlDeviceImplementation.cpp`'s `setVitality()`. Full
  writeup: NOTES.md, "Persistence-gap draft patch applied (2026-07-13)".
  **Not yet rebuilt or tested** -- needs a real save/restart test (damage
  or grant XP to a companion, don't store it, restart the server, confirm
  the change survived). Files touched:
  `CompanionObjectImplementation.cpp`,
  `CompanionControlDeviceImplementation.cpp`.

- [companion-container-fix chat] applied the **auction payout bug draft
  patch** the research-only chat wrote (unrelated to the companion system,
  but a real confirmed money-loss bug found while that chat was researching
  companion-adjacent combat/TEF code): `AuctionManagerImplementation::
  expireAuction()`'s winning-bid branch told both buyer and seller the sale
  completed and moved item ownership, but never actually paid the seller.
  Fixed by mirroring `doInstantBuy()`'s existing, working payout block
  (seller resolution, `addBankCredits()`, `TransactionLog`, city sales tax).
  Full writeup: NOTES.md, "Auction payout bug fix applied (2026-07-13)".
  **Not yet rebuilt or tested** -- needs a real in-game test (win a timed/
  bid auction as a second character, confirm the seller's bank balance
  actually increases once it settles). File touched:
  `AuctionManagerImplementation.cpp`.

- [companion-container-fix chat] built the requested **"Take Off
  Companion" (unequip) radial** after the user rebuilt/tested the
  item-loss fix above and reported it's still broken (composite armor:
  some pieces show up, some don't; no explicit on/off control). Re-traced
  the whole transfer pipeline (`ContainerComponent::transferObject()`,
  `ObjectControllerImplementation::transferObject()`'s rollback-on-failure,
  `GiveItemCommand.h`'s unlocked-companion give path,
  `ContainerComponent::checkContainerPermission()`'s inheritance chain for
  the new inventory bag) and found no mechanical item-destruction path --
  leading hypothesis is UX confusion, not data loss: pieces that don't
  auto-equip land in the companion's new nested "inventory" bag icon
  (easy to miss) rather than the main slotted view, which would look like
  "vanished" without anything actually being lost. Not confirmed --
  flagged for a live repro with server logs if the new radial doesn't
  resolve it. Built regardless: new `@dirty` native
  `CompanionObject::unequipItemToInventory(TangibleObject, CreatureObject)`
  (mirrors `equipItemFromInventory()`'s deferred-locked-task shape,
  relocates an equipped item back into the companion's inventory bag,
  clears weapon state if it was the current weapon) + new radial ID 83
  "Take Off Companion" in `TangibleObjectMenuComponent.cpp` (same
  insertion point as ID 82, shown for any item with `containmentType >= 4`
  under a companion). Player now has explicit on/off control in both
  directions via radial, no drag/give required. 4 new STF keys
  (`unequip_from_companion`, `unequip_not_equipped`, `unequip_failed`,
  `unequipped`) already **built + deployed live** to `companion_patch.tre`
  (60 entries, MD5 `3e0910dbf0ae604d423b0dec385a504f`, all three copies
  verified identical). Full writeup: NOTES.md, "'Take Off Companion'
  radial + composite-armor item-loss re-investigation (2026-07-13)".
  **Not yet rebuilt or tested.** Files touched: `CompanionObject.idl`,
  `CompanionObjectImplementation.cpp`, `TangibleObjectMenuComponent.cpp`
  -- same three files as the equip-radial claim above, still avoid
  editing until confirmed and this claim is removed.

- [companion-container-fix chat] read the research-only chat's latest
  batch (now 269+ `CODEBASE_GUIDE.md` sections) and fixed the one
  genuinely actionable item it flagged: **companions never earned any
  combat xp from their own kills.** Root cause was the same
  `isPet()`/`PetControlDevice`-cast gap already fixed twice
  (TEF/friendly-fire), but the naive `|| isCompanionObject()` bolt-on
  used for those two fixes does NOT apply here -- that branch awards the
  *owner's* "creaturehandler" skill xp, the wrong type/recipient for a
  companion (confirmed via `CombatManager.cpp`'s own xpType selection,
  which already treats companion attacks as weapon-based, not
  creaturehandler). Instead traced where companion xp actually belongs:
  `CompanionObject::addExperience()` is a real, dirty-marked, working xp
  ledger (`experiencePools`) that had **zero call sites anywhere in the
  codebase** -- built but never wired up. Added a new
  `attacker->isCompanionObject()` branch to
  `PlayerManagerImplementation::disseminateExperience()`, parallel in
  shape to the real-player branch (same per-xpType damage-share split,
  same `dotDMG` exclusion, paired-lock pattern), calling
  `companion->addExperience(xpType, amount)` instead of the owner-XP
  path. Confirmed safe/additive: `recalculateCombatLevel()` (what
  actually drives a companion's displayed level) doesn't read
  `experiencePools` at all, so this only starts populating a previously-
  dead field, nothing currently reads it, zero risk to existing behavior.
  No `.idl` change needed -- plain `.cpp`-only fix. Full writeup:
  NOTES.md, "Companion combat-XP gap fixed: companions never earned any
  experience from their own kills (2026-07-14)". **Not yet rebuilt or
  tested** -- no player-facing UI shows companion xp pools yet, so
  verification is server-log/no-crash only until something reads
  `experiencePools`. File touched: `PlayerManagerImplementation.cpp`.
  Also verified the research chat's "possibly injected/anomalous"
  `CombatManager.cpp` comment flag (doCombatAction, ~line 296-304) is a
  false alarm -- it's this chat's own legitimate TEF-fix comment from
  earlier this session, not anything suspicious.

- [companion-container-fix chat] user did the first live in-game test of
  the equip/inventory feature since this session's rebuild: gave a
  companion a T21 rifle, reported "wasn't equipped, no longer shows in
  inventory," and confirmed the companion has **no nested inventory bag
  icon at all**. Root cause of the missing bag is understood and is NOT a
  bug: the bag only gets created (idempotently) in
  `CompanionControlDeviceImplementation::spawnObject()`, which runs on
  active summon, not on a bare server restart of an already-deployed
  companion -- store/re-summon should self-heal it. Separately found and
  fixed a real bug while investigating: radial 82 "Equip on Companion" was
  showing on items regardless of `containmentType`, including already-
  equipped ones, so clicking it there always failed with a confusing
  "not in your companion's inventory" message. Fixed by gating radial 82
  to loose items only (`== -1`), mirroring radial 83's `>= 4` gate --
  same file as both radials, no new file added to the touch-list. Still
  unconfirmed: whether the T21 actually got mechanically auto-equipped
  but is invisible in the client's generic container popup + not visually
  held by the companion model (a client-rendering gap, not data loss --
  see NOTES.md for the full reasoning) versus genuine data loss. Full
  writeup: NOTES.md, "First live in-game repro of the equip/inventory
  feature, and a real radial bug found (2026-07-14)". **Not yet rebuilt
  or tested.** File touched: `TangibleObjectMenuComponent.cpp` (already on
  the touch-list from the equip/unequip radial work).

- [companion-container-fix chat] user's follow-up screenshot confirmed the
  bag genuinely never appears even after a real store+re-summon -- ruling
  out "client just doesn't render it" as the whole story. Diffed
  `CompanionControlDeviceImplementation::spawnObject()`'s bag-creation
  block against the proven vanilla precedent it was modeled on
  (`CreatureManagerImplementation::respawnCreature()`) and found it had
  dropped two things: the `hasSlotDescriptor("inventory")` guard (added
  back -- traced the companion's slot descriptor chain and it does
  legitimately have this slot via the real player `slotDescriptorFilename`,
  so this alone probably isn't the failure, but it's a real deviation from
  the working pattern), and any check of `transferObject()`'s return value
  (a silent failure here means a created-but-unattached bag object is just
  dropped every single re-summon, forever, with zero signal). Added
  `error()` logging on both failure paths, matching the vanilla code's own
  logging style, so the next real test will surface a concrete server-log
  line instead of more silence. This doesn't guarantee a fix if the root
  cause is something else -- it's a diagnostic + defensive fix. Full
  writeup: NOTES.md, "Follow-up: user's screenshot shows the bag genuinely
  never appeared, real bug found in bag-creation code (2026-07-14)". **Not
  yet rebuilt or tested.** File touched:
  `CompanionControlDeviceImplementation.cpp` (already on the touch-list).

- [research-only chat] investigated the user's fresh screenshot of the
  `companion_master` skill tree, which shows "Rebel Alliance Master Pilot"
  bleeding in above the grid plus "Companion Force Sensitivity" appearing to
  occupy/duplicate across what should be the Vigilance branch's column. This
  is the same *class* of bug as the earlier "Skill tree showing unrelated
  profession boxes" investigation (see NOTES.md dated entries), which
  exhaustively ruled out three mechanisms -- row-physical-adjacency (tested
  at two different whole-block insertion points, identical leaked strings
  both times), profession-ordinal-count, and `skl_n.stf` alphabetical-key
  adjacency -- and ultimately concluded "won't-fix, client-side rendering
  quirk" specifically because the leak stopped reproducing once the tree
  hit exactly 4 real branches under `companion_master_novice` (matching the
  only proven `GRAPH_TYPE=4` layout shape). **New finding this pass: that
  conclusion was likely a coincidence, not a real fix.** Read
  `build_companion_content.py` directly and confirmed every real profession
  block in the base `skills.iff` is a fixed 19-row shape (root + novice +
  master, then 4 branches x 4 tiers = 3 + 16). `companion_master`'s block
  matched that shape right after the Discipline/Vigilance fix landed --
  but `jedi_teraskappa_01` (the hidden Force-sensitivity bonus row,
  `PARENT=companion_master_master`, `IS_HIDDEN=1`) was added to the same
  block *after* that fix, as a 20th row, and this specific variable --
  "does an extra hidden row inside an otherwise-correct-shaped block
  retrigger the leak" -- was never isolated or tested by any earlier pass;
  all three ruled-out mechanisms tested moving/counting/sorting the whole
  block, never varying its *internal* row count against the 19-row
  convention. This lines up with the user's screenshot: the leaked Pilot
  box reappearing, plus Force Sensitivity itself visually intruding into
  the branch grid where the client seems to expect exactly the 4 real
  `companion_master_novice` children and nothing else in the block.
  **Suggested fix/diagnostic for whichever chat picks this up:**
  (1) first, re-verify the actually-*deployed* `companion_patch.tre`
  (both `C:\Companion\tre\companion_patch.tre` and
  `C:\SWGEmu\companion_patch.tre`) matches a fresh run of the current
  `build_companion_content.py`/`build_tre_patch.py` by MD5 and row count,
  the same verification pattern the last real fix used (NOTES.md,
  "Rebuild / redeploy") -- it's possible `jedi_teraskappa_01` was authored
  but the patch was never rebuilt/redeployed after, in which case this is
  a stale-deployment issue, not a new code bug. (2) If it is current and
  deployed, test the 20th-row hypothesis directly: temporarily comment out
  the `jedi_teraskappa_01` `add()` call, rebuild, redeploy, and check
  in-game whether the Pilot leak disappears again. If it does, that
  confirms "block must be exactly 19 rows" (not just "exactly 4 branches")
  is the real client-side constraint, and the durable fix is to relocate
  `jedi_teraskappa_01` to its own separate, non-adjacent block/insertion
  point (it doesn't need to physically sit next to `companion_master` at
  all -- it's gated purely by `CompanionSkillTrainer::hasLearnedSkill()`,
  a code check, never by `PARENT`/`SKILLS_REQUIRED` tree position) rather
  than living inside the profession's own 19-row shape. No source files
  edited this pass -- read-only, per this chat's standing research-only
  role; user was offered the choice to have this chat switch to build mode
  and declined, asking for a fix plan instead.

- [research-only chat] the user came back with a concrete design decision
  that supersedes the "relocate `jedi_teraskappa_01` elsewhere" suggestion
  directly above: instead of just moving the hidden row, **remove it from
  `companion_master` entirely** and give it its own new, separate,
  higher-tier profession, "Jedi Companion" -- trainable only after the
  owner has mastered Companion Handler (`companion_master_master`),
  containing essentially just a capstone box ("Master Jedi Companion"),
  and awarding a badge of the same name on completion. Full spec written
  this pass to NOTES.md, dated entry "2026-07-14 -- Design spec: pull the
  Jedi content out of `companion_master` entirely into a new gated 'Jedi
  Companion' profession, with a 'Master Jedi Companion' badge" -- covers
  the exact `skills.iff` row removal/addition, the real-precedent survey
  for how minimal a standalone profession block can safely be (no
  standalone real profession under 19 rows was found; the two sub-19-row
  precedents that exist, `force_title_jedi_rank`/`force_rank_light`/
  `dark`, are both nested sub-trees, not standalone tabs -- flagged as an
  open shape decision for the build chat/user), the `isJediEligible()`
  rewrite (owner's `companion_master_master` badge instead of the
  companion's 11 combat-master badges), the new `datatables/badge/
  badge_map.iff` row (confirmed via `BadgeList.cpp`/`Badge.cpp` reads --
  same TRE-patch pipeline as everything else, `TYPE="master"`), and the
  explicit badge-award call needed in `trainSkill()` since the generic
  `SkillManager` auto-badge mechanism doesn't reach companion-side skill
  grants (with an open player-badge-vs-companion-badge question flagged
  for confirmation). No source files edited this pass -- read-only, same
  standing role; user again chose "write up the fix plan, hand off to
  build chat" rather than switching this chat to build mode.

- [research-only chat] user asked to keep researching; ran 4 parallel
  read-only sub-agents to resolve the open questions left by the entry
  above. Full results in NOTES.md, "2026-07-14 (follow-up, same day) --
  Four open questions from the spec above resolved via parallel read-only
  research." Headline changes to the plan: (1) **the minimal-shape
  question is now settled, not open** -- swept all 51 standalone
  top-level professions in the base 1068-row table and every single one
  is exactly 19 rows, zero exceptions, so `jedi_companion` should be built
  on the proven 19-row/GRAPH_TYPE=4 shape, not an untested minimal one.
  (2) **`datatables/badge/badge_map.iff` has never been extracted into
  this repo at all** (unlike skills.iff/xp_limits.iff/command_table.iff)
  -- this is a newly-identified prerequisite: a build chat needs a real
  game TRE source to extract it from before badge INDEX/CATEGORY/SHOW
  values can be finalized. (3) Traced `SkillManager::awardSkill()`
  (`SkillManager.cpp:420-432`) directly and confirmed its generic
  "*master*"-substring auto-badge mechanism **already fires today** every
  time a player masters `companion_master` (the skill is granted through
  the real player-side pipeline via `trainer_companion_master`, separate
  from the companion-NPC's own bypassed ledger) -- it just silently no-ops
  because no `companion_master_master` badge exists yet. Practical upshot:
  once that one badge_map.iff row is added, the owner-side gate check in
  `isJediEligible()` will work with **zero other source changes** for
  that half -- no bespoke award call needed, it rides the existing real
  pipeline. The "Master Jedi Companion" badge itself still needs an
  explicit award call in `trainSkill()` (its skill grant stays on the
  companion-side ledger, which bypasses SkillManager same as
  `jedi_teraskappa_01` does today) -- confirmed KEY should be
  `jedi_companion_master` per the now-confirmed "badge key = skill name
  verbatim" convention (one hardcoded exception exists for
  `crafting_shipwright_master` only). (4) Found and read the real,
  non-companion Jedi unlock system (`hologrind_jedi_manager.lua`, the
  "master 6 of ~30 profession badges" Lua screenplay) -- confirms
  `isJediEligible()`'s badge-check pattern is a faithful port of the one
  and only precedented gating mechanism this engine has ever used for
  "master profession A to unlock profession B" (a broad grep across all
  managers found no other example, and `SKILLS_REQUIRED` never crosses
  profession boundaries anywhere in the base game data) -- confirms the
  plan's gating approach is correct, not just lower-risk. Updated
  file-touch list is in NOTES.md, same entry, "Updated summary of files a
  build chat would touch." No source files edited this pass -- read-only.

- [companion-container-fix chat] user rebuilt with the bag-creation
  diagnostic logging and hit a **real server SIGSEGV**:
  `WeaponRanges::WeaponRanges()` dereferencing a null `WeaponObject*`
  (`weao=0x0`). Root-caused immediately: `CreatureObjectImplementation::
  setWeapon(nullptr, true)` falls back to `getDefaultWeapon()` (reads a
  `"default_weapon"` slot every real player has from character creation),
  then unconditionally builds a `WeaponRanges` packet from the result with
  no null check -- a companion has no `default_weapon` slot (never
  created through the normal character-creation pipeline), so `weapon`
  stayed null and the packet constructor crashed. **This is a real
  regression from this session's own unequip radial**:
  `unequipItemToInventory()` calls `companion->setWeapon(nullptr, true)`
  when un-equipping the current weapon -- the first thing in this project
  to ever call `setWeapon(nullptr, true)` on a companion specifically (the
  only other call site, a real player's own un-equip-weapon command,
  never hit this because real players always have the fallback slot).
  Fixed at the general engine-method level (guarded `WeaponRanges`
  construction behind `weapon != nullptr` in `setWeapon()` itself) rather
  than avoiding the call companion-side -- protects both call sites,
  can't regress real-player behavior. Also verified
  `refreshCombatAttacks()` (called right after in the same unequip path)
  already null-guards correctly, no crash risk there. Flagged, not fixed:
  companions still have no real default_weapon/innate-unarmed object the
  way real players do -- a more thorough fix for later, not required to
  resolve this crash. Full writeup: NOTES.md, "Real SIGSEGV found and
  fixed: setWeapon(nullptr) crashes on a companion, WeaponRanges
  dereferences a null weapon (2026-07-14)". **Not yet rebuilt or tested.**
  File touched: `CreatureObjectImplementation.cpp` (already on the
  touch-list, shared engine file -- other chats avoid touching
  `setWeapon()` until confirmed and this claim is removed).

- [research-only chat] two new user reports investigated read-only (3
  parallel sub-agents), full writeup NOTES.md "2026-07-14 (later same
  day) -- User report: 'take off' knife didn't stay in the window it was
  viewed from; naming window doesn't pop up on first spawn." Summary:
  (1) the unequipped knife is not lost -- `unequipItemToInventory()`
  really does move it into the companion's nested `"inventory"` bag
  (07-13 fix, see above), the user just needed to open that bag icon
  inside the container window to see it; **also flagged**: if the
  server hasn't been rebuilt since the `setWeapon(nullptr)` SIGSEGV fix
  two entries above landed, taking off a *wielded weapon* specifically
  may have crashed the zone instead -- worth checking logs. (2) the
  companion-naming popup was never built to auto-fire on first spawn --
  by design it's manual-only via "Rename Companion" in the Talk-to-
  Companion dialog (`CompanionDialogMenuSuiCallback.h:74-94`), and per
  this file's own earlier entry (~line 1037) even that manual path's
  C++ side wasn't confirmed rebuilt/tested as of last note either.
  **Follow-up in the same pass**: user then proposed a cleaner
  redesign -- keep the companion inventory window strictly as an
  equipped-only view, and make "Take Off" ("Pick Up") send the item
  straight to the player's own inventory instead of into the nested
  bag. Assessed as a genuine improvement, not just a preference (removes
  the exact confusion just hit, and sidesteps the bag's still-unresolved
  reliability questions for this one flow) -- written up in NOTES.md
  under "Follow-up, same pass: user proposed a simpler alternative UX,"
  including the implementation shape (`unequipItemToInventory()`'s
  `transferObject()` target changes from the companion's bag to the
  owner's own inventory container, radial label updated to "Pick Up").
  Note this only changes the take-off direction -- the nested bag likely
  still needs to exist for the separate give-to-companion direction
  (non-equippable gifted items still need somewhere to land), flagged as
  a smaller, separate open question. No source files edited this pass --
  read-only, same standing role.

- [companion-container-fix chat] implemented the "Pick Up" redesign the
  user approved: `unequipItemToInventory()` now sends the item to the
  **requester's own inventory** instead of the companion's nested bag.
  Used 2 parallel read-only research agents first (per explicit user
  request) to get exact implementation facts before writing code: one
  extracted `TransferItemMiscCommand.h`'s real player-unequip pattern
  (container resolution, locking guarantees, `canAddObject()` error
  relay, exact `transferObject()` signature), the other did a full
  reference-inventory of every file touching this feature so the STF/
  radial rename was accurate, not guessed. Rewrote the method's locking
  chain (companion -> item -> requester -> requester's inventory, each
  cross-locked), removed the now-unused companion-bag self-heal block
  from this direction, added a `canAddObject()` precheck relaying real
  errors (e.g. inventory full). Method name and STF key names kept
  unchanged (only display text updated) to avoid unnecessary `.idl`/key
  churn -- noted explicitly in comments so it doesn't read as an
  oversight later. STF text updated (`"Take Off Companion"` ->
  `"Pick Up"`, plus the two failure/success message strings) and
  **already built + deployed live** to `companion_patch.tre` (60
  entries, MD5 `b5239b5ca9b2330e7f984eb23eddf056`, all three copies
  verified identical) -- hit and worked around a bash-mount staleness
  incident on `build_companion_content.py` mid-rebuild (targeted append
  of a truncated tail, verified via `ast.parse` + `Read`, no full
  rewrite needed). The companion's own nested `"inventory"` bag is
  unaffected and still used for the give-to-companion direction. Full
  writeup: NOTES.md, "'Pick Up' redesign implemented (2026-07-14): take-
  off items now go to the player's own inventory, not the companion's
  nested bag". **C++ side not yet rebuilt or tested** -- TRE/STF side is
  already live. Files touched: `CompanionObject.idl`,
  `CompanionObjectImplementation.cpp`, `TangibleObjectMenuComponent.cpp`
  (all already on the standing touch-list, other chats avoid editing
  until confirmed and this claim is removed).

- [companion-container-fix chat] added `companionformup` as a 7th real
  baseline macro/hotbar command, at the user's explicit request ("make it
  so there is also a macro command given like the other macro commands
  you gave for follow stay attack"). Gives `/hpet formup`'s existing
  functionality (`FormationManager::formUp()`) its own owner-grantable,
  hotbar-draggable ability, exactly matching the treatment of
  `companionfollow`/`companionstay`/`companionpatrol`/`companionstore`/
  `companionattack`. Always forms up in "line" (single-click macros can't
  prompt for a typed argument) -- the full `/hpet formup <line|wedge|box>`
  command is untouched for anyone wanting a specific shape. New file
  `CompanionFormupCommand.h` (modeled on `CompanionStayCommand.h`),
  registered in `CommandConfigManager2.cpp`. `companion_master_novice`'s
  `COMMANDS` field in `build_companion_content.py` now includes
  `companionformup`, with a matching `CMD_N_ENTRIES` display-name entry.
  `build_command_table_rows.py` updated throughout (baseline count 6->7,
  all assertions/name-lists). `CompanionSkillTrainer.cpp`'s three
  companion-command lists (help-sheet doc table, SUI hotbar-ready
  listing, `seenMacros` dedup) also updated. **Already built + deployed
  live**: `build_companion_content.py` -> `build_command_table_rows.py`
  (839 rows: 771 base + 7 baseline + 36 + 25) -> `build_tre_patch.py`
  (`ARCHIVE VERIFIED OK`) -> deployed to both
  `C:\Companion\tre\companion_patch.tre` and
  `C:\SWGEmu\companion_patch.tre`, MD5 `070e538f4ded41006dda638cdfb91c66`
  confirmed identical across build output + both deployed copies. **C++
  side (`CompanionFormupCommand.h` + `CommandConfigManager2.cpp`) not yet
  rebuilt or tested** -- needs a real `ninja` build + in-game verification
  before this claim is removed. Full writeup: NOTES.md, "companionformup
  added as 7th baseline macro command (2026-07-14)".

- [companion-container-fix chat] root-caused and fixed two real bugs from
  live user screenshots: (1) the Skill/Stats/Starter-profession SUI sheets
  showed literal unresolved `"@skl_n:root"`/`"@stat_n:carbine_accuracy"`/
  `"@cmd_n:pointBlankArea1"` text -- `SuiListBox` rows don't auto-resolve
  `"@table:key"` text the way a SUI's own prompt title/text does; fixed via
  a new `resolveStfText()` helper using `StringIdManager`, precedented by
  `StructureManager.cpp`'s own identical workaround. (2) "You can not loot
  that" + T21/armor items vanishing from the companion's inventory -- real
  root cause found: the companion's bag was built from
  `creature_inventory.iff`/`LootContainerComponent`, a **corpse-only**
  component (`canAddObject()` always rejects inserts, `checkContainerPermission()`
  requires an `ownerID` that was never set) never meant for a live
  companion's everyday storage. Fixed via a new dedicated
  `companion_inventory.iff` template using `CompanionContainerComponent`
  instead (no new TRE content -- reuses the same real client appearance),
  plus a migration path that swaps out and refills any already-created old
  bag on next summon. **Caveat**: migration only rescues items still
  physically inside the old bag at that moment -- anything already fully
  lost before this fix (very possibly the T21 and 7 missing armor pieces)
  is not recoverable. Full writeup: NOTES.md, "Two real bugs root-caused
  from live screenshots...". **Not yet rebuilt or tested** -- pure C++/Lua,
  no TRE rebuild needed this pass. Files touched: `CompanionSkillTrainer.cpp`,
  `CompanionControlDeviceImplementation.cpp`, new
  `bin/scripts/object/tangible/inventory/companion_inventory.lua`,
  `serverobjects.lua`.

- [research-only chat] user asked, as a pure feasibility question, whether
  a companion could drive a vehicle with the player riding inside it to a
  player-specified waypoint (a taxi-service idea). Ran 4 parallel
  read-only sub-agents. Full writeup: NOTES.md, "2026-07-14 -- Feasibility
  research: 'companion drives a vehicle with the player inside it to a
  waypoint' (taxi service idea)." **Verdict: the literal vehicle-taxi
  version is a real, substantial, unprecedented build, not a quick win** --
  two independent blockers found: (1) a mounted vehicle's position is a
  direct server-side mirror of the *rider's own client input*
  (`DataTransform.h:156-372` -> `updateVehiclePosition()`), not something
  the server computes/drives itself the way real NPC AI movement is
  (`AiAgentImplementation::findNextPosition()`) -- and `VehicleObject`/
  `AiAgent` are sibling classes (both extend `CreatureObject` directly),
  so a vehicle inherits none of the AI-movement machinery; (2) ground
  vehicles are single-seat (`PlayerArrangement::RIDER`, no passenger slot
  at all -- multi-seat only exists for space ships) and the seat/mount
  system is structurally player-only (`DismountCommand.h` requires a real
  `PlayerObject`/"ghost," which companions don't have). No existing
  precedent to lean on either -- shuttles are pure teleports, space NPC
  pilots run on a fully separate `ShipObject`/`ShipTransform` subsystem,
  GM tools only teleport instantly. **However, a much cheaper adjacent
  feature is nearly free**: the companion walking on foot to a
  player-specified waypoint. The underlying "walk to one fixed point and
  stop" primitive (`AiAgentImplementation::addPatrolPoint()` +
  `PATROLLING` state) is the same proven machinery ordinary NPC patrol AI
  already uses, and `WaypointObject`'s stored X/Y/Z maps cleanly onto it.
  The only gap: `CompanionPatrolCommand.h` currently flips `PATROLLING`
  state but never actually calls `addPatrolPoint()` (its own header
  comment says this was deliberately stubbed, since
  `CompanionControlDevice` never got the waypoint-recording system
  `PetControlDevice` has) -- so a small new `/companiongoto <waypoint>`
  command, cloning the existing `resolveActiveCompanion()` pattern, would
  be genuinely simple to build. Recommend offering the user this cheaper
  alternative explicitly before anyone commits to the harder vehicle
  version. No source files touched this pass -- read-only.

- [research-only chat] **live bug report from the user, urgent**: the
  bag-migration fix (previous entry, "Two real bugs root-caused...")
  is now erroring on rebuild: `"could not attach migrated companion
  inventory bag to companion ... (transferObject to slot 4 failed)"`
  (repeated), and "You can not loot that" is still happening. Traced the
  failure through `ContainerComponent::transferObject()`
  (`ContainerComponent.cpp:272-291`) -- ruled out arrangement-descriptor
  mismatch (old and new bag templates share the same base, confirmed via
  `companion_inventory.lua:89`). **Leading hypothesis**: the old bag's
  `"inventory"` key isn't actually being dropped from the companion's
  `slottedObjects` map when `existingBag->destroyObjectFromWorld(true)`
  runs (`CompanionControlDeviceImplementation.cpp:311`) before the new
  bag is attached at line 313 -- `ContainerComponent::removeObject()`
  (`ContainerComponent.cpp:412-436`) only drops that key if the old bag's
  own `getContainmentType()` still reads 4 at that moment, which may not
  hold if that field didn't survive a server restart (this project has
  already found this exact "native setter never calls the real
  persistence primitive" gap elsewhere for companion/pet objects -- see
  earlier entries). Not confirmed with a live log yet -- **cheapest next
  step for the build chat**: log `companion->getSlottedObject("inventory")`
  and `existingBag->getContainmentType()` right before line 313 to confirm
  or rule this out in one rebuild cycle. Full trace + recommended fix
  shape: NOTES.md, "Live bug report: bag migration failing...". No source
  touched -- diagnosis only.

- [research-only chat] immediately after the bug report above, user
  proposed a bigger redesign that would avoid this whole bug class
  entirely: instead of a companion-side storage bag, keep a dedicated
  backpack *in the player's own inventory* -- whatever's inside it is
  what the companion has equipped -- plus a radial "use on companion"
  option for consumables (food/buffs) so the companion can use them with
  real stat effects without the item ever physically moving onto a
  companion-side object. **Not yet researched** -- full writeup and open
  questions in NOTES.md, same section header as above (final entry).
  Worth prioritizing this research before sinking more time into the
  slot-4 migration bug, since this redesign may make the whole
  companion-side-bag mechanism (and its bugs) moot. No source touched.

- [companion-container-fix chat] fixed the slot-4 migration bug the
  research-only chat diagnosed above. Root cause confirmed:
  `ContainerComponent::removeObject()` doesn't reliably/verifiably clear
  the "inventory" slot before `destroyObjectFromWorld()` finishes, so the
  immediately-following `transferObject(newBag, 4, true)` kept failing
  (`slottedObjects->contains("inventory")` still true) -- old bag never
  actually got replaced, so "You can not loot that" kept happening exactly
  as before. **Fix: migrate in place instead of swapping bag objects** --
  `existingBag->setContainerComponent("CompanionContainerComponent")`
  (a simple, side-effect-free native pointer reassignment; container
  components are stateless shared singletons, and this is the exact same
  method `spawnObject()` already calls unconditionally on the companion
  itself every summon) replaces the entire previous create/migrate-items/
  destroy/re-attach sequence. No object creation, destruction, or item
  transfer involved at all this time -- the bag keeps its same ID, slot,
  and contents; only which component governs it changes. Full writeup:
  NOTES.md, "Bag migration bug fixed: migrate the container component in
  place instead of swapping the bag object". **Recommend deprioritizing
  the player-side "loadout backpack" redesign proposed just above** -- its
  main motivation (escaping this bug class) is now moot since the actual
  root cause is fixed at its source; only worth revisiting for the
  "use consumables directly from your own inventory" convenience angle on
  its own merits. **Not yet rebuilt or tested.** File touched:
  `CompanionControlDeviceImplementation.cpp` only (replaces this same
  file's previous migration block from the entry above -- no other files
  affected by this specific fix).

- [research-only chat] finished the redesign feasibility research the
  user asked to prioritize (before the fix above landed). **Verdict:
  buildable, one genuinely new piece needed** -- (1) a player-side
  "loadout bag" driving companion auto-equip is architecturally sound,
  closely mirrors code already shipped (new dedicated container
  component on a one-off template, same pattern as the companion's own
  bag, nesting already fully supported); (2) medicine/stimpack "use on
  companion" is already fully retargetable as-is --
  `HealDamageCommand.h` has no `PlayerObject`/ghost dependency and
  already has pet-target-handling precedent; (3) food/drink "use on
  companion" hits a real structural blocker (stomach-filling state lives
  on the player's ghost, which a companion doesn't have) -- needs a
  genuinely new companion-safe effect-application branch, not just
  wiring; (4) the radial-menu addition itself is small and precedented,
  but flagged one trap: use `resolveActiveCompanion()` (datapad-scan),
  not `resolveCompanionAncestor()` (item-hierarchy walk) -- the latter
  doesn't work for items sitting in the player's own inventory. Full
  writeup: NOTES.md, "Follow-up: redesign feasibility researched." Given
  the bag-migration bug is now separately fixed (entry above), the
  loadout-bag half of this is no longer urgent, but the medicine/stim
  "use on companion" convenience feature is still cheap and worth
  considering on its own merits if the user wants it. No source touched.

- [research-only chat] **new live bug report**: companion runs away on
  the very first spawn immediately after login, but follows correctly on
  every subsequent store/respawn in the same session. Traced to the
  existing homeLocation fix (`CompanionControlDeviceImplementation.cpp:344`,
  `setHomeLocation(player->getPositionX/Z/Y(), parent)`) -- it captures
  the player's position/cell exactly once, at the instant `spawnObject()`
  runs, and never refreshes it. **Leading hypothesis**: right after a
  fresh login/zone-in, the player's position/parent-cell may not be fully
  settled yet if the companion is spawned quickly, so this first capture
  gets a stale/wrong location -- the companion's generic OBLIVIOUS/
  PATHING_HOME AI then correctly (from its own logic) paths toward that
  wrong "home," away from the player. A second spawn later in the same
  session, once the player is unquestionably fully in-world, captures the
  right location and works fine from then on -- matches the reported
  pattern exactly. Not confirmed with a live log yet. **Cheapest next
  step**: log the player's position/parent right before line 344 on a
  fresh-login-then-spawn test vs. a same-session respawn test to confirm.
  Two fix options written up in NOTES.md: (a) narrow -- verify/wait for a
  valid position before capturing homeLocation, or (b) more thorough --
  stop treating homeLocation as fixed-at-spawn at all and periodically
  refresh it toward the owner while following, since a companion doesn't
  really have a "home" the way an ordinary wild NPC does. Full writeup:
  NOTES.md, "Live bug report: companion runs away on the very first
  spawn...". No source touched -- diagnosis only.

- [research-only chat] ran a 16-agent parallel gap survey across the
  companion system while the user waited on a rebuild (topics: death,
  combat AI depth, multi-companion cap, XP/leveling, zone-transfer
  persistence, group combat, cosmetics, cross-session persistence,
  nameplates, GCW/faction, owner-death, crafting utility, real-pet
  coexistence, chat barks, owner buffs, GM tooling). Full writeup:
  NOTES.md, "16-topic parallel research batch: companion system gap
  survey". **One genuine, previously-undiscovered bug found, flagged
  first in that entry**: `CompanionControlDeviceImplementation::
  handleCompanionDeath()`/`reviveCompanion()` is a complete, seemingly-
  shipped death/revive system (vitality penalty, revive radial, system
  messages) -- but it's never actually called anywhere. Real combat
  death instead falls through to the generic wild-creature death
  pipeline (`CreatureManagerImplementation.cpp:572`, since a companion
  is deliberately not a real pet), which **destroys the companion's
  equipped weapon** (`destroyAllWeapons()`, gated on `!isPet()`) and
  leaves a lootable corpse instead of the intended revivable-down-state.
  Worth fixing before any of the other gaps this batch found, all of
  which are documented design limitations rather than live bad behavior.
  Other notable findings, all confirmed via source: no multi-companion
  cap exists but a second companion would be permanently uncontrollable
  (`resolveActiveCompanion()` always returns the first datapad match);
  companion power is fixed at first-summon with equipment as the only
  real progression lever (an XP ledger now accumulates but nothing reads
  it back into stats); companion + real pet coexistence should work fine
  (zero shared state by design); zone-transfer/shuttle travel likely
  strands a summoned companion in the old zone (confirmed to be a
  pre-existing, project-wide gap shared with real pets, not
  companion-specific); GCW kill-credit is a separate, smaller, un-fixed
  gap from the already-fixed TEF issue; ambient companion speech and
  owner-facing passive buffs are both confirmed complete, unbuilt gaps
  with ready-to-copy engine precedent available if ever wanted. No
  source files touched this pass -- all 16 sub-agents read-only.

- **26-agent general engine architecture research batch (2026-07-14).**
  User asked to shift from companion-specific research to broad,
  general engine architecture knowledge ("learn how it all works") to
  build a foundation for future feature work. Ran 26 parallel
  read-only research agents covering: object class hierarchy, IDL
  autogen codegen pipeline, command dispatch, manager boot wiring, ODB
  persistence internals, zone spatial indexing (quadtree), object
  template pipeline (Lua->IFF->C++), the generic SUI window system,
  combat pipeline end-to-end, skill/ability/command relationship, the
  observer/event system, task scheduling, object locking/threading
  model, network packet serialization, STF text resolution, the Lua
  scripting bridge, the container/slot system (full synthesis), ORB
  multi-process clustering (re-verified), the build system, logging
  conventions, the deed/control-device pattern, the generic
  DataTable/.iff parser, the XP->skill-grant pipeline,
  Faction/GCW/Combat manager relationships, the screenplay/quest
  framework, and the radial/object menu system. All findings written
  as new `CODEBASE_GUIDE.md` sections 337-362 -- this is now the
  primary reference for anyone (including future build chats) who
  needs to understand how a given engine subsystem actually works
  before writing new code. No source files touched -- all 26
  sub-agents read-only.

- [companion-container-fix chat] built the auto-equip half of the
  player-side "loadout backpack" redesign (research-only chat's item #1
  above), after the user independently proposed the same idea unprompted
  when the bag-migration fix showed "no change" in-game. New backpack
  template (`companion_loadout_backpack.lua`) + new
  `CompanionLoadoutContainerComponent` (auto-equips any wearable/weapon
  dropped into it onto the player's active companion, displacing any
  already-equipped occupant into the player's own MAIN inventory to make
  room, per the user's explicit spec) + wired into companion recruitment
  (`SkillManager.cpp`, one-time creation) with a self-heal in
  `CompanionControlDeviceImplementation::spawnObject()` for companions
  recruited before this pass. TRE/STF side (new `companion.stf` entry,
  "Companion Loadout") already built + deployed, MD5
  `44e576854da7a782ee97934bca817c53` confirmed identical across all three
  copies. Full writeup: NOTES.md, "Player-side 'loadout' backpack built
  (2026-07-14)...". **Not yet rebuilt or tested -- and this one needs a
  full `cmake` reconfigure, not just `touch + ninja`**, since it's the
  first pass this session adding a brand-new `.cpp` file
  (`CompanionLoadoutContainerComponent.cpp`) -- this project's build glob
  won't discover it otherwise (same gotcha as the earlier "unrelated
  missing file" build failure entry). **Important open item, flagged to
  the user directly**: two rebuild attempts in a row on the *old*
  companion-side bag path both showed "You can not loot that." completely
  unchanged despite concrete, verified fixes -- genuinely unclear yet
  whether that's a stale-build/rebuild-pipeline problem or something else
  specific to that path. This new backpack path is architecturally
  independent (lives in the player's own inventory, governed by the
  well-proven `PlayerContainerComponent`), so it should give a clean
  signal once actually rebuilt -- if it ALSO shows no change, that
  strongly points at the rebuild pipeline itself, not any companion-
  specific code, and whoever picks this up next should dig into that
  before trusting any further code-level fix. Files touched/added: new
  `CompanionLoadoutContainerComponent.h`/`.cpp`, `ComponentManager.cpp`,
  `SkillManager.cpp`, `CompanionControlDeviceImplementation.cpp`, new
  `companion_loadout_backpack.lua`, `serverobjects.lua`.

- [loadout-backpack-visibility + consume chat, 2026-07-14, later same
  day] **Found the root cause of the "backpack/items not shown in a
  proper container" symptom and closed the two gaps left by the entry
  above.** (1) Root cause: `companion_loadout_backpack.lua` was based
  on `object_tangible_inventory_shared_creature_inventory` -- that
  client template has NO appearance file and gameObjectType 8197 (the
  client's internal creature-inventory type), so even a fully working
  server-side bag renders as an invisible, un-openable nothing inside
  the player's inventory. Rebased it onto
  `object_tangible_wearables_backpack_shared_backpack_s01` (real,
  already-shipped wearable backpack -- icon, open radial, normal
  container window; NO TRE patch needed; load order safe since
  `object/main.lua` loads `allobjects.lua` fully before
  `serverobjects.lua`). Also confirmed first: build pipeline is fine --
  `bin/core3` (14:23) is MD5-identical to the fresh ninja output and
  contains `CompanionLoadoutContainerComponent.cpp.o`, so the earlier
  "no change" mystery was NOT a stale binary this time. (2) Migration
  trap: `clientObjectCRC` is PERSISTED per object (set only in
  `loadTemplateData()`; `initializeTransientMembers()` never refreshes
  it on ODB load), so any backpack already created under the old
  template stays invisible forever -- extended the spawnObject()
  self-heal to detect a loadout bag whose clientObjectCRC != the
  shared_backpack_s01 CRC, rescue its contents into the player's main
  inventory, destroy it, and let the existing create-fresh branch run.
  (3) New consume path (the user's "food/drink so they get the stats"
  half, previously unbuilt): new `Consumable.idl` native method
  `consumeByCreature(consumer, owner)` implemented in
  `ConsumableImplementation.cpp` -- mirrors handleObjectMenuSelect()'s
  EFFECT_ATTRIBUTE/SKILL/DURATION/HEALING handling applied to the
  companion (no stomach filling -- no PlayerObject; spice/instant/
  delayed unsupported, return false, item stays stored; "pets"
  species-restricted food deliberately ALLOWED for companions);
  `CompanionLoadoutContainerComponent`'s auto-equip lambda now routes
  `isConsumable()` items to it under the same companion/item/player
  locks. **Not yet rebuilt or tested. Needs full cmake reconfigure +
  ninja (idl change regenerates Consumable.h), then server restart
  (Lua template change) and a fresh summon (self-heal migration).**
  Files touched: `companion_loadout_backpack.lua`, `Consumable.idl`,
  `ConsumableImplementation.cpp`,
  `CompanionLoadoutContainerComponent.cpp`,
  `CompanionControlDeviceImplementation.cpp`.

- **15-agent architecture batch 2 (2026-07-14), rounding out backend
  coverage.** User asked how many more agents would get to "100%
  backend understanding"; answered honestly that 100% isn't a real
  target for a codebase this size, proposed a 12-15 topic list of
  remaining foundational gaps, user approved launching all of them.
  Ran 15 parallel read-only agents covering: the server main loop
  (confirmed there is NO central tick -- the whole simulation is
  emergent from the task-scheduling system re-scheduling itself, same
  as AI "thinking"), the `Reference`/`ManagedReference`/`WeakReference`
  smart-pointer model, generic engine data structures (`SortedVector`/
  `VectorMap`/`HashTable`), the full login-to-spawn-to-logout session
  lifecycle, baseline network messages (the full-state counterpart to
  deltas), RPC/`DistributedMethod` argument marshaling, the
  packet-receive thread architecture, cross-zone object transfer,
  exception handling conventions, `String`/`UnicodeString` internals,
  the (confirmed unused) `Metrics` telemetry interface, config value
  propagation, the graceful shutdown/save-all sequence, `Zone`
  lifecycle, and atomic thread-safe primitives. Written up as
  `CODEBASE_GUIDE.md` sections 363-377.
  **Important companion-relevant finding from this batch:** the
  cross-zone-transfer research (section 370) pinned down the EXACT
  root cause of the previously-flagged "companion stranded on
  shuttle/zone travel" gap -- `PlayerObjectImplementation::
  unloadSpawnedChildren()` only force-stores datapad objects where
  `isControlDevice()` is true, which is true for
  `PetControlDevice`/`VehicleControlDevice`/`ShipControlDevice` (all
  `extends ControlDevice`) but false for `CompanionControlDevice`
  (which deliberately `extends IntangibleObject` directly instead, by
  design, so it can't cross-recognize with the pet/vehicle system).
  The fix, if ever wanted, is narrow: add a companion-aware branch
  alongside the existing `isControlDevice()` check in
  `unloadSpawnedChildren()` so a summoned companion gets force-stored
  before a zone transfer instead of being left behind. Not
  implemented -- flagged for whichever build chat picks it up. No
  source files touched -- all 15 sub-agents read-only.

- **40-agent architecture batch 3 (2026-07-14) -- IMPLEMENTATION layer
  under systems already documented at the mechanics/feature level.**
  User asked for 40 more agents in parallel. Rather than repeat
  feature-level ground already covered, this pass went one level
  deeper: the actual class structure, algorithms, and call sequences
  behind systems this guide previously only described from the
  outside. Written up as `CODEBASE_GUIDE.md` sections 378-417 (guide
  is now 417 sections). Highlights worth flagging for future
  build-chat work:
  - **REST API server** (`WITH_REST_API`, off by default) exists and
    can inject console commands, mutate objects, and edit live config
    over HTTPS with a bearer token -- worth knowing about for any
    future external tooling/dashboard work.
  - **Vendor NPC restocking is confirmed to not exist as a mechanism
    at all** (zero `restock` hits anywhere in source) -- infinite
    stock, fresh item instance per purchase, no counter. If the user
    ever wants real vendor depletion, this is greenfield, not a bug.
  - **Structure maintenance decay never marks the structure dirty for
    ODB save** -- confirms the same "native mutation never calls
    `_setUpdated()`" bug class already seen in the companion system,
    now found in a second, unrelated subsystem. Durability currently
    depends on luck (another mutation on the same object saving it
    first).
  - **No instancing mechanism exists anywhere in the engine** --
    `Zone` objects are constructed exactly once at boot, ~22 total,
    with zero runtime zone-creation code path. Confirmed independently
    a second time (first flagged in section 22/219).
  - **Structure placement has no slope/terrain-flatness check at all**
    -- the one height-lookup line in `isBuildingPermittedAt()` is
    literally commented out. Real gap, not just an unwritten doc.
  - Admin/GM permission is a real 16-tier system (`adminLevel` 0-15,
    Lua-defined skill sets per tier), not a boolean -- relevant if any
    future feature needs staff-only gating.
  No source files touched -- all 40 sub-agents read-only.

- [Fable chat -> documented after the fact by companion-container-fix
  chat] extended the loadout backpack directly from source, no HANDOFF
  claim posted first -- reviewed and verified, both changes look correct.
  (1) Fixed a real bug: the backpack template reused the same
  appearance-less client asset (`shared_creature_inventory`) the
  companion's own separate bag uses -- fine for a bag opened as a
  dedicated root inventory window, but this backpack needs to render as a
  normal clickable icon nested inside the player's own inventory list,
  which a blank `appearanceFilename` doesn't support. Rebased on
  `shared_backpack_s01` (a real, already-shipped wearable backpack -- icon,
  open-container radial, normal window) instead. **Flagged, not yet
  fixed**: the companion's own separate `companion_inventory.iff` bag
  likely has this exact same invisible-icon problem (same appearance-less
  template, same nested-child-icon usage) -- possibly part of why "there
  is no bag to check" was reported earlier this session. (2) Added a real
  new feature: food/drink dropped into the loadout backpack now auto-feeds
  the companion, via a genuine new native IDL method,
  `Consumable::consumeByCreature()` -- directly builds out the research-
  only chat's own "medicine/food use on companion" feasibility finding
  from its item #2. Full writeup + verification details: NOTES.md,
  "[Fable chat] extended the loadout backpack...". **Not yet rebuilt or
  tested** -- covered by the same `cmake` reconfigure already needed for
  this session's other new `.cpp` file, no separate build step. Files
  touched: `companion_loadout_backpack.lua`,
  `CompanionLoadoutContainerComponent.cpp`, `Consumable.idl`,
  `ConsumableImplementation.cpp`.

- [companion-container-fix chat] found and fixed a **second
  PLAYERUSEMASKERROR bug**, same root cause as the loadout backpack's own
  earlier design mistake, in a different bag. User pasted a live server
  log after the last rebuild showing repeated `errorNumber: 2`
  (`PLAYERUSEMASKERROR`) on every insert attempt. Traced it to
  `CompanionLoadoutContainerComponent` (`extends PlayerContainerComponent`
  directly -- a mistake from when it was first built, not caused by
  Fable's later edits) and, on closer look, to the companion's own
  separate `companion_inventory.iff` bag too (migrated onto
  `CompanionContainerComponent` in this session's earlier "corpse-only
  container" fix -- but `CompanionContainerComponent::canAddObject()`
  only special-cases the companion object itself, so for its own bag it
  fell through to the same broken `PlayerContainerComponent::
  canAddObject()`, which requires the destination to be a CreatureObject).
  Confirmed via `character_inventory.lua` that a real player's own bag
  uses plain `ContainerComponent`, no override at all. **Fixed both**:
  `CompanionLoadoutContainerComponent.h` rebased onto plain
  `ContainerComponent`; new `CompanionBagContainerComponent.h`/`.cpp`
  (plain `ContainerComponent` + `checkContainerPermission()` ownership
  gating copied from `CompanionContainerComponent`) now governs the
  companion's own bag instead, registered in `ComponentManager.cpp`,
  wired into `companion_inventory.lua` and the existing in-place
  migration/self-heal block in `CompanionControlDeviceImplementation.cpp`
  (which will correctly re-fix any companion's bag, however it's
  currently broken, on its very next summon -- no manual DB fix needed).
  `CompanionContainerComponent` itself and the companion's own top-level
  container assignment are unchanged and correct as-is. Full writeup:
  NOTES.md, "PLAYERUSEMASKERROR on every backpack/bag insert -- two bags,
  same root bug, found and fixed in one pass (2026-07-14)". **Not yet
  rebuilt or tested -- needs a full `cmake` reconfigure** (adds one new
  `.cpp` file, `CompanionBagContainerComponent.cpp`), not just `ninja`.
  Files touched: new `CompanionBagContainerComponent.h`/`.cpp`,
  `CompanionLoadoutContainerComponent.h`, `ComponentManager.cpp`,
  `companion_inventory.lua`, `CompanionControlDeviceImplementation.cpp`.

- [loadout-backpack-visibility + consume chat, 2026-07-14, later again]
  **Found the TRUE root cause of "You can not loot that" -- it's
  CLIENT-side, and it explains every prior "no change after verified
  fix" report.** The string exists nowhere in server source; the client
  itself refuses drag-out from a container window opened on a live
  CREATURE (treats it as looting) and never sends the request. Stock
  droid item storage (`DroidItemStorageModuleDataComponent.cpp`) works
  with the same creature-inventory client templates because it opens
  the BAG object, not the droid. Fixed `CompanionMenuComponent.cpp`:
  SERVER_MENU1 now opens the companion's bag droid-style, and a new
  owner-only "Retrieve Gear" radial (SERVER_MENU5, plain-text label to
  avoid a TRE rebuild) unequips all worn weapons/wearables into the
  player's main inventory (canAddObject precheck so a full inventory
  never orphans gear; setWeapon(nullptr) handling included). User also
  confirmed live: inserts + auto-equip + 3D visual update now WORK
  after the latest rebuild. Full writeup: NOTES.md, '"You can not loot
  that" TRUE root cause found'. **Not yet rebuilt/tested; plain touch +
  ninja suffices for this file.** Files touched:
  `CompanionMenuComponent.cpp`.

- [loadout-backpack-visibility + consume chat, 2026-07-14, final] **FINAL
  root cause of every companion item-removal failure found via the
  RetrieveGear diagnostics: `@group:no_loot_permission`, a corpse-loot
  protection on the DESTINATION side.** `ContainerImplementation::
  canAddObject()` rejects items arriving from an AiAgent parent unless the
  AI's "inventory" bag's ContainerPermissions ownerID == receiving player
  (the loot system sets this on corpses; nothing set it on live
  companions). Blocked Retrieve Gear, "Pick Up", AND the loadout swap
  displacement -- at the destination, which is why all the source-side
  fixes showed "no change" on removal. Fixed in `spawnObject()`:
  `bag->getContainerPermissionsForUpdate()->setOwner(playerID)` every
  summon (idempotent self-heal, same API GroupManager::transferLoot()
  uses). Also this pass: "View Equipment" radial (SERVER_MENU6, equipped
  gear had become invisible after the bag-open fix), inventory-full
  detection/messages in Retrieve Gear and the loadout swap. See NOTES.md,
  "FINAL root cause of all companion item-removal failures". Files:
  `CompanionControlDeviceImplementation.cpp`, `CompanionMenuComponent.cpp`,
  `CompanionLoadoutContainerComponent.cpp`.
  **UPDATE, later same day: CONFIRMED WORKING IN-GAME by the user ("all
  is working good")** after two further fixes on top: Retrieve Gear now
  delegates per-item to the proven unequipItemToInventory() path, and all
  server-initiated worn-item -> player-inventory moves use destroy-first +
  silent transfer + DEFERRED (400ms) re-create (the client silently eats
  an immediate re-create of a just-destroyed object ID -- see NOTES.md,
  "CONFIRMED WORKING IN-GAME (2026-07-14)" for the full final fix stack).
  The whole equip/swap/retrieve loop is closed. Only the loadout
  backpack's food/drink consume path remains untested in-game.

- **2026-07-14 (research chat)** -- Feasibility research on the refined
  "companion taxi" feature (cosmetic vehicle model attached to a companion,
  companion's own AI drives movement, player follows on their own real
  vehicle, other group companions get their own vehicles too). **Verdict:
  buildable, and simpler than the earlier literal "player rides inside
  companion-driven vehicle" version** (see NOTES.md, "Feasibility research:
  'companion drives a vehicle with the player inside it to a waypoint'",
  ~line 6673). Full write-up: NOTES.md, "Follow-up: cosmetic companion-taxi
  feasibility research (refined design)". Key points for whoever builds
  this: (1) recommended approach needs no mount/RIDER-slot involvement at
  all -- a parentless `VehicleObject` can just have `setPosition()`/
  `updateZone()` called on it every companion AI tick, piggybacked on the
  same `updateCurrentPosition()` sequence the companion's own pathing
  already calls, and normal broadcast machinery keeps it visible/synced for
  free; (2) also confirmed (not used by the recommended design, but closes
  an open question) that `updateVehiclePosition()`'s rider-to-vehicle
  mirror actually IS caller-agnostic (`GroundZoneComponent.cpp:102-103`)
  and would fire correctly for an AI-driven RIDER-slot occupant too, and
  that `MountCommand.h` has no player-only gate on mount entry (only
  `DismountCommand.h` gates on exit); (3) group-wide convoy trigger should
  be modeled on `RallyCommand`/`FormupCommand`'s direct group-iteration +
  `Locker clocker(member, leader)` pattern, with an explicit new
  group-membership authorization check (the companion movement setters
  don't gate third-party callers today); (4) speed: no BARC speeder exists
  in this codebase, use a real landspeeder's run speed instead (9-17
  range depending on model); AI-driven movement never hits
  `checkSpeedHackTests()` so no anti-cheat concern boosting a companion's
  `runSpeed`. New player-facing surface needed: a `/companiongoto
  <waypoint>` command (already scoped in the earlier NOTES.md entry,
  reuses `addPatrolPoint()`). No source touched -- research only.

- **2026-07-14 (research chat)** -- Deep dive on the group-wide "companion
  convoy" command's actual mechanics (follow-up to the entry above). Full
  write-up: NOTES.md, "Deep dive: group-wide 'companion convoy' command
  design". For whoever builds this: (1) needs TWO nested cross-locks per
  member, not one -- `Locker clocker(member, leader)` then a second
  `Locker companionLocker(companion, member)`, since the companion is a
  distinct `CreatureObject` from its owner (pattern combines
  `RallyCommand.h`'s group-loop locking with `CompanionFollowCommand.h`'s
  own companion-locking); (2) `checkGroupLeader()` is leader-only -- if the
  design wants any group member to be able to trigger the convoy, use a
  looser membership check instead; (3) full command-registration checklist
  confirmed via `CompanionFollowCommand` as the working example -- notably,
  the `command_table.iff` row is a **client TRE asset, not a repo file**;
  new commands need `build_command_table_rows.py` + `build_tre_patch.py`
  repacked into `companion_patch.tre` and deployed to both
  `C:\Companion\tre\` and `C:\SWGEmu\`, easy to miss since it's outside the
  normal C++ build; permission gating is via a `characterAbility` string
  (leaving it blank makes the command unconditionally callable, same as
  `/hpet`); (4) edge cases are all safe to handle with simple
  skip-and-continue, matching `RallyCommand`/`FormupCommand` precedent --
  `resolveActiveCompanion()` never throws, cross-zone members should be
  explicitly skipped (ties into the known companion-stranding bug), and
  there's no state-machine guard preventing the convoy trigger from
  clobbering a companion's existing PATROL/STAY/combat state, so
  peace-then-clobber exactly like `/companionfollow` already does. No
  source touched -- research only.

- **2026-07-14 (research chat)** -- Feasibility research on a new "ghost
  companion squad" ultimate ability (shuttle drop-in entrance, translucent
  glowing-Jedi visual, 15-min temp clones with same abilities, joins the
  vehicle convoy if the group drives). Full write-up: NOTES.md, "New
  feature research: group 'ghost companion squad' ultimate ability". All
  buildable. For whoever builds this: (1) copy
  `managers/gcw/tasks/LambdaShuttleWithReinforcementsTask.h` wholesale as
  the ship-drop-in template -- posture-toggle "flight," no real engine
  primitive for a scripted camera cutscene exists, so don't look for one;
  (2) `CreatureState::GLOWINGJEDI = 0x200000`
  (`templates/params/creature/CreatureState.h:40`) is real, generic, and
  cheap (`setState()`/`clearState()`) but has literally zero call sites
  anywhere in this codebase -- test against a live client before relying
  on it rendering anything; (3) give clones the same LOOK by cloning the
  real companion's equipped items as transient (`persistenceLevel=0`)
  `TangibleObject`s via `ObjectManager::cloneObject(item, true)`, never
  the real item objects -- this is a real item-duplication-exploit
  concern if done wrong; (4) 15-min lifecycle: spawn with
  `persistent=false` + `scheduleDespawn(900, true)`
  (`AiAgentImplementation.cpp:2338`), both proven existing mechanisms, no
  new code needed; (5) **"same abilities" is only PARTIALLY free** --
  autonomous combat moves are free (shared companion template +
  `refreshCombatAttacks()`), but owner-issued special-order hotbar
  commands live on the OWNER's `PlayerObject`, not per-companion, and
  every existing `/companion*` command only resolves the FIRST companion
  on the datapad -- independently commanding ghost clones is real new
  scope, not included for free; (6) no generic cleanup hook exists for
  logout/zone-transfer/group-disband -- use an independent per-clone
  `scheduleDespawn` timer as the sole reliable mechanism, not any of
  those three events. **Two open product questions flagged for the user**:
  display-name collision with the real companion, and whether ghost
  clones need to be individually player-orderable (affects scope per
  point 5). No source touched -- research only.

- [loadout-backpack-visibility + consume chat, 2026-07-15 overnight batch,
  per user's "get it done"] **Four features/fixes landed in source, none
  rebuilt/tested**: (1) follow-regression root-caused (generic wild-mobile
  trees leash the companion back to its summon-spot home) and fixed with a
  dedicated companion AI map -- new `CheckCompanionState` leaf
  (Checks.h/.cpp, AiMap.h), new `ai/companion.lua` (AWARE/IDLE/MOVE
  modeled on pet.lua minus PetControlDevice leafs), templates.lua
  customMap entry, `setCustomAiMap()` in spawnObject() +
  `initializeTransientMembers()` override for DB reloads; (2) `/invite`
  now groups the companion (GroupManager pet-pipeline extended to
  isCompanionObject(); leaveGroup cleanup in storeObject()/
  handleCompanionDeath()); (3) lightsaber-on-companion null-deref crash
  guarded in PlayerContainerComponent::canAddObject(); (4) **COMPANION
  TAXI built** to the researched cosmetic-piggyback design: dialog option
  9, transient taxi state + startTaxiRide()/stopTaxiRide()/
  updateTaxiTick() on CompanionObject (idl regen -- CONFIRMED SAFE, the
  old hand-patched-const autogen worry no longer applies, both idl decls
  are @read), cosmetic persistence-0 x31 shell mirrored on a 500ms tick,
  landspeeder speed save/boost/restore, active-waypoint destination,
  group convoy via the RallyCommand pattern. Full writeup + morning test
  list: NOTES.md, "Overnight batch (2026-07-15)". Files: Checks.h/.cpp,
  AiMap.h, ai/companion.lua (new), ai/templates.lua, GroupManager.cpp,
  PlayerContainerComponent.cpp, CompanionObject.idl,
  CompanionObjectImplementation.cpp,
  CompanionControlDeviceImplementation.cpp, CompanionSkillTrainer.cpp,
  CompanionDialogMenuSuiCallback.h. Plain touch + ninja (no new .cpp, no
  cmake reconfigure); store + re-summon after boot.

- **2026-07-14 (research chat)** -- Ghost companion squad ability: the two
  open design questions from the entry above are resolved and the feature
  is now FULLY SCOPED, no open questions remaining. User decided: ghosts
  are auto-fight/auto-follow only (not individually player-orderable, so
  stays on the free-tier "same abilities" path -- no new command-dispatch
  code needed); clone names get a literal `-=GHOST CLONE=-` suffix (one
  call, `setCustomObjectName()`, exact precedent already in-tree in
  `CompanionRenameSuiCallback.h:75-77`); clone weapons must NOT be the
  real weapon object, must be a new object ID with the same stats.
  Verified: `ObjectManager::cloneObject(item, /*makeTransient*/ true)`
  (`ObjectManager.cpp:578-656`) does a genuine deep serialize/deserialize
  clone (same mechanism as DB persistence) -- correctly copies a crafted
  weapon's actual rolled `minDamage`/`maxDamage`/`attackSpeed` (per-
  instance fields, `WeaponObject.idl:42-47`) under a brand-new,
  independently-allocated object ID, unparented (caller still needs its
  own `transferObject()` to equip it on the ghost). This is the same
  mechanism already scoped for appearance-cloning in the entry above --
  one API covers both "looks right" and "same stats, different ID." Full
  write-up: NOTES.md, "Ghost companion squad: two open design decisions
  resolved by the user, verified buildable." Combined with the entry
  above, this feature has a complete build plan: shuttle drop-in
  (`LambdaShuttleWithReinforcementsTask.h` template), `GLOWINGJEDI` state
  flag (needs live-client test), `cloneObject(..., true)` for gear/
  weapons, `-=GHOST CLONE=-` name suffix, `persistent=false` spawn +
  `scheduleDespawn(900, true)` for the 15-min lifecycle, shared-template
  `refreshCombatAttacks()` for combat, the already-documented AI-tick
  position mirror for follow/vehicle-convoy, independent per-clone
  despawn timers for cleanup. No source touched -- research only.

- **NOTE**: this chat's own "PLAYERUSEMASKERROR ... second instance" claim
  entry (originally right after this line) is fine and unaffected, but its
  matching NOTES.md write-up got silently clobbered by a concurrent
  whole-file `Write` from another chat at some point today -- recreated
  verbatim, see NOTES.md's own "recovering a lost doc entry" note. Flagging
  here so nobody re-investigates a "missing" doc section that's actually
  just been restored.

- [companion-container-fix chat] closed two logout/interrupt gaps
  (2026-07-15), both requested directly by the user after live testing
  the new Companion Taxi feature and the loadout backpack: (1) a
  companion's taxi vehicle was getting orphaned in the world whenever any
  movement command (`/companionfollow`, `/companionstay`,
  `/companionpatrol`, `/companionattack`, `/companionformup`) was issued
  mid-ride -- `stopTaxiRide()` was only ever called from `storeObject()`/
  `handleCompanionDeath()`/arrival, never from the ordinary movement
  commands, which unconditionally clobber companion state with no
  awareness of an active ride. Fixed with a one-line
  `isTaxiActive()`/`stopTaxiRide(false)` guard in all four command files
  (same shape as each command's existing `isInCombat()` peace-out) plus
  an `isCompanionObject()`-gated version inside `FormationManager::
  formUp()`'s per-follower loop (shared across pets/droids/companions).
  (2) A companion was never force-stored on player logout OR zone
  transfer -- `PlayerObjectImplementation::unloadSpawnedChildren()` only
  ever gathered objects where `isControlDevice()` is true, and
  `CompanionControlDevice` deliberately doesn't extend `ControlDevice`
  (this exact gap was already root-caused for zone transfers specifically
  by the research-only chat's earlier cross-zone-transfer batch -- see
  that entry above -- but never implemented until now). Fixed by adding a
  companion-aware scan to `unloadSpawnedChildren()` feeding a new
  `Vector<ManagedReference<CompanionControlDevice*>>` into
  `StoreSpawnedChildrenTask`, force-storing (bypasses the combat gate,
  matching real pets) under the same player lock the task already takes.
  Since the fix lives in the one shared `unloadSpawnedChildren()`
  function, it closes the previously-flagged zone-transfer stranding gap
  for free too, not just logout. Full writeup: NOTES.md, "Two logout/
  interrupt gaps closed: taxi vehicle orphaned mid-ride, companion never
  force-stored on logout (2026-07-15)". **Not yet rebuilt or tested** --
  no new `.cpp` files, plain `touch + ninja`, no `cmake` reconfigure
  needed. Files touched: `CompanionFollowCommand.h`,
  `CompanionStayCommand.h`, `CompanionPatrolCommand.h`,
  `CompanionAttackCommand.h`, `FormationManager.cpp`,
  `StoreSpawnedChildrenTask.h`, `PlayerObjectImplementation.cpp`.

- **Still open, reported by user same session, not yet fixed**: companion
  still isn't visually wearing equipped gear -- appearance shows default
  baked-in clothes from creation regardless of what's equipped, even
  though "Your companion equips the item." fires correctly and the
  loadout backpack now correctly counts inserted items. Needs
  investigation into whether `companion_actor.lua`'s client template can
  even render per-slot wearable appearance (vs. being a single canned
  NPC "dressed" mesh) -- flagged, not yet root-caused.

- [companion-container-fix chat] unlocked all 60 companion ability
  macros at novice for testing (2026-07-15), per direct user request
  ("test everything easy"). Real finding: the *intended* unlock path
  (training the companion in a real profession skill ->
  `grantOwnerAbilitiesForSkill()`) was never reachable through any UI in
  this deployment -- `sendTrainList()`'s own header comment already
  flagged real profession skill-tree enumeration as an unbuilt
  integration TODO, confirmed by reading the candidate-building code
  directly (only `companion_master_*`/`jedi_*` are ever offered). So this
  wasn't a regression to fix, it was a gap nothing had ever closed. Added
  `CompanionSkillTrainer::grantAllAbilitiesForTesting()` -- grants all 60
  macros directly, bypassing the badge gate, called once at the existing
  one-time "first companion" grant site (`CompanionStarterProfessionSuiCallback.h`)
  and self-healed every summon (`CompanionControlDeviceImplementation::spawnObject()`)
  for the user's own already-existing test companion. Scope: only the
  ability macros -- companion's own `learnedSkills`/Skill Sheet untouched.
  **Second part of the request (macro icons matching real commands) --
  investigated, not implemented.** `characterAbility` (the closest thing
  to an icon key in `command_table.iff`) is also the literal gate string
  `PlayerObject::hasAbility()` checks before any of these commands can
  run at all -- renaming it to match a real command's name to fix the
  icon would collide with and break that gate (either disabling it or
  colluding with an unrelated real ability). No separate icon-only table
  found this pass. Flagged back to the user for a steer rather than
  guessing further -- full writeup in NOTES.md, "All 60 companion ability
  macros unlocked at novice for testing; macro-icon investigation".
  **Not yet rebuilt or tested (unlock part)** -- no new `.cpp` files,
  plain `touch + ninja`. Files touched: `CompanionSkillTrainer.h`,
  `CompanionSkillTrainer.cpp`, `CompanionStarterProfessionSuiCallback.h`,
  `CompanionControlDeviceImplementation.cpp`.

- [companion-container-fix chat] built multi-companion support (2026-07-15)
  -- user wants to test 5 companions at once. Asked two clarifying
  questions first (recruit flow shape, command scope) since
  `companion_slots` turned out to have zero enforcement anywhere in the
  server -- answers: instant-grant N on novice (same profession/loadout
  cascaded to all), commands control every summoned companion at once.
  Bumped `companion_slots` 1 -> 5, rewrote `SkillManager.cpp`'s grant
  block to create N companion+device pairs (reads the skill mod live,
  tops up any shortfall rather than a one-time create), gave each a
  distinct default nameplate, cascaded the starter-profession picker to
  every un-launched companion in one answer, and converted every
  `Companion*Command.h` file's `resolveActiveCompanion()` (singular) into
  `resolveActiveCompanions()` (plural) with the command body looped --
  `FormationManager::formUp()` already matched this model, no change
  needed there. Also named each companion's own inventory bag `"<name>
  Inventory"` instead of a static "Companion Storage" (see the loadout-
  backpack entry above's counterpart), and extended the Character Builder
  Terminal's "Enhance Character" HAM buff -- which already looped over
  real pets -- to also loop over the owner's companions (same datapad
  scan every command file already duplicates; `enhanceCharacter()` works
  unmodified on a `CompanionObject`, no `PlayerManager` changes needed).
  **Hit a live file collision rebuilding the TRE**: another concurrent
  chat is actively iterating on `build_tre_patch.py` itself (adding
  `ui_styles.inc` packing for a "macro/command icons" feature -- looks
  like someone picked up the icon investigation flagged in the entry
  above) and the shared script was caught mid-save, truncated, twice.
  Worked around it with an inline equivalent packer rather than editing
  the contested file; verified output byte-identical, deployed, MD5
  `6a2bfc29e94f1911d38a03df2cdf429d` confirmed across all three TRE
  locations. **Flagged, not addressed**: the loadout backpack is still
  shared/singular even with multiple companions out -- its auto-equip
  still only ever targets one companion; extending it to pick which of
  several is a real design question with no answer yet. Full writeup:
  NOTES.md, "Multi-companion support: 5 simultaneous companions,
  squad-order commands, per-companion HAM buffs (2026-07-15)". **Not yet
  rebuilt or tested** (TRE side already deployed) -- no new `.cpp` files,
  plain `touch + ninja`. Files touched: `build_companion_content.py`,
  `SkillManager.cpp`, `CompanionStarterProfessionSuiCallback.h`,
  `CompanionFollowCommand.h`, `CompanionStayCommand.h`,
  `CompanionPatrolCommand.h`, `CompanionAttackCommand.h`,
  `CompanionStoreCommand.h`, `CompanionAbilityCommand.h`,
  `CompanionControlDeviceImplementation.cpp`,
  `CharacterBuilderTerminalImplementation.cpp`.

- **Landed, same chat**: full taxi/vehicle-mimicry redesign per direct
  user request (2026-07-15) -- see NOTES.md, "Taxi/vehicle-mimicry
  redesign: reverted real mount, matched owner's vehicle, added a
  waypoint picker and a general escort hook" for the full write-up.
  Summary: (1) reverted the "genuine mount" attachment back to a
  tightened (200ms) cosmetic position-mirror -- confirmed root cause of
  the teleport bug; (2) `startTaxiRide()` now takes a `vehicleTemplateCRC`
  (matches whatever vehicle is passed in, no longer hardcoded x31) and a
  `hasDestination` flag (false = escort mode, leaves companion
  state/movement untouched); (3) new general hook in
  `VehicleControlDeviceImplementation::spawnObject()`/`storeObject()` --
  every summoned companion now pulls out/stores a matching cosmetic
  vehicle whenever the OWNER calls out/stores their own, independent of
  the Taxi dialog; (4) Taxi dialog (`CompanionDialogMenuSuiCallback.h`
  case 9) now requires the owner to have their own vehicle spawned first,
  then offers a real SUI ListBox of the owner's own waypoints on the
  planet (new `CompanionTaxiWaypointSuiCallback.h`,
  `SuiWindowType::COMPANION_TAXI_WAYPOINT = 1210`) instead of silently
  using the first pre-activated one; group convoy members each mimic
  THEIR OWN spawned vehicle (skipped with a message if they have none
  out). **Not yet rebuilt or tested** -- this chat's sandbox has no C++
  toolchain at all (no `ninja`/`cmake`/`gdb` installed; the real build
  happens on the user's own WSL machine). No new `.cpp` files (only a new
  header) -- plain `touch <files> && ninja` should cover it, no cmake
  reconfigure needed. Files touched: `CompanionObject.idl`,
  `CompanionObjectImplementation.cpp`,
  `VehicleControlDeviceImplementation.cpp`,
  `CompanionDialogMenuSuiCallback.h`, `SuiWindowType.h`, new
  `CompanionTaxiWaypointSuiCallback.h`.

- **2026-07-14 (research chat)** -- Ghost companion squad: user revised
  scope -- ghosts must be fully orderable via the exact same commands a
  real companion uses (not just auto-fight/auto-follow), but must never
  themselves be usable as a source for casting the ability again (no
  recursive cloning). Full write-up: NOTES.md, "Ghost companion squad:
  scope revised -- ghosts fully orderable... but cannot themselves spawn
  more ghosts." **Heads up for whoever builds this**: another build
  chat's concurrent "test 5 companions at once" work has already renamed
  `resolveActiveCompanion()` -> `resolveActiveCompanions()` (plural,
  returns a `Vector`) -- coordinate the ghost-targeting plumbing with
  that effort rather than building a second competing multi-companion
  mechanism; folding ghosts into whatever squad-list concept that work
  produces is almost certainly the right integration point. Technical
  findings: (1) commands already receive a generic `target` (whatever the
  player has selected) via `ObjectControllerImplementation::
  activateCommand()` -- `CompanionAttackCommand`/`CompanionAbilityCommand`
  already use this pattern; extend the other order commands to check
  `target` first (validate via `getLinkedCreature().get() == player`,
  same generic field real companions already get set on via
  `setCreatureLink()`) before falling back to squad-wide behavior; (2)
  recursion is NOT structurally prevented by the engine -- any
  `CreatureObject` (player or AI) can issue commands via
  `executeObjectControllerAction()`, the same mechanism companions
  already use for auto-attack -- add an explicit transient
  `CompanionObject::isGhostClone()` flag and have the ability's own
  clone-source resolution skip anything flagged true, don't rely on the
  datapad-scan naturally excluding ghosts (fragile, breaks once ghosts
  get folded into a squad list); (3) every order command's effect logic
  works fine pointed at a ghost EXCEPT `CompanionStoreCommand`, which
  writes vitality back to the real companion's deed -- a ghost has none,
  so when the target is a ghost this command should skip `storeObject()`
  and just call `destroyObjectFromWorld(true)` directly (early despawn).
  No source touched -- research only.

- **2026-07-14 (research chat)** -- New feature research: "companion
  coordination" -- Doctor buffs gated to camp/hospital, calls out to a
  Ranger companion for a camp, shared multi-owner companion supply bag,
  companions bark when missing an item and trade with each other, always-
  best crafting. Full write-up: NOTES.md, "New feature research:
  'companion coordination'...". **Verdict: every piece is real and
  buildable, several nearly free** -- but this is a much bigger feature
  than prior research passes, and the ORCHESTRATION logic tying the
  pieces together is genuinely novel (nothing like it exists in this
  engine today). Key findings for whoever builds this: (1) the camp/
  hospital buff restriction ALREADY EXISTS for real players
  (`HealEnhanceCommand.h:91-106`'s `private_medical_rating` STRUCTURE
  skill-mod check) -- just read the same aggregate from companion code,
  zero new gating logic; (2) companion chat call-outs work with plain
  runtime-composed strings via `ChatManagerImplementation::
  broadcastChatMessage()` (`:1045`) but MUST be spatial/proximity chat,
  not group chat -- companions can't be real group members, so call-outs
  are audible to any nearby player, not scoped to the group; (3) camp
  auto-deployment has an already-built, companion-safe entry point sitting
  right there: `CampDeploymentManager::deployCamp(owner, companion)`
  (`CampDeploymentManager.cpp:69`), already used by this project's own
  `/hpet camp` -- call it directly, don't route through `HpetCommand`
  (which has a player-only ghost gate `HpetCommand` doesn't); (4) the
  shared companion supply bag needs a NEW permission model -- Guild Bank
  is NOT real precedent (doesn't exist), the real model is
  `StructureContainerComponent`'s explicit ADMIN-list pattern
  (`isOwnerOf || isOnAdminList`), recommend the bag be a real `SceneObject`
  owned by the group leader with its own ID allow-list refreshed on
  group join/leave, not a live `GroupObject` walk per access; (5)
  always-best crafting has real precedent to reuse --
  `CraftingTool::getForceCriticalAssembly/Experiment()` is a genuine
  existing "force critical success" mechanic, but the simpler path is
  `LootkitObjectImplementation::createItem()`'s pattern of just spawning
  a pre-made finished object with no `CraftingSession` involvement at
  all -- still capped by input-resource quality, not stat-uncapped; (6)
  **`CraftingSession` is structurally player-only** (hard-requires a real
  `PlayerObject` ghost, `CraftingSessionImplementation.cpp:48,73-79`) --
  companion "crafting" is NOT buildable through the real session, use
  the same finished-object-spawn shortcut as (5) instead; (7) deployed
  camps have FIXED quality per camp-kit template (not skill/quality-
  rolled) and real players are hard-capped to ONE camp at a time
  (`CampKitMenuComponent.cpp:136-146`) -- "pick the best camp" only
  becomes a real decision if companions are deliberately exempted from
  that cap, otherwise it simplifies to "is my one camp still up." **Four
  open product questions flagged for the user**: camp-cap exemption,
  spatial-vs-group chat audibility being acceptable, what concretely
  triggers a "try again" retry after a contested-item conflict, and
  whether to scope the first build to the Doctor+Ranger camp scenario
  specifically before attempting the full "all professions cooperate"
  vision. No source touched -- research only.

- **2026-07-14 (research chat)** -- Companion coordination: user resolved
  all four open questions from the entry above. Full write-up: NOTES.md,
  "Companion coordination: all four open decisions resolved by the
  user...". **Biggest finding: companion group membership is REAL and
  ALREADY LANDED by a concurrent build chat** (same-day "Overnight batch
  (2026-07-15)") -- `GroupManager::inviteToGroup()`/`joinGroup()`
  (`GroupManager.cpp:37-248`) already had a full pet-group pipeline via
  real `GroupObjectImplementation::addMember()` roster insertion (not a
  UI illusion), and it's now extended to accept `isCompanionObject()`
  alongside `isPet()`. **Status: in source, not yet rebuilt/tested** --
  whoever picks up companion coordination should verify this actually
  works live first, since real group chat for companion call-outs
  depends on it. This also means this file's/NOTES.md's earlier claim
  that companions can never be group members is now STALE/superseded --
  don't trust that older finding without checking this newer one.
  Other decisions: (1) camp handling is "always despawn existing +
  redeploy at current location," not "pick best of several" -- clean
  existing method `CampSiteActiveAreaImplementation::despawnCamp()`
  (`:231-293`) plus the already-found `CampDeploymentManager::
  deployCamp()`, no cap-conflict risk since the one-camp check lives only
  in the player-facing menu component, never in either of these; (2)
  retry trigger is a new radial menu option ("Think Again"), not a chat
  phrase/command -- small, contained, 4 free `SERVER_MENU7-10` slots
  exist on `CompanionMenuComponent`, no client TRE dependency if using a
  plain-text label (existing shortcut already used for other companion
  menu options) -- but needs a NEW state/flag on `CompanionObject` since
  the existing `companionState` enum is movement-only with no "waiting on
  a task" value; (3) scope is confirmed as the general pattern (any
  companion helps any companion that calls out a need), demonstrated
  through Doctor+Ranger first, not a narrower "camp-only" MVP. The one
  piece still needing real new design, not just wiring: the orchestration
  logic that detects shortages, decides who can help, and manages the
  trade -- no existing pattern in this engine to copy. No source touched
  -- research only.

- **2026-07-14 (research chat)** -- Ready-to-add spawn catalog for the
  Character Builder Terminal: max-stat Doctor buff packs + all 6 camp/
  tent kits. Full write-up with exact ready-to-paste Lua blocks: NOTES.md,
  "Ready-to-add spawn catalog: max-stat Doctor buff packs + all 6
  camp/tent kits for the Character Builder Terminal". **Quick summary for
  whoever picks this up**: it's a pure data change, one file --
  `bin/scripts/object/tangible/terminal/terminal_character_builder.lua`,
  `itemList` table -- add two new title/path pairs blocks (exact Lua
  given in NOTES.md), no C++ needed, picked up at server start/reload.
  Doctor buffs: top-tier (`_d` for HAM attributes, `_c` for poison/
  disease) `EnhancePack` templates under
  `object/tangible/medicine/crafted/medpack_enhance_*.lua` -- these ARE
  the max-stat versions (`power=800`/`duration=14200`/`charges=25` for
  HAM; `power=160` for resist packs). No Doctor Mind/Focus/Willpower
  enhance pack exists in this codebase at all (confirmed absent). Camp
  kits: all 6 real tiers exist and are listed (`camp_basic` through
  `camp_luxury`, `object/tangible/scout/camp/*.lua`) -- no 7th tier, no
  faction variants, spawning multiple into inventory is fine but the
  real one-camp-per-owner deploy cap still applies. **Also flagging**:
  this ask came in right after the user hit a live bug (companion
  granted but invisible/uncounted in datapad) that turned out to be the
  exact same issue already root-caused and reverted in this file's
  "Human datapad model REVERTED" entry (same day) -- self-heal added,
  needs a server boot + relog to repair already-broken devices. No
  source touched -- research only.

- **2026-07-14 (research chat)** -- New feature research: public "Hall of
  Records" leaderboard plaque, placeable in open city space, showing
  live stats like Most PvP Kills / Most Creature Kills. Full write-up:
  NOTES.md, "New feature research: public 'Hall of Records' leaderboard
  plaque". **Verdict: fully buildable, well-precedented throughout.**
  Quick summary for whoever picks this up: (1) display -- reuse the
  exact pattern structure signs already use
  (`setCustomObjectName()`/`TangibleObjectImplementation.cpp:1121-1135`,
  safe to call repeatedly), short nameplate teaser + a new "Read Plaque"
  radial (copy `SignObjectImplementation.cpp:14-32`'s pattern) opening a
  multi-line SUI popup for the real leaderboard text; (2) **neither
  underlying stat exists yet** -- there's no PvP kill COUNT today (only
  an Elo `pvpRating`), and no creature-kill counter at all -- both need
  new tracking, but creature kills has a clean, already-proven hook:
  every NPC death already fires `ObserverEventType::KILLEDCREATURE`
  (`CreatureManagerImplementation.cpp`, ~630-677), a stable signal
  already consumed elsewhere; (3) **don't build a "who's on top" query**
  -- Berkeley DB has no secondary index (pure key-value by object ID),
  and this engine's own precedent (GCW score sweep) never scans the full
  offline player population either. Instead maintain a single
  incrementally-updated "current record holder" value per category,
  compared/overwritten at the same moment the underlying counter
  increments -- correctly persists an offline leader's record with zero
  scan, ever; (4) placement in open city space is a real, proven pattern
  -- per-city `CityScreenPlay` Lua files (`bin/scripts/screenplays/
  cities/*.lua`) already spawn permanent ownerless objects at fixed
  coordinates at zone boot (this project's own companion trainer NPC
  used this exact mechanism), and GCW faction-control banners are a live
  precedent for a periodically server-updated ownerless public city
  object -- the no-build-zone/city-zoning validation chain lives only in
  the PLAYER deed-placement flow and doesn't apply here at all. The only
  genuinely new code needed: the two kill counters, the plaque's
  menu/radial component, and a recurring update Task. No source touched
  -- research only.

- **2026-07-14 (research chat)** -- Hall of Records revision: user
  wants NO click at all -- stats auto-cycle on their own. Full write-up:
  NOTES.md, "Hall of Records revision: no-click auto-cycling display
  instead of a 'Read Plaque' radial". **Drop the radial/menu component
  from the entry above entirely** -- simpler design now: just a
  recurring `Task` (mirror `DroidMerchantBarkerTask.h:28-109`'s self-
  rescheduling loop shape) calling `setCustomObjectName()` every 5-8s
  with the next line from a rotating stat array. Note the vendor-barker
  system itself only repeats ONE fixed message
  (`DroidMerchantModuleDataComponent.h:21`) -- it's a precedent for the
  reschedule-loop shape only, the actual multi-line rotation index is
  new (small) code. **Honest limitation for the user, already flagged to
  them**: this will look like the text instantly swapping every few
  seconds, not a smooth scrolling marquee -- `setCustomObjectName()` has
  no transition/animation, and there's no client source in this repo to
  add one even if wanted. Also: swap the base template from a painting
  to `object/static/item/item_scrolling_screen.lua` ("scrolling
  screen") -- a real, already-modeled monitor-style prop that's a much
  better visual fit, drop-in swap, zero new art. No source touched --
  research only.

- **2026-07-14 (research chat)** -- Feasibility research: new planet +
  FPS-style aim-based ground combat "like space." Full write-up:
  NOTES.md, "Feasibility research: new planet + FPS-style aim-based
  ground combat 'like space'". **Two separate honest "no, but"
  verdicts, both worth reading before anyone scopes this:**
  (1) **New planet** -- registering a zone name is trivial (one line in
  `config.lua`'s `ZonesEnabled` list), but the terrain (`.trn`) and
  static geometry (`.ws` WorldSnapshot) a real planet needs are baked by
  SOE's original external terrain-authoring tooling, which this repo
  does not have -- not achievable from scratch. Realistic alternative:
  the commented-out unused zone names already in `config.lua` (lines
  109-126, e.g. `otoh_gunga`/`taanab`/`umbra`) may have real leftover
  assets sitting in the client TRE archives -- worth checking before
  assuming a new planet is impossible outright. A large instanced
  arena/POI carved out of an ALREADY-baked existing planet is the more
  realistic near-term option. (2) **FPS-style ground combat** -- space
  combat turned out to be genuinely, mechanically aim-based (real
  client-sent position+direction vectors, real server-side projectile-
  travel + ray/sphere collision, confirmed via `SpaceCombatManager`/
  `SpaceCollisionManager` -- NOT a misconception on the user's part).
  Ground combat is 100% tab-target + `System::random()` dice roll with
  zero aim/trajectory anywhere in the packet format or `CombatManager`.
  **The real blocker isn't server logic** -- a direction-based raycast
  function would be a plausible, moderate C++ addition on top of the
  existing `CollisionManager` ray/intersection machinery -- **it's that
  the ground CLIENT has never been built to sample/transmit real-time
  aim direction**, unlike the space client, which already does this as
  original SOE functionality. This project can only patch client DATA
  (TRE archives), never client CODE/executable behavior -- closing this
  gap would mean decompiling/rewriting the client itself, a categorically
  different and much larger undertaking than anything else researched in
  this project to date. No source touched -- research only.

- **2026-07-21, c3r -- "JFF Planet Fiteness" (clone Tatooine under a new zone
  name): realistically achievable, no new terrain authoring needed.**
  Terrain/`.trn` and snapshot/`.ws` loading is purely path-string-based
  (`"terrain/" + zoneName + ".trn"`, `"snapshot/" + zoneName + ".ws"`) with
  NO identity check against file contents -- a renamed copy of Tatooine's
  files works as-is under the new zone name. Needs: `ZonesEnabled` config
  entry, a renamed `_regions.lua`, `activeZones`/`zoneRestriction` resource
  entries duplicated, travel/shuttle + client destination-list TRE patch
  (already-proven technique), city lua if cities wanted. One cosmetic-only
  hardcoded C++ check (`zoneName == "tatooine"` Sarlacc spawn,
  `PlanetManagerImplementation.cpp:99`) -- not a blocker. Full detail in
  NOTES.md 2026-07-21. No source touched -- research only, not yet built.
- **2026-07-21, c3r -- Discord channel link unreadable.** User asked to read
  a specific Discord channel URL for tool info; `web_fetch` returned empty
  (Discord SPA requires authenticated session, expected per standing
  web-fetch rules -- no workaround attempted). Needs the user to paste the
  info directly, or a connected authenticated browser session.

### Backlog -- deferred, revisit when the user asks

- **`CombatManager.cpp`'s `addUnmitigatedDamage()` gating** (~line 2477,
  `applyDamage(CreatureObject*, ...)`) excludes real pets
  (`!defender->isPet()`) from whatever it tracks; a companion currently
  is not excluded (same `isPet()`-always-false root cause as everything
  else on this page), but it's unclear without more research whether a
  companion *should* be exempted the same way or not. Not fixed -- flagged
  for whoever next has reason to dig into damage-mitigation tracking.

- **Equipped-item green glow in companion inventory.** The real green
  "equipped" highlight only exists in the client's own persistent character
  Inventory panel -- the companion's inventory opens through the generic
  "open this container" popup instead (same one a crate or vendor uses),
  which has no equipped/not-equipped visual concept at all, regardless of
  the item's real containmentType server-side. Auto-equip already places
  items in a real equip-slot arrangement (same mechanism a player's own
  gear uses) -- there's nothing more to flip server-side. Getting the
  visual would require a fully custom SUI that mimics the real inventory
  panel's layout and manually draws the highlight per item -- comparable
  scope to the "K" skills-window custom SUI work already done for this
  feature. Not started. Only worth doing if the user specifically wants
  this visual badly enough to justify it -- ask before starting.

### Resolved mystery: unrelated third chat was building in this same folder

A real WSL build failed on a missing `BuildingTool.h` (see NOTES.md's
"2026-07-13 -- Real WSL build failed on an unrelated missing file" section
for the full technical root cause -- new untracked `.idl`/`.cpp` files for
an entirely separate, unrelated "build mode" feature under
`docs/buildmode_system/` needed a `cmake` reconfigure, not just `ninja`,
which is why it looked like a companion-system regression but wasn't).
**Root context**: the user had a THIRD chat working on that unrelated
buildmode feature IN THIS SAME `C:\Companion\Core3` folder. The user has
since redirected that chat to a different folder and asked it to remove
anything it added here. **If you're a new chat reading this**: this folder
(`C:\Companion\Core3`) is scoped to the companion-pet system only --
unrelated features should be developed elsewhere, per the user's own
correction.

**Update, 2026-07-13, from the buildmode chat itself**: cleanup is now
done. Removed `BuildingTool.idl`/`BuildingToolImplementation.cpp`,
`objects/player/sessions/buildmode/` (`BuildModeSession.idl`/`.cpp`),
`bin/scripts/object/tangible/building_tool/`,
`bin/scripts/object/tangible/structure/` (the buildmode piece templates),
and `docs/buildmode_system/`. Also reverted the five small hooks that
referenced them: `SessionFacadeType.h` (`BUILDMODE` enum value),
`SceneObjectType.h` (`BUILDINGTOOL` gameObjectType), `managers/object/
objects.h` (the `BuildingTool.h` include), `managers/object/
ObjectManager.cpp` (the `registerObject<BuildingTool>` call), and
`bin/scripts/object/tangible/serverobjects.lua` (the two `includeFile`
lines for `structure/` and `building_tool/`). Verified with a repo-wide
grep for `BuildingTool`/`BUILDMODE`/`buildmode` across `MMOCoreORB/src`
and `MMOCoreORB/bin/scripts` after the revert -- zero hits. The full
buildmode work (11 piece templates + the tool/session code) was copied
out to `C:\Companion-Rust` first, so nothing was lost, just relocated. If
you still see any buildmode-related file under `MMOCoreORB/src/` or
`MMOCoreORB/bin/scripts/`, something regressed -- check with the user
before assuming it's intentional.

**This version supersedes the previous handoff, which had gone stale** --
it described the auto-equip crash as "not yet investigated," but NOTES.md
shows it was root-caused and fixed in source that same day, and a full
additional day of work landed after that was never reflected in the old
handoff at all (see "Landed but never rebuilt/tested" below). If a future
session repeats this, prefer NOTES.md's actual dated entries over this
file's summary -- this file is hand-maintained and can lag.

## What this project is

A "Companion Master" pet/companion system built into a Star Wars Galaxies
Core3 emulator server. Server source: `C:\Companion\Core3`. Game client:
`C:\SWGEmu`. Player builds/restarts the server themselves from a WSL/Debian
terminal -- I give exact copy-paste commands each time, I cannot run them
myself.

## Current status (as of this handoff)

**Working, confirmed in-game (real test, not just code review):**
- Companion summon/store, HAM stats, combat, threat interception, Vigilance
  gating.
- `/hpet` and all `/companion*` commands typeable in chat (client
  `command_table.iff` patch).
- Companion Master skill tree renders correct data per-box (was showing
  stale/wrong-profession data from multiple stacked bugs, all root-caused
  and fixed -- see NOTES.md's several dated sections on this).
- **Radial menu** (`isCompanionObject=true`, "adding companion radial items"
  in server log). Root cause was a missing `const` qualifier on
  `CompanionObject::isCompanionObject()`'s override (six spots in
  `MMOCoreORB/src/autogen/server/zone/objects/companion/
  CompanionObject.h`/`.cpp`), which silently broke virtual dispatch for any
  caller going through a base `SceneObject*` pointer. Fixed, rebuilt, and
  **confirmed working live** -- this is what exposed the crash below.

**Root-caused and fixed in source, but NOT yet rebuilt/redeployed/re-tested
-- do this first in the new chat:**
- **Auto-equip crash** (`TangibleObject.cpp:64` assert,
  `addTemplateSkillMods()`/`isLockedByCurrentThread()`). Happened live,
  right after the radial fix above was confirmed, when a player dragged
  gear into the companion's inventory. Root cause: `attemptAutoEquip()` in
  `CompanionContainerComponent.cpp` ran its nested equip-slot transfer
  inline inside `notifyObjectInserted()`, on whatever thread held the
  *player's* lock, never the companion's -- so the downstream
  `addTemplateSkillMods()` assert on the companion always failed. Fixed by
  moving the mutating work into a `Core::getTaskManager()->executeTask()`
  lambda that takes `Locker(companionRef)` + `Locker(itemRef, companionRef)`
  first (same pattern as the summon/store lambda), then re-validates state
  under lock before transferring. **This fix has not been through a real
  compile or in-game test yet** -- do that before touching anything else.
  **Update:** a second, deeper bug in the same file was found and fixed
  today on top of this (signed/unsigned `containmentType` comparison --
  see "Active work claims" above and NOTES.md) -- the two fixes need to go
  through the same rebuild/test pass together, not separately.

## Landed since the previous handoff, also not yet rebuilt or tested at all

A full day of further C++/IDL/TRE work (2026-07-13) happened after the
auto-equip fix above and is **untested in every sense** -- no compile
confirmation, no in-game pass:

- **Companion macro/hotbar system.** 67 real companion commands (6 baseline
  order commands -- follow/stay/patrol/store/attack -- plus 36 badge-gated
  master-combat-profession abilities plus 25 starter-profession abilities)
  now register as real `command_table.iff` rows and get granted onto the
  owner's own `abilityList` the moment the companion learns the matching
  skill -- draggable to the owner's real hotbar, dispatched via a new
  generic `CompanionAbilityCommand.h` QueueCommand. Revoked again on
  untrain unless another learned skill still grants the same ability.
  `companion_patch.tre`'s `command_table.iff` needs to be regenerated again
  to ship this (67 companion rows now, up from the earlier 6).
- **Companion badge tracking.** New real, persisted IDL field
  `companionBadges` on `CompanionObject` (a genuine `idlc.jar`-regenerated
  autogen pair, not another hand-patched stand-in like the `const` fix).
  Jedi eligibility (`isJediEligible()`) now checks
  `hasCompanionBadge("<profession>_master")` instead of raw learned-skill
  strings, per the user's own framing that holding the badge (proof of past
  mastery) is what should matter, not still having the skill currently
  trained.
- **Skill Sheet SUI rebuilt** to look like a real profession tree (grouped
  by profession, tier-ordered, real name/mod/command text resolved through
  the already-shipped `skl_n`/`stat_n`/`cmd_n` STF tables) instead of a flat
  list of raw internal skill-name strings.
- **New Stats Sheet SUI** (vitals/HAM, companion vitality, resistances, XP,
  aggregate skill-mod bonuses -- explicitly labeled "learned, not yet
  applied to combat" since `grantSkill()` still doesn't call the real
  `addSkillMod()`). **Update, 2026-07-13, from a later research-only
  pass**: root-caused exactly why and what the fix should look like --
  see `CODEBASE_GUIDE.md` section 55 (SkillMod Aggregation Pipeline).
  Short version: `getSkillMod()` only ever reads a pre-computed cache
  (`skillModList`); nothing derives it lazily from a skill list at read
  time. **Further sharpened by a second pass, section 70 (Skill
  Training/Trainer NPC System)**: the real player skill-grant path
  (`SkillManager::awardSkill()`, `SkillManager.cpp:384-391`) doesn't
  actually call a `verify*()` reconciliation function at all for this
  -- it pushes the bonus inline, directly, at grant time: `for each
  modifier the skill provides: creature->addSkillMod
  (SkillModManager::SKILLBOX, modName, modValue, notifyClient)`. That
  exact loop, copied verbatim into companion `grantSkill()`
  (mirrored with `removeSkillMod()` in `removeSkill()`), is very
  likely the complete, minimal fix -- simpler than the originally-
  proposed `verifySkillBoxSkillMods()`/`compareMods()` reconciliation
  approach, which is more of an audit/self-heal mechanism than the
  primary application path. Not yet implemented -- flagged for
  whichever chat next does companion combat-effectiveness work. Also
  worth knowing: `SkillManager.cpp` already contains a real, live
  companion-specific patch (`companion_master_`-prefixed skills are
  exempted from the skill-point cost, not the XP cost, per an inline
  comment citing this project's own NOTES.md) -- so this file has
  precedent for narrow, prefix-gated companion carve-outs in shared
  engine code, the same pattern any `addSkillMod()` fix here should
  probably follow.

None of the above four have a "build confirmed clean" or "confirmed in-game"
note anywhere in NOTES.md -- treat them as needing both a real compile pass
and a full in-game test pass, same as the auto-equip fix.

## Build/restart commands (WSL Debian terminal)

C++ change (`.idl`/`.cpp`/`.h`):
```bash
cd /mnt/c/Companion/Core3/MMOCoreORB
touch <changed files>
ninja -C build/unix/ninja-debug core3
cd bin && gdb ./core3
r
```

Lua-only change: just restart (`kill` / `r` in the running `gdb` session, or
`cd bin && gdb ./core3` then `r`).

TRE/datatable content change (skills.iff, STF strings, command_table.iff):
rebuild `companion_patch.tre` via `docs/companion_system/tools/`, redeploy to
both `C:\Companion\tre\companion_patch.tre` and
`C:\SWGEmu\companion_patch.tre`, then the player needs a **full client
restart** (not just relog) to pick it up.

## Recurring gotcha worth knowing immediately

This sandbox's bash-tool mount of the repo has repeatedly shown stale or
truncated file content, even on files nobody just edited -- confirmed on
multiple separate occasions throughout this project (source files reading
days-stale, freshly-edited Python scripts silently running a stale cached
copy). **Always use the `Read`/`Edit`/`Write` tools for source files, never
trust `cat`/`grep`/`wc` output from the bash tool for anything that
matters.** If a freshly-written/edited file behaves like it still has old
content (stale output, syntax errors that shouldn't be there), re-verify via
`Read` before assuming the logic is wrong, and if confirmed correct,
rewriting under a new filename or writing straight through the bash tool
(`cat > file <<'EOF'`) has reliably worked around it. Documented extensively
in NOTES.md with hard evidence (byte-count mismatches, mid-token truncation,
stale mtimes).

## Task list note

Each session in this project has used the task-tracking tool extensively.
That task list is scoped to its own conversation and won't appear in a new
chat. Treat NOTES.md's dated section headers as the real task history.

## Research-only pass, 2026-07-13 (no code changed)

A separate chat did a read-only research pass (no files touched except this
one and NOTES.md itself) while the build referenced above was running, to
build a precise model of the IDL/autogen codegen mechanics and re-verify the
core companion source files directly rather than trusting prior write-ups
secondhand. See NOTES.md's matching dated section
("Research-only pass: IDL/autogen + build mechanics confirmed, core
companion files re-verified clean") for the details -- short version: found
one new, previously-undocumented build-system fact (`BUILD_IDL`'s
per-file `idlc.jar` regeneration is gated only on the `.idl`'s own mtime,
not the generated output, so a hand-patched `autogen/` file is safe forever
unless its source `.idl` is touched again), and confirmed every file it
re-read (`CompanionContainerComponent`, `CompanionSkillTrainer`'s badge/
macro-list methods, `HpetCommand`, `CompanionAbilityCommand`, the 67-row
`CommandConfigManager2.cpp` registration block) matches NOTES.md's own
description exactly, with zero drift found. **This did not change the
"landed but not yet rebuilt/tested" status above** -- still needs the real
compile + in-game pass.

A second research-only pass (same chat, same day) read
`CompanionControlDeviceImplementation.cpp`'s summon/store locking lambda,
`CompanionThreatObserver`, `FormationManager`/`CampDeploymentManager`, and
the object-menu/radial dispatch chain directly. Worth knowing: it
independently confirmed the already-applied `CompanionStoreCommand.h`
locking fix (the one referenced above, mid-rebuild) matches this codebase's
own established chained-`Locker` pattern -- good corroborating signal, not
just a self-report. Also found two small, non-blocking items for whenever
those files are next touched: `interceptThreatToOwner()`'s comment
overstates the untrained Vigilance intercept chance (says "most of the
time," actual formula gives 40%), and `CompanionMenuComponent.cpp` still
carries diagnostic logging from the `isCompanionObject()` investigation
that's no longer needed now that bug is fixed. Neither is a real bug. See
NOTES.md's matching "Research-only pass #2" section for full detail.

A fifth research-only pass (same chat) finished this project's task list
(items 16-18): the remaining `CompanionSkillTrainer.cpp` SUI methods
(`sendUntrainList`/`sendHelpSheet`) + their callbacks, the trainer
conversation Lua wiring (`trainer_conv.lua`/`trainerData.lua`/
`tatooine_mos_eisley.lua` -- confirmed 100% generic/data-driven, zero
companion-specific conversation code), and the remaining TRE toolchain
scripts (`tre_writer.py`/`stf_codec.py`/`build_tre_patch.py`/
`build_command_table_rows.py`). Zero new bugs, zero drift from existing
docs -- this closes out full companion-system source coverage
(C++/IDL + Lua + Python TRE tooling). See NOTES.md's "Research-only pass
#5" section for full detail.

A sixth research-only pass (same chat) went into general engine internals
(command dispatch pipeline, `CombatManager.cpp`, AI behavior-tree ticking,
`ObjectManager`/`DOBObjectManager` persistence) rather than companion
source directly, per the user's request to learn more from the backend.
While cross-referencing companion code against those general mechanics it
turned up **two new, real, not-yet-fixed findings** worth any future
chat's attention:
1. **Companion attacks likely never set the owner's PvP/GCW TEF, and it's
   actually TWO independently-broken copies of the same bug.**
   `CombatManager.cpp` casts a pet's control device to `PetControlDevice*`
   to find "who gets flagged" -- a companion's `CompanionControlDevice`
   isn't a `PetControlDevice`, so the cast silently fails. This happens
   both in `checkForTefs()` (which computes whether a TEF *should* be
   applied at all -- so for a companion attacker the three TEF booleans
   never even become `true`) and, redundantly, in the later
   `doCombatAction()` block that would apply the timestamp. Fixing only
   one of the two would still leave the other silently no-op-ing. Not a
   crash, low-to-moderate real-world impact (most companion combat is
   PvE), but real and now fully traced.
2. **Companion state (vitality/XP/skills/badges/combat state) has no
   structural save guarantee -- and this is NOT just a crash-only risk.**
   Every mutating method on `CompanionObject` is a hand-written `native`
   setter, and none of them (nor anything else in `objects/companion/`)
   ever calls `updateObjectToDatabase()` -- confirmed zero call sites
   project-wide. Originally assumed a clean shutdown saves everything
   regardless as a safety net; traced the actual shutdown save path
   (`ServerCore::shutdown()` → `createBackup(SAVE_FULL)` →
   `DOBObjectManager::updateModifiedObjectsToDatabase()`) and confirmed it
   **only ever saves objects some code path already marked dirty** --
   `SAVE_FULL` changes how a dirty object is written, not which objects
   get collected. So an object that's never independently dirty-marked
   isn't saved at shutdown either. The only thing that incidentally saves
   companion state today is `setMaxVitality()`, called on every
   summon/store. In practice a player actively using their companion
   probably triggers enough incidental dirty-marks to mostly cover this,
   but there's no structural guarantee the way there is for a player
   character's own HAM pools.

**Update, 2026-07-13 (later same day):** finding #1 (the TEF bug) has now
been fixed -- see "Active work claims" above, "fixed the previously-flagged
'companion attacks never set the owner's PvP/GCW TEF' bug", and NOTES.md's
matching section. Finding #2 (the persistence gap) has not been fixed --
no source touched, this is flagged for whichever
chat next does PvP-adjacent or persistence-adjacent companion work. Full
writeup with exact code, severity reasoning, and a proposed fix shape for
each: NOTES.md, "General-engine research pass: command dispatch, combat,
AI ticking, persistence -- TWO new companion-specific findings" (plus its
"Deeper look" addenda on both findings).

A seventh pass (same chat) read straight through all 24 sections of
`CODEBASE_GUIDE.md` end to end (world/content authoring, missions/
vehicles/resources, economy/crafting/loot/vendors/character-creation,
social/factions/guilds/chat/entertainer) -- not companion-specific, just
general engine fluency per the user's request. The guide holds up well;
no drift found on spot-checks. One real, previously-flagged-as-"possible"
item got upgraded to **confirmed**: `AuctionManagerImplementation::
expireAuction()`'s winning-bid branch never calls
`seller->addBankCredits()` -- a seller whose item sells via a *timed*
auction (bidding, not instant-buy) gets a "sale complete" mail but
apparently never gets paid; the buyer's escrowed credits have no
confirmed destination. Not fixed, no source touched -- see
`CODEBASE_GUIDE.md` section 24 for the full evidence chain and proposed
one-line fix. Worth an in-game test before anyone relies on the auction
system for real transactions.

An eighth pass (same chat) drafted concrete, ready-to-apply-but-NOT-applied
fix patches for all 3 open bugs, plus researched the network/packet layer
and Jedi/Force powers (the latter turned out to already be fully covered
by this guide's own section 9a -- spot-checked one claim
(`villageKnightPrereqsMet()`'s exact `fullTrees >= 2 && totalJediPoints >=
206` logic) against source and it matched exactly, no drift). Status:
- **TEF gap: already fixed by another chat** while this pass was
  underway -- verified the applied fix directly against source, matches
  the proposed shape exactly (extends `isPet()` with
  `isCompanionObject()`, falls back to `getLinkedCreature()`). See NOTES.md.
- **Persistence gap: draft patch written**, but getting there took a real
  detour worth knowing about -- initially found what looked like evidence
  that this codebase's entire dirty-tracking/persistence mechanism might
  be broken by default (`ObjectManager.saveMode` defaults off, not set
  anywhere in `config.lua`), which would have been a much bigger problem
  than just companions. Traced further and resolved it: `saveMode` only
  gates a performance optimization (incremental per-thread tracking vs. a
  full object-directory walk every sweep) -- with it off,
  `DOBObjectManager::runObjectsMarkedForUpdate()` walks every live object
  and saves anything `_setUpdated(true)` was called on, so the mechanism
  works fine either way. Also found that `updateToDatabase()` (the
  convenience method several existing call sites in this codebase use) has
  an **empty body in every class**, seemingly vestigial -- recommend using
  `zoneServer->updateObjectToDatabase(object)` instead, which is
  unambiguously traced end-to-end. **Update: applied** by the
  companion-container-fix chat (see "Active work claims" above) -- see
  NOTES.md, "Persistence-gap draft patch applied (2026-07-13)".
- **Auction payout bug: draft patch written** (add the missing
  `seller->addBankCredits()` + `TransactionLog` to `expireAuction()`'s
  winning-bid branch, mirroring `doInstantBuy()`), in CODEBASE_GUIDE.md
  section 24. **Update: applied** by the companion-container-fix chat (see
  "Active work claims" above) -- see NOTES.md, "Auction payout bug fix
  applied (2026-07-13)".

All three of these patches (TEF, persistence, auction payout) have now
been applied to source (see "Active work claims" above) -- none has been
rebuilt or tested in-game yet. The `updateToDatabase()`-is-empty-everywhere
finding is also now in
CODEBASE_GUIDE.md section 11 as a general engine fact, separate from the
companion-specific fix. Also added a new CODEBASE_GUIDE.md section 25
(Network/Packet Layer): outgoing `BaseMessage` construction, the SOE
transport protocol, and the opcode -> `messageCallbackFactory` -> `Task`
dispatch chain that everything (including the command pipeline in
section 6) ultimately starts from.

A ninth pass (same chat, read-only, no companion/source files touched)
worked straight through a large 14-topic general-engine research batch
at the user's request while they were away, adding **14 new numbered
sections (26-39) to `CODEBASE_GUIDE.md`**, each with concrete file/line
citations and a "practical takeaways" close:
- **26. Space Combat Internals** -- real damage-resolution class is
  `SpaceCombatManager`, not `ShipManager` (which is pure data-loading);
  shield->armor->component->chassis damage waterfall; 25% flat PvP
  damage reduction; companions have zero presence in this system.
- **27. Database/ODB Internals (Deeper)** -- real Berkeley DB backend,
  ~25 separate named databases, object ID's top 16 bits permanently
  encode its home database, a CRC no-op-write check sits *under* the
  dirty-flag sweep from the earlier persistence finding, 5-minute
  save-sweep blocks all task-manager threads.
- **28. Pathfinding/Collision Deep Dive** -- Recast/Detour outdoor
  navmesh vs. a wholly separate custom triangle floor-mesh system
  indoors; AI path caching/re-plan-threshold in `findNextPosition()`;
  anti-speedhack `checkMovementCollision()` is structurally unrelated to
  AI pathing; LOS is geometric ray-cast, not pathfinding-based (old
  dead-code comment implied otherwise).
- **29. Lua Scripting Bridge Internals** -- real Lua C API + Luna
  bindings (~35 `Luna<T>::Register()` calls = the full inventory of
  what's script-visible); one Lua VM **per worker thread**, not shared;
  hot-reload is a lazy per-thread version-counter check, not atomic
  server-wide.
- **30. Login <-> Zone Handshake** -- login and zone servers share one
  MySQL `sessions` table as the actual handoff mechanism (no direct
  socket/RPC between them in the default build); session token is
  single-use, deleted on zone-connect; galaxy bans and character bans
  are separate, zone-side-only checks.
- **31. Buff/Debuff System Internals** -- one `Buff` base class covers
  buffs/debuffs/states alike (states are buffs keyed by
  `Long::hashCode(state)`); attribute/skill-mod/state application is
  idempotent via `modsApplied`; NPC buffs never send client packets
  (`isPlayerCreature()` gate -- flagged as worth checking against
  companions specifically if a visible companion buff aura is ever
  wanted).
- **32. Image Designer / Appearance Customization** -- two-party
  negotiated session; server-enforced 4-minute stat-migration
  anti-bypass timer (independent of client UI); skill-gating is a
  binary per-attribute unlock, not a magnitude scaler (contra player
  folklore); hair is a real re-slotted object, not a variable.
- **33. Droid Engineering / Programming** -- droid capability is 100%
  derived from crafted modules scanned out of a hidden
  `"crafted_components"` container; "programming" a droid is per-module
  parsed chat commands (`handlePetCommand()`), no scripting layer;
  droid stats only apply when `isPet()` -- **the same `isPet()`-branch
  shape already identified as this project's TEF bug root cause**, so
  worth remembering for any droid-adjacent companion work.
- **34. City Specialization System** -- pure Lua content; eligibility
  is gated on the *mayor's own trained ability*, not cost; `cost` is
  actually a recurring treasury maintenance drain that can silently
  auto-revoke the specialization if unpaid; the buff is geofenced to
  physical presence, using a dedicated `SkillModManager::CITY` tag for
  clean bulk removal.
- **35. GCW Base Siege Walkthrough** -- destruction is gated by one
  strictly-ordered 8-stage integer state machine, not raw HP; Covert-
  flagged bases are categorically, unconditionally exempt from the
  whole vulnerability system; final destruction is a real interruptible
  60-second-tick broadcast countdown, not instant.
- **36. Weapon Certification System** -- cert check is OR-per-cert
  (ability-list OR skill-box) but AND-across-the-list; enforcement is a
  damage-floor penalty at hit-resolution time (5-10 dmg, DoTs
  suppressed), never an equip or attack-command block; AI/companion
  attackers are unconditionally exempt (`isPlayerCreature()` gate).
- **37. Housing Decoration (Deeper)** -- one shared `ContainerPermissions`
  bitmask ACL (deny always beats allow) powers house furniture rights,
  general containers, and the droid module-container lockdown from
  section 33 alike; three independent permission layers for a house
  (owner/admin bypass, all-or-nothing entry/ban list, fine-grained
  bitmask); decoration capacity is the same generic per-cell
  `containerVolumeLimit` as any container.
- **38. GM/Staff Admin Command System** -- GM commands are gated by the
  *same* ability-check dispatcher mechanism as ordinary commands
  (section 6), just requiring both `hasGodMode()` and an ability
  matching the command's own name; staff tiers/skill-grants are pure
  Lua content; holding a tier and having admin mode "on" are two
  independently toggleable states; every admin command attempt is
  audit-logged from the central dispatcher itself.
- **39. Anti-Exploit / Duplication Prevention** -- every item move goes
  through one atomic remove-then-add function with an explicit
  post-removal consistency check and a dedicated container lock; player
  trades require both sides to independently verify, locked together,
  gated on whichever verification lands second; a structured
  `TransactionLog` (dozens of named `TrxCode` reasons, auto-captures
  call site) audits essentially every credit/item movement in the game
  already, which is itself the tool for investigating anything that
  didn't go through it.

No companion source files were read or touched this pass (general-engine
only, per the user's request); no bugs found this pass required a
NOTES.md entry. `CODEBASE_GUIDE.md` is now ~39 sections. The "Active work
claims" entry for this batch has been removed above -- this paragraph is
the durable record of what happened.

A tenth pass (same chat, read-only, no companion/source files touched)
worked through a second user-selected 8-topic batch, adding **sections
40-46 to `CODEBASE_GUIDE.md`**:
- **40. Resource Spawning/Harvesting Deep Dive** -- location is real 3D
  simplex noise (seed as the noise function's third axis, no stored
  grid); concentration tier sets a randomized density ceiling; **a
  genuinely confirmed dead field** -- `SpawnDensityMap::totalUnits`/
  `unitsHarvested` are fully wired into serialization but never actually
  compared/incremented anywhere, so harvesting does not measurably
  deplete a location today, only timed despawn ends a resource's life;
  harvester extraction is a real elapsed-time formula (offline-safe,
  fractional remainder carried forward).
- **41. Guild System Deep Dive** -- per-member rights are one byte
  bitmask; "leader" is not a special role, just whoever holds
  `PERMISSION_ALL`; guild wars are a real 4-state directional machine
  (`WAR_NONE/IN/OUT/MUTUAL`) toggled by one shared action; leadership
  elections run an exact 2-week cycle; joining is sponsor-gated, no
  open-apply path exists.
- **42. Manufacturing/Factory Crates** -- per-unit crafting time is
  `complexity * 8` seconds; factories block batch-producing sliced/ADK/
  powered-up items (a factory-specific anti-exploit gate); a factory
  crate is literally a stack with a use-count; a manufacture schematic
  is a finite-run consumable, destroyed at zero remaining uses.
- **43. Vehicle/Garage System** -- `VehicleObject extends CreatureObject`
  (not `AiAgent`), puppeted entirely via `linkedCreature` -- the same
  accessor name/pattern as the companion system; resistances degrade
  with condition, not armor slots (vehicles have none); "garage" turns
  out to mean specifically "valid repair location," not general
  storage/summoning, which have no garage-proximity requirement at all.
- **44. Bounty Hunter System (extends section 16)** -- multi-hunter
  races on a live player target auto-fail every non-winning hunter;
  payout has a hidden flat-rate override (25k/50k for a manifested
  Jedi) during a recurring "debuff window," independent of the posted
  reward; both winning and failing hunters get automatic PvP TEF
  cleanup.
- **45. Mail & Chat Channel Internals (extends section 19)** -- offline
  mail re-checks the ignore list a *second* time at login-delivery (not
  just at send), meaning ignoring someone can retroactively delete
  their unread offline mail to you; chat-room entry for
  Guild/Group/Planet rooms is fully derived from live game state (no
  membership list to maintain), only `CUSTOM` rooms use an explicit
  ban/invite list, and ban always beats invite.
- **46. Instance/Heroic Encounters -- confirmed absent**, same category
  as section 22's "Collections -- confirmed absent": no
  `InstanceManager`, no per-group zone-cloning substrate, nothing named
  "heroic" anywhere except an unrelated furniture item. SWG-authentic
  shared-world dungeons only; true per-group instancing would be
  substantial new engine work, not an extension.
- **Collections & badges**: re-verified section 22's existing
  "Collections -- confirmed absent" finding still holds (no new
  `Collection`-related code exists) -- no new section added since this
  was already fully covered.

No companion source files touched, no bugs found requiring a NOTES.md
entry. `CODEBASE_GUIDE.md` is now 46 sections. The "Active work claims"
entry for this batch has been removed above.

An eleventh pass (same chat, read-only, no companion/source files
touched) worked through a third self-selected batch (per the user's
explicit "just keep learning, don't keep asking me" instruction --
this is the first batch this chat picked topics for on its own, no
`AskUserQuestion` used), adding **sections 47-54 to
`CODEBASE_GUIDE.md`**:
- **47. Observer/Event System Internals** -- `Observable`/`Observer`
  are real `ManagedObject`s holding strong references, not lightweight
  callbacks; the default `notifyObserverEvent()` returning `1` means
  "auto-unregister me," a real footgun for any subclass that forgets
  to override it; notification snapshots the observer list under a
  read lock before releasing it, so callbacks can safely re-register/
  notify/drop mid-iteration; `ObserverEventType` is an append-only
  ordinal enum per an explicit source comment (its raw integer value
  gets persisted on at least crafted food items).
- **48. Task/Scheduling System Internals** -- `TaskManagerImpl`'s
  worker pool/scheduler threads/IO schedulers/named custom queues
  (confirms the "slowQueue" mechanism referenced elsewhere); the
  `blockDuringSaveEvent` parameter refines section 27's "save sweep
  blocks all threads" claim -- some custom queues can opt out.
- **Structural self-correction, caught and fixed during this same
  pass**: a chunk of section 47's own content (the strong-reference/
  append-only-enum/practical-takeaways paragraphs) had been mistakenly
  appended to the very end of the file instead of into section 47
  itself by an earlier turn in this same research batch -- caught via
  a `Grep` structural check before starting section 51, moved back
  into place, verified clean. No content was lost, but worth knowing
  this class of self-inflicted structural error is possible; the fix
  is always: `Grep` for `^## \d+\.` to confirm section boundaries
  before trusting `Read`'s tail, don't assume a successful `Edit`
  landed in the intended spot without that check.
- **49. Config System Internals** -- Lua-backed `config.lua` +
  `config-local.lua` (second-loaded-wins) with dotted-key flattening;
  hot-reload uses the same version-counter lazy-invalidation idiom as
  Lua screenplay reload (section 29) and staff permission tiers
  (section 38) -- a genuine reusable pattern, not three unrelated
  mechanisms; per-account config overrides via `withAccount()`; secret
  redaction via `isSensitiveKey()`.
- **50. ORB/Multi-Process Clustering** -- root-vs-remote broker status
  is decided by whether `Core3.ORB` is empty (default, root/standalone)
  or set (client of a remote root); Stub/Servant/Adapter distributed-
  object pattern; **confirmed the default deployment is always
  single-process** -- the clustering machinery is real and fully wired
  but dormant/opt-in, explaining why section 27's non-root-broker
  save-forwarding code path is real but normally unreachable.
- **51. Medical/Healing Profession Internals** -- damage vs. wounds are
  two structurally distinct pools (wounds lower the healable ceiling,
  not current HP); battle fatigue is a single shared `shockWounds`
  float per creature, capped at 1000, incremented per wound-causing
  hit; wound healing (but not stim healing) requires non-city medical
  rating (a hospital/campsite/droid, city bonus alone doesn't count);
  `isHealableBy()` is the one shared PvP/faction eligibility gate,
  reused by pets-delegate-to-owner too.
- **52. Beast Mastery/Creature Taming** -- only babies are tameable;
  taming is a three-phase self-rescheduling `Task` (10s per phase) with
  an uncapped chance formula that can produce guaranteed outcomes;
  failing has a ferocity-scaled chance to aggro the creature; success
  routes through the exact same `PetControlDevice` abstraction as
  droids/vehicles; pet "vitality" decays specifically from wild-NPC
  kills on a 5-minute cooldown, tiered HAM penalty -- **and the
  companion system reimplements this vitality concept independently
  rather than sharing `PetControlDevice`'s copy**, a concrete, known
  divergence worth remembering for companion-adjacent work.
- **53. Death, Cloning & Item Insurance** -- binding at a cloner (vs.
  cloning at the nearest one) is the difference between a 100-point
  wound/shock penalty or none; insurance is single-use per death (the
  `INSURED` bit is stripped after absorbing 1% decay vs. 5%
  uninsured); Jedi lose 5% of their XP cap on clone, floored at a
  hardcoded total; a `PLAYERCLONED` observer event fires at the end,
  making death hookable without patching the cloning code itself.
- **54. Structure Maintenance, Decay & Condemnation** -- out-of-
  maintenance structures try an automatic one-week bank auto-draft
  before decay starts (`Core3.StructureMaintenanceTask.
  AllowBankPayments`, default on); only `BuildingObject`s get a
  condemnation grace period before destruction -- harvesters/factories/
  turrets are destroyed immediately on first reaching decayed state,
  no grace at all; destruction synchronously exports a
  `TransactionLog` before deleting the object specifically because
  logging after deletion isn't possible; structure contents are not
  preserved through destruction.

No companion source files touched, no bugs found requiring a NOTES.md
entry (the section-47 structural self-correction above was the only
issue, and it was self-caught/fixed within this same pass, not
reported as a bug). `CODEBASE_GUIDE.md` is now 54 sections. The
"Active work claims" entry for this batch has been replaced above with
a new claim for the next self-selected batch (SkillMod pipeline, duel
system, Bio-Engineer, NPC dialogue framework, structure lot/placement).

A twelfth pass (same chat, read-only, no companion/source files
touched) worked through the fourth self-selected batch, adding
**sections 55-59 to `CODEBASE_GUIDE.md`**:
- **55. SkillMod Aggregation Pipeline (`SkillModManager`)** --
  **directly actionable for this project's own open backlog item**:
  `getSkillMod()` only ever reads a pre-computed cache
  (`skillModList`), never derives it lazily; each source type
  (SKILLBOX/WEARABLE/BUFF/STRUCTURE/etc.) has its own independent
  `verify*()`-then-`compareMods()` reconciliation function that's the
  *actual* live-application mechanism, not just an audit tool. This is
  exactly why companion `grantSkill()` produces zero stat effect --
  nothing ever calls the equivalent of `verifySkillBoxSkillMods()`
  against a companion. Fix shape spelled out in both this section and
  a new note added directly to this file's own "Landed but never
  rebuilt/tested" section above.
- **56. PvP Formal Duel System** -- mutual-list-membership model, no
  separate "duel state" field at all; "accepting" a duel is just
  issuing the same challenge command back; flagged that
  `checkForArenaDuel()` (used throughout the healing commands in
  section 51) is a same-named-sounding but **entirely unrelated**
  FRS/Jedi-arena check, not this system -- worth remembering to avoid
  confusing the two by name alone.
- **57. Bio-Engineer DNA Sampling/Creature Creation** -- a fully
  separate pet-acquisition path from Beast Mastery taming (section
  52), sharing only the three-phase self-rescheduling `Task` shape;
  five-outcome roll table where even a *successful* sample can
  simultaneously kill the source creature once a skill-scaled sample
  budget is exceeded; produces a graded crafting resource routed
  through the real `CraftingManager`, not a special item type --
  actual creature creation is a separate downstream step consuming a
  `PetDeed`.
- **58. Generic NPC Conversation/Dialogue Framework** -- one shared
  `ConversationObserver` per template CRC (built directly on the
  `Observable`/`Observer` system from section 47), all real state
  living on the player's session instead; screens are 100% Lua-
  authored data, permanently read-only once loaded, requiring an
  explicit `cloneScreen()` for any C++ "screen handler" to inject
  dynamic content. **Confirms exactly why this project's own
  `trainer_conv.lua` needed zero companion-specific code** -- companion
  trainers just point at the same generic, shared conversation
  template data as any other NPC.
- **59. Structure Lot/Placement Validation** -- lot capacity is a
  live-recomputed sum over currently-owned structures (no separate
  counter to drift out of sync, self-corrects automatically if a
  structure is destroyed through any path); placement runs a long,
  strictly-ordered chain of independent checks with early-exit on
  first failure (so a visible error may not be the only problem);
  footprint collision is a coarse axis-aligned rectangle test, not
  true polygon collision; terrain/water/no-build/POI-proximity checks
  are shared verbatim with the camping system via one common
  `PlanetManager::isBuildingPermittedAt()` function.

No companion source files touched this pass. One direct, actionable
cross-reference was added to this file's own backlog note (section 55
above, on the `grantSkill()`/`addSkillMod()` gap) since it's now fully
root-caused with a proposed fix shape, not just flagged as unclear.
`CODEBASE_GUIDE.md` is now 59 sections. The "Active work claims" entry
for this batch has been replaced above with a new claim for the next
self-selected batch (camp/tent deployment, group loot/master-looter,
NPC lair spawning, combat special-attacks/DoT internals, travel/
shuttle tickets).

A thirteenth pass (same chat, read-only, no companion/source files
touched) worked through the fifth self-selected batch, adding
**sections 60-64 to `CODEBASE_GUIDE.md`**:
- **60. Camp/Tent Deployment System** -- **caught its own naming trap
  before writing anything**: this project has its own
  `server/zone/managers/companion/CampDeploymentManager.h` (a real
  companion-system file, "spec 4E," explicitly header-documented as
  modeled on but separate from the real engine system) -- confirmed
  via that file's own header comment and pivoted to the real system
  (`CampKitMenuComponent.cpp`) instead. Real camps are unconditionally
  banned inside cities, detect nearby NPC lairs via an
  observer-type scan rather than a dedicated lair-object check, and
  provide hospital-equivalent medical rating (cross-references section
  51).
- **61. Group Loot Distribution/Master Looter Rules** -- four loot
  rules (FreeForAll/MasterLooter/Lottery/Random) with genuinely
  different code per rule; credits always split evenly regardless of
  loot rule (only items are rule-gated); Lottery mode hard-locks every
  item via `ContainerPermissions` deny-rules (reusing section 37's ACL
  system) so nobody, including the eventual winner, can loot early.
- **62. NPC Lair Spawning System** -- entirely damage-reactive, not
  time-reactive (spawn waves gated on the lair's own condition-damage
  crossing thresholds, capped at 3 waves); a deliberate "milking"
  repopulation mechanic explicitly rewards farming the same lair
  location; `isDestroyMissionLairObserver()` flag suppresses respawn
  for mission-target lairs specifically.
- **63. Combat DoT Internals** -- two independent DoT sources
  (attack-defined vs. weapon-innate) with a **source-comment-confirmed
  resist gap**: only bleed has a real weapon-DoT resist mod wired up,
  poison/disease/fire apply unresisted via the weapon path. DoTs use a
  shared list-level `nextTick` hint rather than one scheduled Task per
  DoT; same-type reapplication is a strength comparison, not additive
  stacking; disease is hard-capped to one instance total via a
  synthetic shared key.
- **64. Travel/Shuttle Ticket System** -- ticket purchase UI is
  entirely client-native (server just sends a mode-switch opcode
  message); destination city ban list is checked independently from
  the departure city's; shuttle boarding dispatches through a raw
  opcode constant via `executeObjectControllerAction()` rather than a
  normal slash command, the escape hatch for triggering client-side
  built-in behavior with no server `QueueCommand` equivalent.

No companion source files touched this pass -- note the
`CampDeploymentManager` naming near-miss above, worth remembering if a
future pass goes looking for other "sounds-like-engine-code but is
actually ours" files before assuming something is general-engine
material. `CODEBASE_GUIDE.md` is now 64 sections. The "Active work
claims" entry for this batch has been replaced above with a new
open-ended claim -- this chat will keep self-selecting further general-
engine topics indefinitely per the user's standing instruction, adding
sections 65+ and a new dated pass-summary paragraph here after each
further batch, without pausing to ask what to research next.

A fourteenth pass (same chat, read-only, no companion/source files
touched) worked through a sixth self-selected batch, adding
**sections 65-69 to `CODEBASE_GUIDE.md`**:
- **65. Structure Power System** -- a third, independent upkeep
  resource alongside maintenance (section 54), but only for
  installations (harvesters/factories), fueled by literally consuming
  energy-type harvested resources from a player's inventory (weighted
  by that resource's rolled Potential Energy stat); unlike maintenance
  exhaustion, power exhaustion is soft/recoverable (pauses work) rather
  than triggering decay/destruction.
- **66. Static/Dynamic NPC Spawn System** -- `LairObserver` (section
  62) and `DynamicSpawnObserver` are sibling subclasses of one shared
  `SpawnObserver` base; dynamic spawns are respawn-count-limited (each
  creature gets exactly 3 respawns before the whole spawn point
  self-destructs) rather than damage-reactive like lairs, and
  natively support herd-coordinated group movement via
  `CreatureHerdObserver`.
- **67. Force Rank System (FRS) Council Arena** -- the real system
  behind `checkForArenaDuel()`, confirmed genuinely unrelated to both
  the formal duel system (section 56) and ordinary PvP. Rank
  promotion is a real zero-sum swap resolved by an actual deathblow
  inside a dedicated arena, not a simulated/abstracted contest -- three
  separate "structured PvP" systems now fully traced in this codebase,
  each independent.
- **68. Group/Combat XP Distribution Formula** -- deliberately
  structured opposite to loot distribution (section 61): XP is
  strictly damage-proportional per attacker from a shared `ThreatMap`,
  never pooled/split; grouping only adds a flat multiplier on top of
  each attacker's own share; group leaders earn a separate size-scaled
  bonus. **Surfaced a new, not-yet-fixed candidate instance of this
  project's own `isPet()`/`PetControlDevice`-cast bug family** in the
  creature-handler pet-XP branch -- added to the Backlog section below
  with a proposed fix following the exact shape already proven twice
  for the TEF and friendly-fire fixes.
- **69. Crafting Experimentation/Quality-Roll Formulas** -- assembly
  and experimentation share one nine-tier outcome formula shape
  (skill roll + city bonus + Force bonus + food buff + tool
  effectiveness, city-specialization bonus reusing section 34's
  `SkillModManager::CITY` mechanic); confirmed experimentation is a
  real risk/reward mechanic where bad rolls actively subtract quality,
  not merely a capped-at-zero-downside system; quality can never
  exceed the resource-quality-derived ceiling regardless of skill.

No companion source files touched this pass. One new, real (not yet
fixed) finding was added to the Backlog section above (section 68's
creature-handler XP `isPet()` gap) rather than fixed directly, per
this chat's read-only role. `CODEBASE_GUIDE.md` is now 69 sections.
The "Active work claims" entry for this batch has been replaced above
with a new open-ended claim for the next self-selected batch.

---
## 2026-07-15 — Cowork chat update (for all chats)
Latest work done in the Cowork (desktop) chat — details in NOTES.md under the two 2026-07-15 entries:
1. **ContainerImplementation.cpp** — corpse-loot gate now exempts same-creature moves (`&& otherParent != myParent`): fixes "You can not loot that" on Take From Companion after the retrieve-destination change to the companion's own bag. Pending rebuild+test.
2. **Trainer "( )" empty title root cause** — StringIdManager snapshots string/en STFs into the "strings" object DB once, ever; it never re-reads TREs. Fix is one boot with plain arg `reloadstrings` (gdb: `r reloadstrings`). NO code/lua/TRE change needed — do not "fix" the trainer lua for this.
3. **LuaSkill.cpp fully null-guarded** (all six methods) — trainer-conversation SIGSEGV on missing skills can't recur.
4. **Full disaster-recovery backup** created at C:\MasterCompanionServerBackUp (zip + RESTORE_INSTRUCTIONS.md). If your chat makes significant changes, remind Nick to refresh it.

---
## 2026-07-16 — Cowork chat: TAXI RIDER-FLIP landed (do not iterate on the mirror anymore)
The taxi/escort attachment problem is redesigned a third time, per direct user choice — full write-up in NOTES.md ("TAXI THIRD DESIGN"). Short version: a non-persistent driver agent (companion_actor.iff chassis, clientObjectCRC re-stamped to the owner's vehicle, OptionBitmask::VEHICLE+INVULNERABLE) does all the AI driving; the real companion is mounted on it as a native RIDER child (glue is engine-native now, zero mirroring); companion parked STAY/oblivious with home anchored to the driver per tick; mid-ride combat = instant dismount. The 200ms position mirror is GONE — please don't tune or resurrect it. Files: CompanionObjectImplementation.cpp, VehicleControlDeviceImplementation.cpp. No idl change. Pending the shared rebuild+test with everything else.
