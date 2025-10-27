

wampa_heroic = Heroic:new {
  numberOfActs = 1,
  screenplayName = "wampa_heroic",
  title = "Alpha of the White Waste"
}

registerScreenPlay("wampa_heroic", true)

function wampa_heroic:start()
  if isZoneEnabled("hoth") then
    self:spawnTerminal("hoth", -300, 0, -300, "object/tangible/terminal/terminal_mission.iff", 35)
  end
end

function wampa_heroic:startWaves()

  -- Wave 1
  self:message(nil, "The herd defends its lair!")
  self:spawnAround("wampa_alpha", 8, 0, 32)
  createEvent(50, self.screenplayName, "nextWave", nil, "")


  -- Wave 2
  self:message(nil, "Enraged alphas join the battle!")
  self:spawnAround("wampa_alpha", 6, 0, 28)
  createEvent(55, self.screenplayName, "nextWave", nil, "")

end

function wampa_heroic:nextWave()
  self._wave = (self._wave or 0) + 1
  if self._wave == #2 then
    self:message(nil, "Final boss incoming!")
    self:spawnAround("wampa_alpha", 1, 0, 5)
    createEvent(300, self.screenplayName, "endEncounter", nil, "")
  end
end

function wampa_heroic:endEncounter()
  self:message(nil, "Heroic complete. Well fought!")
  self.active = false
  self:cleanup()
end
