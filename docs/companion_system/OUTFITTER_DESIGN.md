# Companion Outfitter System — Design (locked 2026-07-23)

Decisions made by Nick 2026-07-23 (Cowork desktop chat). Status: DESIGN ONLY — not started.
Applies to: armorsmith + weaponsmith companions. One shared system, two specializations.

## Locked decisions
1. **Gear source**: curated, tiered template lists keyed to the smith's skill level. The smith
   CRAFTS each needed piece on demand — crafting animation + timer (10–30s per piece), NO
   resource consumption. Time + choreography are the cost.
2. **Triggers**: event-driven — (a) companion joins the group, (b) a companion finishes
   training and gains new weapon certs, (c) the smith itself reaches a new tier,
   (d) manual "Outfit the group" command. No periodic polling.
3. **Old gear**: replaced pieces stay in the RECIPIENT companion's own pack (backup set).
4. **Player requests own armor suit**: real conversation window (ConversationTemplate) —
   greeting → "Build me a full suit" → armor-type options at smith's tier → confirm.
5. **Weaponsmith multi-option rule**: if several newly-unlocked weapons qualify, pick at random.

## Data model
- `OutfitterTiers` Lua table per profession: tier N → { armorType → {slot → templatePath} }
  for armorsmith; tier N → { weaponCert → templatePath(s) } for weaponsmith.
- Each companion tracks `gearTier` per equip slot (and current weapon tier). Persist with the
  companion (same mechanism as other per-companion persistent state — research chats: confirm
  where per-companion vars live and that they survive server restart).
- "Needs gear" = empty slot OR equipped tier < smith's best craftable tier.
  NO stat-comparison of arbitrary items — tier ints only. This is what makes
  "replace lower-end armor even if they have some equipped" reliable.

## Outfitting round — state machine (runs on the smith)
ASSESS → build queue of group companions in range (~30m / same cell) that need gear.
  Smith outfits ITSELF first, instantly (no walking, no queue slot).
SUMMON → current recipient pathfinds to a point ~2m in front of smith; remaining queue
  members take offset positions behind it (single-file line, ~1.5m spacing).
CRAFT → smith plays crafting animation/pose for craftTime × piecesNeeded (cap total,
  e.g. 60s for a full suit so rounds don't drag). Chat bark: "Give me a minute…"
HANDOFF → both play give/take animation, bark ("Try this on."), transfer objects to
  recipient inventory. Old pieces are unequipped into recipient's pack first.
EQUIP → recipient auto-equips (visual equip rendering already works). Advance queue.
Repeat SUMMON for next in line. Round ends when queue empty.

Interrupts: combat PAUSES the round (resume after); recipient despawn/dismiss drops it
from the queue; smith despawn cancels the round cleanly.

## Events
- onJoinGroup(companion): each smith in group assesses just that companion.
- onTrainingComplete(companion, skillBox): if skillBox grants weapon certs, weaponsmith
  offers a weapon from the newly-unlocked set (random if multiple). Research chats:
  confirm how to enumerate certs granted by a skill box in Core3.
- onSmithTierUp(smith): full round — everyone below the new tier qualifies
  ("better armor available, even if lower-end equipped").
- Manual: "Outfit the group" via smith radial + convo option.
- Per-recipient cooldown (~5 min) so overlapping events can't loop rounds forever.

## Player-facing convo (armorsmith)
Screen 1: greeting + "Build me a full suit of armor" (only if smith idle & not in combat).
Screen 2: armor types available at smith's tier (padded / bone / ubese / composite / …
  per tier table). Screen 3: confirm → smith crafts (same CRAFT choreography) → pieces
  delivered to PLAYER INVENTORY (player equips themselves — no force-equip on players).
Weaponsmith convo mirrors it: "What weapons can you make me?" → cert-filtered list.

## Edge cases (day-one)
- Smith needing own gear: self-equips first, skips choreography.
- Range gate ~30m/same cell; out-of-range companions skipped, not summoned across the map.
- Two smiths of same type in group: highest tier runs the round; other stands down.
- Player mounted/moving: round proceeds — it's companion↔companion, player not needed.
- Companion pack full of old backups: oldest backup piece deleted when pack slot needed
  (or research: cap backups at 1 set per companion).

## Research-chat questions (c3r / c3rr)
1. Animation/posture names: crafting pose (e.g. kneel + object usage), give/take item
   animation pair for the handoff.
2. Best movement call for "walk to point near another companion" + arrival detection
   (setDestination? patrol point? existing escort keep-up monitor code may be reusable).
3. Where per-companion persistent vars live (gearTier per slot) + restart survival.
4. Registering a ConversationTemplate on dynamically-spawned companion NPCs — how the
   trainer services menu did it, reuse that path.
5. Enumerating weapon certs granted by a skill box (for onTrainingComplete).
6. Confirm transferObject → wear sequence used by existing visual-equip code is callable
   for smith→companion handoff without a player transaction.

## Iron-rule reminders for implementation
- Header-only additions preferred (new .cpp = cmake reconfigure).
- No autogen/ edits; .idl edits to existing files regen automatically.
- If any new client strings: STF/string file changes need one boot with `r reloadstrings`.
