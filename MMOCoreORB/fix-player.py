filepath = '/home/swgadmin/Desktop/SWGFlurry/MMOCoreORB/src/server/zone/managers/player/creation/PlayerCreationManager.cpp'

with open(filepath, 'r') as f:
    lines = f.readlines()

new_lines = [
    '\ttry {\n',
    '\t\tStringBuffer query;\n',
    '\t\tquery\n',
    '\t\t\t\t<< "INSERT INTO `characters_dirty` (`character_oid`, `account_id`, `galaxy_id`, `firstname`, `surname`, `race`, `gender`, `template`)"\n',
    '\t\t\t\t<< " VALUES (" << playerCreature->getObjectID() << ","\n',
    '\t\t\t\t<< client->getAccountID() << "," << zoneServer.get()->getGalaxyID()\n',
    '\t\t\t\t<< ",\'" << firstName.escapeString() << "\',\'"\n',
    '\t\t\t\t<< lastName.escapeString() << "\'," << raceID << "," << 0 << ",\'"\n',
    '\t\t\t\t<< raceFile.escapeString() << "\')"; \n',
    '\t\tServerDatabase::instance()->executeStatement(query);\n',
    '\t} catch (DatabaseException& e) {\n',
    '\t\terror(e.getMessage());\n',
    '\t}\n',
    '\n',
    '\ttry {\n',
    '\t\tStringBuffer charQuery;\n',
    '\t\tcharQuery\n',
    '\t\t\t\t<< "INSERT INTO `characters` (`character_oid`, `account_id`, `galaxy_id`, `firstname`, `surname`, `race`, `gender`, `template`)"\n',
    '\t\t\t\t<< " VALUES (" << playerCreature->getObjectID() << ","\n',
    '\t\t\t\t<< client->getAccountID() << "," << zoneServer.get()->getGalaxyID()\n',
    '\t\t\t\t<< ",\'" << firstName.escapeString() << "\',\'"\n',
    '\t\t\t\t<< lastName.escapeString() << "\'," << raceID << "," << 0 << ",\'"\n',
    '\t\t\t\t<< raceFile.escapeString() << "\')"; \n',
    '\t\tServerDatabase::instance()->executeStatement(charQuery);\n',
    '\t} catch (DatabaseException& e) {\n',
    '\t\terror(e.getMessage());\n',
    '\t}\n',
]

lines[584:609] = new_lines

with open(filepath, 'w') as f:
    f.writelines(lines)

print("Done! Verifying lines 585-615:")
for i, line in enumerate(lines[584:614], start=585):
    print(f"{i}: {repr(line)}")
