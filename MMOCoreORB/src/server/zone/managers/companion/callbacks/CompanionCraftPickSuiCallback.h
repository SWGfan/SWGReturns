/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-20, per user request "artisan companions get a
	radial option to pick items to craft"): the craft picker. Enumerates
	every draft schematic the companion's LEARNED skills actually grant
	(Skill::getSchematicsGranted() group names -> SchematicMap::getGroup()),
	shows them in a list, and hands the selection to the Companion chat's
	CompanionCraftingManager::craftItem() -- the full headless craft with
	the bag/harvester/resource-deed acquisition chain behind it. After each
	successful craft the list re-opens ("craft another?", same chaining
	pattern as the taxi waypoint picker), per the approved crafting-GUI
	spec. This SUI picker is the interim UI; the real crafting-window GUI
	(3D preview) remains the planned upgrade on top of the same manager.

	2026-07-24 addition ("test resources" radial, per user request "can I
	have a radial that requests the best resources for a certain item"):
	the exact same schematic list is reused (factored into
	collectKnownSchematics() below so the enumeration logic can't drift
	between the two pickers -- this session already hit two real bugs
	caused by duplicated logic diverging), but the selection routes to
	CompanionCraftTestResourcePickSuiCallback instead, which fills a test
	bag instead of crafting. See CompanionCraftingManager::
	giveTestResourceBag().
*/

#ifndef COMPANIONCRAFTPICKSUICALLBACK_H_
#define COMPANIONCRAFTPICKSUICALLBACK_H_

#include "server/zone/objects/player/sui/SuiCallback.h"
#include "server/zone/objects/player/sui/listbox/SuiListBox.h"
#include "server/zone/objects/player/sui/SuiWindowType.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/draftschematic/DraftSchematic.h"
#include "server/zone/managers/skill/SkillManager.h"
#include "server/zone/managers/crafting/schematicmap/SchematicMap.h"
#include "server/zone/managers/companion/CompanionCraftingManager.h"
#include "server/zone/managers/companion/CompanionCraftTheater.h"
#include "server/zone/managers/companion/CompanionFieldStation.h"
#include "server/zone/managers/stringid/StringIdManager.h"
#include "server/zone/objects/player/sui/inputbox/SuiInputBox.h"
#include "templates/crafting/resourceweight/ResourceWeight.h"

// Companion System (2026-07-29, "too many options / nothing makes a
// difference" fix): need the real ManufactureSchematic/CraftingValues
// types to build the Optimize-For list from the SAME source the real
// experimentation loop reads -- see maybeAskOptimizeLine()'s doc comment
// below for the full explanation. Same include paths
// CompanionCraftingManager.cpp already uses successfully.
#include "server/zone/objects/manufactureschematic/ManufactureSchematic.h"
#include "server/zone/objects/manufactureschematic/craftingvalues/CraftingValues.h"

class CompanionCraftOptimizeSuiCallback;
class CompanionCraftTestResourcePickSuiCallback;
class CompanionCraftCategoryPickSuiCallback;
class CompanionCraftSubcategoryPickSuiCallback;

// Companion System (2026-07-28, "click into a weapon sub category" per
// Nick): which of the three craft-list flows a category-picker window
// (CompanionCraftCategoryPickSuiCallback below) should hand off to once a
// category is chosen. Plain header enum -- no .idl / build-system
// involvement, so no cmake reconfigure is needed for this alone (this
// whole patch only touches this one existing header, no new .cpp files).
enum CompanionCraftMenuMode {
	COMPANION_CRAFT_MENU_MODE_CRAFT = 0,
	COMPANION_CRAFT_MENU_MODE_TEST_RESOURCE = 1,
	COMPANION_CRAFT_MENU_MODE_BATCH = 2
};

class CompanionCraftPickSuiCallback : public SuiCallback {
	ManagedReference<CompanionObject*> companion;
	Vector<String> schematicPaths;

public:
	CompanionCraftPickSuiCallback(ZoneServer* server, CompanionObject* comp, const Vector<String>& paths)
		: SuiCallback(server) {
		companion = comp;
		schematicPaths = paths;
	}

	/** Companion System (2026-07-24 refactor): every draft schematic the
	 * companion's learned skills actually grant, deduplicated by tanoCRC,
	 * with human-readable labels resolved the same way both the craft
	 * picker and the test-resource picker need. Pulled out of
	 * sendCraftList() so both callers can never silently diverge. */
	static void collectKnownSchematics(CompanionObject* comp, Vector<String>& paths, Vector<String>& labels) {
		SortedVector<uint32> seen;
		seen.setNoDuplicateInsertPlan();

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

				// Companion System (2026-07-28 FIX, "missing Architect items"
				// report): removed an artificial 120-item GLOBAL cap here that
				// silently truncated the WHOLE list once any combination of
				// earlier-LEARNED skills' schematic groups filled it --
				// confirmed live: a companion trained Artisan-first and later
				// mastered into Architect showed zero Architect items (no
				// buildings/deeds/factories/structure components) because
				// Artisan's own groups (dice, tools, food, drink, clothing
				// accessories, fireworks, ...) alone filled the cap first. No
				// genuine technical row-count limit was found to justify keeping
				// any cap here -- SuiListBox.idl / SuiListBoxImplementation.cpp's
				// generateMessage() fill loop is a plain
				// `for (int i = 0; i < getMenuSize(); i++)` with nothing capping
				// it -- so the cap is removed outright rather than raised.
				for (int d = 0; d < group->size(); ++d) {
					DraftSchematic* schematic = group->get(d).get();

					if (schematic == nullptr || schematic->getObjectTemplate() == nullptr) {
						continue;
					}

					String path = schematic->getObjectTemplate()->getFullTemplateString();
					uint32 crc = path.hashCode();

					if (seen.contains(crc)) {
						continue;
					}

					seen.put(crc);

					// Companion System (2026-07-29 FIX, per Nick's raw-schematic-name
					// bug report -- craft picker showed "clothing_wke_skirt_s04" etc
					// instead of real item names): DraftSchematic's own customObjectName
					// (schematic .lua "customObjectName = ...") is the actual source of
					// truth for a schematic's display name in this codebase -- see
					// DraftSchematicImplementation::getCustomName() and its established
					// use in CraftingSessionImplementation.cpp ("Selected DraftSchematic: "
					// + draftschematic->getCustomName()). The STF-based getObjectName()
					// lookup below is the right tool for STF-named objects (structures/
					// creatures/terminals -- see StructureObjectImplementation.cpp's
					// identical StringIdManager pattern for THOSE), but draft schematics
					// don't carry a populated STF objectName at all, so it always missed
					// and silently fell through to the raw internal filename fragment --
					// the reported bug. Kept only as a defensive second fallback now.
					String label = schematic->getCustomName();

					if (label.isEmpty()) {
						label = StringIdManager::instance()->getStringId(schematic->getObjectName()->getFullPath().hashCode()).toString();
					}

					if (label.isEmpty()) {
						label = path.subString(path.lastIndexOf('/') + 1, path.lastIndexOf('.'));
					}

					paths.add(path);
					labels.add(label);
				}
			}
		}
	}

	/** Companion System (2026-07-28, "group the items... like the real
	 * crafting tool" per Nick, screenshot showing tabs/bold category
	 * headers like "Building Deed"/"Deed"/"Installation Deed"/"Structure
	 * Component"). SuiListBox is a flat, single-column list -- confirmed
	 * by reading SuiListBox.idl / SuiListBoxImplementation.cpp,
	 * addMenuItem() just appends one more row, no real tree/tab support
	 * and no documented row-count limit. An earlier same-day cut faked
	 * categories with non-selectable header rows in one flat list; per
	 * Nick's follow-up ask ("click into a weapon sub category... then it
	 * shows only the weapons") this proxy now instead drives a REAL
	 * two-step drill-down -- see collectCategories() (Step 1: category
	 * names only) and buildCraftMenuForCategory() (Step 2: one category's
	 * items) below, plus CompanionCraftCategoryPickSuiCallback further
	 * down this file.
	 *
	 * There's no server-side schematic-category datatable to read (the
	 * real client's categorization comes from a datatable this project
	 * doesn't have server-side access to), so this uses the schematic's
	 * own object/draft_schematic/<folder>/... path as a real, always-
	 * available proxy -- verified against the base client's
	 * schematic_group.iff top-level folder list (clothing, chemistry,
	 * food, bio_engineer, weapon, droid, structure, furniture, item,
	 * armor, munition, scout, spices, instrument, slicing). structure/
	 * gets one further, real split so buildings/deeds/installations/
	 * components don't all dump into one generic "Structure" bucket,
	 * matching the real client crafting tool's own categories from
	 * Nick's screenshot. No bank/tent schematics exist in the base
	 * client's schematic_group.iff at all, so any Core3-custom ones in
	 * structure/ just fall into the "Deed" catch-all below. */
	static String categorize(const String& schematicPath) {
		const String marker = "object/draft_schematic/";
		int idx = schematicPath.indexOf(marker);

		if (idx == -1) {
			return "Other";
		}

		String rest = schematicPath.subString(idx + marker.length());
		int slash = rest.indexOf('/');

		if (slash == -1) {
			return "Other";
		}

		String topFolder = rest.subString(0, slash);
		String remainder = rest.subString(slash + 1);

		if (topFolder == "structure") {
			if (remainder.beginsWith("component/")) {
				return "Structure Component";
			}

			int fileSlash = remainder.lastIndexOf('/');
			String fileName = (fileSlash == -1 ? remainder : remainder.subString(fileSlash + 1)).toLowerCase();

			if (fileName.beginsWith("installation_")) {
				return "Installation Deed";
			}

			if (fileName.contains("house")) {
				return "Building Deed";
			}

			// Catch-all within structure/ -- see this file's own patch notes:
			// no bank/tent schematics exist in the base client's
			// schematic_group.iff at all, so any Core3-custom ones here just
			// bucket into "Deed".
			return "Deed";
		}

		// Title Case each '_'-separated word (e.g. "bio_engineer" ->
		// "Bio Engineer") using only String's own
		// subString()/toUpperCase() -- no <cctype>/toupper() dependency.
		String label = "";
		int wordStart = 0;

		for (int i = 0; i <= topFolder.length(); ++i) {
			bool atBreak = (i == topFolder.length()) || (topFolder.charAt(i) == '_');

			if (!atBreak) {
				continue;
			}

			if (i > wordStart) {
				String word = topFolder.subString(wordStart, i);

				if (!label.isEmpty()) {
					label += " ";
				}

				label += word.subString(0, 1).toUpperCase() + word.subString(1);
			}

			wordStart = i + 1;
		}

		return label.isEmpty() ? "Other" : label;
	}

	/** Companion System (2026-07-28, "can we make sub sub categories"
	 * per Nick): given a schematic path already known to be in a top
	 * category (categorize() above), returns a sub-category label from
	 * the base client's own real SECOND object/draft_schematic/<top>/
	 * <sub>/... path segment, Title-Cased the exact same way categorize()
	 * Title-Cases the top folder -- verified against a real extraction of
	 * the base client's datatables/crafting/schematic_group.iff (1066
	 * rows): most top folders are flat (schematic sits directly under
	 * <top>/, e.g. weapon/knife_survival.iff) except for a real
	 * "component/" subfolder present in several of them (clothing,
	 * chemistry, food, weapon, droid, item, armor, munition, and
	 * structure's own "component/" -- already split into its own
	 * "Structure Component" top bucket by categorize() itself, so this
	 * always resolves to the single "Component" label there and never
	 * trips the 2+ threshold below), and bio_engineer, the one top
	 * folder with real STRUCTURAL sub-categories of its own
	 * (dna_template/, creature/, bio_component/) rather than a single
	 * component/ split. Flat items (no real second path segment) get
	 * the constant "General" bucket -- deliberately a real, single,
	 * always-the-same label rather than skipping subcategorize()
	 * outright, so collectSubcategories()'s dedup+count logic below is
	 * the ONLY place that decides whether a category gets the new
	 * Step 2b picker (2+ distinct labels) or falls straight through to
	 * the existing item list (0 or 1) -- no separate bookkeeping needed
	 * for "this category has no real subfolder at all" vs "this
	 * category has a subfolder but everything in it happens to share
	 * one value". */
	static String subcategorize(const String& schematicPath) {
		const String marker = "object/draft_schematic/";
		int idx = schematicPath.indexOf(marker);

		if (idx == -1) {
			return "General";
		}

		String rest = schematicPath.subString(idx + marker.length());
		int slash = rest.indexOf('/');

		if (slash == -1) {
			return "General";
		}

		String remainder = rest.subString(slash + 1);
		int slash2 = remainder.indexOf('/');

		if (slash2 == -1) {
			// Flat -- schematic sits directly under <top>/, no real
			// subfolder segment to report.
			return "General";
		}

		String subFolder = remainder.subString(0, slash2);

		// Title Case each '_'-separated word, identical technique to
		// categorize()'s top-folder Title-Casing above (duplicated rather
		// than factored out, matching this file's existing tolerance for
		// small duplicated logic -- see the two copies of the manual
		// insertion sort in collectCategories()/buildCraftMenuForCategory()
		// -- so categorize() itself stays byte-for-byte unchanged per
		// Nick's ask).
		String label = "";
		int wordStart = 0;

		for (int i = 0; i <= subFolder.length(); ++i) {
			bool atBreak = (i == subFolder.length()) || (subFolder.charAt(i) == '_');

			if (!atBreak) {
				continue;
			}

			if (i > wordStart) {
				String word = subFolder.subString(wordStart, i);

				if (!label.isEmpty()) {
					label += " ";
				}

				label += word.subString(0, 1).toUpperCase() + word.subString(1);
			}

			wordStart = i + 1;
		}

		return label.isEmpty() ? "General" : label;
	}

	/** Companion System (2026-07-28, real two-step drill-down per Nick:
	 * "click into a weapon sub category... then it shows only the
	 * weapons"). Step 1 data: just the sorted, de-duplicated category
	 * NAMES a companion currently has anything craftable in. Same
	 * categorize() proxy as before (schematic path's
	 * object/draft_schematic/<folder>/... segment); dedup is a linear
	 * scan (category counts are small -- a couple dozen at most, unlike
	 * the item counts this project's 2026-07-28 no-cap fix was about) and
	 * the sort is the same manual insertion sort already used elsewhere
	 * in this project for SUI row ordering (see the skill-tree depth sort
	 * in CompanionSkillTrainer.cpp). */
	static void collectCategories(CompanionObject* comp, Vector<String>& categories) {
		Vector<String> rawPaths;
		Vector<String> rawLabels;
		collectKnownSchematics(comp, rawPaths, rawLabels);

		for (int i = 0; i < rawPaths.size(); ++i) {
			String category = categorize(rawPaths.get(i));
			bool found = false;

			for (int c = 0; c < categories.size(); ++c) {
				if (categories.get(c) == category) {
					found = true;
					break;
				}
			}

			if (!found) {
				categories.add(category);
			}
		}

		for (int i = 1; i < categories.size(); ++i) {
			String key = categories.get(i);
			String keyLower = key.toLowerCase();
			int j = i - 1;

			while (j >= 0 && categories.get(j).toLowerCase().compareTo(keyLower) > 0) {
				categories.set(j + 1, categories.get(j));
				--j;
			}

			categories.set(j + 1, key);
		}
	}

	/** Companion System (2026-07-28, "can we make sub sub categories"
	 * per Nick -- a third drill-down level under an already-chosen top
	 * category). Sorted, de-duplicated sub-category labels for
	 * everything a companion can craft within ONE top category, via
	 * subcategorize() above. Same linear-scan-dedup + manual insertion
	 * sort technique as collectCategories() above (sub-category counts
	 * are even smaller than top-category counts -- at most a handful
	 * per category, per the base client's own schematic_group.iff --
	 * e.g. Bio Engineer splits into Dna Template/Creature/Bio Component,
	 * while most other categories only ever produce "General" plus
	 * "Component" wherever the base client actually has a component/
	 * subfolder). Callers only show the new Step 2b picker when this
	 * returns 2 or more entries -- see
	 * CompanionCraftCategoryPickSuiCallback::run() and
	 * openCompanionCraftSubcategoryPick() further down this file. */
	static void collectSubcategories(CompanionObject* comp, const String& category, Vector<String>& subcategories) {
		Vector<String> rawPaths;
		Vector<String> rawLabels;
		collectKnownSchematics(comp, rawPaths, rawLabels);

		for (int i = 0; i < rawPaths.size(); ++i) {
			if (categorize(rawPaths.get(i)) != category) {
				continue;
			}

			String subcategory = subcategorize(rawPaths.get(i));
			bool found = false;

			for (int c = 0; c < subcategories.size(); ++c) {
				if (subcategories.get(c) == subcategory) {
					found = true;
					break;
				}
			}

			if (!found) {
				subcategories.add(subcategory);
			}
		}

		for (int i = 1; i < subcategories.size(); ++i) {
			String key = subcategories.get(i);
			String keyLower = key.toLowerCase();
			int j = i - 1;

			while (j >= 0 && subcategories.get(j).toLowerCase().compareTo(keyLower) > 0) {
				subcategories.set(j + 1, subcategories.get(j));
				--j;
			}

			subcategories.set(j + 1, key);
		}
	}

	/** Companion System (2026-07-28, real two-step drill-down): Step 2
	 * data -- every known schematic whose categorize() matches `category`,
	 * alphabetized by display label. Replaces the old buildCraftMenu(),
	 * which built one giant flat list with fake non-clickable header rows
	 * for ALL categories at once; this only ever returns ONE category's
	 * items, with no header rows at all (the category itself is now a
	 * real, separate Step-1 SUI window, not an inline label). Preserves
	 * the 2026-07-28 no-cap fix -- there is still no row-count cap of any
	 * kind here. Sort is the same manual insertion sort technique as
	 * before, just applied to the filtered subset. */
	static void buildCraftMenuForCategory(CompanionObject* comp, const String& category, Vector<String>& paths, Vector<String>& labels) {
		Vector<String> rawPaths;
		Vector<String> rawLabels;
		collectKnownSchematics(comp, rawPaths, rawLabels);

		Vector<int> matching;

		for (int i = 0; i < rawPaths.size(); ++i) {
			if (categorize(rawPaths.get(i)) == category) {
				matching.add(i);
			}
		}

		for (int i = 1; i < matching.size(); ++i) {
			int key = matching.get(i);
			String keyLabel = rawLabels.get(key).toLowerCase();
			int j = i - 1;

			while (j >= 0 && rawLabels.get(matching.get(j)).toLowerCase().compareTo(keyLabel) > 0) {
				matching.set(j + 1, matching.get(j));
				--j;
			}

			matching.set(j + 1, key);
		}

		for (int i = 0; i < matching.size(); ++i) {
			int idx = matching.get(i);
			paths.add(rawPaths.get(idx));
			labels.add(rawLabels.get(idx));
		}
	}

	/** Companion System (2026-07-28, "can we make sub sub categories"
	 * per Nick): Step 3 data (renumbered from the old Step 2 now that a
	 * real sub-category step can sit between the category list and the
	 * item list) -- every known schematic whose categorize() matches
	 * `category` AND whose subcategorize() matches `subcategory`,
	 * alphabetized by display label. Same filtered-copy-of-
	 * buildCraftMenuForCategory() shape as that function is itself a
	 * filtered copy of the original flat-list builder -- consistent
	 * with this file's existing style of small duplicated filter+sort
	 * passes rather than a shared generic predicate helper. */
	static void buildCraftMenuForSubcategory(CompanionObject* comp, const String& category, const String& subcategory, Vector<String>& paths, Vector<String>& labels) {
		Vector<String> rawPaths;
		Vector<String> rawLabels;
		collectKnownSchematics(comp, rawPaths, rawLabels);

		Vector<int> matching;

		for (int i = 0; i < rawPaths.size(); ++i) {
			if (categorize(rawPaths.get(i)) == category && subcategorize(rawPaths.get(i)) == subcategory) {
				matching.add(i);
			}
		}

		for (int i = 1; i < matching.size(); ++i) {
			int key = matching.get(i);
			String keyLabel = rawLabels.get(key).toLowerCase();
			int j = i - 1;

			while (j >= 0 && rawLabels.get(matching.get(j)).toLowerCase().compareTo(keyLabel) > 0) {
				matching.set(j + 1, matching.get(j));
				--j;
			}

			matching.set(j + 1, key);
		}

		for (int i = 0; i < matching.size(); ++i) {
			int idx = matching.get(i);
			paths.add(rawPaths.get(idx));
			labels.add(rawLabels.get(idx));
		}
	}

	/** Companion System (2026-07-28, category drill-down per Nick: "click
	 * into a weapon sub category"). Step 1 -- shows just the category
	 * NAMES. Body is out-of-line (after
	 * CompanionCraftCategoryPickSuiCallback is fully defined, same reason
	 * sendTestResourceList()/sendBatchCraftList() below are already
	 * out-of-line: constructing that callback needs its complete type). */
	static void sendCraftList(CreatureObject* player, CompanionObject* comp);

	/** Companion System (2026-07-28, category drill-down): Step 2 -- the
	 * actual item list, filtered to one category and alphabetized. Only
	 * constructs this same (already-complete) class, so unlike the Step-1
	 * openers this can stay inline, same as the old flat-list
	 * sendCraftList() used to be. */
	static void sendCraftItemList(CreatureObject* player, CompanionObject* comp, const String& category) {
		if (player == nullptr || comp == nullptr) {
			return;
		}

		ManagedReference<PlayerObject*> ghost = player->getPlayerObject();

		if (ghost == nullptr) {
			return;
		}

		Vector<String> paths;
		Vector<String> labels;
		buildCraftMenuForCategory(comp, category, paths, labels);

		if (paths.size() == 0) {
			// Category emptied out from under us (companion untrained
			// mid-browse, etc.) -- fall back to the category list rather
			// than showing an empty picker.
			sendCraftList(player, comp);
			return;
		}

		ghost->closeSuiWindowType(SuiWindowType::COMPANION_CRAFT_PICK);

		ManagedReference<SuiListBox*> sui = new SuiListBox(player, SuiWindowType::COMPANION_CRAFT_PICK);
		sui->setPromptTitle(comp->getDisplayedName() + " -=COMPANION=- : Crafting -- " + category);
		sui->setPromptText("What should your companion craft? Materials come from its own stock (bag, your harvesters, or resource deeds). Results are always crafted at maximum quality.");
		sui->setCancelButton(true, "@ui:cancel");
		sui->setOkButton(true, "@ui:ok");
		sui->setCallback(new CompanionCraftPickSuiCallback(player->getZoneServer(), comp, paths));

		for (int i = 0; i < labels.size(); ++i) {
			sui->addMenuItem(labels.get(i));
		}

		ghost->addSuiBox(sui);
		player->sendMessage(sui->generateMessage());
	}

	/** Companion System (2026-07-28, "can we make sub sub categories"
	 * per Nick): Step 3 (was Step 2) -- item list filtered to one
	 * category AND one sub-category. Only used for categories where
	 * collectSubcategories() found 2+ real buckets (see
	 * CompanionCraftCategoryPickSuiCallback::run() and
	 * openCompanionCraftSubcategoryPick() further down this file) --
	 * everything else keeps using the 3-argument-free
	 * sendCraftItemList() above, unchanged. */
	static void sendCraftItemList(CreatureObject* player, CompanionObject* comp, const String& category, const String& subcategory) {
		if (player == nullptr || comp == nullptr) {
			return;
		}

		ManagedReference<PlayerObject*> ghost = player->getPlayerObject();

		if (ghost == nullptr) {
			return;
		}

		Vector<String> paths;
		Vector<String> labels;
		buildCraftMenuForSubcategory(comp, category, subcategory, paths, labels);

		if (paths.size() == 0) {
			// Sub-category emptied out from under us -- fall back to the
			// whole category's item list rather than showing an empty
			// picker (mirrors the existing empty-category fallback above).
			sendCraftItemList(player, comp, category);
			return;
		}

		ghost->closeSuiWindowType(SuiWindowType::COMPANION_CRAFT_PICK);

		ManagedReference<SuiListBox*> sui = new SuiListBox(player, SuiWindowType::COMPANION_CRAFT_PICK);
		sui->setPromptTitle(comp->getDisplayedName() + " -=COMPANION=- : Crafting -- " + category + " -- " + subcategory);
		sui->setPromptText("What should your companion craft? Materials come from its own stock (bag, your harvesters, or resource deeds). Results are always crafted at maximum quality.");
		sui->setCancelButton(true, "@ui:cancel");
		sui->setOkButton(true, "@ui:ok");
		sui->setCallback(new CompanionCraftPickSuiCallback(player->getZoneServer(), comp, paths));

		for (int i = 0; i < labels.size(); ++i) {
			sui->addMenuItem(labels.get(i));
		}

		ghost->addSuiBox(sui);
		player->sendMessage(sui->generateMessage());
	}

	/** Companion System (2026-07-24, "test resources" radial; 2026-07-28
	 * category drill-down). Step 1 -- category names only. Body is
	 * out-of-line (after CompanionCraftCategoryPickSuiCallback is fully
	 * defined). */
	static void sendTestResourceList(CreatureObject* player, CompanionObject* comp);

	/** Companion System (2026-07-28, category drill-down): Step 2 --
	 * filtered, alphabetized item list for one category; selecting an
	 * item still fills a test bag (see
	 * CompanionCraftTestResourcePickSuiCallback below). Body is
	 * out-of-line (after that class is fully defined). */
	static void sendTestResourceItemList(CreatureObject* player, CompanionObject* comp, const String& category);

	/** Companion System (2026-07-28, "can we make sub sub categories"):
	 * Step 3 (was Step 2) -- filtered to one category AND one
	 * sub-category. Body out-of-line for the same reason as the 2-arg
	 * version above; the test-resource callback class isn't complete
	 * yet at this point in the file. */
	static void sendTestResourceItemList(CreatureObject* player, CompanionObject* comp, const String& category, const String& subcategory);

	/** Companion System (2026-07-27, "factory runs" per Nick; 2026-07-28
	 * category drill-down). Step 1 -- category names only. Body is
	 * out-of-line (after CompanionCraftCategoryPickSuiCallback is fully
	 * defined). */
	static void sendBatchCraftList(CreatureObject* player, CompanionObject* comp);

	/** Companion System (2026-07-28, category drill-down): Step 2 --
	 * filtered, alphabetized item list for one category; selecting an
	 * item still asks for a quantity (SuiInputBox) then hands off to
	 * CompanionCraftingManager::craftBatch() (see
	 * CompanionCraftBatchPickSuiCallback below). Body is out-of-line --
	 * needs that class fully defined first. */
	static void sendBatchCraftItemList(CreatureObject* player, CompanionObject* comp, const String& category);

	/** Companion System (2026-07-28, "can we make sub sub categories"):
	 * Step 3 (was Step 2) -- filtered to one category AND one
	 * sub-category. Body out-of-line for the same reason as the 2-arg
	 * version above; the batch picker callback class isn't complete
	 * yet at this point in the file. */
	static void sendBatchCraftItemList(CreatureObject* player, CompanionObject* comp, const String& category, const String& subcategory);

	void run(CreatureObject* player, SuiBox* suiBox, uint32 eventIndex, Vector<UnicodeString>* args) {
		if (eventIndex == 1 || player == nullptr || args == nullptr || args->size() <= 0) {
			return;
		}

		int menuSelection = Integer::valueOf(args->get(0).toString());

		if (menuSelection < 0 || menuSelection >= schematicPaths.size()) {
			return;
		}

		ManagedReference<CompanionObject*> strongCompanion = companion;

		if (strongCompanion == nullptr) {
			return;
		}

		String chosenPath = schematicPaths.get(menuSelection);

		// Companion System (2026-07-28 category drill-down): buildCraftMenuForCategory()
		// no longer emits fake header rows (the list is now a real
		// drill-down, not one flat list with headers), so schematicPaths
		// should never actually be empty here -- this guard is kept
		// defensively rather than removed, at effectively zero cost.
		if (chosenPath.isEmpty()) {
			return;
		}

		// Optimize-for step (2026-07-20): if the schematic has more than one
		// experimental line (a harvester's Extraction Rate vs Hopper Size)
		// and the player hasn't chosen which to optimize for yet, ask ONCE
		// -- the answer is remembered per schematic and steers resource-deed
		// picking toward that line. Single-line (or already-chosen)
		// schematics craft straight through.
		if (maybeAskOptimizeLine(player, strongCompanion, chosenPath)) {
			return; // the optimize picker will resume the craft on selection
		}

		performCraft(player, strongCompanion, chosenPath);
	}

	static void performCraft(CreatureObject* player, CompanionObject* strongCompanion, const String& chosenPath) {
		// 2026-07-20 (user request "I want ALL crafting to be a theater"):
		// route every craft through CompanionCraftTheater -- it walks
		// squad-mates over to trade the EXACT materials needed (with chat)
		// before the craft, or crafts straight away if the companion
		// already has everything. craftItem() runs inside the theater.
		{
			Locker clocker(strongCompanion, player);
			// Companion System (2026-07-27, per Nick: "we need a crafting droid
			// for all our crafting companions" -- this is the general craft
			// entry point EVERY crafting profession goes through, not just
			// armorsmiths). CompanionFieldStation::begin() passes straight
			// through to CompanionCraftTheater::begin() (identical signature)
			// when no station is needed or one's already available -- only
			// runs the droid request/build/handoff theater first when the real
			// schematic complexity actually requires one.
			CompanionFieldStation::begin(player, strongCompanion, chosenPath);
		}

		// Craft another? Re-open the same view (category, or
		// category+sub-category if this category has real
		// sub-categories -- 2026-07-28 "sub sub categories" per Nick),
		// not the category/sub-category picker itself -- keeps this a
		// one-click loop, matching the old flat list's reopen behavior,
		// per approved GUI spec #5.
		reopenAfterAction(player, strongCompanion, chosenPath, COMPANION_CRAFT_MENU_MODE_CRAFT);
	}

	/** Companion System (2026-07-28, "can we make sub sub categories"
	 * per Nick): shared "reopen the same view after an action" helper
	 * for the two flows that loop back into a list (performCraft()
	 * above, and CompanionCraftTestResourcePickSuiCallback::run()
	 * further down -- batch doesn't reopen anything, see that
	 * callback's own doc comment, unchanged). Recomputes
	 * categorize(chosenPath) and, if that category turns out to have
	 * 2+ real sub-categories, subcategorize(chosenPath) too, straight
	 * from the just-crafted/tested item's own path -- the exact same
	 * "derive the view from the chosen item, don't carry window state
	 * around" trick this file's pre-existing reopen calls already used
	 * for the category alone, just extended one level deeper. Reopens
	 * the ITEM list directly (Step 3, or Step 2 for a flat category) --
	 * never re-shows the category or sub-category picker itself,
	 * matching the existing "craft/test another" one-click loop. */
	static void reopenAfterAction(CreatureObject* player, CompanionObject* comp, const String& chosenPath, int menuMode) {
		String category = categorize(chosenPath);
		Vector<String> subcategories;
		collectSubcategories(comp, category, subcategories);

		bool hasSubcategories = subcategories.size() >= 2;
		String subcategory = hasSubcategories ? subcategorize(chosenPath) : String();

		switch (menuMode) {
		case COMPANION_CRAFT_MENU_MODE_TEST_RESOURCE:
			if (hasSubcategories) {
				sendTestResourceItemList(player, comp, category, subcategory);
			} else {
				sendTestResourceItemList(player, comp, category);
			}
			break;
		case COMPANION_CRAFT_MENU_MODE_BATCH:
			if (hasSubcategories) {
				sendBatchCraftItemList(player, comp, category, subcategory);
			} else {
				sendBatchCraftItemList(player, comp, category);
			}
			break;
		case COMPANION_CRAFT_MENU_MODE_CRAFT:
		default:
			if (hasSubcategories) {
				sendCraftItemList(player, comp, category, subcategory);
			} else {
				sendCraftItemList(player, comp, category);
			}
			break;
		}
	}

	/** Returns true if an optimize-for picker was shown (craft deferred).
	 * Body out-of-line below -- it constructs CompanionCraftOptimizeSuiCallback,
	 * which is defined after this class. */
	static bool maybeAskOptimizeLine(CreatureObject* player, CompanionObject* comp, const String& schematicPath);

};

