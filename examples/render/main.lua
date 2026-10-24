
local app = require "athenaos.app"

-- Before os.chdir("render"): the UI font and settings live in the boot folder.
local ui = app.AppUI{ title = "Render demo", category = "3d",
  hints = { { "L-stick", "Rotate" }, { "D-pad", "Model/Pipeline" }, { "cross", "Clip" }, { "square", "Shade" }, { "circle", "Texture" }, { "R1", "BBox" } } }

local pipelines = { [0] = "NO_LIGHTS", "DEFAULT", "SPECULAR" }

Screen.setFrameCounter(true)
Screen.setVSync(true)

local canvas = Screen.getMode()
canvas.zbuffering = true
canvas.psm = Screen.CT32
canvas.psmz = Screen.Z24
Screen.setMode(canvas)
Screen.initBuffers()

-- Offscreen decal target painted with the right stick and mapped on meshes.
local decal_buffer = Image()
decal_buffer.filter = LINEAR
decal_buffer.bpp = 32
decal_buffer.texWidth, decal_buffer.texHeight = 128, 128
decal_buffer.renderable = true
decal_buffer.width, decal_buffer.height = 128, 128
decal_buffer.endx, decal_buffer.endy = 128, 128
decal_buffer:lock()

Screen.switchContext()
Screen.setBuffer(Screen.DRAW_BUFFER, decal_buffer)
Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)
Screen.switchContext()

ui:loading("Loading assets...", "Meshes, textures and materials")

Render.init()
Render.setView(60.0, 5.0, 4000.0)

os.chdir("render")

-- Script-built geometry: x, y, z, w per vertex in plain Lua tables.
local vertList = Render.vertexList(
  { 0.5, 0.5, 0.5, 1.0,  0.5, 0.8, 0.5, 1.0,  0.8, 0.8, 0.5, 1.0 },
  { 1.0, 1.0, 1.0, 1.0,  1.0, 1.0, 1.0, 1.0,  1.0, 1.0, 1.0, 1.0 },
  { 0.0, 0.0, 1.0, 1.0,  1.0, 0.0, 1.0, 1.0,  1.0, 1.0, 1.0, 1.0 },
  { 1.0, 0.0, 0.0, 1.0,  0.0, 1.0, 0.0, 1.0,  0.0, 0.0, 1.0, 1.0 })

local listtest = RenderData(vertList)
listtest.face_culling = Render.CULL_FACE_NONE
local trilist_materials = listtest.materials
trilist_materials[1].diffuse = { r = 0.0, g = 0.0, b = 0.0 }
listtest.materials = trilist_materials
listtest:updateMaterial(0, { shininess = 64.0, diffuse = { r = 0.7, g = 0.2, b = 0.2 } })

local skytex = Image("env.png")
skytex.filter = LINEAR

local gltf_box = RenderData("box_bump.gltf")
gltf_box:freeze()

local dragonmesh = RenderData("dragon.obj", Image("dragon.png"))
local blood_tex = Image("blood.png")
blood_tex.width, blood_tex.height = 40, 40

local function withDecal(rd)
  local mats = rd.materials
  mats[1].decal_texture_id = rd:pushTexture(decal_buffer)
  rd.materials = mats
  rd:freeze()
end
withDecal(dragonmesh)

local monkeymesh = RenderData("monkey.obj", Image("monkey.png"))
withDecal(monkeymesh)

local moontex = Image("moon.png")

local car = RenderData("Car.obj")
local envmap_tex_id = car:pushTexture(skytex)
local car_materials = car.materials
for _, mat in ipairs(car_materials) do
  mat.ref_texture_id = envmap_tex_id
  if mat.diffuse.r > 0.4 and mat.diffuse.g == 0.0 and mat.diffuse.b == 0.0 then
    mat.diffuse.r, mat.diffuse.b = 0.25, 0.5
  end
end
car.materials = car_materials
-- car stays unfrozen: its .vertices can still be edited live.

local mill = RenderData("cubes.obj")
mill:setTexture(1, moontex)
mill:freeze()

local boombox = RenderData("Boombox.obj")
boombox:getTexture(0).filter = LINEAR
boombox:freeze()

local render_data = { dragonmesh, gltf_box, monkeymesh, car, listtest, boombox, mill }

local sceneRoot = SceneNode()
local sceneBatch = Batch({ autoSort = true })
local objectNodes = {}
for i, rd in ipairs(render_data) do
  local obj = RenderObject(rd)
  local node = SceneNode()
  node:attach(obj)
  sceneRoot:addChild(node)
  sceneBatch:add(obj)
  objectNodes[i] = node
end

local asyncLoader = AsyncLoader({ jobsPerStep = 2 })
local streamed = 0

