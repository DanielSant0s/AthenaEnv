
-- DOT3 normal mapping after "DOT3 Normal Mapping on the PS2" (Mikkelsen, 2004).
-- Dungeon: a corner of brick walls and stone slabs, plus an octagonal pillar --
-- a few big triangles that Gouraud can only light flat, while DOT3 brings out
-- every brick, chip and crack as the light sweeps across. Side by side: Gouraud | 2-pass | 4-pass, as crates and as low-poly spheres --
-- a flat face gets the same light vector at every vertex, so only a curved
-- surface shows the 2-pass light losing length between vertices. Stress: nine
-- crates, all in one mode, for the cost of each. The light circles the
-- objects; the left stick moves it by hand. See normal_mapping.js.

local app = require "athenaos.app"

local ui = app.AppUI{ title = "Normal mapping", category = "3d",
  hints = { { "L-stick", "Light" }, { "cross", "Layout" }, { "square", "Mode" }, { "circle", "Spin" } } }

local fmt = string.format
local sin, cos, pi, floor = math.sin, math.cos, math.pi, math.floor

Screen.setFrameCounter(true)

local canvas = Screen.getMode()
canvas.zbuffering = true
canvas.psm = Screen.CT32
canvas.psmz = Screen.Z24 -- DOT3 keeps its scratch byte in the top of Z24
Screen.setMode(canvas)
Screen.initBuffers()

ui:loading("Loading assets...", "Bricks, slabs and crates")

Render.init()
Render.setView(60.0, 1.0, 400.0)

os.chdir("render")

local MODES = { Render.DOT3_OFF, Render.DOT3_2PASS, Render.DOT3_4PASS }
local MODE_NAMES = { "Gouraud + emboss", "DOT3 2-pass", "DOT3 4-pass" }

-- One RenderData per mode: the mode is a property of the data.
local crates = {}
for i, mode in ipairs(MODES) do
  crates[i] = RenderData("box_bump.gltf")
  crates[i].normal_mapping = mode
end

local diffuseMap = Image("box_diffuse.png")
local normalMap = Image("box_normalmap.png")
diffuseMap.filter = LINEAR
normalMap.filter = LINEAR