// Optimize-for line picker callback (2026-07-20).
class CompanionCraftOptimizeSuiCallback : public SuiCallback {
	ManagedReference<CompanionObject*> companion;
	String schematicPath;
	uint32 schematicCRC;

public:
	CompanionCraftOptimizeSuiCallback(ZoneServer* server, CompanionObject* comp, const String& path, uint32 crc)
		: SuiCallback(server), companion(comp), schematicPath(path), schematicCRC(crc) {
	}

	void run(CreatureObject* player, SuiBox* suiBox, uint32 eventIndex, Vector<UnicodeString>* args) {
		if (player == nullptr) {
			return;
		}

		ManagedReference<CompanionObject*> strongCompanion = companion;

		if (strongCompanion == nullptr) {
			return;
		}

		// Cancelled -> craft anyway with generic best-quality (no
		// preference stored, so it'll ask again next time).
		if (eventIndex == 1 || args == nullptr || args->size() <= 0) {
			CompanionCraftPickSuiCallback::performCraft(player, strongCompanion, schematicPath);
			return;
		}

		int lineIndex = Integer::valueOf(args->get(0).toString());

		CompanionCraftingManager::instance()->setPreferredLine(player->getObjectID(), schematicCRC, lineIndex);
		player->sendSystemMessage("Your companion will favor the best resources for that quality when crafting this item.");

		CompanionCraftPickSuiCallback::performCraft(player, strongCompanion, schematicPath);
	}

};

