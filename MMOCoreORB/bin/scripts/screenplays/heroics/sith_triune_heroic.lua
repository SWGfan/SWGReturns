

sith_triune_heroic = Heroic:new {
  numberOfActs = 1,
  screenplayName = "sith_triune_heroic",
  title = "Triune of the Dark Side"
}

registerScreenPlay("sith_triune_heroic", true)

function sith_triune_heroic:start()
  if isZoneEnabled("hutta") then
    self:spawnTerminal("hutta", 180, 0, -180, "object/tangible/terminal/terminal_mission.iff", 35)
  end
end

function sith_triune_heroic:startWaves()

  -- Wave 1
  self:message(nil, "Cultists chant in the swamp.")
  self:spawnAround("sith_acolyte", 8, 0, 26)
  createEvent(45, self.screenplayName, "nextWave", nil, "")


  -- Wave 2
  self:message(nil, "Their masters reveal themselves.")
  self:spawnAround("sith_lord", 3, 0, 20)
  createEvent(55, self.screenplayName, "nextWave", nil, "")

end

function sith_triune_heroic:nextWave()
  self._wave = (self._wave or 0) + 1
  if self._wave == #2 then
    self:message(nil, "Final boss incoming!")
    self:spawnAround("sith_boss", 1, 0, 5)
    createEvent(300, self.screenplayName, "endEncounter", nil, "")
  end
end

function sith_triune_heroic:endEncounter()
  self:message(nil, "Heroic complete. Well fought!")
  self.active = false
  self:cleanup()
end
