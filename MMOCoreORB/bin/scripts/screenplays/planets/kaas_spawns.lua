kaas_spawns = ScreenPlay:new {
  numberOfActs = 1,
  screenplayName = "kaas_spawns"
}
registerScreenPlay("kaas_spawns", true)

function kaas_spawns:start()
  if (isZoneEnabled("kaas")) then
    self:spawnMobs()
  end
end

function kaas_spawns:spawnMobs()
  local planet = "kaas"
  local centers = { {0,0}, {512, -256}, {-640, 320} }
  for _,c in ipairs(centers) do
    for i=1, 6 do
      spawnMobile(planet, "sith_acolyte", 60, c[1]+math.random(-40,40), 0, c[2]+math.random(-40,40), math.random(0,360), 0)
    end
    for i=1, 3 do
      spawnMobile(planet, "sith_lord", 120, c[1]+math.random(-30,30), 0, c[2]+math.random(-30,30), math.random(0,360), 0)
    end
  end
  for i=1, 3 do
    spawnMobile(planet, "sith_boss", 180, math.random(-900,900), 0, math.random(-900,900), math.random(0,360), 0)
  end
  spawnMobile(planet, "emperor_palpatine", 3600, 120, 0, -120, 0, 0)
end
