
local app = require "athenaos.app"

-- Before os.chdir("render"): the UI font and settings live in the boot folder.
local ui = app.AppUI{ title = "Skinning demo", category = "3d",
                      hints = { { "L-stick", "Orbit" }, { "L2/R2", "Zoom" }, { "cross", "Animation" } } }

Screen.setFrameCounter(true)

local canvas = Screen.getMode()
canvas.zbuffering = true
canvas.psmz = Screen.Z16S
Screen.setMode(canvas)

Render.init()
Render.setView(60.0, 1.0, 4000.0)

os.chdir("render")

local sky = Image("sky.png")
sky.width, sky.height = canvas.width, canvas.height

local skin_anims = AnimCollection("Twerk.gltf")
local gltf_skin = RenderData("Twerk.gltf")
gltf_skin.accurate_clipping = true
gltf_skin.face_culling = Render.CULL_FACE_BACK
gltf_skin.pipeline = Render.PL_DEFAULT
-- Skinned meshes animate via bone matrices in the VU: the float source can go.
gltf_skin:freeze()

local skin_object = RenderObject(gltf_skin)
skin_object.rotation = { x = math.pi / 2, y = 0.0, z = 0.0 }
skin_object:playAnim(skin_anims[5], false) -- JS skin_anims[4]: collections are 1-based in Lua

local scene = RenderData("scene.gltf")
scene.face_culling = Render.CULL_FACE_NONE
scene.pipeline = Render.PL_DEFAULT
scene:getTexture(0).filter = LINEAR
scene:freeze()
local scene_object = RenderObject(scene)

local box = RenderData("box_bump.gltf")
box.face_culling = Render.CULL_FACE_NONE
box:freeze()
local box_object = RenderObject(box)
box_object.position = { x = 2.0, y = 0.2, z = 2.0 }
box_object.scale = { x = 0.2, y = 0.2, z = 0.2 }

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

Screen.setParam(Screen.ALPHA_TEST_ENABLE, false)
Screen.setParam(Screen.ALPHA_TEST_METHOD, Screen.ALPHA_LESS)
Screen.setParam(Screen.ALPHA_TEST_REF, 0x80)
Screen.setParam(Screen.DEPTH_TEST_ENABLE, true)
Screen.setParam(Screen.DEPTH_TEST_METHOD, Screen.DEPTH_GEQUAL)

local switch_anim = false

local function axis(v) return ((v > 25 or v < -25) and v or 0) / 1024.0 end

while true do
  Screen.clear(gray)
  Camera.update()
  pad:update()

  local lx, ly = axis(pad.lx), axis(pad.ly)
  if lx ~= 0 or ly ~= 0 then Camera.orbit(lx, ly) end

  if pad:pressed(Pads.R2) then
    Camera.zoom(0.2)
  elseif pad:pressed(Pads.L2) then
    Camera.zoom(-0.2)
  end

  if ui:homeRequested(pad) then
    os.chdir("..")
    app.exitToShell()
  end

  if pad:justPressed(Pads.CROSS) then
    skin_object:playAnim(switch_anim and skin_anims.twerk or skin_anims.capoeira, true)
    switch_anim = not switch_anim
  end

  Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)
  sky:draw(0, 0)
  Screen.setParam(Screen.DEPTH_TEST_ENABLE, true)
  Screen.setParam(Screen.DEPTH_TEST_METHOD, Screen.DEPTH_GEQUAL)

  scene_object:render()
  box_object:render()
  skin_object:render()

  Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)

  ui.status = Screen.getFPS(360) .. " FPS"
  ui:hud({ string.format("%d vertices  |  %s  |  VRAM %d KB", gltf_skin.size, free_mem, free_vram // 1024) })
  ui:chrome()

  Screen.flip()
end
