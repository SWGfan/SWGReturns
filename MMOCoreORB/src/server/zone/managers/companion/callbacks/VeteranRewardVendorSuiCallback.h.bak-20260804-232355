/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-20, "companion kill token" pass, per user
	request) -- the veteran reward vendor's token purchase menu. Spawned
	next to the Companion Master trainer in Mos Eisley (see
	tatooine_mos_eisley.lua); every companion kill grants the owner one
	non-tradable "Companion Killed Token" (see CreatureManagerImplementation
	::notifyDestruction() -> grantCompanionKillToken()), redeemable here.

	2026-08-01 (Companion): the placeholder catalog (2 rugs + 2 credit
	options) has been REPLACED with this fork's real custom items -- the
	Obsidian Vanguard armor set, Companion Loadout Backpack, Pocket Boy,
	Jenkin's Survey Tool and Jenkin's Cloner. Every price is 10000 tokens
	per Nick's explicit instruction ("lets make all items cost 10000
	tokens"), replacing the earlier 1-token testing prices throughout.

	Token identification: tokens are created from the SAME stock template
	as the real Hero of Tatooine "Mark of Courage" quest item
	(mark_courage.iff), so a player could coincidentally hold a genuine
	one too -- tokens are told apart ONLY by their custom object name
	("Companion Killed Token", set at grant time and never used by the
	real quest item), never by template alone. See countTokens()/
	consumeTokens() below.

	2026-07-20 follow-up -- two catalog entries grant a whole new "Master
	Jedi" companion (light or dark side) instead of a plain item: full real
	Jedi mastery + a legendary lightsaber, via CompanionSkillTrainer::
	recruitMasterJediCompanion(). Their original 1-token TESTING price was
	raised to 10000 on 2026-08-01 along with everything else.

	2026-08-01 ORDERING RULE, now uniform across all three reward kinds:
	the reward is created and delivered FIRST, and tokens are consumed only
	once delivery is known to have succeeded. Any failure rolls back
	completely and spends nothing. The single-item path previously charged
	first, so a missing template took payment and delivered nothing with no
	message at all -- see run() below.
*/

#ifndef VETERANREWARDVENDORSUICALLBACK_H_
#define VETERANREWARDVENDORSUICALLBACK_H_

#include "server/zone/objects/player/sui/SuiCallback.h"
#include "server/zone/objects/player/sui/listbox/SuiListBox.h"
#include "server/zone/objects/player/sui/SuiWindowType.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/tangible/TangibleObject.h"
#include "server/zone/objects/scene/SceneObject.h"
#include "server/zone/ZoneServer.h"
#include "server/zone/managers/companion/CompanionSkillTrainer.h"

class VeteranRewardVendorSuiCallback : public SuiCallback {
public:

	static const String TOKEN_NAME;
	static const int CATALOG_SIZE = 9;

	// 2026-08-01: the Obsidian Vanguard set is sold as ONE catalog entry
	// delivering all 10 pieces (Nick's choice over 10 separate lines).
	static const int ARMOR_SET_SIZE = 10;
	static const char* const ARMOR_SET[ARMOR_SET_SIZE];

	struct RewardEntry {
		const char* label;
		int cost;
		const char* itemTemplate; // "" -> credits-only (ignored if grantsJediCompanion/grantsArmorSet)
		int credits;
		bool grantsJediCompanion; // 2026-07-20: routes to recruitMasterJediCompanion() instead
		bool jediDarkSide;
		bool grantsArmorSet;      // 2026-08-01: routes to grantArmorSet(), all 10 pieces
	};

	// 2026-08-01: real custom-item catalog. See the definition at the
	// bottom of this file.
	static const RewardEntry CATALOG[CATALOG_SIZE];

	VeteranRewardVendorSuiCallback(ZoneServer* server) : SuiCallback(server) {
	}

	static int countTokens(CreatureObject* player) {
		if (player == nullptr) {
			return 0;
		}

		ManagedReference<SceneObject*> inventory = player->getSlottedObject("inventory");

		if (inventory == nullptr) {
			return 0;
		}

		int count = 0;

		for (int i = 0; i < inventory->getContainerObjectsSize(); ++i) {
			ManagedReference<SceneObject*> obj = inventory->getContainerObject(i);

			if (obj == nullptr || !obj->isTangibleObject()) {
				continue;
			}

			TangibleObject* tano = cast<TangibleObject*>(obj.get());

			if (tano != nullptr && tano->getCustomObjectName().toString() == TOKEN_NAME) {
				// 2026-07-20: tokens STACK via use count now (see
				// grantCompanionKillToken()) -- a stack of N counts as N.
				// Pre-stacking single tokens (useCount 0) count as 1.
				count += Math::max(1, (int) tano->getUseCount());
			}
		}

		return count;
	}

