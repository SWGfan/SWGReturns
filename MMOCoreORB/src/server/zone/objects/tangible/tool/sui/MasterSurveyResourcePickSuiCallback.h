/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Master Survey Tool (2026-07-29, Companion -- see NOTES.md "Master
	Survey Tool"). Resource-choice SUI callback for the new Master Survey
	Tool's "Master Survey: Scan for Hotspots" radial (see
	SurveyToolImplementation.cpp). Same SuiCallback/SuiListBox pick-by-
	index pattern already proven in this codebase's own
	CompanionTaxiWaypointSuiCallback.h.

	Once the player picks a resource name, this runs a wide-radius density
	grid scan via ResourceSpawner::scanForHotspots() (a new, purely
	additive method -- see patch_master_survey_tool_resourcespawner.py),
	keeps the top MAX_HOTSPOTS points at/above the stock "worth a
	waypoint" density floor (the same 0.10f threshold
	ResourceSpawner::sendSurvey() already uses), and drops one real
	waypoint per hotspot using the CONFIRMED waypoint-creation pattern
	already used by SurveySessionImplementation::surveyCnodeMinigame() and
	ResourceSpawner::sendSurvey() itself (same 0xc456e788 WaypointObject
	template CRC, same field-setting sequence). Unlike stock survey's
	single reusable "survey waypoint" slot (PlayerObject::
	getSurveyWaypoint()), this creates a FRESH waypoint every run and does
	not track/reuse them -- see NOTES.md's suggestions section for the
	known "waypoint list clutters up over repeated uses" follow-up.

	Deliberately does NOT charge mind/action cost or apply a pending-task
	cooldown -- the scan is a bounded number of cheap getDensityAt() calls
	(1681 coarse + at most 40 fine passes of 81), and Nick asked for a fast
	first cut to tweak later. See NOTES.md for tunables (SCAN_RANGE, SCAN_GRID_POINTS,
	MAX_HOTSPOTS, MIN_HOTSPOT_DENSITY).
*/

#ifndef MASTERSURVEYRESOURCEPICKSUICALLBACK_H_
#define MASTERSURVEYRESOURCEPICKSUICALLBACK_H_

#include "server/zone/objects/player/sui/SuiCallback.h"
#include "server/zone/objects/player/sui/listbox/SuiListBox.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/objects/tangible/tool/SurveyTool.h"
#include "server/zone/managers/resource/ResourceManager.h"
#include "server/zone/managers/resource/resourcespawner/ResourceSpawner.h"
#include "server/zone/objects/waypoint/WaypointObject.h"
#include "server/zone/Zone.h"
#include "server/zone/ZoneServer.h"

class MasterSurveyResourcePickSuiCallback : public SuiCallback {
	ManagedReference<SurveyTool*> surveyTool;
	Vector<String> resourceNames;

	// Master Survey Tool tunables -- see this class's doc comment and NOTES.md.
	//
	// PLANET-WIDE two-phase scan (2026-08-01, Companion). SCAN_RANGE is the
	// TOTAL span of the coarse grid, not a radius (scanForHotspots() starts
	// at centerX - range/2), so 16384 covers a whole 16km ground map edge to
	// edge. 41 points => samples ~409m apart.
	static const int SCAN_RANGE = 16384;
	static const int SCAN_GRID_POINTS = 41;
	static const int MAX_HOTSPOTS = 5;

	// Phase 2: each coarse candidate is re-scanned at 100m resolution before
	// its waypoint is placed. Without this the reported percentage would be
	// an off-peak sample up to ~290m from the real maximum, and a genuine
	// pool with steep falloff could read under MIN_DENSITY and be dropped.
	static const int REFINE_RANGE = 800;
	static const int REFINE_GRID_POINTS = 9;

	// Minimum metres between any two REPORTED hotspots (Nick's request).
	// Enforced on the refined coordinates, since those are what become
	// waypoints. A coarse node closer than PRUNE_RADIUS to an accepted point
	// can never refine far enough away, so it is dropped without a fine scan.
	static const int MIN_HOTSPOT_SEPARATION = 1000;
	static const int PRUNE_RADIUS = 600;

