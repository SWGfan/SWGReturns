

palpatine_heroic = Heroic:new {
  numberOfActs = 1,
  screenplayName = "palpatine_heroic",
  title = "The Emperor's Decree"
}

registerScreenPlay("palpatine_heroic", true)

function palpatine_heroic:start()
  if isZoneEnabled("kaas") then
    self:spawnTerminal("kaas", 120, 0, -140, "object/tangible/terminal/terminal_mission.iff", 35)
  end
end

function palpatine_heroic:startWaves()

  -- Wave 1
  self:message(nil, "Royal Guardsmen masquerade as Sith Lords!")
  self:spawnAround("sith_lord", 5, 0, 24)
  createEvent(50, self.screenplayName, "nextWave", nil, "")


  -- Wave 2
  self:message(nil, "Shadow Councilors join the fray!")
  self:spawnAround("sith_boss", 2, 0, 18)
  createEvent(60, self.screenplayName, "nextWave", nil, "")

end

function palpatine_heroic:nextWave()
  self._wave = (self._wave or 0) + 1
  if self._wave == #2 then
    self:message(nil, "Final boss incoming!")
    self:spawnAround("emperor_palpatine", 1, 0, 5)
    createEvent(300, self.screenplayName, "endEncounter", nil, "")
  end
end

function palpatine_heroic:endEncounter()
  self:message(nil, "Heroic complete. Well fought!")
  self.active = false
  self:cleanup()
end