// Category picker callback (2026-07-28, "click into a weapon sub category,
// then vehicles, etc." per Nick) -- real two-step drill-down replacing the
// flat list + fake "========== Weapon ==========" header rows added
// earlier the same day. Shown first for all three entry points
// (sendCraftList()/sendTestResourceList()/sendBatchCraftList()); picking a
// category re-opens the matching item list (sendCraftItemList()/
// sendTestResourceItemList()/sendBatchCraftItemList()) filtered to just
// that category. menuMode remembers which of the three flows asked for
// the category list -- stored as a plain member the same way
// CompanionCraftOptimizeSuiCallback above stores schematicPath/
// schematicCRC (this project's existing precedent for threading state
// between two sequential SUI windows), since engine3 constructs a
// brand-new callback instance per SUI window -- there is no shared/global
// state to lean on instead.
//
// Cancel here closes and does nothing -- identical to this picker's
// Cancel behavior before this patch. Cancel from the Step 2 item list
// (below, inside CompanionCraftPickSuiCallback::run() and the other two
// run() methods further down) also just closes rather than returning to
// this category list -- deliberately the simpler of the two options
// Nick's ask allowed for: wiring a real "back" means every Step-2
// callback also has to carry menuMode+categories, which is a materially
// bigger change than the drill-down itself. Left for a follow-up if Nick
// asks for it specifically.
//
// Forward declaration (2026-07-28, "sub sub categories" per Nick):
// CompanionCraftCategoryPickSuiCallback::run() just below needs to call
// this opener when a chosen category turns out to have 2+ real
// sub-categories, but the opener's body can't be written until
// CompanionCraftSubcategoryPickSuiCallback (the new Step 2b class) is
// fully defined -- same declare-early/define-late split this file
// already uses for sendCraftList()/sendTestResourceList()/
// sendBatchCraftList() and their shared Step-1 opener below.
inline void openCompanionCraftSubcategoryPick(CreatureObject* player, CompanionObject* comp, int menuMode, const String& category, const String& title, const String& promptText);

