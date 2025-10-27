

malak_heroic = Heroic:new {
  numberOfActs = 1,
  screenplayName = "malak_heroic",
  title = "Star Forge Scion"
}

registerScreenPlay("malak_heroic", true)

function malak_heroic:start()
  if isZoneEnabled("moraband") then
    self:spawnTerminal("moraband", 40, 0, 220, "object/tangible/terminal/terminal_mission.iff", 35)
  end
end

function malak_heroic:startWaves()

  -- Wave 1
  self:message(nil, "Acolyte vanguard approaches!")
  self:spawnAround("sith_acolyte", 6, 0, 24)
  createEvent(40, self.screenplayName, "nextWave", nil, "")


  -- Wave 2
  self:message(nil, "Champions step forward!")
  self:spawnAround("sith_boss", 2, 0, 18)
  createEvent(55, self.screenplayName, "nextWave", nil, "")

end

function malak_heroic:nextWave()
  self._wave = (self._wave or 0) + 1
  if self._wave == #2 then
    self:message(nil, "Final boss incoming!")
    self:spawnAround("darth_malak", 1, 0, 5)
    createEvent(300, self.screenplayName, "endEncounter", nil, "")
  end
end

function malak_heroic:endEncounter()
  self:message(nil, "Heroic complete. Well fought!")
  self.active = false
  self:cleanup()
end
