# Companion Skill Mirror -- integration guide (2026-07-27, "skill mirror" pass)

Server-side wiring for owner-callable `/companion_<base>` proxy commands.
Two new headers (both header-only):

| File | Destination in repo |
|---|---|
| `CompanionSkillMirror.h` | `MMOCoreORB/src/server/zone/objects/companion/CompanionSkillMirror.h` |
| `CompanionSkillProxyCommand.h` | `MMOCoreORB/src/server/zone/objects/companion/commands/CompanionSkillProxyCommand.h` |

Plus four one-line-ish edits: the registration loop, an instantiation loop,
and three hook call sites, all below.

> **Stale-snapshot warning:** everything quoted below was verified against
> the `Companion-notes` snapshot. The live repo has a NEWER
> `CompanionSkillTrainer.cpp` (and `CompanionCraftingManager`), and another
> chat holds an **unapplied patch touching `CompanionSkillTrainer.cpp`'s
> `sendDialogMenu` and `CompanionDialogMenuSuiCallback.h`** -- do NOT touch
> those two regions. Hook 3 is deliberately specified as a single greppable
> line for that reason. Re-verify quoted context before pasting.

---

## 1. Registration loop -- `CommandConfigManager2.cpp`, `registerCommands2()`

Place directly after the existing companion ability block (the snapshot's
lines 827-872 end with the four starter-profession registrations):

```cpp
	commandFactory.registerCommand<CompanionAbilityCommand>(String("companionhealdamage").toLowerCase());
	commandFactory.registerCommand<CompanionAbilityCommand>(String("companionhealwound").toLowerCase());
	commandFactory.registerCommand<CompanionAbilityCommand>(String("companiontendwound").toLowerCase());
	commandFactory.registerCommand<CompanionAbilityCommand>(String("companiontenddamage").toLowerCase());
```

Add:

```cpp
	// Companion System (2026-07-27, "skill mirror" pass) -- one
	// CompanionSkillProxyCommand class registered under every mirrored
	// "companion_<base>" name (same one-class-many-names mechanic as the
	// CompanionAbilityCommand block above: CommandFactory.h:50-58 keys a
	// creator per NAME; each name later gets its own instance constructed
	// with that name, CommandFactory.h:32-35). The mirrored list lives in
	// ONE place -- CompanionSkillMirror::mirroredBaseCommands() (the
	// COMPANION-MIRROR-PHASE1 block, kept in sync with
	// build_command_table_rows.py). See CompanionSkillProxyCommand.h.
	{
		const Vector<String>& mirrorBases = CompanionSkillMirror::mirroredBaseCommands();

		for (int i = 0; i < mirrorBases.size(); ++i) {
			commandFactory.registerCommand<CompanionSkillProxyCommand>(CompanionSkillMirror::mirrorAbilityName(mirrorBases.get(i)));
		}
	}
```

And with the other companion includes (snapshot lines 366-380, after
`CompanionCraftCommand.h`):

```cpp
#include "server/zone/objects/companion/commands/CompanionSkillProxyCommand.h" // Companion System (2026-07-27, "skill mirror" pass)
```

(`CompanionSkillMirror.h` is pulled in transitively by the proxy header;
`mirrorAbilityName()` returns the already-lowercase `companion_<base>`
factory key, so no extra `.toLowerCase()` is needed.)

## 2. Instantiation loop -- `CommandConfigManager.cpp`, end of `registerSpecialCommands()`

**Why this second loop exists:** `registerCommand<>()` only registers a
creator function. A command only becomes *executable* when
`createCommand(name)` instantiates it into the live `slashCommands` list
(`CommandConfigManager.cpp:322-337`) -- which happens either from a
server-side `command_table.iff` row (`loadCommandData()`,
`CommandConfigManager.cpp:155-161`) or from an explicit call in
`registerSpecialCommands()` (exactly how `/logout` and all the pet orders
are made live without table rows, `CommandConfigManager.cpp:350-395`).
The existing 40 `companion<ability>` commands rely on the project's
server-side TRE rows; this loop makes `/companion_<base>` live **before**
any TRE change, satisfying the test plan below.

Append at the end of `registerSpecialCommands()` (snapshot: after the
`petGetPatrolPoint` line, ~line 396, before the closing brace):

