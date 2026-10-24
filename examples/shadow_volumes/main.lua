
-- Stencil shadow volumes: each crate's silhouette, pushed away from the light,
-- counted into the red channel (+1 front faces, -1 back faces). Unlike the
-- projected shadows, a shadow lands on anything inside the volume -- the floor,
-- another crate, the crate itself. See shadow_volumes.js.

local app = require "athenaos.app"

local ui = app.AppUI{ title = "Shadow volumes", category = "3d",
  hints = { { "L-stick", "Light" }, { "cross", "Shadows" }, { "circle", "Spin" } } }

local fmt = string.format
local sin, cos = math.sin, math.cos

Screen.setFrameCounter(true)
Screen.setVSync(true)

local canvas = Screen.getMode()
canvas.zbuffering = true
canvas.psm = Screen.CT32
canvas.psmz = Screen.Z24 -- the scene's red waits in the top byte of Z24
Screen.setMode(canvas)
Screen.initBuffers()

ui:loading("Loading assets...", "Crates")

Render.init()
Render.setView(60.0, 1.0, 400.0)

os.chdir("render")

-- Floor: two triangles, facing up.
local FLOOR, FLOOR_Y = 9.0, -1.0
local up = { 0, 1, 0, 1 }
local grey = { 0.55, 0.55, 0.6, 1 }
local floorNormals, floorColors, floorUVs = {}, {}, {}
for i = 1, 6 do
  for k = 1, 4 do
    floorNormals[#floorNormals + 1] = up[k]
    floorColors[#floorColors + 1] = grey[k]
    floorUVs[#floorUVs + 1] = 0
  end
end
local floorData = RenderData(Render.vertexList(
  { -FLOOR, FLOOR_Y, -FLOOR, 1,  -FLOOR, FLOOR_Y, FLOOR, 1,  FLOOR, FLOOR_Y, FLOOR, 1,
    -FLOOR, FLOOR_Y, -FLOOR, 1,   FLOOR, FLOOR_Y, FLOOR, 1,  FLOOR, FLOOR_Y, -FLOOR, 1 },
  floorNormals, floorUVs, floorColors))
floorData.texture_mapping = false
floorData.face_culling = Render.CULL_FACE_NONE
local floorObject = RenderObject(floorData)

-- Not frozen: a volume reads the crate's positions once, to find its edges.
local crate = RenderData("box_bump.gltf")
local CRATES = {
  { x = -2.6, y = 0.0, z = 0.8, ry = 0.3 },
  { x = 2.4, y = 0.0, z = -1.2, ry = -0.5 },
  { x = 0.0, y = 0.0, z = -0.2, ry = 0.0 },
  { x = 0.3, y = 2.7, z = 0.4, ry = 0.7 }, -- floats over the middle one
}
local crates, volumes = {}, {}
for i, c in ipairs(CRATES) do
  local obj = RenderObject(crate)
  obj.position = { x = c.x, y = c.y, z = c.z }
  obj.rotation = { x = 0.0, y = c.ry, z = 0.0 }
  crates[i] = obj
  volumes[i] = Shadows.Volume(obj)
  volumes[i].extrusion = 30.0
end

Camera.position(0.0, 6.0, 12.0)
Camera.target(0.0, 0.0, 0.0)

local light = Lights.new()
Lights.set(light, Lights.AMBIENT, 0.25, 0.25, 0.28)
Lights.set(light, Lights.DIFFUSE, 0.85, 0.82, 0.75)

local pad = Pads.get()
local sky = Color.new(52, 58, 70, 128)

local shadowsOn, spin = true, true
local lightAngle, lightPitch, floatAngle = 0.8, 0.9, 0.7

local function axis(v, scale)
  return (v > 25 or v < -25) and v / scale or 0
end

while true do
  Screen.clear(sky)
  Render.begin()
  pad:update()

  if ui:homeRequested(pad) then
    os.chdir("..")
    app.exitToShell()
  end
  if pad:justPressed(Pads.CROSS) then shadowsOn = not shadowsOn end
  if pad:justPressed(Pads.CIRCLE) then spin = not spin end

  local lx, ly = axis(pad.lx, 2048.0), axis(pad.ly, 2048.0)
  if lx ~= 0 or ly ~= 0 then
    lightAngle = lightAngle + lx
    lightPitch = math.max(0.25, math.min(1.45, lightPitch - ly))
  else
    lightAngle = lightAngle + 0.01
  end
  if spin then floatAngle = floatAngle + 0.012 end
  crates[4].rotation = { x = 0.4, y = floatAngle, z = 0.2 }

  -- Direction towards the light.
  local cp = cos(lightPitch)
  Lights.set(light, Lights.DIRECTION, sin(lightAngle) * cp, sin(lightPitch), cos(lightAngle) * cp)

  Camera.update()
  Screen.setParam(Screen.DEPTH_TEST_ENABLE, true)
  Screen.setParam(Screen.DEPTH_TEST_METHOD, Screen.DEPTH_GEQUAL)

  floorObject:render()
  for _, obj in ipairs(crates) do obj:render() end

  if shadowsOn and Shadows.beginVolumes() then
    for _, volume in ipairs(volumes) do volume:render() end
    Shadows.endVolumes(0.55)
  end

  Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)

  local prof = Screen.getProfile()
  ui.status = Screen.getFPS(360) .. " FPS"
  ui:hud({
    fmt("CROSS  Shadows: %s  (%d volumes, %d edges each)", shadowsOn and "ON" or "OFF", #volumes, volumes[1].edges),
    fmt("frame %.1f ms  |  render %.1f  |  gpu %.1f", prof.frame, prof.render, prof.gpuWait),
  })
  ui:chrome()

  Screen.flip()
end
