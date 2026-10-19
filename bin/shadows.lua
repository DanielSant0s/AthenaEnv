-- {"name": "Shadows demo", "author": "Daniel Santos", "version": "10182026", "file": "shadows.lua", "category": "3d", "blurb": "Real-time projected shadows on a skinned character.", "exit": "TRIANGLE"}

local app = require "athenaos.app"

-- Before os.chdir("render"): the UI font and settings live in the boot folder.
local ui = app.AppUI{ title = "Shadows demo", category = "3d",
  hints = { { "L-stick", "Orbit" }, { "L2/R2", "Zoom" }, { "cross", "Animation" }, { "square", "Cull" }, { "circle", "Clip" } } }

Screen.setFrameCounter(true)

local canvas = Screen.getMode()
canvas.zbuffering = true
canvas.psmz = Screen.Z16S
Screen.setMode(canvas)

Screen.initBuffers()
local depth_buffer = Screen.getBuffer(Screen.DEPTH_BUFFER)

Render.init()
Render.setView(60.0, 1.0, 4000.0)

-- Shadow render target (offscreen)
local shadowRT = Image()
shadowRT.filter = LINEAR
shadowRT.renderable = true
shadowRT.bpp = 32
shadowRT.texWidth, shadowRT.texHeight = 256, 256
shadowRT.width, shadowRT.height = 256, 256
shadowRT.endx, shadowRT.endy = 256, 256
shadowRT:lock()

os.chdir("render")

local sky = Image("sky.png")
sky.width, sky.height = canvas.width, canvas.height

local skin_anims = AnimCollection("Twerk.gltf")
local gltf_skin = RenderData("Twerk.gltf")
gltf_skin.accurate_clipping = true
gltf_skin.face_culling = Render.CULL_FACE_BACK
gltf_skin.pipeline = Render.PL_DEFAULT
gltf_skin:freeze()

local skin_object = RenderObject(gltf_skin)
skin_object.rotation = { x = math.pi / 2, y = 0.0, z = 0.0 }
skin_object:playAnim(skin_anims[5], false)

local world = Physics.World()
local space = Physics.Space()
local jgroup = Physics.JointGroup()

-- Shadow projector for the skinned character
local projSkin = Shadows.Projector(shadowRT)
projSkin:setSize(2.0, 2.0)
projSkin:setGrid(12, 12)
local lightDir = { x = 0.0, y = 1.0, z = 1.0 }
projSkin:setLightDir(lightDir.x, lightDir.y, lightDir.z)
projSkin:setBias(-0.02)
projSkin:setColor(0.0, 0.0, 0.0, 0.65)
projSkin:setBlend(Shadows.SHADOW_BLEND_DARKEN)
projSkin:enableRaycast(space, true, 12.0)
projSkin:setLightOffset(1.0)
projSkin:setUVRect(1.0, 0.0, 0.0, 1.0)

Screen.switchContext()
Screen.setBuffer(Screen.DRAW_BUFFER, shadowRT)
Screen.setBuffer(Screen.DEPTH_BUFFER, depth_buffer, 1) -- context 2 does not need Z
Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)
Screen.switchContext()

local scene = RenderData("scene.gltf")
scene.face_culling = Render.CULL_FACE_BACK
scene.accurate_clipping = true
scene.pipeline = Render.PL_DEFAULT
scene:getTexture(0).filter = LINEAR

local scene_object = RenderObject(scene)
-- Not frozen: the trimesh collision is built from the scene positions.
Physics.GeomRenderObject(space, scene_object)

local box = RenderData("box_bump.gltf")
box.face_culling = Render.CULL_FACE_BACK
box:freeze()

local box_object = RenderObject(box)
box_object.position = { x = 2.0, y = 0.2, z = 2.0 }
box_object.scale = { x = 0.2, y = 0.2, z = 0.2 }

