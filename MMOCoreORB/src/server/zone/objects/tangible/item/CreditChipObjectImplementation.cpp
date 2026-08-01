/*
 * CreditChipObjectImplementation.cpp
 */

#include "server/zone/objects/tangible/item/CreditChipObject.h"
#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/packets/scene/AttributeListMessage.h"
#include "server/zone/packets/object/ObjectMenuResponse.h"
#include "server/zone/objects/transaction/TransactionLog.h"

void CreditChipObjectImplementation::fillObjectMenuResponse(ObjectMenuResponse* menuResponse, CreatureObject* player) {
	if (player != nullptr)
		menuResponse->addRadialMenuItem(RadialOptions::ITEM_USE, 3, "@space/space_loot:use_credit_chip");
}

int CreditChipObjectImplementation::handleObjectMenuSelect(CreatureObject* player, byte selectedID) {
	if (player == nullptr || selectedID != RadialOptions::ITEM_USE || !isASubChildOf(player))
		return 0;

	auto chip = _this.getReferenceUnsafeStaticCast();
	Locker playerLock(player, chip);
	int totalValue = getUseCount();

	if (totalValue > 0) {
		setUseCount(0);
		TransactionLog trx(chip, player, TrxCode::CREDITCHIPCLAIM, totalValue, false);
		player->addBankCredits(totalValue, true);
		trx.commit();
	}

	playerLock.release();
	destroyObjectFromWorld(true);
	destroyObjectFromDatabase(true);
	return 1;
}

void CreditChipObjectImplementation::fillAttributeList(AttributeListMessage* message, CreatureObject* player) {
	TangibleObjectImplementation::fillAttributeList(message, player);
}
