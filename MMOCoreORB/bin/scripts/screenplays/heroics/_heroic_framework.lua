-- Simple heroic encounter framework for Core3 ScreenPlays
Heroic = {}
function Heroic:new(o)
  o = o or {}
  setmetatable(o, self); self.__index = self
  return o
end

-- Spawns a clickable start terminal. On use, waves begin.
function Heroic:spawnTerminal(planet, x, z, y, template, radius)
  self.termObj = spawnSceneObject(planet, template or "object/tangible/terminal/terminal_mission.iff", x, z, y, 0, 0, 0, 0)
  if self.termObj ~= nil then
    createObserver(OBJECTRADIALUSED, self.screenplayName, "onTerminalUsed", self.termObj)
    self.center = {x=x, z=z, y=y, planet=planet, r=radius or 30}
  end
end

function Heroic:message(p, txt) broadcastMessage(p or self.center.planet, txt or "Prepare yourselves!") end

function Heroic:cleanup()
  for _,id in ipairs(self.spawned or {}) do
    if id ~= nil then destroySceneObject(id) end
  end
  self.spawned = {}
end

function Heroic:spawnAround(mobile, count, respawn, spread)
  local c = self.center
  self.spawned = self.spawned or {}
  for i=1, count do
    local sx = c.x + math.random(-spread, spread)
    local sy = c.y + math.random(-spread, spread)
    local oid = spawnMobile(c.planet, mobile, respawn or 0, sx, c.z, sy, math.random(0,360), 0)
    table.insert(self.spawned, oid)
  end
end

function Heroic:onTerminalUsed(pPlayer, pObj)
  if self.active then return 0 end
  self.active = true
  self:message(nil, "Heroic: " .. (self.title or self.screenplayName) .. " started!")
  self:startWaves()
  return 0
end
