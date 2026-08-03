/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-23, "full suit" pass, per user request) --
	/companionrequestarmor: target an armorsmith companion, pick an armor
	MATERIAL (composite/ubese/chitin/padded/ris/zam/bone/kashyyykian/...),
	then pick individual body-slot pieces of that material to craft one at a
	time -- each craft runs through the existing, proven CompanionCraftTheater
	(materials trade + walk/hand-off/bow-kowtow + 10s craft glow), exactly
	like the general crafting picker (CompanionCraftPickSuiCallback.h)
	already does for a single item. After each piece, the SAME
	material-filtered list re-opens so asking for "a full suit" is a quick
	series of clicks through just that material's pieces, instead of hunting
	through everything the companion knows how to craft.

	FIX (2026-07-23, same-day follow-up -- user report: "it gave options for
	the armor layers and not the actual armor itself"): the first pass
	grouped by the substring right after "/armor/" in the schematic path,
	which actually matches
	object/draft_schematic/armor/component/armor_layer_<resist>.lua (the
	sub-component "defensive layer" pieces that go INSIDE a segment) and
	object/draft_schematic/armor/armor_segment_<material>.lua (the segment
	itself, still not wearable) -- NOT the real wearable armor. The real,
	equip-able pieces are a separate schematic one tier up:
	object/draft_schematic/clothing/clothing_armor_<material>_<slot>.lua
	(e.g. clothing_armor_composite_chest.lua -> targetTemplate
	.../armor_composite_chest_plate.iff). collectArmorSchematics() now scans
	for that "clothing_armor_" filename prefix specifically and parses
	<material> vs <slot> off the filename using the known body-slot suffix
	list below, so Step 1 offers real materials and Step 2 offers real
	body-slot pieces (chest/leggings/gloves/helmet/boots/bicep_l/bicep_r/
	bracer_l/bracer_r). The 3rd-tier choice -- which defensive-layer RESIST
	type (acid/blast/cold/electrical/energy/environmental/heat/kinetic/
	restraint/ris/stun) goes inside the segment -- is still auto-picked by
	CompanionCraftingManager same as before; that's a separate, deeper
	enhancement not yet scoped (would need CompanionCraftingManager's
	component-slot-filling to accept a caller-specified preferred component
	instead of auto-selecting).

	FEATURE (2026-07-23, same-day follow-up -- per user request "I would
	also like the option to receive the full suit, not just ask for 1 piece
	at a time"): the piece-picker SUI now has a first entry, "-- Craft ALL
	pieces automatically --", alongside every individual piece. Picking it
	calls craftAllPieces(), which recurses through CompanionCraftTheater's
	completion callback -- each piece's full walk/trade/glow choreography
	still plays out exactly as it does for a manual single piece, but the
	NEXT piece starts automatically the moment the current one finishes,
	with no re-opened menu / player click needed in between. Picking an
	individual piece (any entry below the first) still behaves exactly as
	before -- single piece, then the same list re-opens.

	FIX (2026-07-23, same-day, live bug report -- "Armorsmith has finished
	the whole bone suit!" was announced right after a piece had ACTUALLY
	failed, e.g. "is missing a component it can't make:
	shared_armor_segment_bone.iff"): craftAllPieces() originally used
	CompanionCraftTheater's old std::function<void()> callback, which had no
	way to report success/failure -- so the chain always continued to the
	next piece and always declared full success at the end, no matter what
	really happened. Now that CompanionCraftTheater::begin() reports a real
	bool, craftAllPieces() checks it: on failure it STOPS the chain right
	there and tells the player exactly which piece (N of the total) it got
	stuck on, instead of silently skipping ahead and lying about the result.

	FEATURE (2026-07-23, "Field Crafting Droid" pass -- design doc:
	claude/design-field-crafting-droid-2026-07-23.md): both craft call
	sites below (the single-piece path in run(), and the auto-chain in
	craftAllPieces()) now go through CompanionFieldStation::begin() instead
	of calling CompanionCraftTheater::begin() directly. Armor is exactly
	the schematic family that hit the live "missing crafting station" gap
	(the bone-armor bug), so this is where that gate needed to land first.
	CompanionFieldStation::begin() passes straight through to
	CompanionCraftTheater::begin() with the SAME signature when no station
	is needed (or one's already available) -- so this swap is a no-op for
	every piece that doesn't require a station, and only kicks in the
	request/build/handoff/deploy theater for the ones that do.

	Requires ONE new SuiWindowType enum entry -- see the integration notes
	delivered alongside this file; nothing else needs to change to add it.
*/

#ifndef COMPANIONARMORTYPESUICALLBACK_H_
#define COMPANIONARMORTYPESUICALLBACK_H_

#include "server/zone/objects/player/sui/SuiCallback.h"
#include "server/zone/objects/player/sui/listbox/SuiListBox.h"
#include "server/zone/objects/player/sui/SuiWindowType.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/draftschematic/DraftSchematic.h"
#include "server/zone/managers/skill/SkillManager.h"
#include "server/zone/managers/crafting/schematicmap/SchematicMap.h"
#include "server/zone/managers/companion/CompanionCraftTheater.h"
#include "server/zone/managers/companion/CompanionFieldStation.h"
#include "server/zone/managers/stringid/StringIdManager.h"

class CompanionArmorPieceSuiCallback;

class CompanionArmorTypeSuiCallback : public SuiCallback {
	ManagedReference<CompanionObject*> companion;
	Vector<String> typeNames;

public:
	CompanionArmorTypeSuiCallback(ZoneServer* server, CompanionObject* comp, const Vector<String>& types)
		: SuiCallback(server) {
		companion = comp;
		typeNames = types;
	}

	/** Known real-armor body-slot suffixes, exactly as they appear at the end
	 * of clothing_armor_<material>_<slot> schematic filenames. Checked
	 * longest-first isn't actually required here (none is a suffix of
	 * another), but the list is kept explicit and exhaustive rather than
	 * inferred, since a wrong/missing suffix would silently misparse a
	 * material name instead of erroring. */
	static const Vector<String>& slotSuffixes() {
		static Vector<String> suffixes;

		if (suffixes.size() == 0) {
			suffixes.add("bicep_l");
			suffixes.add("bicep_r");
			suffixes.add("bracer_l");
			suffixes.add("bracer_r");
			suffixes.add("boots");
			suffixes.add("chest");
			suffixes.add("gloves");
			suffixes.add("helmet");
			suffixes.add("leggings");
		}

		return suffixes;
	}

	/** Splits a "clothing_armor_<material>_<slot>" stem (the schematic
	 * filename with the "clothing_armor_" prefix and file extension already
	 * stripped) into its material and slot parts. Returns false if it
	 * doesn't end in a recognized slot suffix -- callers skip it rather
	 * than guess. */
	static bool splitMaterialAndSlot(const String& stem, String& material, String& slot) {
		const Vector<String>& suffixes = slotSuffixes();

		for (int i = 0; i < suffixes.size(); ++i) {
			const String& suffix = suffixes.get(i);
			String needle = "_" + suffix;

			if (stem.endsWith(needle)) {
				material = stem.subString(0, stem.length() - needle.length());
				slot = suffix;
				return material.length() > 0;
			}
		}

		return false;
	}

	/** Every real, WEARABLE armor draft schematic the companion's learned
	 * skills grant, grouped by material (composite/ubese/chitin/...). Only
	 * matches object/draft_schematic/clothing/clothing_armor_<material>_
	 * <slot>.iff schematics -- these are the final pieces a player actually
	 * equips. Deliberately does NOT match
	 * object/draft_schematic/armor/armor_segment_<material>.iff (the
	 * intermediate, non-wearable segment) or
	 * object/draft_schematic/armor/component/armor_layer_<resist>.iff (the
	 * resist sub-components that go inside a segment) -- those were the
	 * wrong things the first pass of this picker was showing. */
	static void collectArmorSchematics(CompanionObject* comp, VectorMap<String, Vector<String> >& byType) {
		static const String PREFIX_MARKER = "/clothing/clothing_armor_";

		for (int s = 0; s < comp->getLearnedSkillCount(); ++s) {
			Skill* skill = SkillManager::instance()->getSkill(comp->getLearnedSkill(s));

			if (skill == nullptr) {
				continue;
			}

			const Vector<String>* groups = skill->getSchematicsGranted();

			if (groups == nullptr) {
				continue;
			}

			for (int g = 0; g < groups->size(); ++g) {
				DraftSchematicGroup* group = SchematicMap::instance()->getGroup(groups->get(g));

				if (group == nullptr) {
					continue;
				}

				for (int d = 0; d < group->size(); ++d) {
					DraftSchematic* schematic = group->get(d).get();

					if (schematic == nullptr || schematic->getObjectTemplate() == nullptr) {
						continue;
					}

					String path = schematic->getObjectTemplate()->getFullTemplateString();

					int markerIdx = path.indexOf(PREFIX_MARKER);

					if (markerIdx == -1) {
						continue;
					}

					String stem = path.subString(markerIdx + PREFIX_MARKER.length());
					int dot = stem.lastIndexOf('.');

					if (dot != -1) {
						stem = stem.subString(0, dot);
					}

					String material, slot;

					if (!splitMaterialAndSlot(stem, material, slot)) {
						// Doesn't end in a recognized body-slot suffix --
						// skip rather than guess at parsing it.
						continue;
					}

					if (!byType.contains(material)) {
						byType.put(material, Vector<String>());
					}

					byType.get(material).add(path);
				}
			}
		}
	}

	static void sendTypeList(CreatureObject* player, CompanionObject* comp) {
		if (player == nullptr || comp == nullptr) {
			return;
		}

		ManagedReference<PlayerObject*> ghost = player->getPlayerObject();

		if (ghost == nullptr) {
			return;
		}

		VectorMap<String, Vector<String> > byType;
		collectArmorSchematics(comp, byType);

		if (byType.size() == 0) {
			player->sendSystemMessage(comp->getDisplayedName() + " doesn't know how to craft any armor yet -- train it as an armorsmith first.");
			return;
		}

		Vector<String> typeNames;

		for (int i = 0; i < byType.size(); ++i) {
			typeNames.add(byType.elementAt(i).getKey());
		}

		// Alphabetical order (2026-07-23, per user request "can we list
		// things in alphabetical order") -- selection sort using only
		// Vector methods already proven elsewhere in this codebase
		// (get/size/remove/add -- e.g. CompanionCraftingManager.cpp's
		// resource-candidate picking uses candidates.remove(bestIndex)),
		// rather than assuming an insertElementAt() or assignable get()
		// exists. Fine performance-wise at this list's tiny size.
		{
			Vector<String> sortedTypeNames;

			while (typeNames.size() > 0) {
				int bestIndex = 0;

				for (int i = 1; i < typeNames.size(); ++i) {
					if (typeNames.get(i).compareTo(typeNames.get(bestIndex)) > 0) {
						bestIndex = i;
					}
				}

				sortedTypeNames.add(typeNames.get(bestIndex));
				typeNames.remove(bestIndex);
			}

			typeNames = sortedTypeNames;
		}

		ghost->closeSuiWindowType(SuiWindowType::COMPANION_ARMOR_TYPE_PICK);

		ManagedReference<SuiListBox*> sui = new SuiListBox(player, SuiWindowType::COMPANION_ARMOR_TYPE_PICK);
		sui->setPromptTitle(comp->getDisplayedName() + " -=COMPANION=- : Full Suit");
		sui->setPromptText("Which armor type should I build you a suit of? Pick a type, then I'll walk you through each piece.");
		sui->setCancelButton(true, "@ui:cancel");
		sui->setOkButton(true, "@ui:ok");
		sui->setCallback(new CompanionArmorTypeSuiCallback(player->getZoneServer(), comp, typeNames));

		for (int i = 0; i < typeNames.size(); ++i) {
			sui->addMenuItem(typeNames.get(i));
		}

		ghost->addSuiBox(sui);
		player->sendMessage(sui->generateMessage());
	}

	void run(CreatureObject* player, SuiBox* suiBox, uint32 eventIndex, Vector<UnicodeString>* args);
};

class CompanionArmorPieceSuiCallback : public SuiCallback {
	ManagedReference<CompanionObject*> companion;
	String armorType;
	Vector<String> schematicPaths;

public:
	CompanionArmorPieceSuiCallback(ZoneServer* server, CompanionObject* comp, const String& type, const Vector<String>& paths)
		: SuiCallback(server) {
		companion = comp;
		armorType = type;
		schematicPaths = paths;
	}

	/** Auto-chains through EVERY piece in `paths`, starting at `index`, with
	 * no player interaction in between (2026-07-23 feature). Each piece
	 * still runs the full CompanionCraftTheater choreography (materials
	 * trade, walk, bow/kowtow, 10s craft glow) -- this supplies an
	 * onComplete(bool) that checks the REAL success/failure of the piece
	 * that just ran (2026-07-23 fix -- see file header) before deciding
	 * whether to continue: success recurses to the next index, failure
	 * stops the chain and tells the player exactly where it stopped instead
	 * of silently skipping ahead.
	 * @pre nothing locked (begin() takes its own crosslock per piece, same
	 * as the manual single-piece path). */
	static void craftAllPieces(CreatureObject* player, CompanionObject* comp, const String& armorType, const Vector<String>& paths, int index, int completedCount = 0) {
		if (player == nullptr || comp == nullptr || comp->getZone() == nullptr) {
			return;
		}

		if (index >= paths.size()) {
			player->sendSystemMessage(comp->getDisplayedName() + " has finished the whole " + armorType + " suit! (" + String::valueOf(completedCount) + "/" + String::valueOf(paths.size()) + " pieces)");
			return;
		}

		String chosenPath = paths.get(index);

		ManagedReference<CreatureObject*> playerRef = player;
		ManagedReference<CompanionObject*> compRef = comp;
		String armorTypeCopy = armorType;
		Vector<String> pathsCopy = paths;

		{
			Locker clocker(compRef, player);
			CompanionFieldStation::begin(player, comp, chosenPath, [playerRef, compRef, armorTypeCopy, pathsCopy, index, completedCount] (bool success) {
				CreatureObject* p = playerRef.get();
				CompanionObject* c = compRef.get();

				if (p == nullptr || c == nullptr) {
					return;
				}

				if (!success) {
					// Stop here -- the specific error was already shown to
					// the player by CompanionCraftTheater::finishCraft().
					// Don't continue past a real failure and don't claim
					// the suit finished.
					p->sendSystemMessage(c->getDisplayedName() + " stopped on piece " + String::valueOf(index + 1) + " of " + String::valueOf(pathsCopy.size()) + " for the " + armorTypeCopy + " suit -- see the error above for why. (" + String::valueOf(completedCount) + " piece(s) finished before that.)");
					return;
				}

				craftAllPieces(p, c, armorTypeCopy, pathsCopy, index + 1, completedCount + 1);
			});
		}
	}

	static void sendPieceList(CreatureObject* player, CompanionObject* comp, const String& armorType) {
		if (player == nullptr || comp == nullptr) {
			return;
		}

		ManagedReference<PlayerObject*> ghost = player->getPlayerObject();

		if (ghost == nullptr) {
			return;
		}

		VectorMap<String, Vector<String> > byType;
		CompanionArmorTypeSuiCallback::collectArmorSchematics(comp, byType);

		if (!byType.contains(armorType)) {
			return;
		}

		Vector<String> paths = byType.get(armorType);
		Vector<String> pieceLabels;

		// Re-derive the parsed slot name for each path rather than reusing
		// the raw filename -- "chest" / "leggings" / "bicep_l" etc. instead
		// of "clothing_armor_composite_chest".
		for (int i = 0; i < paths.size(); ++i) {
			const String& path = paths.get(i);
			String stem = path.subString(path.lastIndexOf('/') + 1, path.lastIndexOf('.'));

			static const String FILE_PREFIX = "clothing_armor_";
			int prefixIdx = stem.indexOf(FILE_PREFIX);

			if (prefixIdx != -1) {
				stem = stem.subString(prefixIdx + FILE_PREFIX.length());
			}

			String material, slot;

			if (CompanionArmorTypeSuiCallback::splitMaterialAndSlot(stem, material, slot)) {
				pieceLabels.add(slot);
			} else {
				pieceLabels.add(stem);
			}
		}

		// Alphabetical order (2026-07-23, per user request) -- sorts the
		// REAL pieces only (paths/pieceLabels stay in lockstep), then the
		// "craft all" entry gets pinned back onto the front below. Same
		// proven-safe selection-sort approach as sendTypeList() above.
		{
			Vector<String> sortedPaths;
			Vector<String> sortedLabels;

			while (pieceLabels.size() > 0) {
				int bestIndex = 0;

				for (int i = 1; i < pieceLabels.size(); ++i) {
					if (pieceLabels.get(i).compareTo(pieceLabels.get(bestIndex)) > 0) {
						bestIndex = i;
					}
				}

				sortedLabels.add(pieceLabels.get(bestIndex));
				sortedPaths.add(paths.get(bestIndex));
				pieceLabels.remove(bestIndex);
				paths.remove(bestIndex);
			}

			paths = sortedPaths;
			pieceLabels = sortedLabels;
		}

		// 2026-07-23: the "craft the whole suit automatically" option is
		// ALWAYS entry 0 -- run()'s menuSelection == 0 check below relies on
		// this. Every individual piece label is offset by +1 from its index
		// in `paths` because of this entry -- pinned on AFTER sorting so it
		// never gets alphabetized into the middle of the list.
		Vector<String> labels;
		labels.add("-- Craft ALL pieces automatically --");

		for (int i = 0; i < pieceLabels.size(); ++i) {
			labels.add(pieceLabels.get(i));
		}

		ghost->closeSuiWindowType(SuiWindowType::COMPANION_ARMOR_PIECE_PICK);

		ManagedReference<SuiListBox*> sui = new SuiListBox(player, SuiWindowType::COMPANION_ARMOR_PIECE_PICK);
		sui->setPromptTitle(comp->getDisplayedName() + " -=COMPANION=- : " + armorType + " suit");
		sui->setPromptText("Which piece next? Pick '-- Craft ALL pieces automatically --' at the top to run through the whole suit back-to-back with no further clicks, or pick a single piece (this list re-opens after each one so you can keep going manually).");
		sui->setCancelButton(true, "@ui:cancel");
		sui->setOkButton(true, "@ui:ok");
		sui->setCallback(new CompanionArmorPieceSuiCallback(player->getZoneServer(), comp, armorType, paths));

		for (int i = 0; i < labels.size(); ++i) {
			sui->addMenuItem(labels.get(i));
		}

		ghost->addSuiBox(sui);
		player->sendMessage(sui->generateMessage());
	}

	void run(CreatureObject* player, SuiBox* suiBox, uint32 eventIndex, Vector<UnicodeString>* args) {
		if (eventIndex == 1 || player == nullptr || args == nullptr || args->size() <= 0) {
			return;
		}

		int menuSelection = Integer::valueOf(args->get(0).toString());

		// Entry 0 is always "craft the whole suit" (see sendPieceList()) --
		// every real piece is offset by +1 from its index in schematicPaths.
		if (menuSelection < 0 || menuSelection > schematicPaths.size()) {
			return;
		}

		ManagedReference<CompanionObject*> strongCompanion = companion;

		if (strongCompanion == nullptr) {
			return;
		}

		if (menuSelection == 0) {
			craftAllPieces(player, strongCompanion, armorType, schematicPaths, 0);
			return;
		}

		String chosenPath = schematicPaths.get(menuSelection - 1);

		{
			Locker clocker(strongCompanion, player);
			CompanionFieldStation::begin(player, strongCompanion, chosenPath);
		}

		// Re-open the SAME type-filtered list -- "craft another piece of
		// this suit?" (identical chaining pattern to the general craft
		// picker, just scoped to one armor type).
		sendPieceList(player, strongCompanion, armorType);
	}
};

inline void CompanionArmorTypeSuiCallback::run(CreatureObject* player, SuiBox* suiBox, uint32 eventIndex, Vector<UnicodeString>* args) {
	if (eventIndex == 1 || player == nullptr || args == nullptr || args->size() <= 0) {
		return;
	}

	int menuSelection = Integer::valueOf(args->get(0).toString());

	if (menuSelection < 0 || menuSelection >= typeNames.size()) {
		return;
	}

	ManagedReference<CompanionObject*> strongCompanion = companion;

	if (strongCompanion == nullptr) {
		return;
	}

	CompanionArmorPieceSuiCallback::sendPieceList(player, strongCompanion, typeNames.get(menuSelection));
}

#endif // COMPANIONARMORTYPESUICALLBACK_H_