	// Stock "worth a waypoint" floor, same value ResourceSpawner::sendSurvey()
	// uses. COARSE_FLOOR stops the candidate walk once even refinement could
	// not plausibly lift a node over MIN_DENSITY; MAX_CANDIDATES bounds total
	// work if a planet is saturated with near-miss nodes.
	static constexpr float MIN_DENSITY = 0.1f;
	static constexpr float COARSE_FLOOR = 0.04f;
	static const int MAX_CANDIDATES = 40;

public:
	MasterSurveyResourcePickSuiCallback(ZoneServer* server, SurveyTool* tool, const Vector<String>& names)
		: SuiCallback(server) {
		surveyTool = tool;
		resourceNames = names;
	}

	void run(CreatureObject* player, SuiBox* suiBox, uint32 eventIndex, Vector<UnicodeString>* args) {
		if (eventIndex == 1 || player == nullptr || args == nullptr || args->size() <= 0) {
			return;
		}

		int menuSelection = Integer::valueOf(args->get(0).toString());

		if (menuSelection < 0 || menuSelection >= resourceNames.size()) {
			return;
		}

		String resname = resourceNames.get(menuSelection);

		Zone* zone = player->getZone();

		if (zone == nullptr) {
			return;
		}

		ManagedReference<ResourceManager*> resourceManager = cast<ResourceManager*>(player->getZoneServer()->getResourceManager());

		if (resourceManager == nullptr) {
			return;
		}

		ResourceSpawner* resourceSpawner = resourceManager->getResourceSpawner();

		if (resourceSpawner == nullptr) {
			return;
		}

		String zoneName = zone->getZoneName();

		Vector<float> hotspotX;
		Vector<float> hotspotY;
		Vector<float> hotspotDensity;

		// Centered on the map origin (0,0), NOT the player -- results are now
		// "best on this planet" and no longer depend on where you stand.
		resourceSpawner->scanForHotspots(resname, zoneName, 0.f, 0.f, SCAN_RANGE, SCAN_GRID_POINTS, hotspotX, hotspotY, hotspotDensity);

		int totalPoints = hotspotDensity.size();

		if (totalPoints == 0) {
			player->sendSystemMessage("The master survey tool could not locate any trace of " + resname + " on this planet.");
			return;
		}

		// Two-phase selection (2026-08-01, Companion). Walk coarse candidates
		// in descending density; refine each to its real local peak; enforce
		// MIN_HOTSPOT_SEPARATION on the refined coordinates. Candidate counts
		// are small and bounded by MAX_CANDIDATES.
		SortedVector<int> usedIndexes;
		usedIndexes.setNoDuplicateInsertPlan();

		Vector<float> pickedX;
		Vector<float> pickedY;
		Vector<float> pickedDensity;

		int hotspotsFound = 0;
		int candidatesExamined = 0;
		String report = "Master Survey results for " + resname + ":";

		while (hotspotsFound < MAX_HOTSPOTS && candidatesExamined < MAX_CANDIDATES) {
			int bestIndex = -1;
			float bestDensity = -1.f;

			for (int i = 0; i < totalPoints; ++i) {
				if (usedIndexes.contains(i))
					continue;

				if (hotspotDensity.get(i) > bestDensity) {
					bestDensity = hotspotDensity.get(i);
					bestIndex = i;
				}
			}

			if (bestIndex == -1 || bestDensity < COARSE_FLOOR)
				break;

			usedIndexes.put(bestIndex);

			float cx = hotspotX.get(bestIndex);
			float cy = hotspotY.get(bestIndex);

			// Cheap pre-reject: too close to an accepted point for ANY refined
			// result to clear MIN_HOTSPOT_SEPARATION. Skips the fine scan.
			bool pruned = false;

			for (int p = 0; p < pickedX.size(); ++p) {
				float pdx = cx - pickedX.get(p);
				float pdy = cy - pickedY.get(p);

				if ((pdx * pdx + pdy * pdy) < (float)(PRUNE_RADIUS * PRUNE_RADIUS)) {
					pruned = true;
					break;
				}
			}

			if (pruned)
				continue;

			candidatesExamined++;

			// Phase 2: fine re-scan to find this pool's real peak.
			Vector<float> fineX;
			Vector<float> fineY;
			Vector<float> fineDensity;

			resourceSpawner->scanForHotspots(resname, zoneName, cx, cy, REFINE_RANGE, REFINE_GRID_POINTS, fineX, fineY, fineDensity);

			float hx = cx;
			float hy = cy;
			float hd = bestDensity;

			for (int f = 0; f < fineDensity.size(); ++f) {
				if (fineDensity.get(f) > hd) {
					hd = fineDensity.get(f);
					hx = fineX.get(f);
					hy = fineY.get(f);
				}
			}

			if (hd < MIN_DENSITY)
				continue;

			// Real separation test, on the coordinates that become waypoints.
			bool tooClose = false;

			for (int p = 0; p < pickedX.size(); ++p) {
				float dx = hx - pickedX.get(p);
				float dy = hy - pickedY.get(p);

				if ((dx * dx + dy * dy) < (float)(MIN_HOTSPOT_SEPARATION * MIN_HOTSPOT_SEPARATION)) {
					tooClose = true;
					break;
				}
			}

			if (tooClose)
				continue;

			pickedX.add(hx);
			pickedY.add(hy);
			pickedDensity.add(hd);

			hotspotsFound++;
		}

		// Waypoint creation is DEFERRED until after selection, and runs in
		// REVERSE order -- weakest hotspot first, STRONGEST LAST.
		//
		// 2026-08-01, Nick's report: "the waypoint it gives in the data pad
		// is the lowest percentage". Picks leave the loop above in DESCENDING
		// density, so the old in-loop creation added the weakest one last --
		// and the most recently added waypoint is the one the client leaves
		// selected. Reversing creation order makes the BEST hotspot active.
		//
		// Names are also rank-prefixed ("1." = strongest) so the ranking is
		// unambiguous in the datapad however the client chooses to sort it.
		for (int r = hotspotsFound - 1; r >= 0; --r) {
			int pct = (int)(pickedDensity.get(r) * 100);

			ManagedReference<WaypointObject*> waypoint = player->getZoneServer()->createObject(0xc456e788, 1).castTo<WaypointObject*>();

			if (waypoint == nullptr)
				continue;

			Locker locker(waypoint);

			waypoint->setCustomObjectName(UnicodeString(String::valueOf(r + 1) + ". " + resname + " " + String::valueOf(pct) + "%"), false);
			waypoint->setPlanetCRC(zone->getZoneCRC());
			waypoint->setPosition(pickedX.get(r), 0, pickedY.get(r));
			waypoint->setColor(WaypointObject::COLOR_YELLOW);
			waypoint->setSpecialTypeID(WaypointObject::SPECIALTYPE_RESOURCE);
			waypoint->setActive(true);

			player->getPlayerObject()->addWaypoint(waypoint, false, true);
		}

		// Report is built FORWARD (strongest first) regardless of the order
		// the waypoints were created in.
		for (int r = 0; r < hotspotsFound; ++r) {
			int pct = (int)(pickedDensity.get(r) * 100);

			report = report + "\n  " + String::valueOf(r + 1) + ". " + String::valueOf(pct) + "% at (" + String::valueOf((int)pickedX.get(r)) + ", " + String::valueOf((int)pickedY.get(r)) + ")";
		}

		if (hotspotsFound == 0) {
			player->sendSystemMessage("No significant concentration of " + resname + " was found on this planet.");
			return;
		}

		player->sendSystemMessage(report);
	}
};

#endif // MASTERSURVEYRESOURCEPICKSUICALLBACK_H_
