/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-20, "crafting theater" pass, per user request)
	-- lets an owned CompanionObject actually craft a real item end to end:
	pick a real DraftSchematic, fill its real ResourceSlot/ComponentSlot
	ingredient slots with real resources/components gathered on the owner's
	behalf, assemble deterministically (no experimentation minigame -- see
	below), and hand the finished TangibleObject to the OWNER's inventory.

	Why "headless": the real player crafting flow (CraftingSession,
	RequestCraftingSessionCommand) is built entirely around a live client --
	RequestCraftingSessionCommand hard-blocks anything that isn't
	isPlayerCreature(), and CraftingSession itself requires a PlayerObject
	"ghost" (crafterGhost) that an AiAgent/CompanionObject never has. The
	experimentation minigame specifically is 100% SUI/ObjectControllerMessage
	round-trips with no server-only code path to reuse at all.

	This manager sidesteps the session/SUI layer entirely and drives the
	underlying, already-generic machinery directly:
	  - DraftSchematicImplementation::createManufactureSchematic() and
	    ManufactureSchematicImplementation::addIngredientToSlot() take a
	    plain CreatureObject* and never check isPlayerCreature() -- confirmed
	    safe for an AiAgent (CreatureObjectImplementation::sendMessage() is a
	    harmless no-op when there's no client, i.e. owner.get() == nullptr,
	    which is exactly a companion's normal state).
	  - initializeIngredientSlots() is private in ManufactureSchematic.idl
	    (normally only reached via synchronizedUIListen(), which itself
	    early-returns for non-players) -- see the new public
	    initializeSlotsForHeadlessCraft() wrapper added to
	    ManufactureSchematic.idl/.cpp specifically for this caller.
	  - "Auto-craft, deterministic" (explicit user decision, in place of
	    simulating the experimentation minigame server-side): assembly is
	    always resolved as CraftingManager::GREATSUCCESS -- the best base
	    assembly tier -- via the same craftingManager->setInitialCraftingValues()
	    call GenerateCraftedItemCommand.h (an existing admin/debug item
	    generator) already uses. Real per-resource quality is NOT thrown
	    away by this shortcut: SharedLabratory/ResourceLabratory compute the
	    prototype's actual stats FROM the resource stacks physically sitting
	    in each filled ResourceSlot (ResourceSlot::getCurrentSpawn()->
	    getValueOf()), so genuinely better resources still produce a
	    genuinely better item -- only the luck-roll/experimentation-point
	    layer is skipped, not the resource-quality layer.

	Resource acquisition chain (per user decision -- "use the resource that
	will make it so the item is the best it can be with the resources we
	have"), tried in order per resource slot:
	  1. Owner's own inventory (existing ResourceContainer stacks).
	  2. Companion's own inventory (in case a prior step already staged
	     something there).
	  3. Withdrawal from a HarvesterObject the owner actually owns
	     (getOwnerObjectID() match) with an active spawn matching the
	     slot's resource class, via ResourceContainerImplementation::
	     split(int, CreatureObject*) -- confirmed player-agnostic (only the
	     *command* wrapper, ResourceContainerSplitCommand, is player-gated;
	     the underlying impl method is not).
	  4. Claiming a real ResourceDeed the owner is holding (the actual
	     in-game "veteran free resource" item -- NOT a harvester placement
	     deed, which would require the companion to pick legal world
	     coordinates and pass StructureManager's placement/permission
	     checks; that remains an explicit, unbuilt stretch goal). Consumes
	     the deed exactly like ResourceDeedSuiCallback::run() does
	     (destroyDeed() + ResourceManager::givePlayerResource()), just
	     without the SuiListBox -- resolved via
	     ResourceManager::getCurrentSpawn(resourceClass, zoneName) instead
	     of a player's manual pick.

	When multiple candidate resource stacks exist at any step, the one
	scoring highest against the schematic's own real DraftSchematic::
	getResourceWeight(slotIndex) property weights (ResourceSpawn::getValueOf())
	is preferred -- so the companion always reaches for the better resource
	when it has a choice, using the schematic's own real weighting instead of
	an invented heuristic.

	Component slots (a sub-item from a different profession) are checked
	against owner+companion inventory only in this pass -- if missing, the
	craft fails with a clear "missing component: <template>" message rather
	than silently doing nothing. Automatically sourcing that component from
	a SECOND companion's own crafting is handled by findCrafterForComponent()
	below (2026-07-24 pass) -- see NOTES.md.
*/

#ifndef COMPANIONCRAFTINGMANAGER_H_
#define COMPANIONCRAFTINGMANAGER_H_

#include "engine/engine.h"

namespace server {
namespace zone {
	class ZoneServer;
namespace objects {
namespace creature {
	class CreatureObject;
}
namespace companion {
	class CompanionObject;
}
namespace tangible {
	class TangibleObject;
}
namespace draftschematic {
	class DraftSchematic;
}
namespace manufactureschematic {
	class ManufactureSchematic;
}
namespace resource {
	class ResourceContainer;
	class ResourceSpawn;
}
}
}
}

using namespace server::zone::objects::creature;
using namespace server::zone::objects::companion;
using namespace server::zone::objects::tangible;
using namespace server::zone::objects::draftschematic;
using namespace server::zone::objects::manufactureschematic;
using namespace server::zone::objects::resource;

namespace server {
namespace zone {
namespace managers {
namespace companion {

class CompanionCraftingManager : public Singleton<CompanionCraftingManager>, public Logger, public Object {
public:
	CompanionCraftingManager();

	/**
	 * Full headless craft: owner-supplied companion crafts draftSchematicTemplate
	 * (server script path, e.g. "object/draft_schematic/weapon/...iff", same
	 * convention GenerateCraftedItemCommand.h accepts) using resources gathered
	 * per the acquisition chain documented above, and delivers the finished
	 * item to owner's inventory.
	 *
	 * @param owner the player who will receive the finished item and whose
	 *   inventory/harvesters/deeds are drawn from.
	 * @param companion the companion actually "doing" the crafting (crafter
	 *   name on the finished item, flavor only).
	 * @param draftSchematicTemplate server script path to the DraftSchematic.
	 * @param errorMessage set to a human-readable failure reason on false.
	 * @returns true if a finished item was created and delivered.
	 * @pre { neither owner nor companion locked }
	 * @param isSubComponentCraft (2026-07-28, quality fix) true when this is
	 *   a recursive sub-component craft (fillComponentSlot()) rather than the
	 *   top-level item the player asked for -- floors the assembly roll at
	 *   MODERATESUCCESS so one unlucky sub-roll deep in a recipe can't
	 *   silently wreck the whole item. Defaults to false (unchanged behavior
	 *   for every existing top-level call site).
	 * @param awardXp (2026-07-28, XP-parity REV 2 farming fix) whether this
	 *   specific craftItem() call is allowed to award the companion its
	 *   crafting XP on success. Defaults to true (unchanged behavior for a
	 *   real top-level craftItem() call and any sub-component craft it
	 *   triggers). fillComponentSlot() passes false through here whenever
	 *   ITS call was itself reached from a craftBatch() run, so a factory
	 *   run's auto-crafted sub-components never earn XP -- see
	 *   fillIngredientSlot()'s/fillComponentSlot()'s own doc comments for
	 *   the full thread.
	 */
	bool craftItem(CreatureObject* owner, CompanionObject* companion, const String& draftSchematicTemplate, String& errorMessage, bool isSubComponentCraft = false, bool awardXp = true) const;

	/** Companion System (2026-07-27, "factory runs" per Nick's request "we
	 * need to make it so we can make factory runs of items"). Real stock SWG
	 * never re-rolls quality per unit -- a factory crafts ONE prototype for
	 * real, then clones it. This does the same: one full headless craft
	 * (ingredient slots filled at quantity-times normal, drawn up front, same
	 * "hopper" semantics -- a shortfall fails cleanly before anything is
	 * half-consumed) producing ONE real prototype, then packages it into a
	 * real FactoryCrate via TangibleObject::createFactoryCrate(insertSelf=true)
	 * + setUseCount(actualQuantity). Refuses schematics with no factory crate
	 * defined (DraftSchematic::getFactoryCrateSize() <= 0 -- the same real
	 * stock gate ManufactureSchematicImplementation::allowFactoryRun() uses);
	 * silently caps the requested quantity at the schematic's own crate
	 * capacity rather than only partially fulfilling a larger request with no
	 * explanation (errorMessage carries the cap note back to the caller when
	 * this happens, even though this method itself still returns true).
	 * @param quantity requested run size (>= 1).
	 * @param errorMessage set to a human-readable failure reason on false, or
	 *   a non-empty informational note (still returns true) if the requested
	 *   quantity was capped down to the item's real crate size.
	 * @returns true if a factory crate was created and delivered.
	 * @pre { neither owner nor companion locked }
	 */
	bool craftBatch(CreatureObject* owner, CompanionObject* companion, const String& draftSchematicTemplate, int quantity, String& errorMessage) const;

private:
	/** Attempts to fully fill ingredient slot slotIndex (resource or component)
	 * using the acquisition chain. Returns false (and sets errorMessage) if the
	 * slot could not be filled. quantityMultiplier (2026-07-27, "factory runs"
	 * pass) scales the slot's normal per-unit quantity for a batch run --
	 * default 1 leaves every existing single-craft call site (craftItem())
	 * byte-for-byte unaffected. fromBatchRun (2026-07-28, XP-parity REV 2
	 * farming fix) is false by default (craftItem()'s own call site leaves it
	 * unset); craftBatch() passes true at its own call site so any
	 * sub-component craft this slot fill triggers (via fillComponentSlot())
	 * is told not to award the companion crafting XP. */
	bool fillIngredientSlot(CreatureObject* owner, CompanionObject* companion, ManufactureSchematic* manufactureSchematic, DraftSchematic* draftSchematic, int slotIndex, String& errorMessage, int quantityMultiplier = 1, bool fromBatchRun = false) const;

	/** Companion System (2026-07-28, wrong-slot resource steal fix): returns
	 * the manufacture schematic's slot indexes in the order they should be
	 * filled -- every resource slot first (deepest/most-specific resource
	 * class first, a stable sort), then every non-resource slot in its
	 * original order. See the .cpp for the full bug writeup. */
	Vector<int> buildSpecificFirstSlotFillOrder(ZoneServer* zoneServer, ManufactureSchematic* manufactureSchematic, DraftSchematic* draftSchematic) const;

	/** Resource-slot specific fill: inventory -> harvester -> resource deed. */
	bool fillResourceSlot(CreatureObject* owner, CompanionObject* companion, ManufactureSchematic* manufactureSchematic, DraftSchematic* draftSchematic, int slotIndex, const String& resourceClass, int quantityNeeded, String& errorMessage) const;

	/** Component-slot fill: inventory first, then CRAFT the sub-component if
	 * a schematic the companion knows produces it (2026-07-20, user request
	 * "make those crafting steps if there is a pre-required item"). fromBatchRun
	 * (2026-07-28, XP-parity REV 2 farming fix, default false) is threaded
	 * straight through from fillIngredientSlot() and forwarded as
	 * `awardXp = !fromBatchRun` on this method's own recursive craftItem()
	 * calls, so a craftBatch()-triggered sub-craft never earns companion XP
	 * while a real craftItem()-triggered sub-craft still does. */
	bool fillComponentSlot(CreatureObject* owner, CompanionObject* companion, ManufactureSchematic* manufactureSchematic, const String& componentTemplate, int quantityNeeded, int slotIndex, String& errorMessage, bool fromBatchRun = false) const;

	/** Reverse lookup (2026-07-20): the draft-schematic template path the
	 * companion knows that PRODUCES componentTemplate (matched by the
	 * schematic's getTanoCRC()), or empty if none. */
	String findSchematicForComponent(CompanionObject* companion, const String& componentTemplate) const;

	/** Companion System (2026-07-24, "multi-companion collaboration" pass).
	 * Cross-profession components (e.g. an Armorsmith's composite armor
	 * needing Tailor-made synthetic cloth) previously failed the whole craft
	 * even when a squadmate with the right skill was standing right there --
	 * findSchematicForComponent() only ever checked the ONE companion doing
	 * the craft. This tries primaryCompanion first (unchanged behavior/
	 * priority), then falls back to scanning the owner's other active
	 * companions (same datapad-scan pattern CompanionCraftTheater::begin()
	 * uses for resource trades). Returns the companion that can actually
	 * make it (may be primaryCompanion itself) with producerPath set, or
	 * nullptr with producerPath empty if nobody in the squad knows a
	 * schematic for it. */
	CompanionObject* findCrafterForComponent(CreatureObject* owner, CompanionObject* primaryCompanion, const String& componentTemplate, String& producerPath) const;

	/** Recursion depth guard for sub-component crafting -- prevents an
	 * infinite loop if two components require each other. */
	mutable int craftDepth = 0;

	/** Scores a candidate ResourceSpawn against the schematic's real resource
	 * weight table for this slot (higher is better). Falls back to 0 (neutral)
	 * if the schematic defines no weights for this slot. */
	float scoreResourceSpawn(DraftSchematic* draftSchematic, int slotIndex, ResourceSpawn* spawn) const;

	/** Withdraws up to quantityNeeded units of matching resource from any
	 * Harvester the owner actually owns within range, staging it in the
	 * companion's own inventory. Returns the number of units actually
	 * withdrawn (0 if no matching harvester/insufficient hopper contents). */
	/** requiredSpawn (2026-07-27 FIX): when set, only withdraws units of
	 * this EXACT spawn -- used once something is already committed to the
	 * target slot, since a slot can never mix two different spawns even of
	 * the same class. nullptr (default) keeps the original "best of class"
	 * behavior for an empty slot. */
	int withdrawFromOwnerHarvester(CreatureObject* owner, CompanionObject* companion, const String& resourceClass, int quantityNeeded, ResourceSpawn* requiredSpawn = nullptr) const;

public:
	/** Finds and consumes one real ResourceDeed for resourceClass (owner
	 * inventory, companion, companion bag, one level into crates), granting
	 * a fresh best-quality stack directly into companion's inventory.
	 * Returns true on success. PUBLIC since 2026-07-20: the fireworks
	 * show's craft-your-own fallback (CompanionFireworksShow.h) reuses it.
	 * 2026-07-20 optimization pass: when a schematic is supplied, the deed
	 * claims the resource that maximizes the owner's PREFERRED experimental
	 * line for that schematic (BER over hopper size, etc.) via
	 * ResourceManager::getBestSpawnOfTypeWeighted(); no schematic/no
	 * preference falls back to generic best-quality. */
	/** requiredSpawn (2026-07-27 FIX, same reasoning as
	 * withdrawFromOwnerHarvester() above): when set, the deed is claimed
	 * directly against this EXACT spawn instead of "best of class," so it
	 * can actually merge into a slot that already has this spawn
	 * committed. nullptr (default) keeps today's "best of class" pick. */
	bool claimResourceDeedForClass(CreatureObject* owner, CompanionObject* companion, const String& resourceClass, DraftSchematic* draftSchematic = nullptr, ResourceSpawn* requiredSpawn = nullptr) const;

	/** Companion System (2026-07-24, "test resources" radial, per user
	 * request "can I have a radial that requests the best resources for a
	 * certain item, so I can test what the companions are using -- place a
	 * bag in the user's inventory and fill it with the same resources the
	 * companion used to craft the same item, so we can compare"):
	 *
	 * Non-destructive, no-deed-consumed tool. For EVERY resource slot in
	 * draftSchematicTemplate, resolves the best currently-spawned resource
	 * using the exact same schematic-weighted selection fillResourceSlot()/
	 * claimResourceDeedForClass() already use (getBestSpawnOfTypeWeighted()
	 * keyed on that slot's own index, falling back to getBestSpawnOfType()
	 * if the schematic defines no weight for that slot), then grants a real
	 * 30,000-unit stack of it (ResourceManager::givePlayerResource() -- same
	 * call the real deed-claim flow uses) and moves that exact stack into a
	 * freshly created bag in the owner's inventory (matched by ResourceSpawn
	 * pointer identity, since givePlayerResource() has no container-target
	 * overload and always lands the stack in the target creature's own
	 * top-level inventory first).
	 *
	 * Component slots are skipped entirely -- this is a resource-quality
	 * comparison tool, not a full ingredient kit. Doesn't touch the
	 * companion, any real resource deed, or any of the owner's existing
	 * resource stacks.
	 *
	 * 2026-07-24 FIX (live report: "I used a different metal I already had
	 * and got a BETTER outcome than your bag"): the first version of this
	 * only ever compared candidates from getBestSpawnOfTypeWeighted() --
	 * i.e. "best of what's CURRENTLY actively spawning galaxy-wide", the
	 * same scope a fresh resource-deed claim has. It never looked at what
	 * the owner/companion already had BANKED in inventory (a stack from an
	 * earlier, since-rotated spawn can absolutely outscore anything
	 * currently spawning). That's also the real explanation for why the
	 * companion's actual crafts didn't visibly improve after the deed-
	 * weighting fix: fillResourceSlot()'s tier-0 (existing inventory) scan
	 * already satisfies a slot's QUANTITY need and stops there -- it never
	 * escalates tiers for QUALITY, only for shortfall -- so a mediocre
	 * already-owned stack silently wins over a better fresh deed claim
	 * every time there's already "enough" of it on hand. This method now
	 * scores owner+companion inventory candidates (scoreResourceSpawn(),
	 * same formula fillResourceSlot() uses) AND the best currently-
	 * spawning candidate, and grants a fresh stack of whichever actually
	 * scores highest -- so the test bag reflects the TRUE best resource
	 * for this schematic slot, not just "best of what a deed could claim".
	 *
	 * @param owner the player who will receive the test bag.
	 * @param companion the companion whose inventory is also checked (may
	 *   be nullptr to skip that check and consider only currently-spawning
	 *   resources plus the owner's own inventory).
	 * @param draftSchematicTemplate server script path to the DraftSchematic.
	 * @param errorMessage set to a human-readable failure reason on false.
	 * @returns true if the bag was created and at least one resource slot
	 *   was filled.
	 */
	bool giveTestResourceBag(CreatureObject* owner, CompanionObject* companion, const String& draftSchematicTemplate, String& errorMessage) const;

	/** Companion System (2026-07-20, "optimize for BER" pass -- see
	 * NOTES.md): remembered per-player, per-schematic experimentation-line
	 * preference (index into DraftSchematic::getResourceWeight()).
	 * In-memory only -- resets on server restart, re-asked on next craft. */
	int getPreferredLine(uint64 playerID, uint32 schematicCRC) const;
	void setPreferredLine(uint64 playerID, uint32 schematicCRC, int lineIndex);

	/** Companion System (2026-07-20, "same serial number" pass -- see
	 * NOTES.md): stamps every TangibleObject in `components` with one
	 * shared factory serial number, so a schematic that requires several
	 * components "from the exact same factory run" (identical serial)
	 * accepts them. The serial is generated once from the first component's
	 * template + a time seed. For use by the component-crafting path when
	 * it produces a batch of factory components for one item. */
	void stampSharedSerial(const Vector<ManagedReference<TangibleObject*> >& components) const;

private:
	mutable VectorMap<String, int> preferredExperimentalLines;

	/** Companion System (2026-07-28, quality/experimentation fix -- see
	 * NOTES.md): CompanionObjectImplementation::grantSkill() only ever
	 * appends to learnedSkills and never calls addSkillMod(), so a
	 * companion's real skill mods (CreatureObject::getSkillMod()) never
	 * reflect what it has actually trained -- only WEARABLE mods from
	 * equipped gear are real. calculateAssemblySuccess()/
	 * calculateExperimentationSuccess()/FailureRate() all read
	 * player->getSkillMod() directly and take a plain CreatureObject*, so
	 * this sums (name -> total value) every skill modifier granted by
	 * every skill in learnedSkills (Skill::getSkillModifiers(), via
	 * SkillManager::getSkill()) WITHOUT touching learnedSkills or calling
	 * addSkill() -- purely a crafting-roll-time lookup. */
	VectorMap<String, int> buildEffectiveCraftingSkillMods(CompanionObject* companion, ZoneServer* zoneServer) const;

	/** Applies (sign=1) or reverts (sign=-1) the skill mods computed by
	 * buildEffectiveCraftingSkillMods(), via the same
	 * CreatureObject::addSkillMod(SkillModManager::SKILLBOX, ...) real
	 * skill training already uses (SkillManager.cpp) -- scoped to the
	 * duration of one craft's assembly+experimentation rolls only.
	 * Applying then reverting the identical map nets to zero: no
	 * permanent skill-mod or combat-balance change survives past the
	 * craft that requested it. */
	void applyEffectiveCraftingSkillMods(CompanionObject* companion, const VectorMap<String, int>& mods, int sign) const;

	/** Human-readable label for a CraftingManager::AMAZINGSUCCESS..
	 * CRITICALFAILURE tier constant, for roll-tier chat messaging (risk
	 * mitigation -- variance should read as real gameplay feedback, not a
	 * silent/invisible bug). */
	String craftingResultTierName(int tier) const;

	/** Companion System (2026-07-29, real-roll hardening): CraftingManagerImplementation's internal
	 * `labs` HashTable only ever registers RESOURCE_LAB(0)/GENETIC_LAB(1)/
	 * DROID_LAB(2) and has no null check of its own before dereferencing
	 * the looked-up SharedLabratory* -- a null `lab` would crash the whole
	 * zone thread. DraftSchematicObjectTemplate::LabType currently only
	 * defines those same 3 values, so this bounds check is a defensive
	 * guard against a future/malformed schematic reporting an
	 * out-of-range labratory id, not a currently-reachable case. */
	bool isRegisteredLabratory(int labratoryType) const;

	/** Companion System (2026-07-29, real-roll hardening, owner food-buff fold-in): a companion can't
	 * personally eat a Pyollian Cake, but the OWNER might have --
	 * SharedLabratory::calculateAssemblySuccess()/
	 * CraftingManagerImplementation::calculateExperimentationSuccess()
	 * both read player->hasBuff(buffCRC) directly off the CreatureObject*
	 * passed in for the roll (the companion), so the owner's real food
	 * buff would otherwise be invisible. Both stock functions apply their
	 * buff bonus the same way -- multiplying an existing "toolModifier" by
	 * (1 + bonus/100) -- so folding the OWNER's bonus into the
	 * effectiveness-shaped value we already pass in reproduces that
	 * multiplier exactly: (1+eff/100)*(1+bonus/100) == 1+eff'/100 where
	 * eff' = eff + bonus + eff*bonus/100.
	 * KNOWN SIMPLIFICATION (documented, not a bug): this does NOT
	 * reproduce the buff's SECOND effect in the stock formulas -- widening
	 * the AMAZINGSUCCESS luck-roll threshold and narrowing the
	 * double-failure threshold -- since those read hasBuff()/getBuff()
	 * directly on the passed-in creature and aren't exposed as separate
	 * parameters. Copying the owner's live Buff* onto the companion's own
	 * buff list was considered and rejected: Buff duration/removal hooks
	 * are tied to a single owning creature, and duplicating that
	 * reference risked a second, unrelated buff-cleanup hazard for a
	 * minor QoL bonus. */
	float foldFoodBuffBonusIntoEffectiveness(CreatureObject* owner, unsigned int buffCRC, const String& modifierName, float effectiveness) const;
};

}
}
}
}

using namespace server::zone::managers::companion;

#endif // COMPANIONCRAFTINGMANAGER_H_
