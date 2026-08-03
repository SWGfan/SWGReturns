/*
Copyright <SWGEmu>
See file COPYING for copying conditions.

Companion System (2026-07-31, "ability registry" hotfix pass) --
/companionresync: re-grants every companion-macro ability the owner
should currently have, without needing to untrain/retrain anything.

Why this exists: CompanionSkillTrainer.cpp grants "companion_<ability>"
macros (baseline order commands, per-skill combat/medic/entertainer
abilities, and the "test everything" combat/starter list) by adding an
ad-hoc Ability object directly to the owner's live ability list --
bypassing SkillManager's abilityMap entirely. Until tonight's registry
hotfix (see docs/companion_system/NOTES.md, 2026-07-31), abilityMap had
no "companion_"-prefixed entries at all, so every one of these got
silently dropped on the next relog/restart (AbilityList::loadFromNames()
logs "<name> is null when trying to load from database" and drops it).
That registry hotfix stops FUTURE drops, but does nothing for
abilities a character already lost from a PAST restart -- this command
is the one-time (or repeatable, it's fully idempotent) fix-up for that.

Deliberately just re-runs the exact same grant functions
CompanionSkillTrainer already exposes (grantBaselineOwnerOrderAbilities(),
grantAllAbilitiesForTesting(), and grantOwnerAbilitiesForSkill() for
every skill every active companion has actually learned) rather than
reimplementing any grant logic here -- every one of those is already
hasAbility()-guarded internally, so running this repeatedly, on any
character, at any time, only ever tops up what's missing. Never
touches XP, skill points, or learned-skill state -- owner ability
bookkeeping only.
*/

// COMPANION_RESYNC_ABILITIES_2026_07_31 -- idempotency/marker anchor for
// patch_companion_resync_abilities_command_2026-07-31.py. Do not remove.
#ifndef COMPANIONRESYNCABILITIESCOMMAND_H_
#define COMPANIONRESYNCABILITIESCOMMAND_H_

#include "server/zone/objects/creature/commands/QueueCommand.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/managers/companion/CompanionSkillTrainer.h"

class CompanionResyncAbilitiesCommand : public QueueCommand {
public:

CompanionResyncAbilitiesCommand(const String& name, ZoneProcessServer* server)
: QueueCommand(name, server) {

}

/** See CompanionAbilityCommand.h's identical helper for the full
 * rationale -- duplicated rather than shared to keep this command
 * self-contained. */
void resolveActiveCompanions(CreatureObject* player, Vector<ManagedReference<CompanionObject*>>& companions) const {
if (player == nullptr) {
return;
}

ManagedReference<SceneObject*> datapad = player->getSlottedObject("datapad");

if (datapad == nullptr) {
return;
}

for (int i = 0; i < datapad->getContainerObjectsSize(); ++i) {
ManagedReference<SceneObject*> obj = datapad->getContainerObject(i);

if (obj == nullptr || !obj->isCompanionControlDevice()) {
continue;
}

CompanionControlDevice* device = cast<CompanionControlDevice*>(obj.get());

if (device->isCompanionDead()) {
continue;
}

CompanionObject* companion = device->getCompanionObject();

if (companion == nullptr || companion->getZone() == nullptr) {
continue;
}

if (companion->getLinkedCreature().get() != player) {
continue;
}

companions.add(companion);
}
}

int doQueueCommand(CreatureObject* creature, const uint64& target, const UnicodeString& arguments) const {
if (!checkStateMask(creature)) {
return INVALIDSTATE;
}

if (!checkInvalidLocomotions(creature)) {
return INVALIDLOCOMOTION;
}

CompanionSkillTrainer* trainer = CompanionSkillTrainer::instance();

if (trainer == nullptr) {
creature->sendSystemMessage("Companion ability resync is unavailable right now.");
return GENERALERROR;
}

// Baseline order commands (follow/stay/patrol/store/attack/etc.) --
// always-available regardless of any companion's learned skills.
trainer->grantBaselineOwnerOrderAbilities(creature);

// The combat/medic/entertainer macro list ("test everything from
// novice" grant -- see CompanionSkillTrainer.cpp's own doc comment
// on grantAllAbilitiesForTesting() for why this is the only reach
// for these abilities in this deployment).
trainer->grantAllAbilitiesForTesting(creature);

// Also re-run the real per-skill grant for every skill every
// active companion has actually learned, in case any of those
// (rather than the two blanket grants above) are what originally
// unlocked a given macro for this owner.
Vector<ManagedReference<CompanionObject*>> companions;
resolveActiveCompanions(creature, companions);

for (int i = 0; i < companions.size(); ++i) {
CompanionObject* companion = companions.get(i);

if (companion == nullptr) {
continue;
}

for (int j = 0; j < companion->getLearnedSkillCount(); ++j) {
const String& skillName = companion->getLearnedSkill(j);

if (skillName.isEmpty()) {
continue;
}

trainer->grantOwnerAbilitiesForSkill(creature, skillName);
}
}

creature->sendSystemMessage("Your companion ability macros have been resynced -- check your Command Browser / hotbar.");

return SUCCESS;
}
};

#endif // COMPANIONRESYNCABILITIESCOMMAND_H_