print("[Render] enqueue BoxTextured.gltf")
asyncLoader:enqueue("BoxTextured.gltf", function(path, renderData)
  print("[Render] loaded:", path, "size:", renderData.size)
  renderData:freeze()
  local obj = RenderObject(renderData)
  streamed = streamed + 1
  local node = SceneNode()
  node.position = { x = 0.0, y = 6.0 + streamed * 2.5, z = 0.0 }
  node:attach(obj)
  sceneRoot:addChild(node)
  sceneBatch:add(obj)
end)

Camera.position(0.0, 0.0, 35.0)

local light = Lights.new()
Lights.set(light, Lights.DIRECTION, 0.0, 0.5, 1.0)
Lights.set(light, Lights.AMBIENT, 0.12, 0.15, 0.2)
Lights.set(light, Lights.DIFFUSE, 0.5, 0.5, 0.5)
Lights.set(light, Lights.SPECULAR, 1.0, 1.0, 1.0)

local pad = Pads.get()
local model = 1
local savedlx, savedly, savedrz = 0.0, 0.0, 0.0
local decal_x, decal_y = 32, 0

local ee_info = System.getCPUInfo()
local free_mem = string.format("RAM: %dMB / %dMB", System.getMemoryStats().used // 1048576, ee_info.RAMSize // 1048576)

local gray = Color.new(40, 40, 40, 128)
local bbox = false

local function axis(v, scale) return ((v > 25 or v < -25) and v or 0) / scale end

while true do
  Screen.clear(gray)
  Render.begin()
  Camera.update()
  pad:update()
  asyncLoader:process(2)

  local lx, ly = axis(pad.lx, 4096.0), axis(pad.ly, 4096.0)
  savedlx, savedly = savedlx - lx, savedly - ly

  local rx, ry = axis(pad.rx, 2048.0), axis(pad.ry, 2048.0)

  if pad:pressed(Pads.R2) then
    savedrz = savedrz - 0.05
  elseif pad:pressed(Pads.L2) then
    savedrz = savedrz + 0.05
  end

  if rx ~= 0 or ry ~= 0 then
    decal_x = decal_x + rx * 32.0
    decal_y = decal_y + ry * 32.0
  end

  Camera.target(0, 0, savedrz)

  local rd = render_data[model]
  if pad:justPressed(Pads.LEFT) and model > 1 then model = model - 1 end
  if pad:justPressed(Pads.RIGHT) and model < #render_data then model = model + 1 end
  rd = render_data[model]
  if pad:justPressed(Pads.UP) and rd.pipeline > 0 then rd.pipeline = rd.pipeline - 1 end
  if pad:justPressed(Pads.DOWN) and rd.pipeline < 2 then rd.pipeline = rd.pipeline + 1 end

  if ui:homeRequested(pad) then
    asyncLoader:clear()
    asyncLoader:destroy()
    os.chdir("..")
    app.exitToShell()
  end

  if pad:justPressed(Pads.R1) then bbox = not bbox end
  if pad:justPressed(Pads.SQUARE) then rd.shade_model = rd.shade_model ~ 1 end
  if pad:justPressed(Pads.CIRCLE) then rd.texture_mapping = not rd.texture_mapping end
  if pad:justPressed(Pads.CROSS) then rd.accurate_clipping = not rd.accurate_clipping end

  if lx ~= 0 or ly ~= 0 then
    objectNodes[model].rotation = { x = savedly, y = savedlx, z = 0.0 }
  end

  Screen.switchContext()
  Draw.rect(0, 0, 128, 128, Color.new(0, 0, 0, 0))
  blood_tex:draw(decal_x, decal_y)
  Screen.switchContext()

  decal_buffer:draw(0, 100)

  Screen.setParam(Screen.DEPTH_TEST_ENABLE, true)
  Screen.setParam(Screen.DEPTH_TEST_METHOD, Screen.DEPTH_GEQUAL)

  sceneRoot:update()
  sceneBatch:render()

  Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)

  local renderStats = Render.stats()
  ui.status = Screen.getFPS(360) .. " FPS"
  ui:hud({
    string.format("Model %d/%d  -  %d vertices", model, #render_data, rd.size),
    string.format("Pipeline: %s  |  Draws: %d  |  Tris: %d", pipelines[rd.pipeline], renderStats.drawCalls, renderStats.triangles),
    string.format("%s  |  VRAM static %d KB  dynamic %d KB", free_mem,
      Screen.getMemoryStats(Screen.VRAM_USED_STATIC) // 1024, Screen.getMemoryStats(Screen.VRAM_USED_DYNAMIC) // 1024),
    string.format("Batch: %d objects  |  Loader queue: %d  |  Streamed: %d", sceneBatch.size, asyncLoader:size(), streamed),
  })
  ui:chrome()

  Screen.flip()
end