-- A UV sphere as a triangle list, the crate maps wrapped twice around it.
local function makeSphere(rings, segments, radius, mode)
  local positions, normals, texcoords, colors = {}, {}, {}, {}
  local function put(r, s)
    local theta, phi = pi * r / rings, 2 * pi * s / segments
    local x, y, z = sin(theta) * cos(phi), cos(theta), sin(theta) * sin(phi)
    for _, v in ipairs({ x * radius, y * radius, z * radius, 1 }) do positions[#positions + 1] = v end
    for _, v in ipairs({ x, y, z, 1 }) do normals[#normals + 1] = v end
    for _, v in ipairs({ 2 * s / segments, r / rings, 1, 1 }) do texcoords[#texcoords + 1] = v end
    for _ = 1, 4 do colors[#colors + 1] = 0 end
  end
  for r = 0, rings - 1 do
    for s = 0, segments - 1 do
      put(r, s); put(r + 1, s); put(r + 1, s + 1)
      put(r, s); put(r + 1, s + 1); put(r, s + 1)
    end
  end
  local data = RenderData(Render.vertexList(positions, normals, texcoords, colors))
  data.face_culling = Render.CULL_FACE_NONE
  local materials = data.materials
  materials[1].diffuse = { r = 1, g = 1, b = 1, a = 1 }
  materials[1].texture_id = data:pushTexture(diffuseMap)
  materials[1].normal_texture_id = data:pushTexture(normalMap)
  data.materials = materials
  data.normal_mapping = mode
  return data
end

local spheres = {}
for i, mode in ipairs(MODES) do spheres[i] = makeSphere(8, 12, 1.5, mode) end

-- Dungeon --------------------------------------------------------------------------
-- Procedural, tileable 256x256 maps (bricks and stone slabs) whose relief is
-- what Gouraud cannot show: flat albedo, all the shape in the normal map.
local brickDiffuse = Image("bricks_diffuse.png")
local brickNormal = Image("bricks_normal.png")
local tileDiffuse = Image("tiles_diffuse.png")
local tileNormal = Image("tiles_normal.png")
for _, img in ipairs({ brickDiffuse, brickNormal, tileDiffuse, tileNormal }) do img.filter = LINEAR end

-- A mesh from triangles given as { position, normal, uv } corners.
local function makeMesh(corners, diffuse, normalMap, mode)
  local positions, normals, texcoords, colors = {}, {}, {}, {}
  for _, c in ipairs(corners) do
    local p, n, uv = c[1], c[2], c[3]
    for _, v in ipairs({ p[1], p[2], p[3], 1 }) do positions[#positions + 1] = v end
    for _, v in ipairs({ n[1], n[2], n[3], 1 }) do normals[#normals + 1] = v end
    for _, v in ipairs({ uv[1], uv[2], 1, 1 }) do texcoords[#texcoords + 1] = v end
    for _ = 1, 4 do colors[#colors + 1] = 0 end
  end
  local data = RenderData(Render.vertexList(positions, normals, texcoords, colors))
  data.face_culling = Render.CULL_FACE_NONE
  local materials = data.materials
  materials[1].diffuse = { r = 1, g = 1, b = 1, a = 1 }
  materials[1].texture_id = data:pushTexture(diffuse)
  materials[1].normal_texture_id = data:pushTexture(normalMap)
  data.materials = materials
  data.normal_mapping = mode
  return data
end

-- A rectangle from its top-left corner o and the edges u (rightwards) and v
-- (downwards): texture u along u, texture v along v, repeated ru x rv times.
local function quad(corners, o, u, v, n, ru, rv)
  local function at(s, t)
    return { { o[1] + u[1] * s + v[1] * t, o[2] + u[2] * s + v[2] * t, o[3] + u[3] * s + v[3] * t }, n, { s * ru, t * rv } }
  end
  for _, st in ipairs({ { 0, 0 }, { 0, 1 }, { 1, 1 }, { 0, 0 }, { 1, 1 }, { 1, 0 } }) do
    corners[#corners + 1] = at(st[1], st[2])
  end
  return corners
end

-- The pillar: 8 sides, smooth normals, the bricks once around.
local function pillar(corners, x, z, radius, height)
  local function side(k, t)
    local a = 2 * pi * k / 8
    local n = { sin(a), 0, cos(a) }
    return { { x + n[1] * radius, height * (1 - t), z + n[3] * radius }, n, { k / 8, t * height / 4 } }
  end
  for k = 0, 7 do
    for _, c in ipairs({ side(k, 0), side(k, 1), side(k + 1, 1), side(k, 0), side(k + 1, 1), side(k + 1, 0) }) do
      corners[#corners + 1] = c
    end
  end
  return corners
end

local ROOM, WALL = 4.0, 4.0
local dungeonObjects = {}
for i, mode in ipairs(MODES) do
  local walls = {}
  quad(walls, { -ROOM, WALL, -ROOM }, { 2 * ROOM, 0, 0 }, { 0, -WALL, 0 }, { 0, 0, 1 }, 2, 1)  -- back wall
  quad(walls, { -ROOM, WALL, ROOM }, { 0, 0, -2 * ROOM }, { 0, -WALL, 0 }, { 1, 0, 0 }, 2, 1) -- left wall
  pillar(walls, 1.4, -1.2, 0.6, WALL)
  local floorCorners = quad({}, { -ROOM, 0, -ROOM }, { 2 * ROOM, 0, 0 }, { 0, 0, 2 * ROOM }, { 0, 1, 0 }, 2, 2)
  dungeonObjects[i] = {
    RenderObject(makeMesh(walls, brickDiffuse, brickNormal, mode)),
    RenderObject(makeMesh(floorCorners, tileDiffuse, tileNormal, mode)),
  }
end

local DUNGEON, SIDE, SPHERES, STRESS = 1, 2, 3, 4
local LAYOUT_NAMES = { "dungeon", "crates side by side", "spheres side by side", "9 crates" }
local layout = DUNGEON
local shownMode = 3
local spin = true

local function placedRow(datas, spacing)
  local objects = {}
  for i, data in ipairs(datas) do
    local obj = RenderObject(data)
    obj.position = { x = (i - 2) * spacing, y = 0.0, z = 0.0 }
    objects[i] = obj
  end
  return objects
end
local sideObjects = placedRow(crates, 3.2)
local sphereObjects = placedRow(spheres, 3.4)

-- 3x3 crates, the same mode, rebound when the mode changes.
local stressObjects = {}
local function buildStress()
  stressObjects = {}
  for i = 0, 8 do
    local obj = RenderObject(crates[shownMode])
    obj.position = { x = ((i % 3) - 1) * 3.0, y = (floor(i / 3) - 1) * 2.6, z = -2.0 }
    stressObjects[i + 1] = obj
  end
end
buildStress()

Camera.position(0.0, 0.0, 9.0)
Camera.target(0.0, 0.0, 0.0)

local light = Lights.new()
Lights.set(light, Lights.AMBIENT, 0.18, 0.18, 0.18)
Lights.set(light, Lights.DIFFUSE, 0.9, 0.9, 0.9)

local pad = Pads.get()
local gray = Color.new(40, 40, 40, 128)

local lightAngle, lightPitch, crateAngle = 0.6, 0.5, 0.4
local sweep = 0.01 -- dungeon: the light swings across the walls, low and grazing

-- Automatic benchmark -------------------------------------------------------------
-- Cycles the stress layout through the three modes, BENCH_PHASE_MS each, and
-- logs the averaged Screen.getProfile() of every phase. VSync is off meanwhile,
-- so the frame time is what the frame really costs.
local BENCHMARK = rawget(_G, "ATHENA_BENCHMARK") or false
Screen.setVSync(not BENCHMARK)
local BENCH_PHASE_MS = 10000
local BENCH_WARMUP_MS = 2000
local BENCH_KEYS = { "frame", "script", "render", "gpuWait", "vsync" }
local benchPhase = 1
local benchSum, benchSamples, benchFrames = nil, 0, 0
local benchTimer = Timer.new()
local function benchApply()
  layout = STRESS
  shownMode = benchPhase
  buildStress()
  benchSum = {}
  for _, k in ipairs(BENCH_KEYS) do benchSum[k] = 0 end
  benchSamples, benchFrames = 0, 0
  benchTimer:reset()
end
local function benchTick()
  local elapsed = benchTimer:getTime() / 1000
  benchFrames = benchFrames + 1
  if elapsed >= BENCH_WARMUP_MS and benchFrames > 60 then
    local prof = Screen.getProfile()
    for _, k in ipairs(BENCH_KEYS) do benchSum[k] = benchSum[k] + prof[k] end
    benchSamples = benchSamples + 1
  end
  if elapsed < BENCH_PHASE_MS or benchSamples == 0 then return end
  local avg = {}
  for _, k in ipairs(BENCH_KEYS) do avg[k] = benchSum[k] / benchSamples end
  print(fmt("[bench] %s x9 | %.1f fps | frame %.1f | script %.1f | render %.1f | gpu wait %.1f | vsync %.1f (ms)",
    MODE_NAMES[benchPhase], 1000 / avg.frame, avg.frame, avg.script, avg.render, avg.gpuWait, avg.vsync))
  benchPhase = benchPhase % #MODES + 1
  benchApply()
end
if BENCHMARK then
  print("[bench] automatic benchmark on: 3 phases, " .. (BENCH_PHASE_MS / 1000) .. " s each")
  benchApply()
end

local function axis(v, scale)
  return (v > 25 or v < -25) and v / scale or 0
end

while true do
  Screen.clear(gray)
  Render.begin()
  pad:update()

  if ui:homeRequested(pad) then
    os.chdir("..")
    app.exitToShell()
  end

  if not BENCHMARK then
    if pad:justPressed(Pads.CROSS) then layout = layout % #LAYOUT_NAMES + 1 end
    if pad:justPressed(Pads.SQUARE) then
      shownMode = shownMode % #MODES + 1
      buildStress()
    end
    if pad:justPressed(Pads.CIRCLE) then spin = not spin end
  end

  local lx, ly = axis(pad.lx, 2048.0), axis(pad.ly, 2048.0)
  if lx ~= 0 or ly ~= 0 then
    lightAngle = lightAngle + lx
    lightPitch = math.max(-1.2, math.min(1.2, lightPitch - ly))
  elseif layout == DUNGEON then
    if math.abs(lightAngle) > 1.25 then sweep = lightAngle > 0 and -0.01 or 0.01 end
    lightAngle = lightAngle + sweep
  else
    lightAngle = lightAngle + 0.012
  end
  if spin then crateAngle = crateAngle + 0.006 end

  -- Direction towards the light.
  local cp = cos(lightPitch)
  Lights.set(light, Lights.DIRECTION, sin(lightAngle) * cp, sin(lightPitch), cos(lightAngle) * cp)

  if layout == DUNGEON then
    Camera.position(1.0, 2.2, 6.0)
    Camera.target(-0.6, 1.4, -2.0)
  else
    Camera.position(0.0, 0.0, 9.0)
    Camera.target(0.0, 0.0, 0.0)
  end
  Camera.update()
  Screen.setParam(Screen.DEPTH_TEST_ENABLE, true)
  Screen.setParam(Screen.DEPTH_TEST_METHOD, Screen.DEPTH_GEQUAL)

  if layout == DUNGEON then
    for _, obj in ipairs(dungeonObjects[shownMode]) do obj:render() end
  else
    local objects = layout == SIDE and sideObjects or (layout == SPHERES and sphereObjects or stressObjects)
    for _, obj in ipairs(objects) do
      obj.rotation = { x = 0.35, y = crateAngle, z = 0.0 }
      obj:render()
    end
  end

  Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)

  local prof = Screen.getProfile()
  ui.status = Screen.getFPS(360) .. " FPS"
  local lines
  if layout == SIDE or layout == SPHERES then
    lines = { "CROSS  Layout: " .. LAYOUT_NAMES[layout],
              MODE_NAMES[1] .. "  |  " .. MODE_NAMES[2] .. "  |  " .. MODE_NAMES[3] }
  else
    lines = { "CROSS  Layout: " .. LAYOUT_NAMES[layout], "SQUARE  Mode: " .. MODE_NAMES[shownMode] }
  end
  lines[#lines + 1] = fmt("frame %.1f ms  |  render %.1f  |  gpu %.1f", prof.frame, prof.render, prof.gpuWait)
  ui:hud(lines)
  ui:chrome()

  Screen.flip()

  if BENCHMARK then benchTick() end
end