class CompanionCraftCategoryPickSuiCallback : public SuiCallback {
	ManagedReference<CompanionObject*> companion;
	int menuMode;
	Vector<String> categories;

public:
	CompanionCraftCategoryPickSuiCallback(ZoneServer* server, CompanionObject* comp, int mode, const Vector<String>& cats)
		: SuiCallback(server) {
		companion = comp;
		menuMode = mode;
		categories = cats;
	}

	void run(CreatureObject* player, SuiBox* suiBox, uint32 eventIndex, Vector<UnicodeString>* args) {
		if (eventIndex == 1 || player == nullptr || args == nullptr || args->size() <= 0) {
			return;
		}

		int menuSelection = Integer::valueOf(args->get(0).toString());

		if (menuSelection < 0 || menuSelection >= categories.size()) {
			return;
		}

		ManagedReference<CompanionObject*> strongCompanion = companion;

		if (strongCompanion == nullptr) {
			return;
		}

		String chosenCategory = categories.get(menuSelection);

		// Companion System (2026-07-28, "can we make sub sub categories"
		// per Nick): only categories with 2+ real sub-categories get the
		// new Step 2b sub-picker -- see subcategorize()/
		// collectSubcategories() doc comments on CompanionCraftPickSuiCallback
		// for how "real" is determined. A flat category (0 or 1 sub-buckets)
		// falls straight through to the existing item list below, exactly
		// as before this patch -- no pointless single-option click.
		Vector<String> subcategories;
		CompanionCraftPickSuiCallback::collectSubcategories(strongCompanion, chosenCategory, subcategories);

		if (subcategories.size() >= 2) {
			String title;
			String promptText;

			switch (menuMode) {
			case COMPANION_CRAFT_MENU_MODE_TEST_RESOURCE:
				title = strongCompanion->getDisplayedName() + " -=COMPANION=- : Test Resources -- " + chosenCategory;
				promptText = "Pick a sub-category to fill a test resource bag for one of its items.";
				break;
			case COMPANION_CRAFT_MENU_MODE_BATCH:
				title = strongCompanion->getDisplayedName() + " -=COMPANION=- : Factory Run -- " + chosenCategory;
				promptText = "Pick a sub-category to mass-produce one of its items.";
				break;
			case COMPANION_CRAFT_MENU_MODE_CRAFT:
			default:
				title = strongCompanion->getDisplayedName() + " -=COMPANION=- : Crafting -- " + chosenCategory;
				promptText = "Pick a sub-category to see what your companion can craft.";
				break;
			}

			openCompanionCraftSubcategoryPick(player, strongCompanion, menuMode, chosenCategory, title, promptText);
			return;
		}

		switch (menuMode) {
		case COMPANION_CRAFT_MENU_MODE_TEST_RESOURCE:
			CompanionCraftPickSuiCallback::sendTestResourceItemList(player, strongCompanion, chosenCategory);
			break;
		case COMPANION_CRAFT_MENU_MODE_BATCH:
			CompanionCraftPickSuiCallback::sendBatchCraftItemList(player, strongCompanion, chosenCategory);
			break;
		case COMPANION_CRAFT_MENU_MODE_CRAFT:
		default:
			CompanionCraftPickSuiCallback::sendCraftItemList(player, strongCompanion, chosenCategory);
			break;
		}
	}

};

