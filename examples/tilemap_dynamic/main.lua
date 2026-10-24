
local app = require "athenaos.app"

local ui = app.AppUI{ title = "Tilemap Dynamic", category = "2d",
                      hints = { { "cross", "Spawn" }, { "circle", "Remove" }, { "SELECT", "Restart" } } }
local pad = Pads.get()
Screen.setFrameCounter(true)

local TILE_SIZE = 16
local MAX_SPRITES = 1024
local random = math.random
math.randomseed(os.millis())

local descriptor = TileMap.Descriptor{
  textures = { "simple_atlas.png" },
  materials = { {
    texture_index = 0,
    blend_mode = Screen.alphaEquation(Screen.ZERO_RGB, Screen.SRC_RGB, Screen.SRC_ALPHA, Screen.DST_RGB, 0),
    end_offset = MAX_SPRITES - 1,
  } },
}

local spriteBuffer = TileMap.SpriteBuffer.create(MAX_SPRITES)
local tileMap = TileMap.Instance{ descriptor = descriptor, spriteBuffer = spriteBuffer }

local stride, offsets = TileMap.layout.stride, TileMap.layout.offsets
local buf = tileMap:getSpriteBuffer()

-- Sprite records are 0-based like the buffer; slots[i + 1] tracks sprite i.
local slots = {}
for i = 1, MAX_SPRITES do
  slots[i] = { alive = false, life = 0, phase = 0, speed = 0, tx = 0, ty = 0 }
end
local alive = 0

local function setF(i, field, v) buf:setF32(i * stride + offsets[field], v) end
local function setU(i, field, v) buf:setU32(i * stride + offsets[field], v) end

local function hideSprite(i)
  setF(i, "w", 0)
  setF(i, "h", 0)
  setU(i, "a", 0)
  if slots[i + 1].alive then alive = alive - 1 end
  slots[i + 1].alive = false
end

local function initSlot(i, x, y)
  local slot = slots[i + 1]
  slot.alive = true
  slot.life = 4 + random() * 6
  slot.phase = random() * math.pi * 2
  slot.speed = 0.5 + random()
  slot.tx, slot.ty = x, y
  alive = alive + 1

  setF(i, "x", x)
  setF(i, "y", y)
  setF(i, "zindex", 1)
  setF(i, "w", TILE_SIZE)
  setF(i, "h", TILE_SIZE)

  local atlasX, atlasY = random() > 0.5 and 64 or 0, random() > 0.5 and 64 or 0
  setF(i, "u1", atlasX)
  setF(i, "v1", atlasY)
  setF(i, "u2", atlasX + 64)
  setF(i, "v2", atlasY + 64)

  local tint = 120 + random(0, 119)
  setU(i, "r", tint)
  setU(i, "g", 220 - tint // 2)
  setU(i, "b", 200)
  setU(i, "a", 255)
end

local function aliveIndices()
  local list = {}
  for i, slot in ipairs(slots) do
    if slot.alive then list[#list + 1] = i - 1 end
  end
  return list
end

local function spawnRandom()
  for i, slot in ipairs(slots) do
    if not slot.alive then
      initSlot(i - 1, 40 + random() * 600, 40 + random() * 400)
      return
    end
  end
end

local function removeRandom()
  local list = aliveIndices()
  if #list > 0 then hideSprite(list[random(#list)]) end
end

local time, spawnAccumulator = 0, 0

Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)
TileMap.init()

while true do
  Screen.clear(Color.new(200, 200, 200))
  pad:update()
  TileMap.begin()

  if ui:homeRequested(pad) then app.exitToShell() end
  if pad:justPressed(Pads.SELECT) then System.reload("apps/tilemap_dynamic/main.lua") end
  if pad:justPressed(Pads.CROSS) then spawnRandom() end
  if pad:justPressed(Pads.CIRCLE) then removeRandom() end

  time = time + 0.016
  spawnAccumulator = spawnAccumulator + 0.016
  if spawnAccumulator > 0.35 then
    spawnRandom()
    spawnAccumulator = 0
  end

  local list = aliveIndices()
  for _ = 1, math.min(10, #list) do
    local pick = table.remove(list, random(#list))
    local slot = slots[pick + 1]

    slot.life = slot.life - 0.016
    slot.phase = slot.phase + slot.speed * 0.05

    setF(pick, "x", slot.tx + math.sin(slot.phase) * 6)
    setF(pick, "y", slot.ty + math.cos(slot.phase * 1.3) * 4)

    local pulse = (math.sin(time * 3 + pick * 0.25) + 1) * 0.5
    setU(pick, "a", 160 + math.floor(pulse * 80))

    if slot.life <= 0 then hideSprite(pick) end
  end

  tileMap:render(20, 20)

  ui.status = Screen.getFPS(60) .. " FPS"
  ui:hud({ alive .. " / " .. MAX_SPRITES .. " sprites alive" })
  ui:chrome()

  Screen.flip()
end
