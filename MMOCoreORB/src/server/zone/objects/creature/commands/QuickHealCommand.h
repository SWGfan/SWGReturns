/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.*/

#ifndef QUICKHEALCOMMAND_H_
#define QUICKHEALCOMMAND_H_

#include "server/zone/objects/scene/SceneObject.h"
#include "server/zone/ZoneServer.h"
#include "server/zone/managers/player/PlayerManager.h"

class QuickHealCommand : public QueueCommand {
	int actionCost;
	int actionWoundCost;

	int healthHealed;
	int actionHealed;
	int mindHealed;

	float speed;
	float range;
public:

	QuickHealCommand(const String& name, ZoneProcessServer* server)
		: QueueCommand(name, server) {

		healthHealed = 0;
		actionHealed = 0;
		mindHealed = 0;

		actionCost = 1000;
		actionWoundCost = 0;

		speed = 1;
		range = 6;
	}

	void doAnimations(CreatureObject* creature, CreatureObject* creatureTarget) const {
		creatureTarget->playEffect("clienteffect/healing_healdamage.cef", "");

		if (creature == creatureTarget)
			creature->doAnimation("heal_self");
		else
			creature->doAnimation("heal_other");
	}

	void sendHealMessage(CreatureObject* creature, CreatureObject* creatureTarget, int healthDamage) const {
		if (!creature->isPlayerCreature())
			return;

		CreatureObject* player = cast<CreatureObject*>(creature);

		StringBuffer msgPlayer, msgTarget, msgBody, msgTail;

		if (healthDamage > 0) {
			msgBody << healthDamage << " health";
		} else {
			return; //No damage to heal.
		}

		msgTail << " damage.";

		if (creature == creatureTarget) {
			msgPlayer << "You heal yourself for " << msgBody.toString() << msgTail.toString();
			player->sendSystemMessage(msgPlayer.toString());
		} else if (creatureTarget->isPlayerCreature()){
			msgPlayer << "You heal " << creatureTarget->getFirstName() << " for " << msgBody.toString() << msgTail.toString();
			msgTarget << player->getFirstName() << " heals you for " << msgBody.toString() << msgTail.toString();

			player->sendSystemMessage(msgPlayer.toString());
			creatureTarget->sendSystemMessage(msgTarget.toString());
		} else {
			msgPlayer << "You heal " << creatureTarget->getDisplayedName() << " for " << msgBody.toString() << msgTail.toString();
			player->sendSystemMessage(msgPlayer.toString());
		}
	}

	int doQueueCommand(CreatureObject* creature, const uint64& target, const UnicodeString& arguments) const {

		int result = doCommonMedicalCommandChecks(creature);

		if (result != SUCCESS)
			return result;

		ManagedReference<SceneObject*> object = server->getZoneServer()->getObject(target);

		if (object != nullptr) {
			if (!object->isCreatureObject()) {
				TangibleObject* tangibleObject = dynamic_cast<TangibleObject*>(object.get());

				if (tangibleObject != nullptr && tangibleObject->isAttackableBy(creature)) {
					object = creature;
				} else {
					creature->sendSystemMessage("@healing_response:healing_response_99"); //Target must be a player or a creature pet in order to quick heal.
					return GENERALERROR;
				}
			}
		} else {
			object = creature;
		}

		CreatureObject* creatureTarget = cast<CreatureObject*>( object.get());

		Locker clocker(creatureTarget, creature);

		if(!checkDistance(creature, creatureTarget, range))
			return TOOFAR;

		if ((creatureTarget->isAiAgent() && !creatureTarget->isPet()) || creatureTarget->isDroidObject() || creatureTarget->isDead() || creatureTarget->isRidingMount() || creatureTarget->isAttackableBy(creature))
			creatureTarget = creature;

		if (creature != creatureTarget && checkForArenaDuel(creatureTarget)) {
			creature->sendSystemMessage("@jedi_spam:no_help_target"); // You are not permitted to help that target.
			return GENERALERROR;
		}

		if (!creatureTarget->isHealableBy(creature)) {
			creature->sendSystemMessage("@healing:pvp_no_help");  //It would be unwise to help such a patient.
			return GENERALERROR;
		}

		int actionCostNew = creature->calculateCostAdjustment(CreatureAttribute::STAMINA, actionCost);

		if (creature->getHAM(CreatureAttribute::ACTION) < abs(actionCostNew)) {
			creature->sendSystemMessage("@healing_response:not_enough_action"); //You do not have enough mind to do that.
			return GENERALERROR;
		}

		if (!creatureTarget->hasDamage(CreatureAttribute::HEALTH) && !creatureTarget->hasDamage(CreatureAttribute::ACTION)) {
			if (creatureTarget == creature)
				creature->sendSystemMessage("@healing_response:healing_response_61"); //You have no damage to heal.
			else if (creatureTarget->isPlayerCreature()) {
				StringIdChatParameter stringId("healing_response", "healing_response_63"); //%NT has no damage to heal.
				stringId.setTT(creatureTarget->getObjectID());
				creature->sendSystemMessage(stringId);
			} else {
				StringBuffer message;
				message << creatureTarget->getDisplayedName() << " has no damage to heal.";
				creature->sendSystemMessage(message.toString());
			}

			return GENERALERROR;
		}

		int healPower = (int) round(150 + System::random(600));

		int healedHealth = creatureTarget->healDamage(creature, CreatureAttribute::HEALTH, healPower);

		if (creature->isPlayerCreature()) {
			PlayerManager* playerManager = server->getZoneServer()->getPlayerManager();
			playerManager->sendBattleFatigueMessage(creature, creatureTarget);
		}

		sendHealMessage(creature, creatureTarget, healedHealth);

		creature->inflictDamage(creature, CreatureAttribute::ACTION, actionCostNew, false);
		creature->addWounds(CreatureAttribute::QUICKNESS, actionWoundCost, true);
		creature->addWounds(CreatureAttribute::STAMINA, actionWoundCost, true);

		doAnimations(creature, creatureTarget);

		checkForTef(creature, creatureTarget);

		return SUCCESS;
	}

};

#endif //QUICKHEALCOMMAND_H_