// Shared Step-1 opener (2026-07-28): builds the sorted category list and
// shows it as a SuiListBox under COMPANION_CRAFT_CATEGORY_PICK
// (SuiWindowType.h -- ID 1223, added 2026-07-24 for this exact purpose but
// left unused until now, so this patch needs NO SuiWindowType.h edit).
// Out-of-line (not a CompanionCraftPickSuiCallback member) because it
// constructs CompanionCraftCategoryPickSuiCallback, which must be a
// complete type first; a free inline function is simplest here since all
// three sendXList() callers below are themselves already out-of-line for
// the same reason.
inline void openCompanionCraftCategoryPick(CreatureObject* player, CompanionObject* comp, int menuMode, const String& title, const String& promptText) {
	if (player == nullptr || comp == nullptr) {
		return;
	}

	ManagedReference<PlayerObject*> ghost = player->getPlayerObject();

	if (ghost == nullptr) {
		return;
	}

	Vector<String> categories;
	CompanionCraftPickSuiCallback::collectCategories(comp, categories);

	if (categories.size() == 0) {
		player->sendSystemMessage("Your companion doesn't know how to craft anything yet -- train it in a crafting profession first.");
		return;
	}

	ghost->closeSuiWindowType(SuiWindowType::COMPANION_CRAFT_CATEGORY_PICK);

	ManagedReference<SuiListBox*> sui = new SuiListBox(player, SuiWindowType::COMPANION_CRAFT_CATEGORY_PICK);
	sui->setPromptTitle(title);
	sui->setPromptText(promptText);
	sui->setCancelButton(true, "@ui:cancel");
	sui->setOkButton(true, "@ui:ok");
	sui->setCallback(new CompanionCraftCategoryPickSuiCallback(player->getZoneServer(), comp, menuMode, categories));

	for (int i = 0; i < categories.size(); ++i) {
		sui->addMenuItem(categories.get(i));
	}

	ghost->addSuiBox(sui);
	player->sendMessage(sui->generateMessage());
}

inline void CompanionCraftPickSuiCallback::sendCraftList(CreatureObject* player, CompanionObject* comp) {
	if (comp == nullptr) {
		return;
	}

	openCompanionCraftCategoryPick(player, comp, COMPANION_CRAFT_MENU_MODE_CRAFT,
			comp->getDisplayedName() + " -=COMPANION=- : Crafting Categories",
			"Pick a category to see what your companion can craft.");
}

inline void CompanionCraftPickSuiCallback::sendTestResourceList(CreatureObject* player, CompanionObject* comp) {
	if (comp == nullptr) {
		return;
	}

	openCompanionCraftCategoryPick(player, comp, COMPANION_CRAFT_MENU_MODE_TEST_RESOURCE,
			comp->getDisplayedName() + " -=COMPANION=- : Test Resources -- Categories",
			"Pick a category to fill a test resource bag for one of its items.");
}

