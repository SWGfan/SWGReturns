hoth_spawns = ScreenPlay:new { numberOfActs = 1, screenplayName = "hoth_spawns" }
registerScreenPlay("hoth_spawns", true)

function hoth_spawns:start()
  if (isZoneEnabled("hoth")) then self:spawnMobs() end
end

function hoth_spawns:spawnMobs()
  local p = "hoth"
  for i=1, 12 do
    spawnMobile(p, "wampa_alpha", 90, math.random(-600,600), 0, math.random(-600,600), math.random(0,360), 0)
  end
  for i=1, 4 do
    spawnMobile(p, "sith_boss", 240, math.random(-1200,-800), 0, math.random(-1200,-800), math.random(0,360), 0)
  end
  spawnMobile(p, "luke_skywalker_rotj", 3600, -100, 0, 100, 180, 0)
end