-- Tristrip + face culling test (see shadows.js): solid or gone is right,
-- striped means the strip parity is wrong.
local RIBBON_QUADS = 8
local ribbon_positions, ribbon_colors = {}, {}
for i = 0, RIBBON_QUADS do
  local x = -4.0 + (8.0 * i) / RIBBON_QUADS
  local warm = i % 2 == 0
  local r, b = warm and 0.95 or 0.15, warm and 0.15 or 0.95
  -- top vertex before bottom: first triangle is counter-clockwise towards +Z
  for _, v in ipairs({ x, 4.0, -3.0, 1.0, x, 1.0, -3.0, 1.0 }) do ribbon_positions[#ribbon_positions + 1] = v end
  for _ = 1, 2 do
    for _, v in ipairs({ r, 0.35, b, 1.0 }) do ribbon_colors[#ribbon_colors + 1] = v end
  end
end

local ribbon_list = Render.vertexList(ribbon_positions, nil, nil, ribbon_colors)
local ribbon = RenderData(ribbon_list, nil, true) -- third argument marks a strip
ribbon.pipeline = Render.PL_NO_LIGHTS
ribbon.texture_mapping = false
ribbon.accurate_clipping = false
ribbon.face_culling = Render.CULL_FACE_BACK

local ribbon_object = RenderObject(ribbon)
local ribbon_culling, ribbon_clipping = true, false

Camera.position(0.0, 0.0, 20.0)
Camera.target(0.0, 1.0, 0.0)
Camera.orbit(0.0, 0.5)

local light = Lights.new()
Lights.set(light, Lights.DIRECTION, 0.0, 1.0, 1.0)
Lights.set(light, Lights.AMBIENT,   0.12, 0.15, 0.2)
Lights.set(light, Lights.DIFFUSE,   0.5, 0.5, 0.5)
Lights.set(light, Lights.SPECULAR,  1.0, 1.0, 0.0)

local pad = Pads.get()
local ee_info = System.getCPUInfo()
local free_mem = string.format("RAM: %dMB / %dMB", System.getMemoryStats().used // 1048576, ee_info.RAMSize // 1048576)
local free_vram = Screen.getMemoryStats()
local gray = Color.new(180, 180, 220, 128)
local clear = Color.new(0, 0, 0, 0)

Screen.setParam(Screen.ALPHA_TEST_ENABLE, false)
Screen.setParam(Screen.ALPHA_TEST_METHOD, Screen.ALPHA_LESS)
Screen.setParam(Screen.ALPHA_TEST_REF, 0x80)
Screen.setParam(Screen.DEPTH_TEST_ENABLE, true)
Screen.setParam(Screen.DEPTH_TEST_METHOD, Screen.DEPTH_GEQUAL)

local switch_anim = false

local function axis(v) return ((v > 25 or v < -25) and v or 0) / 1024.0 end

-- Renders `obj` (unlit, untextured) into shadowRT from a light-aligned camera.
local function renderSilhouette(obj, data, target, mainCam)
  Screen.switchContext()
  Draw.rect(0, 0, shadowRT.width, shadowRT.height, clear)
  Render.setView(60.0, 1.0, 4000.0, shadowRT.width, shadowRT.height)
  local len = math.sqrt(lightDir.x ^ 2 + lightDir.y ^ 2 + lightDir.z ^ 2)
  local dx, dy, dz = lightDir.x / len, lightDir.y / len, lightDir.z / len
  local dist = 1.0
  Camera.position(target.x - dx * dist, target.y - dy * dist, target.z - dz * dist)
  Camera.target(target.x, target.y, target.z)
  Camera.update()
  data.texture_mapping = false
  data.shade_model = 0
  data.pipeline = Render.PL_NO_LIGHTS
  obj:render()
  Camera.restore(mainCam)
  Camera.update()
  Screen.switchContext()
  Render.setView(60.0, 1.0, 4000.0)
end

local function restoreLit(data)
  data.texture_mapping = true
  data.shade_model = 1
  data.pipeline = Render.PL_DEFAULT
end

while true do
  Screen.clear(gray)
  Camera.update()
  pad:update()

  local lx, ly = axis(pad.lx), axis(pad.ly)
  if lx ~= 0 or ly ~= 0 then Camera.orbit(lx, ly) end

  if pad:pressed(Pads.R2) then Camera.zoom(0.2) elseif pad:pressed(Pads.L2) then Camera.zoom(-0.2) end

  if ui:homeRequested(pad) then
    os.chdir("..")
    app.exitToShell()
  end

  if pad:justPressed(Pads.CROSS) then
    skin_object:playAnim(switch_anim and skin_anims.twerk or skin_anims.capoeira, true)
    switch_anim = not switch_anim
  end
  if pad:justPressed(Pads.SQUARE) then
    ribbon_culling = not ribbon_culling
    ribbon.face_culling = ribbon_culling and Render.CULL_FACE_BACK or Render.CULL_FACE_NONE
  end
  if pad:justPressed(Pads.CIRCLE) then
    ribbon_clipping = not ribbon_clipping
    ribbon.accurate_clipping = ribbon_clipping
  end

  Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)
  sky:draw(0, 0)

  local mainCam = Camera.save()
  local sp = skin_object.position
  renderSilhouette(skin_object, gltf_skin, { x = sp.x, y = sp.y, z = sp.z + 0.1 }, mainCam)

  Screen.setParam(Screen.DEPTH_TEST_ENABLE, true)
  Screen.setParam(Screen.DEPTH_TEST_METHOD, Screen.DEPTH_GEQUAL)

  scene_object:render()
  ribbon_object:render()

  -- Projector follows the character on XZ, projected on the ground (y = 0)
  projSkin.position = { x = sp.x, y = 0.0, z = sp.z }
  projSkin:setLightOffset(1.0)
  projSkin:render()
  world:stepWithContacts(space, jgroup, 0.01)

  restoreLit(gltf_skin)
  skin_object:render()

  local bp = box_object.position
  box_object.scale = { x = 0.23, y = 0.23, z = 0.23 }
  renderSilhouette(box_object, box, bp, mainCam)

  projSkin.position = { x = bp.x, y = 0.0, z = bp.z }
  projSkin:setLightOffset(1.2)
  projSkin:render()
  world:stepWithContacts(space, jgroup, 0.01)

  restoreLit(box)
  box_object.scale = { x = 0.2, y = 0.2, z = 0.2 }
  box_object:render()

  Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)

  ui.status = Screen.getFPS(360) .. " FPS"
  ui:hud({
    string.format("%d vertices  |  %s  |  VRAM %d KB", gltf_skin.size, free_mem, free_vram // 1024),
    string.format("Tristrip ribbon: %d quads  |  cull %s  |  clip %s", RIBBON_QUADS,
      ribbon_culling and "BACK" or "NONE", ribbon_clipping and "ACCURATE" or "FAST"),
    "Ribbon solid or gone = ok, striped = wrong parity",
  })
  ui:chrome()

  Screen.flip()
end
