-- Enhancement terminal (starport).
--
-- Companion System (2026-08-04): everything here applies to the player AND to
-- every summoned companion. The engine could always do this -- enhanceCharacter,
-- setWounds and addDotState all take a CreatureObject, and a CompanionObject IS
-- one. Nothing had ever handed them a companion.
--
-- The two "Apply" options exist to TEST the healing loop: give yourself and your
-- companions real wounds or a real disease, then heal them back with the medic
-- abilities and see whether it works.
--
-- SUI note: the server cannot position a window, so two open at once means one is
-- hidden behind the other with no way for a new player to know. Every chain here
-- opens the next box only from the previous box's callback, after it has closed.

buffTerminalMenuComponent = {  }

-- The nine HAM attributes, in the engine's own order (setWounds pool index).
buffTerminalMenuComponent.ATTRIBUTES = {
	{ "Health",      0 }, { "Strength",  1 }, { "Constitution", 2 },
	{ "Action",      3 }, { "Quickness", 4 }, { "Stamina",      5 },
	{ "Mind",        6 }, { "Focus",     7 }, { "Willpower",    8 },
}

function buffTerminalMenuComponent:fillObjectMenuResponse(pSceneObject, pMenuResponse, pPlayer)
	local response = LuaObjectMenuResponse(pMenuResponse)
	response:addRadialMenuItem(20, 3, "Get Buffs")
	response:addRadialMenuItem(21, 3, "Clear Wounds")
	response:addRadialMenuItem(22, 3, "Apply Wounds...")
	response:addRadialMenuItem(23, 3, "Apply Disease...")
end

function buffTerminalMenuComponent:logUsage(pPlayer, what)
	local creature = CreatureObject(pPlayer)
	local fh = io.open("log/buffTerminalMenuComponent.log", "a+")
	fh:write(string.format(
	    "%s [buffTerminalMenuComponent] %s on %s (oid: %d) on %s at %s %s\n",
		getFormattedTime(),
	    what,
		creature:getFirstName(),
		creature:getObjectID(),
		creature:getZoneName(),
		math.floor(creature:getWorldPositionX()),
		math.floor(creature:getWorldPositionY())
	))
	fh:flush()
	fh:close()
end

-- Run fn(pTarget) against the player and each summoned companion.
-- getCompanionIDs() is the one binding that made this possible; see
-- LuaCreatureObject::getCompanionIDs for why it returns IDs rather than a
-- purpose-built helper per feature.
function buffTerminalMenuComponent:forSelfAndCompanions(pPlayer, fn)
	local n = 0

	fn(pPlayer)

	local ids = CreatureObject(pPlayer):getCompanionIDs()

	if ids ~= nil then
		for _, oid in pairs(ids) do
			local pComp = getSceneObject(oid)

			if pComp ~= nil then
				fn(pComp)
				n = n + 1
			end
		end
	end

	return n
end

function buffTerminalMenuComponent:handleObjectMenuSelect(pSceneObject, pPlayer, selectedID)
	if CreatureObject(pPlayer):isInCombat() or CreatureObject(pPlayer):isIncapacitated() or CreatureObject(pPlayer):isDead() then
		return 0
	end

	if not CreatureObject(pPlayer):isInRangeWithObject(pSceneObject, 6) then
		return 0
	end

	if selectedID == 20 then
		CreatureObject(pPlayer):enhanceCharacter()
		CreatureObject(pPlayer):enhanceCompanions()
		buffTerminalMenuComponent:logUsage(pPlayer, "enhanceCharacter")

	elseif selectedID == 21 then
		local n = buffTerminalMenuComponent:forSelfAndCompanions(pPlayer, function(pTarget)
			for i = 0, 8 do
				CreatureObject(pTarget):setWounds(i, 0)
			end
			CreatureObject(pTarget):setShockWounds(0)
		end)

		if n > 0 then
			CreatureObject(pPlayer):sendSystemMessage("Your companions' wounds are tended as well.")
		end

		buffTerminalMenuComponent:logUsage(pPlayer, "clearWounds")

	elseif selectedID == 22 then
		buffTerminalMenuComponent:askAttribute(pPlayer, "wounds")

	elseif selectedID == 23 then
		buffTerminalMenuComponent:askAttribute(pPlayer, "disease")
	end

	return 0