	/** @returns true if `amount` tokens were found and consumed.
	 * 2026-07-20: stack-aware -- draws down use counts across however many
	 * stacks it takes, destroying only stacks that reach zero. Verified
	 * affordable first (countTokens sums the same way), so a partial
	 * drain can't happen. */
	static bool consumeTokens(CreatureObject* player, int amount) {
		if (player == nullptr || amount <= 0) {
			return false;
		}

		if (countTokens(player) < amount) {
			return false;
		}

		ManagedReference<SceneObject*> inventory = player->getSlottedObject("inventory");

		if (inventory == nullptr) {
			return false;
		}

		Vector<ManagedReference<TangibleObject*> > stacks;

		for (int i = 0; i < inventory->getContainerObjectsSize(); ++i) {
			ManagedReference<SceneObject*> obj = inventory->getContainerObject(i);

			if (obj == nullptr || !obj->isTangibleObject()) {
				continue;
			}

			TangibleObject* tano = cast<TangibleObject*>(obj.get());

			if (tano != nullptr && tano->getCustomObjectName().toString() == TOKEN_NAME) {
				stacks.add(tano);
			}
		}

		int remaining = amount;

		for (int i = 0; i < stacks.size() && remaining > 0; ++i) {
			TangibleObject* stack = stacks.get(i).get();
			int stackCount = Math::max(1, (int) stack->getUseCount());
			int take = Math::min(stackCount, remaining);

			Locker itemLocker(stack, player);

			if (take >= stackCount) {
				stack->destroyObjectFromWorld(true);
				stack->destroyObjectFromDatabase(true);
			} else {
				stack->setUseCount(stackCount - take, true);
			}

			remaining -= take;
		}

		return remaining <= 0;
	}

	/** 2026-08-01: delivers the full Obsidian Vanguard set, ALL OR NOTHING.
	 * @returns true only if every piece was created AND transferred. On any
	 * failure, every piece delivered during THIS attempt is destroyed again
	 * and false is returned, so the caller bails out before spending a
	 * single token.
	 *
	 * REQUIRES the caller to already hold the player lock -- the cross-lock
	 * Locker(piece, player) below depends on it (Iron Rule 8). run() takes
	 * Locker clocker(player) before reaching here.
	 *
	 * The two failure causes are reported DIFFERENTLY on purpose: a missing
	 * template is a server-config problem, a failed transfer is a full
	 * inventory, and telling a player 'inventory full' when the real cause
	 * is a bad .iff path sends them chasing the wrong thing. */
	static bool grantArmorSet(CreatureObject* player) {
		if (player == nullptr) {
			return false;
		}

		ZoneServer* zoneServer = player->getZoneServer();
		ManagedReference<SceneObject*> inventory = player->getSlottedObject("inventory");

		if (zoneServer == nullptr || inventory == nullptr) {
			return false;
		}

		Vector<ManagedReference<SceneObject*> > delivered;
		bool templateMissing = false;
		bool inventoryFull = false;

		for (int i = 0; i < ARMOR_SET_SIZE; ++i) {
			ManagedReference<SceneObject*> piece = zoneServer->createObject(String::hashCode(ARMOR_SET[i]), 1);

			if (piece == nullptr) {
				templateMissing = true;
				break;
			}

			if (!inventory->transferObject(piece, -1, true)) {
				Locker failedLocker(piece, player);
				piece->destroyObjectFromDatabase(true);
				inventoryFull = true;
				break;
			}

			inventory->broadcastObject(piece, true);
			delivered.add(piece);
		}

		if (!templateMissing && !inventoryFull) {
			return true;
		}

		// Roll back everything handed over during this attempt.
		for (int i = 0; i < delivered.size(); ++i) {
			ManagedReference<SceneObject*> piece = delivered.get(i);

			if (piece == nullptr) {
				continue;
			}

			Locker pieceLocker(piece, player);
			piece->destroyObjectFromWorld(true);
			piece->destroyObjectFromDatabase(true);
		}

		if (templateMissing) {
			player->sendSystemMessage("One of the armor set's item templates is missing on this server -- nothing was delivered and NO tokens were spent. Please report this.");
		} else {
			player->sendSystemMessage("Not enough room in your inventory for all 10 pieces -- nothing was delivered and NO tokens were spent.");
		}

		return false;
	}

