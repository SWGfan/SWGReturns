

hk47_heroic = Heroic:new {
  numberOfActs = 1,
  screenplayName = "hk47_heroic",
  title = "Meatbag Extermination Protocol"
}

registerScreenPlay("hk47_heroic", true)

function hk47_heroic:start()
  if isZoneEnabled("mustafar") then
    self:spawnTerminal("mustafar", 400, 0, -400, "object/tangible/terminal/terminal_mission.iff", 35)
  end
end

function hk47_heroic:startWaves()

  -- Wave 1
  self:message(nil, "Disposable organics inbound.")
  self:spawnAround("sith_acolyte", 6, 0, 25)
  createEvent(40, self.screenplayName, "nextWave", nil, "")


  -- Wave 2
  self:message(nil, "Priority targets detected.")
  self:spawnAround("sith_lord", 2, 0, 18)
  createEvent(55, self.screenplayName, "nextWave", nil, "")

end

function hk47_heroic:nextWave()
  self._wave = (self._wave or 0) + 1
  if self._wave == #2 then
    self:message(nil, "Final boss incoming!")
    self:spawnAround("hk47", 1, 0, 5)
    createEvent(300, self.screenplayName, "endEncounter", nil, "")
  end
end

function hk47_heroic:endEncounter()
  self:message(nil, "Heroic complete. Well fought!")
  self.active = false
  self:cleanup()
end
