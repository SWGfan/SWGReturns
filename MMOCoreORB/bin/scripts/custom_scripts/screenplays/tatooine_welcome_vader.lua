TatooineWelcomeVaderScreenPlay = ScreenPlay:new {
	numberOfActs = 1,
}

registerScreenPlay("TatooineWelcomeVaderScreenPlay", true)

function TatooineWelcomeVaderScreenPlay:start()
	if (isZoneEnabled("tatooine")) then
		self:spawnVader()
	end
end

function TatooineWelcomeVaderScreenPlay:spawnVader()
	-- Peaceful decorative Vader greeting new players just outside the Mos Eisley starport.
	-- Position anchored to the dev-placed "just outside starport" landmark
	-- (object/tangible/crafting/station/public_space_station.iff at 3520.67, -4822.73)
	-- in scripts/screenplays/cities/tatooine_mos_eisley.lua, offset a few meters to avoid overlap.
	local pVader = spawnMobile("tatooine", "darth_vader", 1, 3521, 5, -4820, 40, 0)

	if pVader == nil then
		return
	end

	createObserver(OBJECTRADIALOPENED, "TatooineWelcomeVaderScreenPlay", "grantYachtDeed", pVader)
	createEvent(15 * 1000, "TatooineWelcomeVaderScreenPlay", "greet", pVader, "")
end

function TatooineWelcomeVaderScreenPlay:grantYachtDeed(pVader, pPlayer)
	if pVader == nil or pPlayer == nil then
		return 0
	end

	local player = CreatureObject(pPlayer)

	-- Each character can claim this welcome gift once.
	if player:hasScreenPlayState(1, "tatooine_welcome_vader_yacht") then
		player:sendSystemMessage("Lord Vader has already issued you a Sorosuub Luxury Yacht deed.")
		return 0
	end

	local pInventory = SceneObject(pPlayer):getSlottedObject("inventory")

	if pInventory == nil or SceneObject(pInventory):isContainerFullRecursive() then
		player:sendSystemMessage("Lord Vader cannot issue your yacht deed until you make room in your inventory.")
		return 0
	end

	local pDeed = giveItem(pInventory, "object/tangible/space/veteran_reward/sorosuub_space_yacht_deed.iff", -1)

	if pDeed == nil then
		player:sendSystemMessage("Lord Vader was unable to issue the yacht deed. Please try again.")
		return 0
	end

	player:setScreenPlayState(1, "tatooine_welcome_vader_yacht")
	player:sendSystemMessage("Lord Vader has issued you a Sorosuub Luxury Yacht deed. Use it from your inventory, then launch the ship at a starport space terminal.")
	spatialChat(pVader, "Your transport awaits. Do not disappoint me.")

	return 0
end

function TatooineWelcomeVaderScreenPlay:greet(pVader)
	if pVader == nil then
		return
	end

	spatialChat(pVader, "Welcome to SWG Returns. Please head to the cantina to make new friends.")

	createEvent(90 * 1000, "TatooineWelcomeVaderScreenPlay", "greet", pVader, "")
end