	static void sendPurchaseMenu(CreatureObject* player) {
		if (player == nullptr) {
			return;
		}

		ManagedReference<PlayerObject*> ghost = player->getPlayerObject();

		if (ghost == nullptr) {
			return;
		}

		if (ghost->hasSuiBoxWindowType(SuiWindowType::COMPANION_KILL_TOKEN_VENDOR)) {
			return;
		}

		int have = countTokens(player);

		ManagedReference<SuiListBox*> sui = new SuiListBox(player, SuiWindowType::COMPANION_KILL_TOKEN_VENDOR);
		sui->setPromptTitle("Veteran Reward Vendor");
		sui->setPromptText("You have " + String::valueOf(have) + " Companion Killed Token(s). Choose a reward:");
		sui->setCancelButton(true, "@ui:cancel");
		sui->setOkButton(true, "@ui:ok");
		sui->setCallback(new VeteranRewardVendorSuiCallback(player->getZoneServer()));

		for (int i = 0; i < CATALOG_SIZE; ++i) {
			const RewardEntry& entry = CATALOG[i];
			sui->addMenuItem(String(entry.label) + " -- " + String::valueOf(entry.cost) + " token(s)");
		}

		ghost->addSuiBox(sui);
		player->sendMessage(sui->generateMessage());
	}

	void run(CreatureObject* player, SuiBox* suiBox, uint32 eventIndex, Vector<UnicodeString>* args) {
		if (eventIndex == 1 || player == nullptr || args == nullptr || args->size() <= 0) {
			return;
		}

		int selection = Integer::valueOf(args->get(0).toString());

		if (selection < 0 || selection >= CATALOG_SIZE) {
			return;
		}

		const RewardEntry& entry = CATALOG[selection];

		Locker clocker(player);

		if (countTokens(player) < entry.cost) {
			player->sendSystemMessage("You don't have enough Companion Killed Tokens for that -- need " + String::valueOf(entry.cost) + ".");
			return;
		}

		// 2026-08-01 -- delivered BEFORE tokens are spent. grantArmorSet()
		// rolls itself back completely on failure, so a full inventory or a
		// bad template never costs a token.
		if (entry.grantsArmorSet) {
			if (!grantArmorSet(player)) {
				return;
			}

			if (!consumeTokens(player, entry.cost)) {
				player->sendSystemMessage("Something went wrong redeeming your tokens -- nothing was taken.");
				return;
			}

			player->sendSystemMessage("Redeemed " + String::valueOf(entry.cost) + " token(s) for the Obsidian Vanguard armor set (10 pieces).");
			return;
		}

		// Companion System (2026-07-20, "Master Jedi companion" pass) --
		// checked BEFORE spending tokens (recruitMasterJediCompanion()
		// itself refuses and messages the player if there's no free
		// companion slot), so a full datapad never costs a token for
		// nothing.
		if (entry.grantsJediCompanion) {
			if (!CompanionSkillTrainer::instance()->recruitMasterJediCompanion(player, entry.jediDarkSide)) {
				return;
			}

			if (!consumeTokens(player, entry.cost)) {
				player->sendSystemMessage("Something went wrong redeeming your tokens -- nothing was taken.");
			}

			return;
		}

		// 2026-08-01 REORDERED: the item is created and delivered FIRST, and
		// tokens are consumed only once that succeeded. The previous order
		// consumed tokens first and then attempted delivery, so:
		//   * a MISSING template silently ate the tokens and said NOTHING
		//     (createObject() returns null, the old code just fell off the
		//     end of the function), and
		//   * a FULL inventory ate the tokens and told the player to go see
		//     the trainer.
		// That was tolerable when the catalog held two known-good stock rug
		// templates; it is not tolerable now that it holds real custom
		// templates. Every failure below spends nothing.
		ManagedReference<SceneObject*> item = nullptr;

		if (!String(entry.itemTemplate).isEmpty()) {
			ZoneServer* zoneServer = player->getZoneServer();
			ManagedReference<SceneObject*> inventory = player->getSlottedObject("inventory");

			if (zoneServer == nullptr || inventory == nullptr) {
				player->sendSystemMessage("Couldn't reach your inventory -- no tokens were spent.");
				return;
			}

			// STRING_HASHCODE() forces compile-time evaluation via a non-type
			// template argument, which entry.itemTemplate can't satisfy (entry
			// is chosen at runtime from CATALOG[selection]). String::hashCode()
			// is the same function as a plain runtime call -- correct here.
			item = zoneServer->createObject(String::hashCode(entry.itemTemplate), 1);

			if (item == nullptr) {
				player->sendSystemMessage("That reward's item template is missing on this server -- NO tokens were spent. Please report this.");
				return;
			}

			if (!inventory->transferObject(item, -1, true)) {
				Locker itemLocker(item, player);
				item->destroyObjectFromDatabase(true);
				player->sendSystemMessage("Your inventory is full -- NO tokens were spent.");
				return;
			}

			inventory->broadcastObject(item, true);
		}

		if (!consumeTokens(player, entry.cost)) {
			if (item != nullptr) {
				Locker itemLocker(item, player);
				item->destroyObjectFromWorld(true);
				item->destroyObjectFromDatabase(true);
			}

			player->sendSystemMessage("Something went wrong redeeming your tokens -- nothing was taken.");
			return;
		}

		if (entry.credits > 0) {
			// Same call the post-combat loot sweep uses -- on-hand cash, not bank.
			player->addCashCredits(entry.credits, true);
			player->sendSystemMessage("Redeemed " + String::valueOf(entry.cost) + " token(s) for " + String::valueOf(entry.credits) + " credits.");
		}

		if (item != nullptr) {
			player->sendSystemMessage("Redeemed " + String::valueOf(entry.cost) + " token(s) for " + String(entry.label) + ".");
		}
	}
};