inline void CompanionCraftPickSuiCallback::sendBatchCraftList(CreatureObject* player, CompanionObject* comp) {
	if (comp == nullptr) {
		return;
	}

	openCompanionCraftCategoryPick(player, comp, COMPANION_CRAFT_MENU_MODE_BATCH,
			comp->getDisplayedName() + " -=COMPANION=- : Factory Run -- Categories",
			"Pick a category to mass-produce one of its items.");
}

// Sub-category picker callback (2026-07-28, "can we make sub sub
// categories" per Nick, verbatim -- the third drill-down level, shown
// ONLY for categories with 2+ real sub-categories; see
// CompanionCraftPickSuiCallback::subcategorize()/collectSubcategories()
// for how "real" is determined from the base client's own
// object/draft_schematic/<top>/<sub>/... path segments, verified against
// a real extraction of datatables/crafting/schematic_group.iff). Reuses
// the exact same SuiWindowType::COMPANION_CRAFT_CATEGORY_PICK window
// type as the Step-1 category picker above (per Nick's ask: reuse, don't
// invent a new SuiWindowType) and the same menuMode plumbing pattern.
// Cancel here closes and does nothing -- identical deliberate
// simplification as Step 1's Cancel and Step 2/3's Cancel, see
// CompanionCraftCategoryPickSuiCallback's own doc comment above for the
// reasoning (a real "back" button would need every step to carry the
// full chain of state, a materially bigger change than the drill-down
// itself).
class CompanionCraftSubcategoryPickSuiCallback : public SuiCallback {
	ManagedReference<CompanionObject*> companion;
	int menuMode;
	String category;
	Vector<String> subcategories;

public:
	CompanionCraftSubcategoryPickSuiCallback(ZoneServer* server, CompanionObject* comp, int mode, const String& cat, const Vector<String>& subs)
		: SuiCallback(server) {
		companion = comp;
		menuMode = mode;
		category = cat;
		subcategories = subs;
	}

	void run(CreatureObject* player, SuiBox* suiBox, uint32 eventIndex, Vector<UnicodeString>* args) {
		if (eventIndex == 1 || player == nullptr || args == nullptr || args->size() <= 0) {
			return;
		}

		int menuSelection = Integer::valueOf(args->get(0).toString());

		if (menuSelection < 0 || menuSelection >= subcategories.size()) {
			return;
		}

		ManagedReference<CompanionObject*> strongCompanion = companion;

		if (strongCompanion == nullptr) {
			return;
		}

		String chosenSubcategory = subcategories.get(menuSelection);

		switch (menuMode) {
		case COMPANION_CRAFT_MENU_MODE_TEST_RESOURCE:
			CompanionCraftPickSuiCallback::sendTestResourceItemList(player, strongCompanion, category, chosenSubcategory);
			break;
		case COMPANION_CRAFT_MENU_MODE_BATCH:
			CompanionCraftPickSuiCallback::sendBatchCraftItemList(player, strongCompanion, category, chosenSubcategory);
			break;
		case COMPANION_CRAFT_MENU_MODE_CRAFT:
		default:
			CompanionCraftPickSuiCallback::sendCraftItemList(player, strongCompanion, category, chosenSubcategory);
			break;
		}
	}

};

// Shared Step-2b opener (2026-07-28, "sub sub categories" per Nick) --
// mirrors openCompanionCraftCategoryPick() above, one level deeper:
// builds the sorted sub-category list for one already-chosen top
// category and shows it under the SAME
// SuiWindowType::COMPANION_CRAFT_CATEGORY_PICK window type (reused per
// Nick's ask, not a new SuiWindowType). Defines the forward-declared
// prototype above CompanionCraftCategoryPickSuiCallback -- out-of-line
// for the same reason as that declaration's own comment: constructs
// CompanionCraftSubcategoryPickSuiCallback, which must be a complete
// type first.
inline void openCompanionCraftSubcategoryPick(CreatureObject* player, CompanionObject* comp, int menuMode, const String& category, const String& title, const String& promptText) {
	if (player == nullptr || comp == nullptr) {
		return;
	}

	ManagedReference<PlayerObject*> ghost = player->getPlayerObject();

	if (ghost == nullptr) {
		return;
	}

	Vector<String> subcategories;
	CompanionCraftPickSuiCallback::collectSubcategories(comp, category, subcategories);

	if (subcategories.size() < 2) {
		// Sub-categories evaporated from under us (companion untrained
		// mid-browse, etc.) -- fall straight through to the item list
		// rather than showing a pointless single-option (or empty) picker.
		switch (menuMode) {
		case COMPANION_CRAFT_MENU_MODE_TEST_RESOURCE:
			CompanionCraftPickSuiCallback::sendTestResourceItemList(player, comp, category);
			break;
		case COMPANION_CRAFT_MENU_MODE_BATCH:
			CompanionCraftPickSuiCallback::sendBatchCraftItemList(player, comp, category);
			break;
		case COMPANION_CRAFT_MENU_MODE_CRAFT:
		default:
			CompanionCraftPickSuiCallback::sendCraftItemList(player, comp, category);
			break;
		}
		return;
	}

	ghost->closeSuiWindowType(SuiWindowType::COMPANION_CRAFT_CATEGORY_PICK);

	ManagedReference<SuiListBox*> sui = new SuiListBox(player, SuiWindowType::COMPANION_CRAFT_CATEGORY_PICK);
	sui->setPromptTitle(title);
	sui->setPromptText(promptText);
	sui->setCancelButton(true, "@ui:cancel");
	sui->setOkButton(true, "@ui:ok");
	sui->setCallback(new CompanionCraftSubcategoryPickSuiCallback(player->getZoneServer(), comp, menuMode, category, subcategories));

	for (int i = 0; i < subcategories.size(); ++i) {
		sui->addMenuItem(subcategories.get(i));
	}

	ghost->addSuiBox(sui);
	player->sendMessage(sui->generateMessage());
}

// Test-resource picker callback (2026-07-24). Same menu selection flow as
// CompanionCraftPickSuiCallback, but run() hands off to
// CompanionCraftingManager::giveTestResourceBag() instead of crafting.
class CompanionCraftTestResourcePickSuiCallback : public SuiCallback {
	ManagedReference<CompanionObject*> companion;
	Vector<String> schematicPaths;

public:
	CompanionCraftTestResourcePickSuiCallback(ZoneServer* server, CompanionObject* comp, const Vector<String>& paths)
		: SuiCallback(server) {
		companion = comp;
		schematicPaths = paths;
	}

	void run(CreatureObject* player, SuiBox* suiBox, uint32 eventIndex, Vector<UnicodeString>* args) {
		if (eventIndex == 1 || player == nullptr || args == nullptr || args->size() <= 0) {
			return;
		}

		int menuSelection = Integer::valueOf(args->get(0).toString());

		if (menuSelection < 0 || menuSelection >= schematicPaths.size()) {
			return;
		}

		String chosenPath = schematicPaths.get(menuSelection);

		// Companion System (2026-07-28 category drill-down): see the same
		// guard (and why it's now purely defensive, not load-bearing) in
		// CompanionCraftPickSuiCallback::run() above.
		if (chosenPath.isEmpty()) {
			return;
		}

		String errorMessage;

		// 2026-07-24 fix: pass the companion through so giveTestResourceBag()
		// can also check ITS inventory (not just the owner's) for an
		// already-banked resource that beats anything currently spawning --
		// see the method's own doc comment in CompanionCraftingManager.h.
		ManagedReference<CompanionObject*> strongCompanion = companion;

		bool ok = CompanionCraftingManager::instance()->giveTestResourceBag(player, strongCompanion, chosenPath, errorMessage);

		if (!ok) {
			player->sendSystemMessage("Couldn't fill a test bag: " + errorMessage);
		}

		// Test another item? Re-open the same view (category, or
		// category+sub-category if this category has real
		// sub-categories -- 2026-07-28 "sub sub categories" per Nick) --
		// keeps this a one-click loop, same as before.
		if (strongCompanion != nullptr) {
			CompanionCraftPickSuiCallback::reopenAfterAction(player, strongCompanion, chosenPath, COMPANION_CRAFT_MENU_MODE_TEST_RESOURCE);
		}
	}

};

