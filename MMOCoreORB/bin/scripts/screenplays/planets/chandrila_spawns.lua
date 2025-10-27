chandrila_spawns = ScreenPlay:new { numberOfActs = 1, screenplayName = "chandrila_spawns" }
registerScreenPlay("chandrila_spawns", true)

function chandrila_spawns:start()
  if (isZoneEnabled("chandrila")) then self:spawnMobs() end
end

function chandrila_spawns:spawnMobs()
  local p = "chandrila"
  for i=1, 6 do
    spawnMobile(p, "sith_acolyte", 60, math.random(-500,500), 0, math.random(-500,500), math.random(0,360), 0)
  end
  for i=1, 3 do
    spawnMobile(p, "sith_lord", 120, math.random(-450,450), 0, math.random(-450,450), math.random(0,360), 0)
  end
  spawnMobile(p, "bastila_shan", 2400, 64, 0, 64, 270, 0)
  spawnMobile(p, "luke_skywalker_rotj", 3600, -64, 0, -64, 90, 0)
end
