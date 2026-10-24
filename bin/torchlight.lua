-- {"name": "Torchlight", "author": "Daniel Santos", "version": "23102026", "file": "torchlight.lua", "category": "3d", "blurb": "A torch in a dungeon: DOT3 normal mapping, stencil shadow volumes and a point light.", "exit": "TRIANGLE"}

-- Unified lighting, PS2 style: one flickering torch moves through a closed room.
-- Every surface is DOT3 normal mapped against it, and the pillars and crates
-- cast stencil shadow volumes from it -- onto the walls, the floor and each
-- other. The light is a point light, so both the relief and the shadows swing
-- as it passes. See torchlight.js.

local app = require "athenaos.app"

local ui = app.AppUI{ title = "Torchlight", category = "3d",
  hints = { { "L-stick", "Look" }, { "cross", "Shadows" }, { "square", "Mode" }, { "circle", "Torch" } } }

local fmt = string.format
local sin, cos, pi, sqrt = math.sin, math.cos, math.pi, math.sqrt

Screen.setFrameCounter(true)
Screen.setVSync(true)

local canvas = Screen.getMode()
canvas.zbuffering = true
canvas.psm = Screen.CT32
canvas.psmz = Screen.Z24 -- DOT3 and the shadow volumes both keep scratch in the top of Z24
Screen.setMode(canvas)
Screen.initBuffers()

ui:loading("Loading assets...", "Bricks, slabs and crates")

Render.init()
Render.setView(60.0, 0.5, 200.0)

os.chdir("render")

local MODES = { Render.DOT3_OFF, Render.DOT3_2PASS, Render.DOT3_4PASS }
local MODE_NAMES = { "Gouraud", "DOT3 2-pass", "DOT3 4-pass" }
local mode = 3

local brickDiffuse = Image("bricks_diffuse.png")
local brickNormal = Image("bricks_normal.png")
local tileDiffuse = Image("tiles_diffuse.png")
local tileNormal = Image("tiles_normal.png")
for _, img in ipairs({ brickDiffuse, brickNormal, tileDiffuse, tileNormal }) do img.filter = LINEAR end

-- Geometry -------------------------------------------------------------------------