// Out-of-line: needs the complete CompanionCraftOptimizeSuiCallback above.
inline bool CompanionCraftPickSuiCallback::maybeAskOptimizeLine(CreatureObject* player, CompanionObject* comp, const String& schematicPath) {
	ManagedReference<PlayerObject*> ghost = player->getPlayerObject();
	ZoneServer* zoneServer = player->getZoneServer();

	if (ghost == nullptr || zoneServer == nullptr) {
		return false;
	}

	String file = schematicPath;

	if (file.indexOf(".iff") == -1) {
		file = file + ".iff";
	}

	ManagedReference<DraftSchematic*> schematic = zoneServer->createObject(file.hashCode(), 0).castTo<DraftSchematic*>();

	if (schematic == nullptr) {
		return false;
	}

	uint32 schematicCRC = schematic->getServerObjectCRC();

	if (CompanionCraftingManager::instance()->getPreferredLine(player->getObjectID(), schematicCRC) >= 0) {
		// Already have a stored preference for this exact schematic -- 2026-07-29
		// fix: the original code returned here WITHOUT ever destroying the
		// throwaway `schematic` object it just created, leaking one orphan
		// DraftSchematic into the database every time this was called. Same
		// destroyObjectFromDatabase(true) idiom CompanionCraftingManager.cpp
		// already uses everywhere for its own throwaway schematics.
		schematic->destroyObjectFromDatabase(true);
		return false;
	}

	// Companion System (2026-07-29, "too many options / nothing makes a
	// difference" fix per Nick). This list used to be built straight from
	// the schematic's raw ResourceWeight rows (getResourceWeightCount() /
	// getExperimentalTitle()) -- one entry per raw (group,attribute) pair,
	// NOT deduplicated, NOT in the same order the real crafting tool uses.
	// The auto-experimentation loop in CompanionCraftingManager.cpp spends
	// points against craftingValues->getTotalVisibleAttributeGroups()/
	// getVisibleAttributeGroup(i) -- a DEDUPLICATED, differently-ordered
	// list built by AttributesMap::addVisibleGroup(). Feeding a raw
	// ResourceWeight index into that as "preferredLine" meant the SUI could
	// show the same group name several times over, AND meant picking
	// anything past the real (smaller) visible-group count silently failed
	// the `preferredLine < numberOfExperimentRows` bounds check and fell
	// back to the identical round-robin spend every time. Fix: build a REAL,
	// throwaway ManufactureSchematic the same way CompanionCraftingManager's
	// own crafting flow does (createManufactureSchematic() +
	// initializeSlotsForHeadlessCraft() (added 2026-07-20 for exactly this
	// "no client, no session" companion-crafting case), same Locker /
	// destroyObjectFromDatabase() lifecycle used throughout
	// CompanionCraftingManager.cpp) so the list comes from the exact same
	// CraftingValues the real experiment loop will read -- guaranteeing the
	// index picked here lines up with the row it actually controls.
	ManagedReference<ManufactureSchematic*> manufactureSchematic = schematic->createManufactureSchematic(nullptr).castTo<ManufactureSchematic*>();

	if (manufactureSchematic == nullptr) {
		schematic->destroyObjectFromDatabase(true);
		return false;
	}

	Locker manuLocker(manufactureSchematic);

	manufactureSchematic->initializeSlotsForHeadlessCraft();

	CraftingValues* craftingValues = manufactureSchematic->getCraftingValues();
	// genesis port: getTotalVisibleAttributeGroups() -> getVisibleExperimentalPropertyTitleSize().
	int lineCount = craftingValues != nullptr ? craftingValues->getVisibleExperimentalPropertyTitleSize() : 0;

	if (lineCount < 2) {
		manuLocker.release();
		manufactureSchematic->destroyObjectFromDatabase(true);
		schematic->destroyObjectFromDatabase(true);
		return false;
	}

	Vector<String> lineTitles;

	for (int i = 0; i < lineCount; ++i) {
		// genesis port: getVisibleAttributeGroup(i) -> getVisibleExperimentalPropertyTitle(i)
		// (genesis returns const String&; same visible experimental-property row label).
		String title = craftingValues->getVisibleExperimentalPropertyTitle(i);

		if (title.isEmpty()) {
			title = "Property line " + String::valueOf(i + 1);
		}

		if (title.beginsWith("@")) {
			String resolved = StringIdManager::instance()->getStringId(title.subString(1).hashCode()).toString();

			if (!resolved.isEmpty()) {
				title = resolved;
			}
		}

		lineTitles.add(title);
	}

	// Done reading everything we need out of the throwaway crafting
	// objects -- clean them up now (same idiom/order as
	// CompanionCraftingManager.cpp's own end-of-craft cleanup) rather than
	// holding them alive through the SUI send below.
	manuLocker.release();
	manufactureSchematic->destroyObjectFromDatabase(true);
	schematic->destroyObjectFromDatabase(true);

	ghost->closeSuiWindowType(SuiWindowType::COMPANION_CRAFT_OPTIMIZE);

	ManagedReference<SuiListBox*> sui = new SuiListBox(player, SuiWindowType::COMPANION_CRAFT_OPTIMIZE);
	sui->setPromptTitle(comp->getDisplayedName() + " -=COMPANION=- : Optimize For");
	sui->setPromptText("This item has multiple qualities you can favor. Which should your companion pick the best resources for? (Remembered for this item.)");
	sui->setCancelButton(true, "@ui:cancel");
	sui->setOkButton(true, "@ui:ok");
	sui->setCallback(new CompanionCraftOptimizeSuiCallback(zoneServer, comp, schematicPath, schematicCRC));

	for (int i = 0; i < lineTitles.size(); ++i) {
		sui->addMenuItem(lineTitles.get(i));
	}

	ghost->addSuiBox(sui);
	player->sendMessage(sui->generateMessage());
	return true;
}

// Out-of-line: needs CompanionCraftTestResourcePickSuiCallback fully
// defined. Step 2 (2026-07-28 category drill-down) -- filtered to one
// category.
inline void CompanionCraftPickSuiCallback::sendTestResourceItemList(CreatureObject* player, CompanionObject* comp, const String& category) {
	if (player == nullptr || comp == nullptr) {
		return;
	}

	ManagedReference<PlayerObject*> ghost = player->getPlayerObject();

	if (ghost == nullptr) {
		return;
	}

	Vector<String> paths;
	Vector<String> labels;
	buildCraftMenuForCategory(comp, category, paths, labels);

	if (paths.size() == 0) {
		sendTestResourceList(player, comp);
		return;
	}

	ghost->closeSuiWindowType(SuiWindowType::COMPANION_TEST_RESOURCE_PICK);

	ManagedReference<SuiListBox*> sui = new SuiListBox(player, SuiWindowType::COMPANION_TEST_RESOURCE_PICK);
	sui->setPromptTitle(comp->getDisplayedName() + " -=COMPANION=- : Test Resources -- " + category);
	sui->setPromptText("Pick an item -- your companion's best currently-available resource for each material slot (30,000 units each) will be placed in a bag in your inventory, so you can craft it yourself and compare quality.");
	sui->setCancelButton(true, "@ui:cancel");
	sui->setOkButton(true, "@ui:ok");
	sui->setCallback(new CompanionCraftTestResourcePickSuiCallback(player->getZoneServer(), comp, paths));

	for (int i = 0; i < labels.size(); ++i) {
		sui->addMenuItem(labels.get(i));
	}

	ghost->addSuiBox(sui);
	player->sendMessage(sui->generateMessage());
}

// Out-of-line: needs CompanionCraftTestResourcePickSuiCallback fully
// defined (2026-07-28 "sub sub categories" per Nick) -- filtered to one
// category AND one sub-category; only reached when collectSubcategories()
// found 2+ real buckets.
inline void CompanionCraftPickSuiCallback::sendTestResourceItemList(CreatureObject* player, CompanionObject* comp, const String& category, const String& subcategory) {
	if (player == nullptr || comp == nullptr) {
		return;
	}

	ManagedReference<PlayerObject*> ghost = player->getPlayerObject();

	if (ghost == nullptr) {
		return;
	}

	Vector<String> paths;
	Vector<String> labels;
	buildCraftMenuForSubcategory(comp, category, subcategory, paths, labels);

	if (paths.size() == 0) {
		sendTestResourceItemList(player, comp, category);
		return;
	}

	ghost->closeSuiWindowType(SuiWindowType::COMPANION_TEST_RESOURCE_PICK);

	ManagedReference<SuiListBox*> sui = new SuiListBox(player, SuiWindowType::COMPANION_TEST_RESOURCE_PICK);
	sui->setPromptTitle(comp->getDisplayedName() + " -=COMPANION=- : Test Resources -- " + category + " -- " + subcategory);
	sui->setPromptText("Pick an item -- your companion's best currently-available resource for each material slot (30,000 units each) will be placed in a bag in your inventory, so you can craft it yourself and compare quality.");
	sui->setCancelButton(true, "@ui:cancel");
	sui->setOkButton(true, "@ui:ok");
	sui->setCallback(new CompanionCraftTestResourcePickSuiCallback(player->getZoneServer(), comp, paths));

	for (int i = 0; i < labels.size(); ++i) {
		sui->addMenuItem(labels.get(i));
	}

	ghost->addSuiBox(sui);
	player->sendMessage(sui->generateMessage());
}

