-- {"name": "Tilemap", "author": "Daniel Santos", "version": "10182026", "file": "tilemap.lua", "category": "2d", "blurb": "High-throughput VU1 tilemap renderer.", "exit": "TRIANGLE"}

local app = require "athenaos.app"

local ui = app.AppUI{ title = "Tilemap", category = "2d", hints = {} }
local pad = Pads.get()
Screen.setFrameCounter(true)

local tilesX, tilesY, tileSize = 6, 4, 48

local spriteDefs = {}
for y = 0, tilesY - 1 do
  for x = 0, tilesX - 1 do
    local atlasX, atlasY = (x % 2) * 64, (y % 2) * 64
    spriteDefs[#spriteDefs + 1] = {
      x = x * tileSize, y = y * tileSize, w = tileSize, h = tileSize, zindex = 1,
      u1 = atlasX, v1 = atlasY, u2 = atlasX + 64, v2 = atlasY + 64,
      r = 180, g = 180, b = 180, a = 200,
    }
  end
end

local descriptor = TileMap.Descriptor{
  textures = { "simple_atlas.png" },
  materials = { {
    texture_index = 0,
    blend_mode = Screen.alphaEquation(Screen.ZERO_RGB, Screen.SRC_RGB, Screen.SRC_ALPHA, Screen.DST_RGB, 0),
    end_offset = #spriteDefs - 1,
  } },
}

local spriteBuffer = TileMap.SpriteBuffer.fromObjects(spriteDefs)
local tileMap = TileMap.Instance{ descriptor = descriptor, spriteBuffer = spriteBuffer }

-- Direct record access through the layout, like a DataView in JS.
local stride, offsets = TileMap.layout.stride, TileMap.layout.offsets
local buf = tileMap:getSpriteBuffer()

local function updateBuffer(time)
  for i, base in ipairs(spriteDefs) do
    local idx = i - 1
    local row, col = idx // tilesX, idx % tilesX
    local at = idx * stride
    buf:setF32(at + offsets.x, base.x + math.sin(time + col * 0.5) * 6)
    buf:setF32(at + offsets.y, base.y + math.cos(time * 0.8 + row * 0.6) * 4)
    local pulse = math.sin(time + idx * 0.15) * 0.5 + 0.5
    local hue = 120 + math.floor(pulse * 100)
    buf:setU32(at + offsets.r, hue)
    buf:setU32(at + offsets.g, 200 - hue // 2)
    buf:setU32(at + offsets.b, 220)
  end
end

local time = 0
Screen.setParam(Screen.DEPTH_TEST_ENABLE, true)
TileMap.init()

while true do
  Screen.clear(Color.new(200, 200, 200))
  pad:update()
  TileMap.begin()

  if ui:homeRequested(pad) then app.exitToShell() end

  time = time + 0.05
  updateBuffer(time)
  tileMap:render(80, 60)

  ui.status = Screen.getFPS(60) .. " FPS"
  ui:hud({ #spriteDefs .. " sprites, one VU1 draw" })
  ui:chrome()
  Screen.flip()
end
