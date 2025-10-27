moraband_spawns = ScreenPlay:new { numberOfActs = 1, screenplayName = "moraband_spawns" }
registerScreenPlay("moraband_spawns", true)

function moraband_spawns:start()
  if (isZoneEnabled("moraband")) then self:spawnMobs() end
end

function moraband_spawns:spawnMobs()
  local p = "moraband"
  for i=1, 8 do
    spawnMobile(p, "sith_acolyte", 60, math.random(-700,700), 0, math.random(-700,700), math.random(0,360), 0)
  end
  for i=1, 4 do
    spawnMobile(p, "sith_lord", 120, math.random(-650,650), 0, math.random(-650,650), math.random(0,360), 0)
  end
  spawnMobile(p, "darth_revan", 5400, 0, 0, 256, 0, 0)
  spawnMobile(p, "darth_malak", 4800, 32, 0, 224, 180, 0)
  spawnMobile(p, "sith_boss", 240, -64, 0, 192, 90, 0)
end
