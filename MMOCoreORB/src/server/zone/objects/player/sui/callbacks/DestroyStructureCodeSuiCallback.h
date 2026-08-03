/*
 * DestroyStructureCodeSuiCallback.h
 *
 *  Created on: Jun 22, 2011
 *      Author: crush
 */

#ifndef DESTROYSTRUCTURECODESUICALLBACK_H_
#define DESTROYSTRUCTURECODESUICALLBACK_H_

#include "server/zone/objects/player/sui/SuiCallback.h"
#include "server/zone/objects/player/sessions/DestroyStructureSession.h"


class DestroyStructureCodeSuiCallback : public SuiCallback {
public:
	DestroyStructureCodeSuiCallback(ZoneServer* serv) : SuiCallback(serv) {
	}

	void run(CreatureObject* player, SuiBox* sui, uint32 eventIndex, Vector<UnicodeString>* args) {
		bool cancelPressed = (eventIndex == 1);

		ManagedReference<DestroyStructureSession*> session = player->getActiveSession(SessionFacadeType::DESTROYSTRUCTURE).castTo<DestroyStructureSession*>();

		if (session == nullptr)
			return;

		if (cancelPressed) {
			session->cancelSession();
			return;
		}

		// Companion System (2026-07-29 fix, per Nick: "instead of making it so we
		// need to add in numbers, lets just make it so a user types in 'yes'").
		// Same typed-word-confirmation idiom this codebase already uses for GCW
		// field faction changes -- see FieldFactionChangeSuiCallback.h.
		StringTokenizer tokenizer(args->get(0).toString());

		if (!tokenizer.hasMoreTokens() || tokenizer.getStringToken().toLowerCase() != "yes") {
			player->sendSystemMessage("@player_structure:incorrect_destroy_code"); //You have entered an incorrect code. You will have to issue the /destroyStructure again if you wish to continue.
			session->cancelSession();
			return;
		}

		session->destroyStructure();
	}
};

#endif /* DESTROYSTRUCTURECODESUICALLBACK_H_ */