```cpp
	// Companion System (2026-07-27, "skill mirror" pass) -- instantiate
	// every mirrored companion_<base> proxy exactly the way /logout and
	// the pet orders are made live without a command_table row (see the
	// createCommand() calls above). Guarded so that once the generated
	// server-side command_table rows land, the row-driven instance (with
	// its characterAbility gate and state masks) is never clobbered.
	{
		const Vector<String>& mirrorBases = CompanionSkillMirror::mirroredBaseCommands();

		for (int i = 0; i < mirrorBases.size(); ++i) {
			String mirrorName = CompanionSkillMirror::mirrorAbilityName(mirrorBases.get(i));

			if (slashCommands->getSlashCommand(mirrorName) == nullptr) {
				createCommand(mirrorName);
			}
		}
	}
```

Include needed at the top of `CommandConfigManager.cpp`:

```cpp
#include "server/zone/objects/companion/CompanionSkillMirror.h" // Companion System (2026-07-27, "skill mirror" pass)
```

Ordering note: `registerSpecialCommands()` runs after the constructor
(which runs `registerCommands2()`, so the factory creators exist) and the
guard keeps this loop correct whether it runs before or after
`loadSlashCommandsFile()`. Pre-TRE these instances have no
`characterAbility` gate -- harmless, because the proxy itself refuses
unless a summoned companion genuinely knows the base command.

## 3. Hook call sites

### Hook A -- grant on summon: `CompanionControlDeviceImplementation::spawnObject()`

Lock context: runs inside the `callObject()` deferred task holding
`Locker(player)` FIRST, then the device and companion cross-locked against
the player (`CompanionControlDeviceImplementation.cpp:116-145`) -- owner
already locked, so `grantFor()` runs inline, no new locks (iron rule
satisfied).

Quoted context (snapshot ~line 704-713, the spawnObject success tail):

```cpp
	// Attach the owner-status threat observer (spec 4A) so the companion can
	// auto-intercept attackers while idle/passive.
	if (threatObserver == nullptr) {
		threatObserver = new CompanionThreatObserver(companion);
	}

	player->registerObserver(ObserverEventType::DAMAGERECEIVED, threatObserver);
	player->registerObserver(ObserverEventType::STARTCOMBAT, threatObserver);

	player->sendSystemMessage("@companion:summoned"); // Your companion has been summoned.
```

Insert immediately after the `@companion:summoned` line:

```cpp
	CompanionSkillMirror::grantFor(companion, player); // Companion System (2026-07-27, "skill mirror" pass) -- see CompanionSkillMirror.h
```

### Hook B -- revoke on store: `CompanionControlDeviceImplementation::storeObject()`

Same deferred-task lock context as Hook A (owner locked first). The
`revokeFor()` still-justified scan cross-locks other summoned companions
per-iteration as `Locker(other, player)` -- owner lock held first, per the
iron rule.

Quoted context (snapshot ~line 774-781, the storeObject tail):

```cpp
	companion->destroyObjectFromWorld(true);

	if (companionGroup != nullptr) {
		GroupManager::instance()->leaveGroup(companionGroup, companion);
	}

	player->sendSystemMessage("@companion:stored"); // Your companion has been stored.
```

Insert immediately before the `@companion:stored` line:

```cpp
	CompanionSkillMirror::revokeFor(companion, player); // Companion System (2026-07-27, "skill mirror" pass) -- see CompanionSkillMirror.h
```

(The just-stored companion is excluded from the "still justified" scan by
pointer AND by its now-null zone, so placement after
`destroyObjectFromWorld()` is safe.)

Include needed at the top of `CompanionControlDeviceImplementation.cpp`
(with the other companion includes, snapshot lines 8-26):

```cpp
#include "server/zone/objects/companion/CompanionSkillMirror.h"
```

### Hook C -- grant on mid-summon training: `CompanionSkillTrainer::trainSkill()`

**Not quotable from this snapshot** -- `CompanionSkillTrainer.cpp` is
absent from the uploaded copy and the live file is newer, with another
chat's unapplied patch nearby (`sendDialogMenu` +
`CompanionDialogMenuSuiCallback.h` -- keep out of both regions). The hook
is a single line at a well-defined anchor:

In `trainSkill()`'s SUCCESS path, immediately AFTER its existing
`grantOwnerAbilitiesForSkill(...)` call (the 2026-07-13 "macro list" pass
call site -- find it with:

```
grep -n "grantOwnerAbilitiesForSkill" src/server/zone/managers/companion/CompanionSkillTrainer.cpp
```

pick the occurrence inside `trainSkill()`, not the starter-profession
callback), add:

