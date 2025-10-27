

luke_heroic = Heroic:new {
  numberOfActs = 1,
  screenplayName = "luke_heroic",
  title = "Knight of the New Hope"
}

registerScreenPlay("luke_heroic", true)

function luke_heroic:start()
  if isZoneEnabled("hoth") then
    self:spawnTerminal("hoth", -100, 0, 100, "object/tangible/terminal/terminal_mission.iff", 35)
  end
end

function luke_heroic:startWaves()

  -- Wave 1
  self:message(nil, "The ice cracks as Wampas charge!")
  self:spawnAround("wampa_alpha", 6, 0, 30)
  createEvent(45, self.screenplayName, "nextWave", nil, "")


  -- Wave 2
  self:message(nil, "Dark presences test the hero!")
  self:spawnAround("sith_lord", 3, 0, 20)
  createEvent(55, self.screenplayName, "nextWave", nil, "")

end

function luke_heroic:nextWave()
  self._wave = (self._wave or 0) + 1
  if self._wave == #2 then
    self:message(nil, "Final boss incoming!")
    self:spawnAround("luke_skywalker_rotj", 1, 0, 5)
    createEvent(300, self.screenplayName, "endEncounter", nil, "")
  end
end

function luke_heroic:endEncounter()
  self:message(nil, "Heroic complete. Well fought!")
  self.active = false
  self:cleanup()
end
