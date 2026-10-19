-- {"name": "Tilemap Particles", "author": "Daniel Santos", "version": "10182026", "file": "tilemap_particles.lua", "category": "2d", "blurb": "Thousands of tilemap-driven particles.", "exit": "TRIANGLE"}

local app = require "athenaos.app"

local ui = app.AppUI{ title = "Tilemap Particles", category = "2d",
                      hints = { { "cross", "Burst" }, { "square", "More" }, { "circle", "Fewer" }, { "SELECT", "Restart" } } }
local pad = Pads.get()
Screen.setFrameCounter(true)

local MAX_PARTICLES = 256
local INITIAL_PARTICLES = 32
local TILE_SIZE = 16
local random = math.random
math.randomseed(os.millis())

local activeCount = INITIAL_PARTICLES

local function buildDescriptor(endIndex)
  return TileMap.Descriptor{
    textures = {},
    materials = { {
      texture_index = -1,
      blend_mode = Screen.alphaEquation(Screen.SRC_RGB, Screen.DST_RGB, Screen.SRC_ALPHA, Screen.DST_RGB, 0),
      end_offset = endIndex,
    } },
  }
end

local stride, offsets = TileMap.layout.stride, TileMap.layout.offsets
local descriptor = buildDescriptor(activeCount - 1)
local spriteBuffer = TileMap.SpriteBuffer.create(MAX_PARTICLES)
local tileMap = TileMap.Instance{ descriptor = descriptor, spriteBuffer = spriteBuffer }

-- particles[i + 1] drives sprite record i (records are 0-based)
local particles = {}
for i = 1, MAX_PARTICLES do
  particles[i] = { alive = false, x = 0, y = 0, vx = 0, vy = 0, size = TILE_SIZE, life = 0 }
end

local function setF(i, field, v) spriteBuffer:setF32(i * stride + offsets[field], v) end
local function setU(i, field, v) spriteBuffer:setU32(i * stride + offsets[field], v) end

local function randomChannel(base, spread)
  return math.max(0, math.min(255, base + math.floor((random() - 0.5) * spread)))
end

local function setInactive(i)
  particles[i + 1].alive = false
  setU(i, "a", 0)
end

local function initParticle(i, x, y, vx, vy)
  local p = particles[i + 1]
  p.alive = true
  p.x, p.y, p.vx, p.vy = x, y, vx, vy
  p.life = 1.5 + random() * 1.5
  p.size = 12 + random() * 10

  setF(i, "x", x)
  setF(i, "y", y)
  setF(i, "zindex", 0.5)
  setF(i, "w", p.size)
  setF(i, "h", p.size)
  setU(i, "r", randomChannel(100, 80))
  setU(i, "g", randomChannel(60, 60))
  setU(i, "b", randomChannel(128, 40))
  setU(i, "a", 255)
end

local function spawnBurst(count)
  for _ = 1, count do
    local idx
    for i = 0, activeCount - 1 do
      if not particles[i + 1].alive then idx = i break end
    end
    if not idx then return end
    local speed = 30 + random() * 90
    local angle = random() * math.pi * 2
    initParticle(idx, 220, 140, math.cos(angle) * speed, math.sin(angle) * speed)
  end
end

local function resizeActiveCount(delta)
  local newCount = math.max(1, math.min(MAX_PARTICLES, activeCount + delta))
  if newCount == activeCount then return end

  for i = newCount, activeCount - 1 do setInactive(i) end
  activeCount = newCount

  -- a smaller/larger buffer with the surviving records, as the JS sample does
  local newBuffer = Buffer(activeCount * stride)
  newBuffer:copy(spriteBuffer, 0, 0, math.min(#spriteBuffer, #newBuffer))
  descriptor = buildDescriptor(activeCount - 1)
  spriteBuffer = newBuffer
  tileMap = TileMap.Instance{ descriptor = descriptor, spriteBuffer = spriteBuffer }
end

local time = 0
Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)
TileMap.init()

-- start with something on screen
spawnBurst(INITIAL_PARTICLES)

while true do
  Screen.clear(Color.new(70, 5, 150))
  pad:update()
  TileMap.begin()

  if ui:homeRequested(pad) then app.exitToShell() end
  if pad:justPressed(Pads.SELECT) then System.reload("tilemap_particles.lua") end
  if pad:justPressed(Pads.CROSS) then spawnBurst(8) end
  if pad:justPressed(Pads.SQUARE) then resizeActiveCount(32) end
  if pad:justPressed(Pads.CIRCLE) then resizeActiveCount(-32) end

  time = time + 0.016

  for i = 0, activeCount - 1 do
    local p = particles[i + 1]
    if p.alive then
      p.life = p.life - 0.016
      if p.life <= 0 then
        setInactive(i)
      else
        p.vy = p.vy + 30 * 0.016
        p.vx, p.vy = p.vx * 0.98, p.vy * 0.98
        p.x, p.y = p.x + p.vx * 0.016, p.y + p.vy * 0.016
        setF(i, "x", p.x)
        setF(i, "y", p.y)
        setU(i, "a", math.floor(255 * math.max(0, p.life / 3)))
      end
    end
  end

  tileMap:render(0, 0)

  ui.status = Screen.getFPS(60) .. " FPS"
  ui:hud({ activeCount .. " particle slots active" })
  ui:chrome()
  Screen.flip()
end