// Batch quantity-input callback (2026-07-27, "factory runs" per Nick).
class CompanionCraftBatchQuantitySuiCallback : public SuiCallback {
	ManagedReference<CompanionObject*> companion;
	String schematicPath;

public:
	CompanionCraftBatchQuantitySuiCallback(ZoneServer* server, CompanionObject* comp, const String& path)
		: SuiCallback(server), companion(comp), schematicPath(path) {
	}

	void run(CreatureObject* player, SuiBox* suiBox, uint32 eventIndex, Vector<UnicodeString>* args) {
		if (eventIndex == 1 || player == nullptr || args == nullptr || args->size() <= 0) {
			return;
		}

		ManagedReference<CompanionObject*> strongCompanion = companion;

		if (strongCompanion == nullptr) {
			return;
		}

		int quantity = Integer::valueOf(args->get(0).toString().trim());

		if (quantity < 1) {
			player->sendSystemMessage("Enter a quantity of at least 1.");
			return;
		}

		String errorMessage;
		bool ok;

		{
			Locker clocker(strongCompanion, player);
			ok = CompanionCraftingManager::instance()->craftBatch(player, strongCompanion, schematicPath, quantity, errorMessage);
		}

		if (!ok) {
			player->sendSystemMessage(!errorMessage.isEmpty() ? errorMessage : "The factory run failed.");
		} else if (!errorMessage.isEmpty()) {
			// Non-empty errorMessage on a successful run is the "quantity was
			// capped" informational note -- see craftBatch()'s doc comment.
			player->sendSystemMessage(errorMessage);
		}
	}

};

// Batch picker (2026-07-27, "factory runs" per Nick). Same known-schematic
// list as the normal craft picker; selecting an item opens a quantity
// prompt instead of crafting immediately.
class CompanionCraftBatchPickSuiCallback : public SuiCallback {
	ManagedReference<CompanionObject*> companion;
	Vector<String> schematicPaths;

public:
	CompanionCraftBatchPickSuiCallback(ZoneServer* server, CompanionObject* comp, const Vector<String>& paths)
		: SuiCallback(server) {
		companion = comp;
		schematicPaths = paths;
	}

	void run(CreatureObject* player, SuiBox* suiBox, uint32 eventIndex, Vector<UnicodeString>* args) {
		if (eventIndex == 1 || player == nullptr || args == nullptr || args->size() <= 0) {
			return;
		}

		int menuSelection = Integer::valueOf(args->get(0).toString());

		if (menuSelection < 0 || menuSelection >= schematicPaths.size()) {
			return;
		}

		ManagedReference<CompanionObject*> strongCompanion = companion;

		if (strongCompanion == nullptr) {
			return;
		}

		String chosenPath = schematicPaths.get(menuSelection);

		// Companion System (2026-07-28 category drill-down): see the same
		// guard (and why it's now purely defensive) in
		// CompanionCraftPickSuiCallback::run() above.
		if (chosenPath.isEmpty()) {
			return;
		}

		ManagedReference<PlayerObject*> ghost = player->getPlayerObject();

		if (ghost == nullptr) {
			return;
		}

		ghost->closeSuiWindowType(SuiWindowType::COMPANION_CRAFT_BATCH_QUANTITY);

		ManagedReference<SuiInputBox*> inputBox = new SuiInputBox(player, SuiWindowType::COMPANION_CRAFT_BATCH_QUANTITY, 0x00);
		inputBox->setPromptTitle(strongCompanion->getDisplayedName() + " -=COMPANION=- : Factory Run");
		inputBox->setPromptText("How many should your companion make? Materials for ALL of them are drawn up front -- the run fails cleanly with nothing consumed if there isn't enough.");
		inputBox->setMaxInputSize(4);
		inputBox->setDefaultInput("10");
		inputBox->setCallback(new CompanionCraftBatchQuantitySuiCallback(player->getZoneServer(), strongCompanion, chosenPath));

		ghost->addSuiBox(inputBox);
		player->sendMessage(inputBox->generateMessage());
	}

};

// Out-of-line: needs CompanionCraftBatchPickSuiCallback fully defined.
// Step 2 (2026-07-28 category drill-down) -- filtered to one category.
inline void CompanionCraftPickSuiCallback::sendBatchCraftItemList(CreatureObject* player, CompanionObject* comp, const String& category) {
	if (player == nullptr || comp == nullptr) {
		return;
	}

	ManagedReference<PlayerObject*> ghost = player->getPlayerObject();

	if (ghost == nullptr) {
		return;
	}

	Vector<String> paths;
	Vector<String> labels;
	buildCraftMenuForCategory(comp, category, paths, labels);

	if (paths.size() == 0) {
		sendBatchCraftList(player, comp);
		return;
	}

	ghost->closeSuiWindowType(SuiWindowType::COMPANION_CRAFT_BATCH_PICK);

	ManagedReference<SuiListBox*> sui = new SuiListBox(player, SuiWindowType::COMPANION_CRAFT_BATCH_PICK);
	sui->setPromptTitle(comp->getDisplayedName() + " -=COMPANION=- : Factory Run -- " + category);
	sui->setPromptText("Pick an item to mass-produce. Your companion crafts one for real, then clones it -- quality is identical across the whole run, exactly like a real SWG factory. Not every item supports this (only ones with a real factory crate defined).");
	sui->setCancelButton(true, "@ui:cancel");
	sui->setOkButton(true, "@ui:ok");
	sui->setCallback(new CompanionCraftBatchPickSuiCallback(player->getZoneServer(), comp, paths));

	for (int i = 0; i < labels.size(); ++i) {
		sui->addMenuItem(labels.get(i));
	}

	ghost->addSuiBox(sui);
	player->sendMessage(sui->generateMessage());
}

// Out-of-line: needs CompanionCraftBatchPickSuiCallback fully defined
// (2026-07-28 "sub sub categories" per Nick) -- filtered to one category
// AND one sub-category; only reached when collectSubcategories() found
// 2+ real buckets.
inline void CompanionCraftPickSuiCallback::sendBatchCraftItemList(CreatureObject* player, CompanionObject* comp, const String& category, const String& subcategory) {
	if (player == nullptr || comp == nullptr) {
		return;
	}

	ManagedReference<PlayerObject*> ghost = player->getPlayerObject();

	if (ghost == nullptr) {
		return;
	}

	Vector<String> paths;
	Vector<String> labels;
	buildCraftMenuForSubcategory(comp, category, subcategory, paths, labels);

	if (paths.size() == 0) {
		sendBatchCraftItemList(player, comp, category);
		return;
	}

	ghost->closeSuiWindowType(SuiWindowType::COMPANION_CRAFT_BATCH_PICK);

	ManagedReference<SuiListBox*> sui = new SuiListBox(player, SuiWindowType::COMPANION_CRAFT_BATCH_PICK);
	sui->setPromptTitle(comp->getDisplayedName() + " -=COMPANION=- : Factory Run -- " + category + " -- " + subcategory);
	sui->setPromptText("Pick an item to mass-produce. Your companion crafts one for real, then clones it -- quality is identical across the whole run, exactly like a real SWG factory. Not every item supports this (only ones with a real factory crate defined).");
	sui->setCancelButton(true, "@ui:cancel");
	sui->setOkButton(true, "@ui:ok");
	sui->setCallback(new CompanionCraftBatchPickSuiCallback(player->getZoneServer(), comp, paths));

	for (int i = 0; i < labels.size(); ++i) {
		sui->addMenuItem(labels.get(i));
	}

	ghost->addSuiBox(sui);
	player->sendMessage(sui->generateMessage());
}

#endif // COMPANIONCRAFTPICKSUICALLBACK_H_