end

-- Step 1 of both chains: which attribute?
function buffTerminalMenuComponent:askAttribute(pPlayer, mode)
	local sui = SuiListBox.new("buffTerminalMenuComponent", "onAttributePicked")
	sui.setTitle("-=TERMINAL=- : Apply " .. (mode == "disease" and "Disease" or "Wounds"))
	sui.setPrompt("Which attribute?\n\nApplies to YOU and every summoned companion, so you can heal it back and see whether the healing actually works.")

	for i = 1, #buffTerminalMenuComponent.ATTRIBUTES do
		sui.add(buffTerminalMenuComponent.ATTRIBUTES[i][1], "")
	end

	sui.add("ALL nine attributes", "")

	writeData(CreatureObject(pPlayer):getObjectID(), "buffTerminalMode", mode == "disease" and 1 or 0)
	sui.sendTo(pPlayer)
end

function buffTerminalMenuComponent:onAttributePicked(pPlayer, pSui, eventIndex, args)
	local cancel = (eventIndex == 1)

	if cancel then
		return
	end

	local index = tonumber(args) or 0
	local oid = CreatureObject(pPlayer):getObjectID()

	writeData(oid, "buffTerminalAttribute", index)

	local isDisease = readData(oid, "buffTerminalMode") == 1
	local label = (index >= #buffTerminalMenuComponent.ATTRIBUTES)
			and "ALL nine attributes"
			or buffTerminalMenuComponent.ATTRIBUTES[index + 1][1]

	-- Step 2, opened only now that the list box has closed -- two SUI windows on
	-- screen at once means one is hidden behind the other.
	local sui = SuiInputBox.new("buffTerminalMenuComponent", "onAmountEntered")
	sui.setTitle("-=TERMINAL=- : " .. label)

	if isDisease then
		sui.setPrompt("Disease on " .. label .. ".\n\nType:  <damage per tick> <seconds>\nFor example:  120 300")
	else
		sui.setPrompt("Wounds on " .. label .. ".\n\nType the amount of wound damage.\nFor example:  500")
	end

	sui.sendTo(pPlayer)
end

function buffTerminalMenuComponent:onAmountEntered(pPlayer, pSui, eventIndex, args)
	if eventIndex == 1 then
		return
	end

	local oid = CreatureObject(pPlayer):getObjectID()
	local index = readData(oid, "buffTerminalAttribute")
	local isDisease = readData(oid, "buffTerminalMode") == 1

	local amount, seconds = 0, 0

	for a, b in string.gmatch(tostring(args), "(%d+)%s*(%d*)") do
		amount = tonumber(a) or 0
		seconds = tonumber(b) or 0
		break
	end

	if amount <= 0 then
		CreatureObject(pPlayer):sendSystemMessage("No amount entered -- nothing applied.")
		return
	end

	if seconds <= 0 then
		seconds = 300
	end

	local pools = {}

	if index >= #buffTerminalMenuComponent.ATTRIBUTES then
		for i = 0, 8 do
			pools[#pools + 1] = i
		end
	else
		pools[1] = buffTerminalMenuComponent.ATTRIBUTES[index + 1][2]
	end

	local n = buffTerminalMenuComponent:forSelfAndCompanions(pPlayer, function(pTarget)
		for _, pool in ipairs(pools) do
			if isDisease then
				-- defense 0 so it always lands. This is a test tool, not a combat
				-- ability -- a debuff that silently resists is useless for
				-- checking whether a heal works.
				CreatureObject(pTarget):addDotState(pPlayer, DISEASED, amount, pool, seconds, 100, 0, 0)
			else
				local current = CreatureObject(pTarget):getWounds(pool)
				CreatureObject(pTarget):setWounds(pool, current + amount)
			end
		end
	end)

	local what = isDisease
			and string.format("Disease applied: %d per tick for %ds", amount, seconds)
			or string.format("Wounds applied: %d", amount)

	CreatureObject(pPlayer):sendSystemMessage(what
			.. (n > 0 and string.format(" (you and %d companion(s))", n) or " (you)")
			.. " -- now heal it back.")

	buffTerminalMenuComponent:logUsage(pPlayer, isDisease and "applyDisease" or "applyWounds")
end
