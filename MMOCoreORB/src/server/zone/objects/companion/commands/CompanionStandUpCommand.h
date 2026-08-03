/*
Copyright <SWGEmu>
See file COPYING for copying conditions.

Companion System (2026-07-31, "Stand Up 1" -- per Nick: "give me a
comedy show with text, make sure if text is displayed, it needs to be
said slowly so the user has time to read, so give a tiny pause before
the next text is displayed, 3 seconds should be enough time"). One
companion delivers a short stand-up set, line by line, each line
STANDUP_LINE_DELAY_MS apart so there's real reading time between them.

No world NPCs or props are spawned by this show -- it is pure
companion speech via the same ChatManager::broadcastChatMessage()
say() helper every sibling Theater Mode show already uses (copied
verbatim from CompanionTheLandingCommand.h's own say(), itself
copied from CompanionFireworksShow.h). Because nothing hostile is
spawned, the INVULNERABLE/AIENABLED safety rule that governs Battle
Theater/Siege/Duel of Champions/Convoy Ambush does not apply here --
there is nothing to make invulnerable.

Participates in the same shared "companion_theater_mode_busy" owner
cooldown guard every Theater Mode show uses (see
CompanionTheLandingCommand.h's own start()/finish() for the pattern
this copies).

NOT wired into CompanionDialogMenuSuiCallback.h / CompanionTheaterModeSuiCallback.h
by this file -- that happens in a separate, centrally-coordinated
patch alongside every other new show built tonight, to avoid the
anchor-collision bugs already hit multiple times when several
scripts touch those same two shared files independently.
*/

#ifndef COMPANIONSTANDUPCOMMAND_H_
#define COMPANIONSTANDUPCOMMAND_H_

#include "server/zone/objects/creature/commands/QueueCommand.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/ZoneServer.h"
#include "server/zone/managers/chat/ChatManager.h"

// STANDUP_2026_07_31 -- 3s between lines, per Nick's explicit ask ("3
// seconds should be enough time between texts").
#define STANDUP_LINE_DELAY_MS 3000

// STANDUP_2026_07_31 -- jokes 1-6 from the 10 proposed, exactly as Nick
// picked them, split into natural pause beats (setup / dialogue /
// punchline where a joke has three parts, setup / punchline otherwise).
static const char* STANDUP_LINES[] = {
"Alright, settle in -- this one's called Stand Up 1.",                                       // 0 intro
"A Jawa walks into a cantina.",                                                               // 1  joke 1
"The bartender says, \"We don't serve your kind here.\"",                                     // 2  joke 1
"The Jawa says, \"That's fine, I'm just here to salvage the jukebox.\"",                      // 3  joke 1
"Why don't stormtroopers ever win at hide and seek?",                                         // 4  joke 2
"Because they can't hit anything they're aiming at, including the seeker.",                   // 5  joke 2
"What do you call a Wookiee with a parking ticket?",                                          // 6  joke 3
"Irrelevant -- nobody's ever seen one written.",                                               // 7  joke 3
"A bounty hunter walks into a bar and says, \"I'm looking for someone.\"",                    // 8  joke 4
"The bartender says, \"Aren't we all.\"",                                                     // 9  joke 4
"Why did the droid go to therapy?",                                                           // 10 joke 5
"Too many unresolved bugs from childhood.",                                                   // 11 joke 5
"What's Jabba's least favorite exercise?",                                                    // 12 joke 6
"Anything with the word \"core.\"",                                                            // 13 joke 6
"Thank you, you've been a great crowd -- goodnight!"                                          // 14 closer
};
#define STANDUP_LINE_COUNT 15

/** STANDUP_2026_07_31 -- delivers STANDUP_LINES[index], then reschedules
 * itself for index+1 after STANDUP_LINE_DELAY_MS, until every line has
 * played, at which point it clears the busy cooldown. Re-resolves the
 * comedian by object ID fresh every tick (never holds a raw pointer
 * across the delay), matching every other deferred-task precedent in
 * this project. */
class CompanionStandUpJokeTask : public Task {
ManagedWeakReference<CreatureObject*> ownerRef;
uint64 comedianID;
int index;

public:
CompanionStandUpJokeTask(CreatureObject* owner, uint64 comedianID, int index) {
ownerRef = owner;
this->comedianID = comedianID;
this->index = index;
}

void run() {
CreatureObject* owner = ownerRef.get();

if (owner == nullptr) {
return;
}

if (index >= STANDUP_LINE_COUNT) {
Locker locker(owner);
owner->updateCooldownTimer("companion_theater_mode_busy", 0);
return;
}

ZoneServer* zoneServer = owner->getZoneServer();

if (zoneServer == nullptr) {
Locker locker(owner);
owner->updateCooldownTimer("companion_theater_mode_busy", 0);
return;
}

ManagedReference<SceneObject*> comedianObj = zoneServer->getObject(comedianID);

if (comedianObj == nullptr) {
// Comedian gone (stored/despawned mid-set) -- stop cleanly
// rather than erroring on every remaining line.
Locker locker(owner);
owner->updateCooldownTimer("companion_theater_mode_busy", 0);
return;
}

CompanionObject* comedian = comedianObj.castTo<CompanionObject*>().get();

if (comedian == nullptr) {
Locker locker(owner);
owner->updateCooldownTimer("companion_theater_mode_busy", 0);
return;
}

ChatManager* chatManager = zoneServer->getChatManager();

if (chatManager != nullptr) {
chatManager->broadcastChatMessage(comedian, UnicodeString(STANDUP_LINES[index]), 0, 0, 0, 0, 1);
}

Reference<CompanionStandUpJokeTask*> next = new CompanionStandUpJokeTask(owner, comedianID, index + 1);
next->schedule(STANDUP_LINE_DELAY_MS);
}
};

/** STANDUP_2026_07_31 -- see file header for the full design. Mirrors
 * CompanionTheLandingShow/CompanionBattleTheaterShow's own shape (static
 * helpers + a start() entry point) even though this show never spawns
 * anything -- it's pure paced companion speech. */
class CompanionStandUpShow {
public:
/** @pre { nothing locked -- locks owner itself } */
static void start(CreatureObject* owner, Vector<ManagedReference<CompanionObject*> >& companions) {
if (owner == nullptr) {
return;
}

uint64 comedianID = 0;

{
Locker locker(owner);

if (!owner->checkCooldownRecovery("companion_theater_mode_busy")) {
owner->sendSystemMessage("Your companions are already busy putting on a show.");
return;
}

if (companions.size() == 0) {
owner->sendSystemMessage("You need at least one active companion to perform Stand Up 1.");
return;
}

owner->updateCooldownTimer("companion_theater_mode_busy", 600000);

CompanionObject* comedian = companions.get(0);
comedianID = comedian->getObjectID();
}

Reference<CompanionStandUpJokeTask*> firstLine = new CompanionStandUpJokeTask(owner, comedianID, 0);
firstLine->schedule(1000);
}
};

#endif // COMPANIONSTANDUPCOMMAND_H_