const String VeteranRewardVendorSuiCallback::TOKEN_NAME = "Companion Killed Token";

// 2026-08-01 (Companion) -- the 10 Obsidian Vanguard pieces, delivered
// together by grantArmorSet(). Paths follow each piece's own .lua file
// name. VERIFY against obsidian_vanguard/serverobjects.lua before trusting:
// if a path is wrong, createObject() returns null and grantArmorSet() now
// reports a MISSING TEMPLATE and spends nothing, rather than failing quietly.
const char* const VeteranRewardVendorSuiCallback::ARMOR_SET[VeteranRewardVendorSuiCallback::ARMOR_SET_SIZE] = {
	"object/tangible/wearables/armor/obsidian_vanguard/obsidian_helmet.iff",
	"object/tangible/wearables/armor/obsidian_vanguard/obsidian_chest_plate.iff",
	"object/tangible/wearables/armor/obsidian_vanguard/obsidian_leggings.iff",
	"object/tangible/wearables/armor/obsidian_vanguard/obsidian_boots.iff",
	"object/tangible/wearables/armor/obsidian_vanguard/obsidian_gloves.iff",
	"object/tangible/wearables/armor/obsidian_vanguard/obsidian_belt.iff",
	"object/tangible/wearables/armor/obsidian_vanguard/obsidian_bicep_l.iff",
	"object/tangible/wearables/armor/obsidian_vanguard/obsidian_bicep_r.iff",
	"object/tangible/wearables/armor/obsidian_vanguard/obsidian_bracer_l.iff",
	"object/tangible/wearables/armor/obsidian_vanguard/obsidian_bracer_r.iff",
};

// 2026-08-01 (Companion) -- real custom-item catalog, replacing the
// 2026-07-20 placeholder (2 rugs + 2 credit options). Every entry is an
// object template this fork actually added. ALL entries cost 10000 tokens
// per Nick's instruction ("lets make all items cost 10000 tokens").
//
// CONFIRMED template paths (read from each object's own .lua addTemplate
// call): companion_loadout_backpack, pocket_boy, jenkins_cloner,
// master_survey_tool. UNCONFIRMED: the 10 obsidian pieces above.
const VeteranRewardVendorSuiCallback::RewardEntry VeteranRewardVendorSuiCallback::CATALOG[VeteranRewardVendorSuiCallback::CATALOG_SIZE] = {
	// --- custom items ---
	{ "Obsidian Vanguard Armor Set (10 pieces)", 10000, "", 0, false, false, true },
	{ "Companion Loadout Backpack", 10000, "object/tangible/inventory/companion_loadout_backpack.iff", 0, false, false, false },
	{ "Pocket Boy", 10000, "object/tangible/gambling/slot/pocket_boy.iff", 0, false, false, false },
	{ "Jenkin's Survey Tool", 10000, "object/tangible/survey_tool/master_survey_tool.iff", 0, false, false, false },
	{ "Jenkin's Cloner", 10000, "object/tangible/terminal/jenkins_cloner.iff", 0, false, false, false },
	// --- credits ---
	{ "500 Credits", 10000, "", 500, false, false, false },
	{ "5,000 Credits", 10000, "", 5000, false, false, false },
	// --- Master Jedi companions ---
	{ "Master Jedi Companion (Light Side)", 10000, "", 0, true, false, false },
	{ "Master Jedi Companion (Dark Side)", 10000, "", 0, true, true, false },
};

#endif // VETERANREWARDVENDORSUICALLBACK_H_
