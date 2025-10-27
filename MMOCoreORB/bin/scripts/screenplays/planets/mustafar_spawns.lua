mustafar_spawns = ScreenPlay:new { numberOfActs = 1, screenplayName = "mustafar_spawns" }
registerScreenPlay("mustafar_spawns", true)

function mustafar_spawns:start()
  if (isZoneEnabled("mustafar")) then self:spawnMobs() end
end

function mustafar_spawns:spawnMobs()
  local p = "mustafar"
  for i=1, 3 do
    spawnMobile(p, "hk47", 600, math.random(200,800), 0, math.random(-800,-200), math.random(0,360), 0)
  end
  for i=1, 5 do
    spawnMobile(p, "sith_boss", 180, math.random(100,900), 0, math.random(-900,-100), math.random(0,360), 0)
  end
  spawnMobile(p, "darth_vader", 5400, 256, 0, -256, 90, 0)
end
