--Copyright (C) 2024 <SWGEmu>

--This File is part of Core3.

--This program is free software; you can redistribute
--it and/or modify it under the terms of the GNU Lesser
--General Public License as published by the Free Software
--Foundation; either version 2 of the License,
--or (at your option) any later version.

--Companion System -- genesis port (2026-08-04).
--The C++ side registers this command in CommandConfigManager2.cpp; without a
--matching lua declaration the client rejects the slash command locally and it
--never reaches the server. Both halves are required.

CompanionFollowotherCommand = {
	name = "companionfollowother",
}

AddCommand(CompanionFollowotherCommand)