```cpp
	CompanionSkillMirror::grantFor(companion, owner); // Companion System (2026-07-27, "skill mirror" pass) -- see CompanionSkillMirror.h
```

Adjust the two variable names to whatever `trainSkill()` locally calls the
companion and the owning player. Lock context: same SUI-callback context in
which `grantOwnerAbilitiesForSkill()` already mutates the owner's
abilityList -- owner locked, no new locks taken. Add
`#include "server/zone/objects/companion/CompanionSkillMirror.h"` to
`CompanionSkillTrainer.cpp` (NOT the .h, to keep the trainer header
untouched).

## 4. CMake / build note

Both new files are header-only: **no CMake reconfigure needed** -- Core3's
build globs `.cpp` files and headers ride along via includes. The touched
`.cpp` files (`CommandConfigManager.cpp`, `CommandConfigManager2.cpp`,
`CompanionControlDeviceImplementation.cpp`, `CompanionSkillTrainer.cpp`)
are already in the build. No `.idl` changed, so no idlc regeneration.

```
cd /mnt/c/Companion/Core3/MMOCoreORB
ninja -C build-ninja-debug
```

then restart the server.

## 5. Test plan (all BEFORE any TRE change -- browser entries come later)

Typed slash commands reach the server without client table rows (same
mechanism the pre-existing row-less special commands rely on); the browser
UI is the only part gated on the future TRE rows. If a typed
`/companion_headshot1` produces a client-side "unknown command" error
instead of reaching the server, fall back to a macro
(`/macro test /companion_headshot1`) or verify via a pre-existing
row-backed command first -- and note the finding in NOTES.md.

1. **Registration sanity:** server boots clean; log shows no
   "Could not create command" for any `companion_*` name.
2. **No companion out:** `/companion_headshot1` ->
   "You have no active companion."
3. **Companion out, doesn't know it:** summon a companion without the
   rifleman skill that grants `headShot1` -> "None of your companions
   know that."
4. **Grant on summon:** summon a companion whose learnedSkills include a
   phase-1 base -> "New companion commands available." system message;
   `/companion_<thatbase>` with a hostile target selected -> the
   *companion* performs the real ability (animation, HAM cost, damage --
   the engine's own pipeline).
5. **Performer selection:** with two summoned companions where only one
   knows the base, target the other one -> the knower still performs
   (nearest-knower fallback). Target the knower directly -> it performs.
6. **State skip:** incapacitate the only knower -> "Your companions are in
   no condition to do that."
7. **Revoke on store:** store the knower -> `/companion_<base>` now says
   "None of your companions know that."; with a second summoned knower of
   the same base still out, the grant survives the first store.
8. **Mid-summon training:** train a skill granting a phase-1 base while
   summoned -> "New companion commands available." fires and the command
   works immediately, no relog (live PLAY9 delta,
   `PlayerObjectImplementation.cpp:985-1017`).

## 6. Decision flag -- gate-string overlap with the 2026-07-13 macro-list pass

The PHASE1 list deliberately contains no base command the old pass's 61
`companion<Ability>` rows already mirror (that de-duplication is baked
into both PHASE1 blocks). But the ability STRINGS can still collide:
`CompanionSkillTrainer::grantOwnerAbilitiesForSkill()` grants
`companion_<ability>` **permanently at train time for EVERY ability of the
trained skill** (NOTES.md, 2026-07-13 entry) -- so if a trained skill
grants e.g. `headShot1`, the owner already holds `companion_headshot1`
before the mirror ever runs. Consequence of `revokeFor()` at store: that
string becomes **summon-scoped** from then on (store -> removed -> next
summon or next training re-grants). Since only the new
`/companion_<base>` commands (and their future client rows) gate on these
particular strings, the practical effect is exactly the intended
"callable only while a knowing companion is summoned" semantics -- but it
does mean the trainer's "permanent grant" bookkeeping and the mirror now
share strings.

- **Recommended: accept it** -- the summon-scoping is the feature, the
  overlap is self-healing (`grantFor()` on every summon and after every
  training), and both surfaces stay on the one agreed string convention
  (`companion_<base.lower()>`, matching build_companion_skill_mirror.py's
  characterAbility).
- Alternative: distinct strings (e.g. `companionmirror_<base>`) leave the
  trainer's grants untouched -- but diverge from the client-generator
  contract and double-book the gate namespace.

If accepted, append the finding to `docs/companion_system/NOTES.md` and
refresh backups after the change lands.
