mosEisleyStarportNpcsScreenplay = ScreenPlay:new {
	numberOfActs = 1,

	npcs = {
		{ template = "han_solo",           x = 3520, z = 4,  y = -4795, angle = 90 },
		{ template = "leia_organa",        x = 3524, z = 4,  y = -4798, angle = 90 },
		{ template = "luke_skywalker",     x = 3532, z = 4, y = -4798, angle = 270 },
		{ template = "chewbacca",          x = 3536, z = 4,  y = -4795, angle = 270 },
		{ template = "character_builder_frog", x = 3528, z = 4, y = -4790, angle = 180 },
		{ template = "ahsoka_tano",         x = 3529, z = 4,  y = -4787, angle = 180 },
	}
}

registerScreenPlay("mosEisleyStarportNpcsScreenplay", true)

function mosEisleyStarportNpcsScreenplay:start()
	if (isZoneEnabled("tatooine")) then
		self:spawnMobiles()
	end
end

function mosEisleyStarportNpcsScreenplay:spawnMobiles()
	local npcs = self.npcs
	for i = 1, #npcs, 1 do
		spawnMobile("tatooine", npcs[i].template, 0, npcs[i].x, npcs[i].z, npcs[i].y, npcs[i].angle, 0)
	end
end
