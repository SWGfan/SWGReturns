/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.*/


#include "objects.h"

#include "LoginPacketHandler.h"
#include "server/login/LoginServer.h"
#include "LoginProcessServerImplementation.h"

#include "account/AccountManager.h"

LoginPacketHandler::LoginPacketHandler(const String& s, LoginProcessServerImplementation* serv)
		: Logger(s) {

	processServer = serv;

	server = processServer->getLoginServer();

	setGlobalLogging(true);
	setLogging(true);
}

void LoginPacketHandler::handleMessage(Message* pack) {
	Reference<LoginClient*> client = server->getLoginClient(pack->getClient());

	if (client == nullptr)
		return;

	debug() << "parsing " << *pack;

	uint16 opcount = pack->parseShort();
	uint32 opcode = pack->parseInt();

	// LOG ALL PACKETS FOR DEBUGGING
	StringBuffer logMsg;
	logMsg << "LOGIN PACKET: opcount=" << opcount << " opcode=0x" << hex << opcode;
	info(logMsg.toString(), true);

	switch (opcount) {
	case 1:
		break;
	case 2:
		break;
	case 3:
		// LOG CASE 3 SPECIFICALLY
		info() << "Case 3 packet received with opcode: 0x" << hex << opcode;
		
		switch (opcode) {
			case 0xE87AD031:
				info("!!! DELETE CHARACTER MESSAGE DETECTED !!!", true);
				handleDeleteCharacterMessage(client, pack);
				break;
			default:
				info() << "Unknown opcount 3 opcode: 0x" << hex << opcode << " - IGNORING";
				break;
		}
		break;
	case 4:
		switch (opcode) {
		case 0x41131F96: //LoginClientID CLIENT VERSION BUILD DATE AND LOGIN INFO
			handleLoginClientID(client, pack);
			break;
		}
		break;
	case 5:
		break;
	default:
		break;
	}
}

void LoginPacketHandler::handleLoginClientID(LoginClient* client, Message* pack) {
	AccountManager* accountManager = server->getAccountManager();
	accountManager->loginAccount(client, pack);
}

void LoginPacketHandler::handleDeleteCharacterMessage(LoginClient* client, Message* pack) {

	info("=== ENTERING handleDeleteCharacterMessage ===", true);

	if(!client->hasAccount()) {
		info("ERROR: Client has no account - sending FAIL", true);
		Message* msg = new DeleteCharacterReplyMessage(1); //FAIL
		client->sendMessage(msg);
		return;
	}

	uint32 accountId = client->getAccountID();

	uint32 ServerId = pack->parseInt();

	//pack->shiftOffset(4);
    uint64 charId = pack->parseLong();

	StringBuffer deleteLogMsg;
	deleteLogMsg << "DELETE REQUEST: accountId=" << accountId << " serverId=" << ServerId << " charId=" << charId;
	info(deleteLogMsg.toString(), true);

    StringBuffer moveStatement;
    moveStatement << "INSERT INTO deleted_characters SELECT *, 0 as db_deleted FROM characters WHERE character_oid = " << charId;
    moveStatement << " AND account_id = " << accountId << " AND galaxy_id = " << ServerId << ";";

    StringBuffer verifyStatement;
    verifyStatement << "SELECT * from deleted_characters WHERE character_oid = " << charId;
    verifyStatement << " AND account_id = " << accountId << " AND galaxy_id = " << ServerId << ";";

    StringBuffer deleteStatement;
    deleteStatement << "DELETE FROM characters WHERE character_oid = " << charId;
    deleteStatement << " AND account_id = " << accountId << " AND galaxy_id = " << ServerId << ";";

    int dbDelete = 0;

    try {

    	Reference<ResultSet*> moveResults = ServerDatabase::instance()->executeQuery(moveStatement.toString());

    	if(moveResults == nullptr || moveResults.get()->getRowsAffected() == 0){
    		dbDelete = 1;
    		StringBuffer errMsg;
    		errMsg << "ERROR: Could not move character to deleted_characters table. " << endl;
    		errMsg << "QUERY: " << moveStatement.toString();
    		info(errMsg.toString(),true);

    	} else {
			info("Successfully moved character to deleted_characters", true);
		}

    	Reference<ResultSet*> verifyResults  = ServerDatabase::instance()->executeQuery(verifyStatement.toString());

    	if(verifyResults == nullptr || verifyResults.get()->getRowsAffected() == 0){
    		dbDelete = 1;
    		StringBuffer errMsg;
        	errMsg << "ERROR: Could not verify character was moved to deleted_characters " << endl;
        	errMsg << "QUERY: " << moveStatement.toString();
			info(errMsg.toString(), true);
    	} else {
			info("Verified character in deleted_characters", true);
		}

    } catch (DatabaseException& e) {
    	dbDelete = 1;
		StringBuffer errMsg;
		errMsg << "Database exception during character move: " << e.getMessage();
		info(errMsg.toString(), true);
    	System::out << e.getMessage();
    } catch (Exception& e) {
    	dbDelete = 1;
		StringBuffer errMsg;
		errMsg << "Exception during character move: " << e.getMessage();
		info(errMsg.toString(), true);
       	System::out << e.getMessage();
    }

    if(!dbDelete){
		info("Proceeding to delete character from characters table", true);
    	try {
    		Reference<ResultSet*> deleteResults = ServerDatabase::instance()->executeQuery(deleteStatement);

    		if(deleteResults == nullptr || deleteResults.get()->getRowsAffected() == 0){
    			StringBuffer errMsg;
    			errMsg << "ERROR: Unable to delete character from character table. " << endl;
    			errMsg << "QUERY: " << deleteStatement.toString();
				info(errMsg.toString(), true);
    			dbDelete = 1;
    		} else {
				info("Successfully deleted character from characters table", true);
			}


    	} catch (DatabaseException& e) {
			StringBuffer errMsg;
			errMsg << "Database exception during character deletion: " << e.getMessage();
			info(errMsg.toString(), true);
    		System::out << e.getMessage();
    		dbDelete = 1;
    	} catch (Exception& e) {
			StringBuffer errMsg;
			errMsg << "Exception during character deletion: " << e.getMessage();
			info(errMsg.toString(), true);
    		System::out << e.getMessage();
    		dbDelete = 1;
    	}
    } else {
		info("Skipping character deletion due to move/verify failure", true);
	}

	StringBuffer resultMsg;
	resultMsg << "DELETE RESULT: dbDelete=" << dbDelete << " (0=success, 1=failure)";
	info(resultMsg.toString(), true);

   	Message* msg = new DeleteCharacterReplyMessage(dbDelete);
	client->sendMessage(msg);
	
	info("=== EXITING handleDeleteCharacterMessage ===", true);
}
