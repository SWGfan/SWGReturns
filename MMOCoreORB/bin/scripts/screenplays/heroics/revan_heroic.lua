

revan_heroic = Heroic:new {
  numberOfActs = 1,
  screenplayName = "revan_heroic",
  title = "Vault of the Prodigal Knight"
}

registerScreenPlay("revan_heroic", true)

function revan_heroic:start()
  if isZoneEnabled("moraband") then
    self:spawnTerminal("moraband", 20, 0, 240, "object/tangible/terminal/terminal_mission.iff", 35)
  end
end

function revan_heroic:startWaves()

  -- Wave 1
  self:message(nil, "Acolytes pour from the tombs!")
  self:spawnAround("sith_acolyte", 8, 0, 28)
  createEvent(45, self.screenplayName, "nextWave", nil, "")


  -- Wave 2
  self:message(nil, "Sith Lords emerge to challenge you!")
  self:spawnAround("sith_lord", 4, 0, 22)
  createEvent(55, self.screenplayName, "nextWave", nil, "")

end

function revan_heroic:nextWave()
  self._wave = (self._wave or 0) + 1
  if self._wave == #2 then
    self:message(nil, "Final boss incoming!")
    self:spawnAround("darth_revan", 1, 0, 5)
    createEvent(300, self.screenplayName, "endEncounter", nil, "")
  end
end

function revan_heroic:endEncounter()
  self:message(nil, "Heroic complete. Well fought!")
  self.active = false
  self:cleanup()
end
