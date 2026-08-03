/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.

	Companion System (2026-07-27, "skill mirror" pass) -- owner-typed
	"/companion_<base>" proxy: ONE class registered under every mirrored
	name (CompanionSkillMirror::mirroredBaseCommands(),
	CommandConfigManager2.cpp loop -- same one-class-many-names mechanic
	already proven by CompanionAbilityCommand.h's 40 registrations and
	CompanionSpecialAttackCommand's two: CommandFactory keys a creator
	function per NAME (CommandFactory.h:50-58) and each name gets its own
	instance constructed with that name (CommandFactory.h:32-35,
	CommandConfigManager.cpp:322-337)).

	The base command this instance proxies is derived from its own
	registered name in the constructor: strip the "companion_" prefix,
	exactly as CompanionAbilityCommand::resolveUnderlyingAbilityName()
	strips its "companion" prefix (CompanionAbilityCommand.h:125-133).
	Registered names are lowercase factory keys, so the remainder is
	already the lowercase real command name whose hash the engine's
	dispatch expects (CompanionSpecialAttackCommand.h:177-181).

	Unlike CompanionAbilityCommand (squad-order: EVERY summoned companion
	fires at once), this proxy picks ONE performing companion:
	  1. the owner's currently TARGETED own companion, if it is summoned,
	     alive, and its learnedSkills grant the base command; otherwise
	  2. the NEAREST summoned companion whose learnedSkills grant it.
	Knowledge is checked dynamically against the companion's own ledger
	(CompanionSkillMirror::companionGrantsBase() -- the same
	Skill::getAbilities() walk as CompanionSpecialAttackCommand.h:82-104),
	so the command stays correct even if the owner somehow retains a stale
	"companion_<base>" ability grant.

	Dispatch reuses the exact proven pattern of
	CompanionSpecialAttackCommand.h:159-183: cross-lock the companion
	against the (queue-locked) owner, interrupt any taxi ride, prime
	ATTACK state / addDefender / clear follow for a hostile target, then
	companion->executeObjectControllerAction(<lowercase base hash>,
	targetID, args) -- which runs the REAL base command through the
	engine's own per-ability pipeline (HAM costs, cooldowns, damage,
	animation) on the companion with the command's own configured
	priority. No ability logic is reimplemented here.

	Target resolution: explicit target if the invocation carried one,
	else the owner's current target (CompanionSpecialAttackCommand.h:126,
	CompanionAbilityCommand.h:158-163); if still none, the companion
	itself (self-target fallback for non-combat bases in later phases --
	a combat base invoked targetless simply fails the real command's own
	target validation with its own message).
*/

#ifndef COMPANIONSKILLPROXYCOMMAND_H_
#define COMPANIONSKILLPROXYCOMMAND_H_

#include "server/zone/objects/creature/commands/QueueCommand.h"
#include "server/zone/objects/companion/CompanionObject.h"
#include "server/zone/objects/companion/CompanionControlDevice.h"
#include "server/zone/objects/companion/CompanionSkillMirror.h"

class CompanionSkillProxyCommand : public QueueCommand {

	// Lowercase base command name this instance proxies, derived from the
	// registered "companion_<base>" name at construction.
	String baseCommandName;

public:

	CompanionSkillProxyCommand(const String& name, ZoneProcessServer* server)
		: QueueCommand(name, server) {

		if (name.beginsWith("companion_")) {
			baseCommandName = name.subString(10);
		} else {
			baseCommandName = name;
		}
	}

	/** See CompanionFollowCommand.h's identical helper for the rationale. */
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

		if (baseCommandName.isEmpty()) {
			return GENERALERROR;
		}

		Vector<ManagedReference<CompanionObject*>> companions;
		resolveActiveCompanions(creature, companions);

		if (companions.size() == 0) {
			creature->sendSystemMessage("@companion:no_active_companion"); // You have no active companion.
			return GENERALERROR;
		}

		// Only companions whose OWN learnedSkills ledger grants the base
		// command qualify -- dynamic, authoritative, never stale.
		Vector<ManagedReference<CompanionObject*>> knowers;

		for (int i = 0; i < companions.size(); ++i) {
			CompanionObject* companion = companions.get(i);

			if (CompanionSkillMirror::companionGrantsBase(companion, baseCommandName)) {
				knowers.add(companion);
			}
		}

		if (knowers.size() == 0) {
			creature->sendSystemMessage("None of your companions know that.");
			return GENERALERROR;
		}

		// Performer selection: owner's targeted own companion if it
		// qualifies, else the nearest qualified one. Dead/incapacitated
		// knowers are state-skipped (with a clear message if that skips
		// everyone) rather than silently no-oping.
		uint64 ownerTargetID = creature->getTargetID();

		ManagedReference<CompanionObject*> performer = nullptr;
		float bestDistance = 0.f;

		for (int i = 0; i < knowers.size(); ++i) {
			CompanionObject* companion = knowers.get(i);

			if (companion->isDead() || companion->isIncapacitated()) {
				continue;
			}

			if (companion->getObjectID() == ownerTargetID) {
				performer = companion;
				break;
			}

			float distance = creature->getDistanceTo(companion);

			if (performer == nullptr || distance < bestDistance) {
				performer = companion;
				bestDistance = distance;
			}
		}

		if (performer == nullptr) {
			creature->sendSystemMessage("Your companions are in no condition to do that.");
			return GENERALERROR;
		}

		// Owner's current target for combat commands, companion self
		// otherwise (targetless invocation) -- see file header.
		uint64 targetID = (target != 0) ? target : ownerTargetID;

		if (targetID == 0) {
			targetID = performer->getObjectID();
		}

		// Exact dispatch pattern of CompanionSpecialAttackCommand.h:159-183.
		Locker clocker(performer, creature);

		if (performer->isDead() || performer->isIncapacitated()) {
			creature->sendSystemMessage("Your companions are in no condition to do that.");
			return GENERALERROR;
		}

		// Companion Taxi interrupt -- see CompanionFollowCommand.h's
		// identical guard.
		if (performer->isTaxiActive()) {
			performer->stopTaxiRide(false);
		}

		// Prime combat state only for a genuinely hostile tangible target
		// (isTangibleObject(), not isCreatureObject() -- lairs/structures
		// count; see CompanionSpecialAttackCommand.h:129-137's bug-fix
		// rationale). Harmless skip for self/ally targets.
		if (targetID != performer->getObjectID()) {
			ManagedReference<SceneObject*> targetObject = creature->getZoneServer()->getObject(targetID, true);

			if (targetObject != nullptr && targetObject->isTangibleObject() && targetObject != creature) {
				TangibleObject* hostileTarget = cast<TangibleObject*>(targetObject.get());

				if (hostileTarget != performer
						&& (performer->isAttackableBy(hostileTarget) || hostileTarget->isAttackableBy(performer))) {
					performer->setCompanionState(CompanionObject::ATTACK);
					performer->addDefender(hostileTarget);
					performer->setFollowObject(nullptr);
				}
			}
		}

		// Registered QueueCommand names are lowercase
		// (CommandConfigManager*.cpp's String("...").toLowerCase()
		// pattern); baseCommandName came from a lowercase factory key, so
		// its hash is exactly the real command's dispatch hash
		// (CompanionSpecialAttackCommand.h:177-181).
		performer->executeObjectControllerAction(baseCommandName.hashCode(), targetID, arguments.toString());

		return SUCCESS;
	}
};

#endif // COMPANIONSKILLPROXYCOMMAND_H_
