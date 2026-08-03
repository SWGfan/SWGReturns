/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	POCKET BOY -- Nick's stake.com slot game (C:\pocket-boy-final, a Pixi.js
	stake-engine frontend) ported into SWG as a slot machine terminal
	(2026-07-19 -- see NOTES.md). The original frontend receives its spin
	outcomes from stake's RGS server, so the exact reel strips aren't in the
	client repo -- this port reimplements the game server-side from the
	frontend's real rules data (apps/pocket-boy/src/game/config.ts):

	  - 5 reels x 5 rows, the 15 real paylines, the real paytable
	    (L1-L4: 0.1/0.5/2.0, H3-H5: 0.5/2/5, H1-H2: 1/3/10, W 5-oak: 10 --
	    all in total-bet multipliers), wilds substitute, scatters trigger
	    the bonus, max win 10,000x bet.
	  - Wild multipliers ADD across winning wilds ("1x treated as 0", per
	    the game's own info text), and the total multiplies the spin's wins.
	  - 3+ scatters -> Level Up Bonus: 10 free spins, +2 per 3-scatter spin
	    (cap 30), boosted wilds carrying 2x-20x multipliers -- auto-run,
	    itemized in the results window. (The original's expanding-wild
	    columns/levels are simplified into the boosted free spins.)
	  - Reel weights are THIS PORT'S own, Monte-Carlo tuned (150k-spin runs)
	    to ~95% +/- variance of the original's 0.96 RTP target.

	Header-only on purpose: registered in the existing ComponentManager.cpp
	(new .cpp files need a cmake reconfigure; headers don't).
*/

#ifndef POCKETBOYMENUCOMPONENT_H_
#define POCKETBOYMENUCOMPONENT_H_

#include "server/zone/objects/tangible/components/TangibleObjectMenuComponent.h"
#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/objects/player/sui/SuiCallback.h"
#include "server/zone/objects/player/sui/listbox/SuiListBox.h"
#include "server/zone/objects/player/sui/SuiWindowType.h"
#include "server/zone/packets/object/ObjectMenuResponse.h"
#include "server/zone/managers/radial/RadialOptions.h"

namespace PocketBoy {

	// ---- Game data (mirrors config.ts; see file-top comment) ---------------
	enum Symbol { L1, L2, L3, L4, H5, H4, H3, H2, H1, W, S, SYMBOL_COUNT };

	inline const char* symbolText(int s) {
		switch (s) {
		case L1: return " L1 ";
		case L2: return " L2 ";
		case L3: return " L3 ";
		case L4: return " L4 ";
		case H5: return " H5 ";
		case H4: return " H4 ";
		case H3: return " H3 ";
		case H2: return " H2 ";
		case H1: return " H1 ";
		case W: return "WILD";
		case S: return "SCAT";
		default: return " ?? ";
		}
	}

	// Paytable in total-bet multipliers x10 (so ints): [3oak,4oak,5oak]
	inline int payX10(int symbol, int count) {
		static const int table[SYMBOL_COUNT][3] = {
			{ 1, 5, 20 },   // L1
			{ 1, 5, 20 },   // L2
			{ 1, 5, 20 },   // L3
			{ 1, 5, 20 },   // L4
			{ 5, 20, 50 },  // H5
			{ 5, 20, 50 },  // H4
			{ 5, 20, 50 },  // H3
			{ 10, 30, 100 },// H2
			{ 10, 30, 100 },// H1
			{ 0, 0, 100 },  // W (5-oak only)
			{ 0, 0, 0 }     // S
		};

		if (symbol < 0 || symbol >= SYMBOL_COUNT || count < 3 || count > 5) {
			return 0;
		}

		return table[symbol][count - 3];
	}

	// The 15 real paylines (row index per reel).
	inline const int* payline(int i) {
		static const int lines[15][5] = {
			{0,0,0,0,0},{1,1,1,1,1},{2,2,2,2,2},{3,3,3,3,3},{4,4,4,4,4},
			{0,1,0,1,0},{1,2,1,2,1},{2,3,2,3,2},{3,4,3,4,3},
			{1,0,1,0,1},{2,1,2,1,2},{3,2,3,2,3},{4,3,4,3,4},
			{0,1,2,3,4},{4,3,2,1,0}
		};

		return lines[i];
	}

	// Monte-Carlo-tuned cell weights (base game / bonus game).
	inline int drawSymbol(bool bonus) {
		static const int baseWeights[SYMBOL_COUNT] = { 300,300,300,300,140,140,140,80,80,68,31 };
		static const int bonusWeights[SYMBOL_COUNT] = { 300,300,300,300,140,140,140,80,80,200,31 };

		const int* weights = bonus ? bonusWeights : baseWeights;
		int total = 0;

		for (int i = 0; i < SYMBOL_COUNT; ++i) {
			total += weights[i];
		}

		int roll = System::random(total - 1);

		for (int i = 0; i < SYMBOL_COUNT; ++i) {
			roll -= weights[i];

			if (roll < 0) {
				return i;
			}
		}

		return L1;
	}

	inline int rollWildMultiplier(bool bonus) {
		if (bonus) {
			static const int table[12] = { 2,2,2,3,3,4,5,5,8,10,15,20 };
			return table[System::random(11)];
		}

		int roll = System::random(99);

		if (roll < 85) return 1;
		if (roll < 93) return 2;
		if (roll < 97) return 3;
		if (roll < 99) return 5;
		return 10;
	}

	struct SpinResult {
		int grid[5][5];      // [reel][row]
		int wildMult[5][5];  // parallel: multiplier on each W cell
		int lineWinsX10 = 0; // sum of line pays, x10, BEFORE the multiplier
		int totalMult = 1;   // summed winning-wild multipliers (1 if none >1)
		int scatters = 0;
		int winningLines = 0;
	};

	inline void spin(SpinResult& result, bool bonus) {
		bool winningWild[5][5] = { { false } };

		for (int reel = 0; reel < 5; ++reel) {
			for (int row = 0; row < 5; ++row) {
				result.grid[reel][row] = drawSymbol(bonus);
				result.wildMult[reel][row] = result.grid[reel][row] == W ? rollWildMultiplier(bonus) : 0;

				if (result.grid[reel][row] == S) {
					++result.scatters;
				}
			}
		}

		for (int l = 0; l < 15; ++l) {
			const int* line = payline(l);
			int first = -1;
			int count = 0;

			for (int reel = 0; reel < 5; ++reel) {
				int s = result.grid[reel][line[reel]];

				if (s == S) {
					break;
				}

				if (s == W) {
					++count;
					continue;
				}

				if (first == -1) {
					first = s;
					++count;
				} else if (s == first) {
					++count;
				} else {
					break;
				}
			}

			if (first == -1 && count > 0) {
				first = W; // all-wild line
			}

			int pay = first != -1 ? payX10(first, count > 5 ? 5 : count) : 0;

			if (pay > 0) {
				result.lineWinsX10 += pay;
				++result.winningLines;

				for (int reel = 0; reel < count; ++reel) {
					if (result.grid[reel][line[reel]] == W) {
						winningWild[reel][line[reel]] = true;
					}
				}
			}
		}

		int multSum = 0;

		for (int reel = 0; reel < 5; ++reel) {
			for (int row = 0; row < 5; ++row) {
				if (winningWild[reel][row] && result.wildMult[reel][row] > 1) {
					multSum += result.wildMult[reel][row];
				}
			}
		}

		result.totalMult = multSum > 0 ? multSum : 1;
	}

	inline void appendGridRows(SuiListBox* sui, const SpinResult& result) {
		for (int row = 0; row < 5; ++row) {
			String line = "";

			for (int reel = 0; reel < 5; ++reel) {
				if (reel > 0) {
					line += " | ";
				}

				int s = result.grid[reel][row];

				if (s == W && result.wildMult[reel][row] > 1) {
					line += "Wx" + String::valueOf(result.wildMult[reel][row]);
				} else {
					line += symbolText(s);
				}
			}

			sui->addMenuItem(line);
		}
	}

	static const int BET_OPTIONS[6] = { 100, 500, 1000, 5000, 10000, 25000 };
	constexpr int MAX_WIN_MULTIPLIER = 10000; // config.ts max_win

}

class PocketBoySuiCallback : public SuiCallback {
	int mode; // 0 = bet picker, 1 = results ("Spin Again")
	int bet;

public:
	PocketBoySuiCallback(ZoneServer* server, int suiMode, int betAmount)
		: SuiCallback(server), mode(suiMode), bet(betAmount) {
	}

	static void sendBetMenu(CreatureObject* player) {
		ManagedReference<PlayerObject*> ghost = player != nullptr ? player->getPlayerObject() : nullptr;

		if (ghost == nullptr) {
			return;
		}

		ghost->closeSuiWindowType(SuiWindowType::POCKET_BOY_SLOTS);

		ManagedReference<SuiListBox*> sui = new SuiListBox(player, SuiWindowType::POCKET_BOY_SLOTS);
		sui->setPromptTitle("POCKET BOY -- by Cloud Frog Labs");
		sui->setPromptText("5x5, 15 paylines. Wilds substitute and their multipliers ADD. 3+ scatters trigger the Level Up Bonus (free spins with boosted, multiplied wilds). Max win 10,000x bet.\n\nYour credits: " + String::valueOf(player->getCashCredits()) + "\n\nPick your bet:");
		sui->setCancelButton(true, "@ui:cancel");
		sui->setOkButton(true, "@ui:ok");
		sui->setCallback(new PocketBoySuiCallback(player->getZoneServer(), 0, 0));

		for (int i = 0; i < 6; ++i) {
			sui->addMenuItem(String::valueOf(PocketBoy::BET_OPTIONS[i]) + " credits");
		}

		ghost->addSuiBox(sui);
		player->sendMessage(sui->generateMessage());
	}

	static void playSpin(CreatureObject* player, int betAmount) {
		ManagedReference<PlayerObject*> ghost = player != nullptr ? player->getPlayerObject() : nullptr;

		if (ghost == nullptr || betAmount <= 0) {
			return;
		}

		if (player->getCashCredits() < betAmount) {
			player->sendSystemMessage("You don't have enough credits for that bet.");
			return;
		}

		player->subtractCashCredits(betAmount);

		// ---- Base spin ----
		PocketBoy::SpinResult base;
		PocketBoy::spin(base, false);

		// win = lineWinsX10 / 10 * bet * mult (64-bit intermediate).
		int64 totalWin = ((int64) base.lineWinsX10 * betAmount * base.totalMult) / 10;

		// ---- Bonus (3+ scatters): auto-run free spins ----
		int bonusSpins = 0;
		int bonusExtra = 0;
		int64 bonusWin = 0;

		if (base.scatters >= 3) {
			bonusSpins = 10;
			int played = 0;

			while (played < bonusSpins && bonusSpins <= 30) {
				PocketBoy::SpinResult fs;
				PocketBoy::spin(fs, true);

				bonusWin += ((int64) fs.lineWinsX10 * betAmount * fs.totalMult) / 10;

				if (fs.scatters >= 3 && bonusSpins < 30) {
					bonusSpins += 2;
					bonusExtra += 2;
				}

				++played;
			}

			totalWin += bonusWin;
		}

		// Max-win cap (config.ts: 10,000x bet).
		int64 cap = (int64) betAmount * PocketBoy::MAX_WIN_MULTIPLIER;

		if (totalWin > cap) {
			totalWin = cap;
		}

		if (totalWin > 0) {
			player->addCashCredits((int) totalWin, true);
		}

		// ---- Results window ----
		ghost->closeSuiWindowType(SuiWindowType::POCKET_BOY_SLOTS);

		ManagedReference<SuiListBox*> sui = new SuiListBox(player, SuiWindowType::POCKET_BOY_SLOTS);
		sui->setPromptTitle("POCKET BOY -- bet " + String::valueOf(betAmount));

		String headline;

		if (totalWin <= 0) {
			headline = "No win this spin.";
		} else if (base.scatters >= 3) {
			headline = "BONUS! " + String::valueOf(base.scatters) + " scatters -> " + String::valueOf(bonusSpins) + " free spins (+" + String::valueOf(bonusExtra) + " extra)!\nTOTAL WIN: " + String::valueOf((long long) totalWin) + " credits (" + String::valueOf((long long) bonusWin) + " from the bonus)!";
		} else {
			headline = "WIN: " + String::valueOf((long long) totalWin) + " credits on " + String::valueOf(base.winningLines) + (base.winningLines == 1 ? " line" : " lines") + (base.totalMult > 1 ? " with a " + String::valueOf(base.totalMult) + "x wild multiplier" : "") + "!";
		}

		sui->setPromptText(headline + "\n\nYour credits: " + String::valueOf(player->getCashCredits()) + "\n\nPress Spin Again to replay your " + String::valueOf(betAmount) + "-credit bet, or Cancel to walk away.");
		sui->setCancelButton(true, "Walk Away");
		sui->setOkButton(true, "Spin Again");
		sui->setCallback(new PocketBoySuiCallback(player->getZoneServer(), 1, betAmount));

		PocketBoy::appendGridRows(sui, base);

		if (base.scatters >= 3) {
			sui->addMenuItem(" ");
			sui->addMenuItem("== Level Up Bonus: " + String::valueOf(bonusSpins) + " free spins, won " + String::valueOf((long long) bonusWin) + " ==");
		}

		ghost->addSuiBox(sui);
		player->sendMessage(sui->generateMessage());
	}

	void run(CreatureObject* player, SuiBox* suiBox, uint32 eventIndex, Vector<UnicodeString>* args) {
		if (player == nullptr) {
			return;
		}

		if (mode == 0) {
			// Bet picker: need OK + a row.
			if (eventIndex == 1 || args == nullptr || args->size() <= 0) {
				return;
			}

			int selection = Integer::valueOf(args->get(0).toString());

			if (selection < 0 || selection >= 6) {
				return;
			}

			playSpin(player, PocketBoy::BET_OPTIONS[selection]);
		} else {
			// Results: OK (eventIndex 0) = Spin Again with the same bet.
			if (eventIndex != 0) {
				return;
			}

			playSpin(player, bet);
		}
	}

};

class PocketBoyMenuComponent : public TangibleObjectMenuComponent {
public:

	virtual void fillObjectMenuResponse(SceneObject* sceneObject, ObjectMenuResponse* menuResponse, CreatureObject* player) const {
		menuResponse->addRadialMenuItem(RadialOptions::SERVER_MENU1, 3, "Play Pocket Boy");
		menuResponse->addRadialMenuItem(RadialOptions::SERVER_MENU2, 3, "Paytable & Rules");

		TangibleObjectMenuComponent::fillObjectMenuResponse(sceneObject, menuResponse, player);
	}

	virtual int handleObjectMenuSelect(SceneObject* sceneObject, CreatureObject* player, byte selectedID) const {
		if (player == nullptr || !player->isPlayerCreature()) {
			return 0;
		}

		if (selectedID == RadialOptions::SERVER_MENU1) {
			PocketBoySuiCallback::sendBetMenu(player);
			return 0;
		}

		if (selectedID == RadialOptions::SERVER_MENU2) {
			ManagedReference<PlayerObject*> ghost = player->getPlayerObject();

			if (ghost == nullptr) {
				return 0;
			}

			ManagedReference<SuiListBox*> sui = new SuiListBox(player, SuiWindowType::POCKET_BOY_SLOTS);
			sui->setPromptTitle("POCKET BOY -- Paytable & Rules");
			sui->setPromptText("All pays are TOTAL-BET multipliers, left to right on 15 paylines.");
			sui->setCancelButton(true, "@ui:cancel");
			sui->setOkButton(true, "@ui:ok");

			sui->addMenuItem("H1 / H2:  3 of a kind 1x, 4oak 3x, 5oak 10x");
			sui->addMenuItem("H3 / H4 / H5:  3oak 0.5x, 4oak 2x, 5oak 5x");
			sui->addMenuItem("L1 - L4:  3oak 0.1x, 4oak 0.5x, 5oak 2x");
			sui->addMenuItem("WILD: substitutes for everything; 5 wilds pay 10x");
			sui->addMenuItem("Wild multipliers (up to 10x base / 20x bonus) ADD together");
			sui->addMenuItem("SCATTER x3+: Level Up Bonus -- 10 free spins with boosted wilds");
			sui->addMenuItem("3+ scatters during the bonus: +2 free spins (max 30)");
			sui->addMenuItem("Max win: 10,000x your bet");

			ghost->addSuiBox(sui);
			player->sendMessage(sui->generateMessage());
			return 0;
		}

		return TangibleObjectMenuComponent::handleObjectMenuSelect(sceneObject, player, selectedID);
	}

};

#endif /* POCKETBOYMENUCOMPONENT_H_ */
