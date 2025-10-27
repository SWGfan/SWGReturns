hutta_spawns = ScreenPlay:new { numberOfActs = 1, screenplayName = "hutta_spawns" }
registerScreenPlay("hutta_spawns", true)

function hutta_spawns:start()
  if (isZoneEnabled("hutta")) then self:spawnMobs() end
end

function hutta_spawns:spawnMobs()
  local p = "hutta"
  for i=1, 5 do
    spawnMobile(p, "sith_acolyte", 60, math.random(-400,400), 0, math.random(-400,400), math.random(0,360), 0)
  end
  for i=1, 2 do
    spawnMobile(p, "sith_lord", 120, math.random(-380,380), 0, math.random(-380,380), math.random(0,360), 0)
  end
  spawnMobile(p, "hk47", 3600, -220, 0, 140, 0, 0)
  spawnMobile(p, "emperor_palpatine", 7200, 180, 0, -180, 0, 0)
end