-- A mesh from triangles given as { position, normal, uv } corners, drawn with
-- diffuse modulated by the torch and normal mapped with normalMap.
local function makeMesh(corners, diffuse, normalMap)
  local positions, normals, texcoords, colors = {}, {}, {}, {}
  for _, c in ipairs(corners) do
    local p, n, uv = c[1], c[2], c[3]
    for _, v in ipairs({ p[1], p[2], p[3], 1 }) do positions[#positions + 1] = v end
    for _, v in ipairs({ n[1], n[2], n[3], 1 }) do normals[#normals + 1] = v end
    for _, v in ipairs({ uv[1], uv[2], 1, 1 }) do texcoords[#texcoords + 1] = v end
    for _ = 1, 4 do colors[#colors + 1] = 0 end
  end
  local data = RenderData(Render.vertexList(positions, normals, texcoords, colors))
  local materials = data.materials
  materials[1].diffuse = { r = 1, g = 1, b = 1, a = 1 }
  materials[1].texture_id = data:pushTexture(diffuse)
  materials[1].normal_texture_id = data:pushTexture(normalMap)
  data.materials = materials
  return data
end

local function cross(a, b)
  local c = { a[2] * b[3] - a[3] * b[2], a[3] * b[1] - a[1] * b[3], a[1] * b[2] - a[2] * b[1] }
  local l = sqrt(c[1] * c[1] + c[2] * c[2] + c[3] * c[3])
  return { c[1] / l, c[2] / l, c[3] / l }
end

-- A rectangle from its top-left corner o and the edges u (rightwards) and v
-- (downwards), seen from the front; the texture repeated ru x rv times.
local function quad(corners, o, u, v, ru, rv)
  local n = cross(v, u)
  local function at(s, t)
    return { { o[1] + u[1] * s + v[1] * t, o[2] + u[2] * s + v[2] * t, o[3] + u[3] * s + v[3] * t }, n, { s * ru, t * rv } }
  end
  for _, st in ipairs({ { 0, 0 }, { 0, 1 }, { 1, 1 }, { 0, 0 }, { 1, 1 }, { 1, 0 } }) do
    corners[#corners + 1] = at(st[1], st[2])
  end
  return corners
end

-- An octagonal pillar around the origin, capped at both ends: a shadow volume
-- needs a closed mesh. Smooth side normals, the bricks once around.
local function pillar(radius, height)
  local corners = {}
  local function ring(k, y)
    local a = 2 * pi * k / 8
    return { sin(a) * radius, y, cos(a) * radius }
  end
  local function side(k, t)
    local a = 2 * pi * k / 8
    return { ring(k, height * (1 - t)), { sin(a), 0, cos(a) }, { k / 8, t * height / 4 } }
  end
  local function push(...)
    for _, c in ipairs({ ... }) do corners[#corners + 1] = c end
  end
  for k = 0, 7 do
    push(side(k, 0), side(k, 1), side(k + 1, 1), side(k, 0), side(k + 1, 1), side(k + 1, 0))
    -- Caps, wound to face up and down.
    push({ { 0, height, 0 }, { 0, 1, 0 }, { 0.5, 0.5 } }, { ring(k, height), { 0, 1, 0 }, { 0.5, 0 } },
         { ring(k + 1, height), { 0, 1, 0 }, { 0.6, 0 } })
    push({ { 0, 0, 0 }, { 0, -1, 0 }, { 0.5, 0.5 } }, { ring(k + 1, 0), { 0, -1, 0 }, { 0.6, 0 } },
         { ring(k, 0), { 0, -1, 0 }, { 0.5, 0 } })
  end
  return corners
end

-- A brick tile is 4 x 4 units (8 rows), a slab 2 x 2.
local R, H = 8.0, 5.0
local wallCorners = {}
quad(wallCorners, { -R, H, -R }, { 2 * R, 0, 0 }, { 0, -H, 0 }, 2 * R / 4, H / 4)  -- north, facing +z
quad(wallCorners, { R, H, R }, { -2 * R, 0, 0 }, { 0, -H, 0 }, 2 * R / 4, H / 4)   -- south, facing -z
quad(wallCorners, { -R, H, R }, { 0, 0, -2 * R }, { 0, -H, 0 }, 2 * R / 4, H / 4)  -- west, facing +x
quad(wallCorners, { R, H, -R }, { 0, 0, 2 * R }, { 0, -H, 0 }, 2 * R / 4, H / 4)   -- east, facing -x
local walls = makeMesh(wallCorners, brickDiffuse, brickNormal)
local floor = makeMesh(quad({}, { -R, 0, -R }, { 2 * R, 0, 0 }, { 0, 0, 2 * R }, 2 * R / 4, 2 * R / 4), tileDiffuse, tileNormal)
local ceiling = makeMesh(quad({}, { -R, H, R }, { 2 * R, 0, 0 }, { 0, 0, -2 * R }, 2 * R / 4, 2 * R / 4), tileDiffuse, tileNormal)
local pillarData = makeMesh(pillar(0.55, H), brickDiffuse, brickNormal)
local crateData = RenderData("box_bump.gltf")

local datas = { walls, floor, ceiling, pillarData, crateData }
local function applyMode()
  for _, data in ipairs(datas) do data.normal_mapping = MODES[mode] end
end
applyMode()

local room = { RenderObject(walls), RenderObject(floor), RenderObject(ceiling) }

-- Casters: everything inside the room. Not frozen -- each volume reads its
-- caster's positions once.
local casters = {}
local function place(data, x, y, z, ry)
  local obj = RenderObject(data)
  obj.position = { x = x, y = y, z = z }
  obj.rotation = { x = 0, y = ry, z = 0 }
  casters[#casters + 1] = obj
  return obj
end
-- Pillars within 3.2 of the centre, crates in the corners: the torch circles
-- between the two, the camera outside it.
place(pillarData, -2.2, 0, -1.4, 0.0)
place(pillarData, 2.2, 0, 1.4, 0.4)
place(crateData, 6.6, 1.0, -6.6, 0.3)
place(crateData, -6.6, 1.0, 6.6, -0.5)
place(crateData, -6.6, 3.0, 6.6, 0.2)
place(crateData, 6.4, 1.0, 6.2, 0.9)
local floater = place(crateData, 0.3, 3.0, -0.5, 0.0)
floater.scale = { x = 0.6, y = 0.6, z = 0.6 }

local scene = {}
for _, obj in ipairs(room) do scene[#scene + 1] = obj end
for _, obj in ipairs(casters) do scene[#scene + 1] = obj end
local volumes = {}
for i, obj in ipairs(casters) do
  volumes[i] = Shadows.Volume(obj)
  volumes[i].extrusion = 30.0
end

-- The torch -----------------------------------------------------------------------

local light = Lights.new()
Lights.set(light, Lights.AMBIENT, 0.08, 0.08, 0.1)
Lights.set(light, Lights.ATTENUATION, 0, 0, 0.02)

-- Fire: additive camera-facing puffs from the soft cell of the shared FX atlas,
-- and a few embers.
local fxAtlas = Image("fx_atlas.png")
fxAtlas.filter = LINEAR
local fire = Particles.System{ texture = fxAtlas, capacity = 256, space = "world", blend = "additive", seed = 1234 }
local flame = fire:createEmitter{
  frame = { 0, 0, 32, 32 }, lifetime = { 0.25, 0.5 }, size = { 0.22, 0.34 }, scaleEnd = 0.2,
  velocityMin = { -0.15, 0.6, -0.15 }, velocityMax = { 0.15, 1.2, 0.15 }, spread = { 0.05, 0.02, 0.05 },
  color = { 128, 80, 30, 90 }, colorEnd = { 110, 30, 8, 0 }, rate = 90,
}
local embers = fire:createEmitter{
  frame = { 0, 0, 32, 32 }, lifetime = { 0.6, 1.2 }, size = { 0.03, 0.05 }, scaleEnd = 0.3,
  velocityMin = { -0.4, 0.5, -0.4 }, velocityMax = { 0.4, 1.6, 0.4 }, acceleration = { 0, -0.6, 0 },
  color = { 128, 90, 30, 128 }, colorEnd = { 128, 40, 10, 0 }, rate = 12,
}
flame:start()
embers:start()

-- Flicker: a few incommensurate sines, cheap and never repeating visibly.
local function flicker(t)
  return 0.86 + 0.07 * sin(t * 17.0) + 0.05 * sin(t * 23.7 + 1.3) + 0.03 * sin(t * 41.3 + 0.4)
end

-- The torch circles the room between the pillars and the crates, weaving in
-- and out and low enough to throw long shadows up the walls.
local function torchAngle(t) return t * 0.3 end
local function torchPath(t)
  local a, r = torchAngle(t), 4.6 + 0.4 * sin(t * 0.7)
  return sin(a) * r, 1.3 + 0.3 * sin(t * 0.53), cos(a) * r
end

-- Camera ---------------------------------------------------------------------------

-- The camera follows the torch around the room from across it, outside its
-- path, looking at the flame: the torch stays in view and the pillars throw
-- their shadows towards the camera. The stick swings it round.
local yaw, pitch = pi - 0.55, 0.0
local CAM_RADIUS, CAM_HEIGHT = 6.6, 3.0

local pad = Pads.get()
local black = Color.new(0, 0, 0, 128)
local timer = Timer.new()
local shadowsOn, torchMoving = true, true
local torchT, lastTime = 0.0, 0.0

local function axis(v)
  return (v > 25 or v < -25) and v / 128.0 or 0
end

while true do
  Screen.clear(black)
  Render.begin()
  pad:update()

  if ui:homeRequested(pad) then
    fire:dispose()
    os.chdir("..")
    app.exitToShell()
  end

  if pad:justPressed(Pads.CROSS) then shadowsOn = not shadowsOn end
  if pad:justPressed(Pads.SQUARE) then
    mode = mode % #MODES + 1
    applyMode()
  end
  if pad:justPressed(Pads.CIRCLE) then torchMoving = not torchMoving end

  local now = timer:getTime() / 1000000
  local dt = math.min(0.1, math.max(0, now - lastTime))
  lastTime = now
  if torchMoving then torchT = torchT + dt end

  yaw = yaw + axis(pad.lx) * dt * 1.6
  pitch = math.max(-1.0, math.min(1.0, pitch - axis(pad.ly) * dt * 1.5))

  local tx, ty, tz = torchPath(torchT)
  local ca = torchAngle(torchT) + yaw
  Camera.position(sin(ca) * CAM_RADIUS, CAM_HEIGHT, cos(ca) * CAM_RADIUS)
  Camera.target(tx * 0.7, 1.4 + pitch * 2.0, tz * 0.7)

  local f = flicker(now)
  local jitter = 0.03 * sin(now * 29.0)
  Lights.set(light, Lights.POSITION, tx + jitter, ty, tz - jitter)
  Lights.set(light, Lights.DIFFUSE, 1.7 * f, 1.3 * f, 0.85 * f)

  floater.rotation = { x = 0.5, y = now * 0.4, z = 0.3 }

  Camera.update()
  Screen.setParam(Screen.DEPTH_TEST_ENABLE, true)
  Screen.setParam(Screen.DEPTH_TEST_METHOD, Screen.DEPTH_GEQUAL)

  for _, obj in ipairs(room) do obj:render() end
  for _, obj in ipairs(casters) do obj:render() end

  -- Shadowed means ambient only: those pixels are drawn again without the
  -- torch, the way Doom 3 lights a scene.
  if shadowsOn and Shadows.beginVolumes() then
    for _, volume in ipairs(volumes) do volume:render() end
    Shadows.endVolumes(scene)
  end

  flame:setPosition(tx, ty - 0.05, tz)
  embers:setPosition(tx, ty, tz)
  fire:update(dt)
  fire:draw()

  Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)

  local prof = Screen.getProfile()
  ui.status = Screen.getFPS(360) .. " FPS"
  ui:hud({
    fmt("SQUARE  %s   CROSS  Shadows: %s (%d volumes)", MODE_NAMES[mode], shadowsOn and "ON" or "OFF", #volumes),
    fmt("frame %.1f ms  |  render %.1f  |  gpu %.1f", prof.frame, prof.render, prof.gpuWait),
  })
  ui:chrome()

  Screen.flip()
end
