local function vlog(msg)
	local f = io.open("vader_debug.txt", "a")
	if f then
		f:write(os.date() .. " " .. msg .. "\n")
		f:close()
	end
end

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
	local pVader = spawnMobile("tatooine", "dressed_echo_base_darth_vader", 1, 3521, 5, -4820, 40, 0)

	vlog("spawnMobile returned " .. tostring(pVader))

	if pVader == nil then
		return
	end

	createEvent(15 * 1000, "TatooineWelcomeVaderScreenPlay", "greet", pVader, "")
end

function TatooineWelcomeVaderScreenPlay:greet(pVader)
	if pVader == nil then
		return
	end

	spatialChat(pVader, "Welcome to SWG Returns. Please head to the cantina to make new friends.")

	createEvent(90 * 1000, "TatooineWelcomeVaderScreenPlay", "greet", pVader, "")
end
