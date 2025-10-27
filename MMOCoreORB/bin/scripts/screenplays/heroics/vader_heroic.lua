

vader_heroic = Heroic:new {
  numberOfActs = 1,
  screenplayName = "vader_heroic",
  title = "The Fist of the Empire"
}

registerScreenPlay("vader_heroic", true)

function vader_heroic:start()
  if isZoneEnabled("mustafar") then
    self:spawnTerminal("mustafar", 256, 0, -256, "object/tangible/terminal/terminal_mission.iff", 35)
  end
end

function vader_heroic:startWaves()

  -- Wave 1
  self:message(nil, "Imperial adepts secure the site!")
  self:spawnAround("sith_acolyte", 8, 0, 26)
  createEvent(45, self.screenplayName, "nextWave", nil, "")


  -- Wave 2
  self:message(nil, "Elite enforcers advance!")
  self:spawnAround("sith_boss", 2, 0, 20)
  createEvent(55, self.screenplayName, "nextWave", nil, "")

end

function vader_heroic:nextWave()
  self._wave = (self._wave or 0) + 1
  if self._wave == #2 then
    self:message(nil, "Final boss incoming!")
    self:spawnAround("darth_vader", 1, 0, 5)
    createEvent(300, self.screenplayName, "endEncounter", nil, "")
  end
end

function vader_heroic:endEncounter()
  self:message(nil, "Heroic complete. Well fought!")
  self.active = false
  self:cleanup()
end
