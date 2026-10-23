-- {"name": "Physics demo", "author": "Daniel Santos", "version": "10182026", "file": "physics_collision.lua", "category": "3d", "blurb": "Rigid bodies, BVH trimesh, joints and shadows in a courtyard.", "exit": "TRIANGLE"}
-- Lua port of physics_collision.js; also the JS-vs-Lua benchmark (BENCHMARK below).

local app = require "athenaos.app"

local sin, cos, sqrt, abs, atan, floor, min, max, pi =
  math.sin, math.cos, math.sqrt, math.abs, math.atan, math.floor, math.min, math.max, math.pi
local fmt = string.format

-- Before os.chdir("render"): the UI font and settings live in the boot folder.
local ui = app.AppUI{ title = "Physics demo", category = "3d",
  hints = { { "L-stick", "Walk" }, { "R1", "Run" }, { "cross", "Jump" }, { "square", "Kick" }, { "circle", "Throw" }, { "L1", "Dance" } } }

Screen.setFrameCounter(true)

local canvas = Screen.getMode()
canvas.psm = Screen.CT32
canvas.zbuffering = true
canvas.psmz = Screen.Z24
Screen.setMode(canvas)

Screen.initBuffers()
local fbuf = Screen.getBuffer(Screen.DRAW_BUFFER)
local zbuf = Screen.getBuffer(Screen.DEPTH_BUFFER)

Render.init()
Render.setView(60.0, 2.0, 250.0)

-- Change root folder to "render" for asset loading
os.chdir("render")

local sky = Image("sky.png")
sky.width = canvas.width
sky.height = canvas.height

-- Native particles: one shared atlas/pool for dust, wood chips and stone grit.
-- Emit only on footsteps, landings and impacts; R3 changes the density budget.
local FX_LEVELS = { [0] = 0, 128, 256, 512, 1024 }
local FX_LEVEL_COUNT = 5
local fxAtlas = Image("fx_atlas.png")
fxAtlas.filter = LINEAR
local fxSystem = Particles.System{ texture = fxAtlas, capacity = 1024, space = "world", seed = 42978 }
local fxDust = fxSystem:createEmitter{
  frame = { 0, 0, 32, 32 }, lifetime = { 0.45, 0.9 }, size = { 0.14, 0.28 }, scaleEnd = 3,
  velocityMin = { -0.4, 0.15, -0.4 }, velocityMax = { 0.4, 0.65, 0.4 },
  acceleration = { 0, -0.12, 0 }, drag = 1.8, spread = { 0.08, 0.02, 0.08 },
  color = { 128, 118, 100, 45 }, colorEnd = { 128, 118, 100, 0 }, fadeIn = 0.08,
}
local fxWood = fxSystem:createEmitter{
  frame = { 32, 0, 32, 32 }, lifetime = { 0.35, 0.8 }, size = { 0.025, 0.065 }, scaleEnd = 0.65,
  velocityMin = { -1.8, 1, -1.8 }, velocityMax = { 1.8, 3, 1.8 }, acceleration = { 0, -9, 0 },
  color = { 128, 90, 55, 128 }, colorEnd = { 128, 90, 55, 0 },
}
local fxStone = fxSystem:createEmitter{
  frame = { 32, 0, 32, 32 }, lifetime = { 0.3, 0.65 }, size = { 0.02, 0.055 }, scaleEnd = 0.65,
  velocityMin = { -1.5, 0.6, -1.5 }, velocityMax = { 1.5, 2, 1.5 }, acceleration = { 0, -9, 0 },
  color = { 120, 118, 112, 128 }, colorEnd = { 120, 118, 112, 0 },
}
local fxLevel = 1
local fxNextStep = 0
local function fxReset(level)
  fxLevel = level
  fxNextStep = 0
  fxSystem:clear(42978)
end
-- material: 0 = dust, 1 = wood fragments + dust, 2 = stone grit + dust.
local function fxBurst(x, y, z, count, material)
  material = material or 0
  local budget = FX_LEVELS[fxLevel]
  count = min(budget - fxSystem.activeCount, math.ceil(count * budget / 128))
  if count <= 0 then return end
  local dust = material == 0 and count or math.ceil(count / 3)
  fxDust:burst(dust, x, y, z)
  if count > dust then (material == 1 and fxWood or fxStone):burst(count - dust, x, y, z) end
end
local function fxRender()
  -- Same fixed simulation step as the rigid-body world below.
  fxSystem:update(0.016)
  fxSystem:draw()
end

-- Physics World & Broadphase Space -------------------------------------------
local world = Physics.World()
local jgroup = Physics.JointGroup()
local space = Physics.Space()
-- Static receivers only: a caster must not raycast onto its own collider.
local shadowSpace = Physics.Space()

world:setGravity(0.0, -18.0, 0.0)

-- Static Environment (Sponza Palace Atrium + Walkable North Terrace) ----------
local scene = RenderData("sponza_strip.gltf")
scene.face_culling = Render.CULL_FACE_BACK
scene.accurate_clipping = true
scene.pipeline = Render.PL_DEFAULT
for i = 0, 7 do
  local tex = scene:getTexture(i)
  if tex then tex.filter = LINEAR end
end

local scene_object = RenderObject(scene)

-- Watertight scene collision from native primitives. Geoms created in a space
-- are kept alive by that space, so no extra bookkeeping is needed in Lua.
local function addStaticBox(x0, y0, z0, x1, y1, z1)
  local g = Physics.GeomBox(space, x1 - x0, y1 - y0, z1 - z0)
  g:setPosition(0.5 * (x0 + x1), 0.5 * (y0 + y1), 0.5 * (z0 + z1))
  local receiver = Physics.GeomBox(shadowSpace, x1 - x0, y1 - y0, z1 - z0)
  receiver:setPosition(0.5 * (x0 + x1), 0.5 * (y0 + y1), 0.5 * (z0 + z1))
  return g
end

-- 1. Courtyard floor slab. Top face is Y = 0, matching the visual floor.
addStaticBox(-8.53, -0.80, -19.10, 8.53, 0.0, 19.10)

-- 2. Outer boundary walls
addStaticBox(-9.53, 0.0, -20.0, -8.53, 12.0,  20.0)
addStaticBox( 8.53, 0.0, -20.0,  9.53, 12.0,  20.0)
addStaticBox(-9.53, 0.0, -20.1,  9.53, 12.0, -19.1)
addStaticBox(-9.53, 0.0,  19.1,  9.53, 12.0,  20.1)

-- 3. Colonnade columns + corner piers
for _, cz in ipairs({ -7.775, -2.534, 2.610, 7.775 }) do
  addStaticBox(-3.735, 0.0, cz - 0.36, -3.015, 3.20, cz + 0.36)
  addStaticBox( 3.043, 0.0, cz - 0.36,  3.765, 3.20, cz + 0.36)
end
for _, pz in ipairs({ -13.139, 13.114 }) do
  addStaticBox(-3.763, 0.0, pz - 0.57, -2.618, 3.20, pz + 0.57)
  addStaticBox( 2.619, 0.0, pz - 0.57,  3.764, 3.20, pz + 0.57)
end

-- 4. Stone planters
for _, p in ipairs({ { -2.62, -12.58 }, { 2.68, -12.58 }, { -2.64, -2.52 }, { 2.63, -2.52 },
                     { -2.65, 2.58 }, { 2.63, 2.58 }, { -2.65, 12.58 }, { 2.63, 12.58 } }) do
  addStaticBox(p[1] - 0.42, 0.0, p[2] - 0.42, p[1] + 0.42, 0.92, p[2] + 0.42)
end

-- 5. North terrace: steps, platforms and trophy pedestal
for s = 0, 4 do
  local zFront = -4.20 - s * (3.60 / 5.0)
  local yTop = (s + 1) * (1.05 / 5.0)
  addStaticBox(-1.60, 0.0, -7.80, 1.60, yTop, zFront)
end
addStaticBox(-3.10, 0.0, -10.20, 3.10, 1.05, -7.80)
for s = 0, 3 do
  local zFront = -9.60 - s * (2.50 / 4.0)
  local yTop = 1.05 + (s + 1) * (0.90 / 4.0)
  addStaticBox(-2.95, 0.0, -12.10, -1.65, yTop, zFront)
  addStaticBox( 1.65, 0.0, -12.10,  2.95, yTop, zFront)
end
addStaticBox(-3.10, 0.0, -13.60, 3.10, 1.95, -12.10)
addStaticBox(-1.15, 1.95, -13.55, 1.15, 2.55, -12.25)

-- Player character (3rd-person walking simulator) ------------------------------
local skin_anims = AnimCollection("Twerk.gltf")
local gltf_skin = RenderData("Twerk.gltf")
gltf_skin.accurate_clipping = true
gltf_skin.face_culling = Render.CULL_FACE_BACK
gltf_skin.pipeline = Render.PL_SPECULAR

local skin_object = RenderObject(gltf_skin)

local PLAYER_RADIUS = 0.34
local PLAYER_START = { x = 0.0, y = 1.0, z = 6.5 }

local player_geom = Physics.GeomSphere(space, PLAYER_RADIUS)
local player_body = Physics.Body(world)
player_body:setMassSphere(2.5, PLAYER_RADIUS)
player_geom:setBody(player_body)
player_body:setPosition(PLAYER_START.x, PLAYER_START.y, PLAYER_START.z)

local playerYaw = pi -- facing -Z toward the crate pyramid & north terrace
local walkCycle = 0.0
local currentAnim = "idle"
local emotes = { "samba", "gangnam", "hiphop", "twerk", "capoeira" }
local emoteIdx = 0
local emoteActive = false
local RIGHT_FOOT_BONE = 22 + 1 -- mixamorig:RightFoot (bones table is 1-based)
local RIGHT_TOE_BONE  = 23 + 1 -- mixamorig:RightToeBase
local animTimer = 0
local jumpWindup = 0
local jumpCooldown = 0
local groundGrace = 6
local kickPhase = 0
local kickHits = 0
local kickCooldown = 0
local kickHitBodies = {}
local statusText = "SPONZA ATRIUM READY"
local statusTimer = 120

skin_object.position = { x = PLAYER_START.x, y = PLAYER_START.y - PLAYER_RADIUS, z = PLAYER_START.z }
skin_object.rotation = { x = pi / 2, y = 0.0, z = playerYaw }
skin_object:playAnim(skin_anims.idle or skin_anims.twerk, true)

-- Dynamic crates -------------------------------------------------------------
local box = RenderData("box_bump.gltf")
box.face_culling = Render.CULL_FACE_BACK
box.pipeline = Render.PL_SPECULAR

local CRATE_HALF = 0.38
local CRATE_SIZE = CRATE_HALF * 2.0
local crateSpawns = {
  { x = -0.82, y = CRATE_HALF + 0.02, z = 0.5 },
  { x =  0.00, y = CRATE_HALF + 0.02, z = 0.5 },
  { x =  0.82, y = CRATE_HALF + 0.02, z = 0.5 },
  { x = -0.41, y = CRATE_HALF * 3.0 + 0.05, z = 0.5 },
  { x =  0.41, y = CRATE_HALF * 3.0 + 0.05, z = 0.5 },
  { x =  0.00, y = CRATE_HALF * 5.0 + 0.08, z = 0.5 },
  { x = -2.25, y = 1.05 + CRATE_HALF + 0.03, z = -8.5 },
  { x =  2.25, y = 1.05 + CRATE_HALF + 0.03, z = -8.5 },
  { x = -2.10, y = 1.95 + CRATE_HALF + 0.03, z = -12.6 },
  { x =  2.10, y = 1.95 + CRATE_HALF + 0.03, z = -12.6 },
  { x =  0.00, y = 2.55 + CRATE_HALF + 0.03, z = -12.95 },
}

local crates = {}
for _, sp in ipairs(crateSpawns) do
  local obj = RenderObject(box)
  obj.scale = { x = CRATE_HALF, y = CRATE_HALF, z = CRATE_HALF }
  obj.position = { x = sp.x, y = sp.y, z = sp.z }

  local geom = Physics.GeomBox(space, CRATE_SIZE, CRATE_SIZE, CRATE_SIZE)
  local body = Physics.Body(world)
  body:setMassBox(0.8, CRATE_SIZE, CRATE_SIZE, CRATE_SIZE)
  geom:setBody(body)
  body:setPosition(sp.x, sp.y, sp.z)
  obj:setPhysics(body)

  crates[#crates + 1] = { obj = obj, geom = geom, body = body, spawn = sp }
end

-- Throwable monkey spheres -------------------------------------------------------
local monkeyTex = Image("monkey.png")
local monkeyData = RenderData("monkey_strip.gltf", monkeyTex)
monkeyData.face_culling = Render.CULL_FACE_BACK
monkeyData.pipeline = Render.PL_SPECULAR

local BALL_RADIUS = 0.45
local ballSpawns = {
  { x = -1.8, y = 1.5, z = 3.5 },
  { x =  1.8, y = 1.5, z = 3.5 },
  { x =  0.0, y = 2.2, z = -9.0 },
}

local balls = {}
local nextBallIdx = 1
for _, sp in ipairs(ballSpawns) do
  local obj = RenderObject(monkeyData)
  obj.scale = { x = BALL_RADIUS, y = BALL_RADIUS, z = BALL_RADIUS }
  obj.position = { x = sp.x, y = sp.y, z = sp.z }

  local geom = Physics.GeomSphere(space, BALL_RADIUS)
  local body = Physics.Body(world)
  body:setMassSphere(0.7, BALL_RADIUS)
  geom:setBody(body)
  body:setPosition(sp.x, sp.y, sp.z)
  obj:setPhysics(body)

  balls[#balls + 1] = { obj = obj, geom = geom, body = body, spawn = sp }
end

local allProps = {}
for _, c in ipairs(crates) do allProps[#allProps + 1] = c end
for _, b in ipairs(balls) do allProps[#allProps + 1] = b end
for _, p in ipairs(allProps) do
  p.fxPreviousVy = 0
  p.fxImpactCooldown = 0
end

local function fxPropImpacts()
  for i, prop in ipairs(allProps) do
    if fxLevel == 0 then
      prop.fxPreviousVy = 0
    else
      prop.fxImpactCooldown = prop.fxImpactCooldown - 0.016
      local _, vy = prop.body:getLinearVel()
      -- A falling prop abruptly loses downward speed at a real collision.
      if prop.fxPreviousVy < -2.2 and vy - prop.fxPreviousVy > 1.8 and prop.fxImpactCooldown <= 0 then
        local x, y, z = prop.body:getPosition()
        local isCrate = i <= #crates
        fxBurst(x, y - (isCrate and CRATE_HALF or BALL_RADIUS) + 0.025, z, 16, isCrate and 1 or 2)
        prop.fxImpactCooldown = 0.2
      end
      prop.fxPreviousVy = vy
    end
  end
end

local identityRot = { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 }

local function resetSandbox()
  player_body:setPosition(PLAYER_START.x, PLAYER_START.y, PLAYER_START.z)
  player_body:setLinearVel(0.0, 0.0, 0.0)
  player_body:setAngularVel(0.0, 0.0, 0.0)
  player_body:setRotation(identityRot)
  playerYaw = pi
  jumpWindup = 0
  groundGrace = 6
  kickPhase = 0
  for _, p in ipairs(allProps) do
    p.fxPreviousVy = 0
    p.fxImpactCooldown = 0
    p.body:setPosition(p.spawn.x, p.spawn.y, p.spawn.z)
    p.body:setLinearVel(0.0, 0.0, 0.0)
    p.body:setAngularVel(0.0, 0.0, 0.0)
    p.body:setRotation(identityRot)
  end
  statusText = "SANDBOX RESET!"
  statusTimer = 90
end

-- Third-person follow camera state ---------------------------------------------
local camYaw = 0.0
local camPitch = 0.34
local camDist = 5.8
local camTargetX = PLAYER_START.x
local camTargetY = PLAYER_START.y + 0.8
local camTargetZ = PLAYER_START.z

-- Atrium lighting (sunlight + warm colonnade fill) ------------------------------
local SUN_DIR = { x = 0.35, y = 0.90, z = 0.45 }
local light = Lights.new()
Lights.set(light, Lights.DIRECTION, SUN_DIR.x, SUN_DIR.y, SUN_DIR.z)
Lights.set(light, Lights.AMBIENT,   0.26, 0.27, 0.30)
Lights.set(light, Lights.DIFFUSE,   0.78, 0.74, 0.66)
Lights.set(light, Lights.SPECULAR,  1.00, 1.00, 1.00)

local fillLight = Lights.new()
Lights.set(fillLight, Lights.DIRECTION, -0.45, 0.55, -0.70)
Lights.set(fillLight, Lights.AMBIENT,    0.05, 0.04, 0.03)
Lights.set(fillLight, Lights.DIFFUSE,    0.34, 0.27, 0.20)
Lights.set(fillLight, Lights.SPECULAR,   0.70, 0.50, 0.30)

-- Dynamic silhouette shadows (L3: exact -> fast overlaps -> off) -- see physics_collision.js
local SHADOW_PROP_LIMIT = 6
local SHADOW_DISTANCE = 20
local SHADOW_RAY_LENGTH = 12
local SHADOW_CAMERA_DISTANCE = 32
local SHADOW_OPACITY = 0.42
local shadowRT = Image()
shadowRT.renderable = true
shadowRT.bpp = 32
shadowRT.texWidth, shadowRT.texHeight = 128, 128
shadowRT.width, shadowRT.height = 128, 128
shadowRT.endx, shadowRT.endy = 128, 128
shadowRT.filter = LINEAR
if not shadowRT:lock() then error("Unable to reserve the shadow render target") end
local shadowClear = Color.new(128, 128, 128, 0)
local shadowCoverageClear = Color.new(127, 127, 127, 128)
local shadowCopyBlend = Screen.alphaEquation(Screen.SRC_RGB, Screen.ZERO_RGB, Screen.ALPHA_FIX, Screen.ZERO_RGB, 128)
local shadowSilhouetteBlend = Screen.alphaEquation(Screen.ZERO_RGB, Screen.ZERO_RGB, Screen.ALPHA_FIX, Screen.DST_RGB, 0)
local shadowSceneBlend = Screen.alphaEquation(Screen.SRC_RGB, Screen.ZERO_RGB, Screen.DST_ALPHA, Screen.ZERO_RGB, 0)
local shadowDarkenBlend = Screen.alphaEquation(Screen.DST_RGB, Screen.ZERO_RGB, Screen.DST_ALPHA, Screen.ZERO_RGB, 0)
local shadowIdentityLUT = Screen.createShuffleLUT(function(v) return Color.new(v, v, v, v) end)
-- Each shadow adds 128 * SHADOW_OPACITY * coverage to red (127 base); the sum is
-- capped at full opacity, so overlaps darken once.
local shadowMaskLUT = Screen.createShuffleLUT(function(v)
  local darken = min(max(0, v - 127), 128 * SHADOW_OPACITY)
  return Color.new(0, 0, 0, floor(128 - darken + 0.5))
end)
local sunLength = sqrt(SUN_DIR.x * SUN_DIR.x + SUN_DIR.y * SUN_DIR.y + SUN_DIR.z * SUN_DIR.z)
local shadowLight = { x = SUN_DIR.x / sunLength, y = SUN_DIR.y / sunLength, z = SUN_DIR.z / sunLength }
local shadowHorizontal = sqrt(shadowLight.x * shadowLight.x + shadowLight.z * shadowLight.z)
-- 1-based: elements 13..15 are the translation the JS version writes at [12..14].
local shadowTransform = {
  shadowLight.z / shadowHorizontal, 0, -shadowLight.x / shadowHorizontal, 0,
  shadowLight.x, shadowLight.y, shadowLight.z, 0,
  shadowLight.x * shadowLight.y / shadowHorizontal, -shadowHorizontal,
  shadowLight.z * shadowLight.y / shadowHorizontal, 0,
  0, 0, 0, 1,
}
local function createShadowProjector(size, grid)
  local projector = Shadows.Projector(shadowRT)
  projector:setLightDir(shadowLight.x, shadowLight.y, shadowLight.z)
  projector:setLightOffset(0)
  projector:setBias(-0.018)
  projector:setSlopeLimit(0.45)
  -- Opacity rides in the vertex alpha: both blends below read it from the texture alpha.
  projector:setColor(1, 1, 1, SHADOW_OPACITY)
  projector:setSize(size, size)
  projector:setGrid(grid, grid) -- color is baked when geometry is built
  projector:enableRaycast(shadowSpace, true, SHADOW_RAY_LENGTH)
  return projector
end
local playerShadow = createShadowProjector(3.2, 6)
-- Per-frame shadow state, one table: the main chunk is at Lua's 200-local limit.
-- projectors: one per shadowed prop -- screenBounds() builds a projector's grid
-- and keeps it for its next render(), so each caster needs its own.
-- casters/count: casters whose shadow is on screen this frame, with their
-- screen bounds. groups/groupCount: casters whose bounds intersect, directly or
-- through another caster; only shadows that can overlap need more than one pass.
-- EXACT resolves overlaps with the coverage passes; FAST marks the darkened
-- pixels in alpha instead, where the first shadow to reach a pixel wins.
local shadowFrame = {
  projectors = {}, casters = {}, count = 0, groups = {}, groupCount = 0, overlapping = 0,
  OFF = 0, EXACT = 1, FAST = 2, MODE_NAMES = { [0] = "OFF", "ON", "ON (fast overlaps)" },
  stencilClear = Color.new(0, 0, 0, 0),
}
for i = 1, SHADOW_PROP_LIMIT do shadowFrame.projectors[i] = createShadowProjector(1.65, 4) end
for i = 1, SHADOW_PROP_LIMIT + 1 do
  shadowFrame.casters[i] = {}
  shadowFrame.groups[i] = { members = {}, count = 0, x0 = 0, y0 = 0, x1 = 0, y1 = 0 }
end
local shadowCandidates = {}
for i, prop in ipairs(allProps) do
  shadowCandidates[i] = { prop = prop, data = i <= #crates and box or monkeyData, x = 0, y = 0, z = 0, distance = 0 }
end
local function shadowDistanceOrder(a, b) return a.distance < b.distance end
local shadowView = Camera.save()
local shadowMode = shadowFrame.EXACT
local shadowDraws = 0

-- Reserve context 2 for silhouette generation; its Z writes stay masked.
Screen.switchContext()
Screen.setBuffer(Screen.DRAW_BUFFER, shadowRT)
Screen.setBuffer(Screen.DEPTH_BUFFER, zbuf, 1)
Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)
Screen.setParam(Screen.ALPHA_TEST_ENABLE, false)
Screen.setParam(Screen.SCISSOR_BOUNDS, { x1 = 0, y1 = 0, x2 = 127, y2 = 127 })
Screen.setParam(Screen.ALPHA_BLEND_EQUATION, shadowCopyBlend)
Screen.switchContext()
Screen.setBuffer(Screen.DEPTH_BUFFER, zbuf, 0)

-- Places a caster's projector under it and queues it when its shadow is on screen.
local function placeCasterShadow(obj, data, projector, size, x, y, z)
  -- Start just above the whole caster, including the grid's lower corners.
  local rayOffset = (SHADOW_RAY_LENGTH - size) * 0.5
  shadowTransform[13] = x - shadowLight.x * rayOffset
  shadowTransform[14] = y - shadowLight.y * rayOffset + shadowLight.y
  shadowTransform[15] = z - shadowLight.z * rayOffset
  projector:setTransform(shadowTransform)
  local b = projector:screenBounds()
  if not b then return end
  local f = shadowFrame
  f.count = f.count + 1
  local c = f.casters[f.count]
  c.obj, c.data, c.projector, c.size, c.x, c.y, c.z = obj, data, projector, size, x, y, z
  c.x0, c.y0, c.x1, c.y1 = b.x, b.y, b.x + b.width, b.y + b.height
end

function shadowFrame.groupCasters()
  local f = shadowFrame
  for i = 1, f.count do f.casters[i].group = 0 end
  f.groupCount = 0
  for i = 1, f.count do
    if f.casters[i].group == 0 then
      f.groupCount = f.groupCount + 1
      local group = f.groups[f.groupCount]
      f.casters[i].group = f.groupCount
      group.members[1] = f.casters[i]
      group.count = 1
      -- Members join while the loop runs, so their neighbours join too.
      local m = 1
      while m <= group.count do
        local a = group.members[m]
        for j = i + 1, f.count do
          local b = f.casters[j]
          if b.group == 0 and a.x0 < b.x1 and b.x0 < a.x1 and a.y0 < b.y1 and b.y0 < a.y1 then
            b.group = a.group
            group.count = group.count + 1
            group.members[group.count] = b
          end
        end
        m = m + 1
      end
      local first = group.members[1]
      group.x0, group.y0, group.x1, group.y1 = first.x0, first.y0, first.x1, first.y1
      for k = 2, group.count do
        local c = group.members[k]
        group.x0, group.y0 = min(group.x0, c.x0), min(group.y0, c.y0)
        group.x1, group.y1 = max(group.x1, c.x1), max(group.y1, c.y1)
      end
    end
  end
end

local function renderCasterShadow(obj, data, projector, size, x, y, z)
  Screen.switchContext()
  Draw.rect(0, 0, 128, 128, shadowClear)
  Screen.setParam(Screen.ALPHA_BLEND_EQUATION, shadowSilhouetteBlend)
  -- A distant narrow-FOV light camera approximates directional projection.
  Render.setView(2 * atan(size / (2 * SHADOW_CAMERA_DISTANCE)) * 180 / pi, 1, 64, 128, 128)
  Camera.position(x + shadowLight.x * SHADOW_CAMERA_DISTANCE,
    y + shadowLight.y * SHADOW_CAMERA_DISTANCE, z + shadowLight.z * SHADOW_CAMERA_DISTANCE)
  Camera.target(x, y, z)
  Camera.update()
  local textureMapping, shadeModel, pipeline = data.texture_mapping, data.shade_model, data.pipeline
  data.texture_mapping = false
  data.shade_model = 0
  data.pipeline = Render.PL_NO_LIGHTS
  local ok, err = pcall(obj.render, obj)
  data.texture_mapping = textureMapping
  data.shade_model = shadeModel
  data.pipeline = pipeline
  Screen.setParam(Screen.ALPHA_BLEND_EQUATION, shadowCopyBlend)
  if not ok then error(err, 0) end
  Screen.switchContext()
  Render.setView(60, 2, 250)
  Camera.restore(shadowView)
  Camera.update()
  -- Drawn from the grid placeCasterShadow() built.
  projector:render()
  shadowDraws = shadowDraws + 1
end

-- Multiplies the scene straight from each caster's projector: all a shadow
-- alone on screen needs. With stencil (FAST overlaps), alpha bit 7 marks every
-- pixel already darkened so an overlap darkens once.
function shadowFrame.renderDirect(group, stencil, depthTest, depthMethod)
  Screen.setParam(Screen.ALPHA_TEST_ENABLE, false)
  Screen.setParam(Screen.DST_ALPHA_TEST_ENABLE, false)
  if stencil then
    Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)
    Screen.setParam(Screen.ALPHA_BLEND_EQUATION, shadowCopyBlend)
    Screen.setBuffer(Screen.DRAW_BUFFER, fbuf, 0x00FFFFFF)
    Draw.rect(group.x0, group.y0, group.x1 - group.x0, group.y1 - group.y0, shadowFrame.stencilClear)
    Screen.setBuffer(Screen.DRAW_BUFFER, fbuf, 0)
    Screen.setParam(Screen.DST_ALPHA_TEST_ENABLE, true)
    Screen.setParam(Screen.DST_ALPHA_TEST_METHOD, Screen.DST_ALPHA_ZERO)
    Screen.setParam(Screen.ALPHA_CORRECTION, true)
  else
    -- RGB only: the scene's alpha stays as it was.
    Screen.setBuffer(Screen.DRAW_BUFFER, fbuf, 0xFF000000)
  end
  Screen.setParam(Screen.DEPTH_TEST_ENABLE, depthTest)
  Screen.setParam(Screen.DEPTH_TEST_METHOD, depthMethod)
  Screen.setParam(Screen.ALPHA_TEST_ENABLE, true)
  Screen.setParam(Screen.ALPHA_TEST_METHOD, Screen.ALPHA_GREATER)
  Screen.setParam(Screen.ALPHA_TEST_REF, 0)
  Screen.setParam(Screen.ALPHA_TEST_FAIL, Screen.ALPHA_FAIL_NO_UPDATE)
  for i = 1, group.count do
    local c = group.members[i]
    c.projector:setBlend(Shadows.SHADOW_BLEND_MULTIPLY)
    renderCasterShadow(c.obj, c.data, c.projector, c.size, c.x, c.y, c.z)
  end
  if stencil then
    Screen.setParam(Screen.ALPHA_CORRECTION, false)
    Screen.setParam(Screen.DST_ALPHA_TEST_ENABLE, false)
  end
end

-- Overlapping shadows, resolved exactly: red becomes a coverage buffer that
-- every shadow of the group adds to, then the scene is darkened once by it.
function shadowFrame.renderCoverage(group, depthTest, depthMethod)
  -- Whole GS pages (64x32): the channel shuffles work page by page, and every
  -- pass has to cover exactly the same pixels.
  local rx, ry = group.x0 & ~63, group.y0 & ~31
  local region = {
    x = rx, y = ry,
    width = min(canvas.width, (group.x1 + 63) & ~63) - rx,
    height = min(canvas.height, (group.y1 + 31) & ~31) - ry,
  }
  -- Save red in the unused Z24 alpha byte using a CT32 physical view.
  Screen.setParam(Screen.DST_ALPHA_TEST_ENABLE, false)
  Screen.setParam(Screen.ALPHA_TEST_ENABLE, false)
  Screen.channelShuffle(fbuf, shadowIdentityLUT, Screen.CHANNEL_RED, {
    dst = zbuf, rawDestination = true, outMask = Screen.CHANNEL_ALPHA, alphaBlend = false, alphaTest = false,
    x = region.x, y = region.y, width = region.width, height = region.height,
  })
  Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)
  Screen.setParam(Screen.ALPHA_BLEND_EQUATION, shadowCopyBlend)
  Screen.setBuffer(Screen.DRAW_BUFFER, fbuf, 0xFFFFFF00)
  Draw.rect(region.x, region.y, region.width, region.height, shadowCoverageClear)
  Screen.setParam(Screen.DEPTH_TEST_ENABLE, depthTest)
  Screen.setParam(Screen.DEPTH_TEST_METHOD, depthMethod)

  -- Red += filtered coverage, clamped to 255.
  Screen.setParam(Screen.ALPHA_TEST_ENABLE, true)
  Screen.setParam(Screen.ALPHA_TEST_METHOD, Screen.ALPHA_GREATER)
  Screen.setParam(Screen.ALPHA_TEST_REF, 0)
  Screen.setParam(Screen.ALPHA_TEST_FAIL, Screen.ALPHA_FAIL_NO_UPDATE)

  for i = 1, group.count do
    local c = group.members[i]
    c.projector:setBlend(Shadows.SHADOW_BLEND_ADD)
    renderCasterShadow(c.obj, c.data, c.projector, c.size, c.x, c.y, c.z)
  end
  Screen.channelShuffle(Screen.DRAW_BUFFER, shadowMaskLUT, Screen.CHANNEL_RED, {
    outMask = Screen.CHANNEL_ALPHA, alphaBlend = false, alphaTest = false,
    x = region.x, y = region.y, width = region.width, height = region.height,
  })
  -- Restore saved red multiplied by transmittance (same physical layout).
  Screen.channelShuffle(zbuf, shadowIdentityLUT, Screen.CHANNEL_ALPHA, {
    rawSource = true, outMask = Screen.CHANNEL_RED, alphaBlend = true,
    blendEquation = shadowSceneBlend, alphaTest = false,
    x = region.x, y = region.y, width = region.width, height = region.height,
  })
  Screen.setParam(Screen.ALPHA_TEST_ENABLE, false)
  Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)
  Screen.setBuffer(Screen.DRAW_BUFFER, fbuf, 0xFF0000FF)
  Screen.setParam(Screen.ALPHA_BLEND_EQUATION, shadowDarkenBlend)
  Draw.rect(region.x, region.y, region.width, region.height, shadowCoverageClear)
end

local function renderDynamicShadows(px, py, pz, camX, camY, camZ)
  shadowDraws = 0
  shadowFrame.overlapping = 0
  if shadowMode == shadowFrame.OFF then return end
  local sp, st = shadowView.position, shadowView.target
  sp.x, sp.y, sp.z = camX, camY, camZ
  st.x, st.y, st.z = camTargetX, camTargetY, camTargetZ

  local f = shadowFrame
  f.count = 0
  placeCasterShadow(skin_object, gltf_skin, playerShadow, 3.2, px, py - PLAYER_RADIUS + 0.9, pz)
  for _, c in ipairs(shadowCandidates) do
    local x, y, z = c.prop.body:getPosition()
    c.x, c.y, c.z = x, y, z
    local dx, dz = x - px, z - pz
    c.distance = dx * dx + dz * dz
  end
  table.sort(shadowCandidates, shadowDistanceOrder)
  for i = 1, SHADOW_PROP_LIMIT do
    local c = shadowCandidates[i]
    if c.distance > SHADOW_DISTANCE * SHADOW_DISTANCE then break end
    placeCasterShadow(c.prop.obj, c.data, f.projectors[i], 1.65, c.x, c.y, c.z)
  end
  if f.count == 0 then return end
  f.groupCasters()

  local saved = {}
  for _, p in ipairs({ "ALPHA_TEST_ENABLE", "ALPHA_TEST_METHOD", "ALPHA_TEST_REF", "ALPHA_TEST_FAIL",
                       "DST_ALPHA_TEST_ENABLE", "DST_ALPHA_TEST_METHOD", "DEPTH_TEST_ENABLE",
                       "DEPTH_TEST_METHOD", "ALPHA_BLEND_EQUATION", "COLOR_CLAMP_MODE",
                       "PIXEL_ALPHA_BLEND_ENABLE", "ALPHA_CORRECTION" }) do
    saved[#saved + 1] = { Screen[p], Screen.getParam(Screen[p]) }
  end
  local depthTest = Screen.getParam(Screen.DEPTH_TEST_ENABLE)
  local depthMethod = Screen.getParam(Screen.DEPTH_TEST_METHOD)
  Screen.setBuffer(Screen.DEPTH_BUFFER, zbuf, 1)

  local ok, err = pcall(function()
    Screen.setParam(Screen.COLOR_CLAMP_MODE, 1)
    Screen.setParam(Screen.PIXEL_ALPHA_BLEND_ENABLE, false)
    for g = 1, f.groupCount do
      local group = f.groups[g]
      if group.count == 1 then
        f.renderDirect(group, false, depthTest, depthMethod)
      else
        f.overlapping = f.overlapping + 1
        if shadowMode == f.FAST then f.renderDirect(group, true, depthTest, depthMethod)
        else f.renderCoverage(group, depthTest, depthMethod) end
      end
    end
  end)

  for _, s in ipairs(saved) do Screen.setParam(s[1], s[2]) end
  Screen.setBuffer(Screen.DRAW_BUFFER, fbuf, 0)
  Screen.setBuffer(Screen.DEPTH_BUFFER, zbuf, 0)
  if not ok then error(err, 0) end
end

local function renderOpaqueScene()
  scene_object:render()
  for i = 1, #crates do crates[i].obj:render() end
  for i = 1, #balls do balls[i].obj:render() end
  skin_object:render()
end

local pad = Pads.get()
local gray = Color.new(180, 180, 220, 128)

-- Hardware channel shuffle, depth fog & DOF presets (SELECT to cycle) ----------
local DOF_SHIFT = 3

local FOG_PRESETS = {
  { name = "MIST FOG (LINEAR)", type = "fog", color = Color.new(170, 125, 100, 110), start = 8.5, ["end"] = 36.0,
    opts = { curve = Screen.FOG_LINEAR, skyFog = 0.35, shift = 4 } },
  { name = "DEPTH OF FIELD (DOF)", type = "dof", strength = 124 },
  { name = "DOF + MIST FOG", type = "dof_fog", strength = 118, color = Color.new(182, 198, 225, 108),
    start = 9.0, ["end"] = 36.0, opts = { curve = Screen.FOG_LINEAR, skyFog = 0.35, shift = DOF_SHIFT } },
  { name = "SILENT HILL (EXP2)", type = "fog", color = Color.new(195, 200, 208, 124), start = 5.5, ["end"] = 24.0,
    opts = { curve = Screen.FOG_EXP2, density = 1.85, skyFog = 0.85, shift = 4 } },
  { name = "GOLDEN DUSK (EXP)", type = "fog", color = Color.new(235, 165, 105, 110), start = 7.5, ["end"] = 32.0,
    opts = { curve = Screen.FOG_EXP, density = 1.6, skyFog = 0.45, shift = 4 } },
  { name = "Z-BUFFER SHUFFLE LUT", type = "lut" },
  { name = "NORMALIZED Z (SHIFT 4)", type = "shift", shift = 4 },
  { name = "POSTFX OFF", type = "off" },
}
local fogPresetIdx = 1

-- Automatic benchmark -------------------------------------------------------------
-- Cycles lighting on/off x post-fx on/off, BENCH_PHASE_MS each, and logs the
-- averaged Screen.getProfile() of every phase (same protocol as the JS demo).
local BENCHMARK = rawget(_G, "ATHENA_BENCHMARK") or false
local BENCH_PHASE_MS = 10000
local BENCH_WARMUP_MS = 2000
local BENCH_PHASES = {
  { name = "lights ON  | postfx ON ", lights = true,  postfx = true },
  { name = "lights ON  | postfx OFF", lights = true,  postfx = false },
  { name = "lights OFF | postfx ON ", lights = false, postfx = true },
  { name = "lights OFF | postfx OFF", lights = false, postfx = false },
}
local BENCH_KEYS = { "frame", "script", "render", "anim", "physics", "gpuWait", "vsync" }
local benchPhase = 1
local benchPostfx = true
local benchSum, benchFastClip, benchSamples, benchFrames = nil, 0, 0, 0
local benchTimer = Timer.new()
local function benchApply()
  local phase = BENCH_PHASES[benchPhase]
  scene.pipeline = phase.lights and Render.PL_DEFAULT or Render.PL_NO_LIGHTS
  benchPostfx = phase.postfx
  benchSum = {}
  for _, k in ipairs(BENCH_KEYS) do benchSum[k] = 0 end
  benchFastClip, benchSamples, benchFrames = 0, 0, 0
  benchTimer:reset()
end
local function benchTick()
  local elapsed = benchTimer:getTime() / 1000
  benchFrames = benchFrames + 1
  if elapsed >= BENCH_WARMUP_MS and benchFrames > 60 then
    local prof = Screen.getProfile()
    for _, k in ipairs(BENCH_KEYS) do benchSum[k] = benchSum[k] + prof[k] end
    benchFastClip = benchFastClip + Render.stats().fastClip
    benchSamples = benchSamples + 1
  end
  if elapsed < BENCH_PHASE_MS or benchSamples == 0 then return end
  local avg = {}
  for _, k in ipairs(BENCH_KEYS) do avg[k] = benchSum[k] / benchSamples end
  local mem = System.getMemoryStats()
  print(fmt("[bench] %s | %.1f fps | frame %.1f | script %.1f | render %.1f | anim %.1f | phys %.1f | gpu wait %.1f | vsync %.1f (ms) | fast clip groups %.1f | RAM %d KB | Lua heap %d KB",
    BENCH_PHASES[benchPhase].name, 1000 / avg.frame, avg.frame, avg.script, avg.render, avg.anim, avg.physics,
    avg.gpuWait, avg.vsync, benchFastClip / benchSamples, mem.used // 1024, mem.luaHeap // 1024))
  benchPhase = benchPhase % #BENCH_PHASES + 1
  benchApply()
end

if BENCHMARK then
  print("[bench] automatic benchmark on: 4 phases, " .. (BENCH_PHASE_MS / 1000) .. " s each")
  benchApply()
end

-- DOF alpha mask LUT: eye-space distance from the shifted Z24 MSB byte.
local dofMaskLUT = Screen.createShuffleLUT(function(b)
  if b == 0 then return Color.new(0, 0, 0, 128) end -- sky / far horizon: full blur
  local byteScale = 65536.0 / (1 << DOF_SHIFT)
  local screenScaleZ = 4194304.0
  local nearZ, farZ = 2.0, 250.0
  local zGs = min(screenScaleZ * 2.0, (b + 0.5) * byteScale)
  local zNdc = (zGs / screenScaleZ) - 1.0
  local denom = (farZ + nearZ) + zNdc * (farZ - nearZ)
  local d = denom > 0.0001 and (2.0 * farZ * nearZ) / denom or farZ
  local focusEnd, blurFull = 9.5, 26.0
  if d <= focusEnd then return Color.new(0, 0, 0, 0) end
  local t = min(1.0, (d - focusEnd) / (blurFull - focusEnd))
  local smooth = t * t * (3.0 - 2.0 * t)
  return Color.new(0, 0, 0, min(128, floor(smooth * 128.0 + 0.5)))
end)

-- Draws img over [0, dstW) x [0, dstH) as vertical strips of stripW pixels so
-- the GS stays within one texture page / frame page column at a time.
local function drawInStrips(img, dstW, dstH, u0, v0, u1, v1, stripW)
  local du = (u1 - u0) / dstW
  img.starty = v0
  img.endy = v1
  img.height = dstH
  for x = 0, dstW - 1, stripW do
    local x1 = min(x + stripW, dstW)
    img.startx = u0 + x * du
    img.endx = u0 + x1 * du
    img.width = x1 - x
    img:draw(x, 0)
  end
  img.startx = u0
  img.endx = u1
end

local dofDownBlend = Screen.alphaEquation(Screen.SRC_RGB, Screen.ZERO_RGB, Screen.ALPHA_FIX, Screen.ZERO_RGB, 0x80)
local dofUpBlend = Screen.alphaEquation(Screen.SRC_RGB, Screen.DST_RGB, Screen.SRC_ALPHA, Screen.DST_RGB, 0)

local function renderDepthOfField(strength, alreadyShifted)
  local w, h = canvas.width, canvas.height
  local halfW, halfH = w >> 1, h >> 1

  if not alreadyShifted then Screen.depthBitShift(DOF_SHIFT) end

  -- Z-buffer blue MSB -> framebuffer alpha only (RGB untouched)
  Screen.channelShuffle(Screen.DEPTH_BUFFER, dofMaskLUT, Screen.CHANNEL_BLUE, {
    outMask = Screen.CHANNEL_ALPHA, alphaBlend = false, alphaTest = false,
  })

  -- Downsample RGB + DOF alpha 2x2 into the Z-buffer as scratch (Z writes masked)
  Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)
  Screen.setParam(Screen.ALPHA_TEST_ENABLE, false)
  Screen.setBuffer(Screen.DEPTH_BUFFER, zbuf, 1)
  zbuf.bpp = 32
  Screen.setBuffer(Screen.DRAW_BUFFER, zbuf, 0)
  Screen.setParam(Screen.ALPHA_BLEND_EQUATION, dofDownBlend)
  fbuf.filter = LINEAR
  fbuf.color = Color.new(128, 128, 128, 128)
  drawInStrips(fbuf, halfW, halfH, 0.5, 0.5, w - 0.5, h - 0.5, 32)

  -- Upsample weighted by the blurred alpha mask
  Screen.setBuffer(Screen.DRAW_BUFFER, fbuf, 0xFF000000)
  Screen.setParam(Screen.ALPHA_BLEND_EQUATION, dofUpBlend)
  zbuf.filter = LINEAR
  zbuf.color = Color.new(128, 128, 128, strength)
  drawInStrips(zbuf, w, h, 0.5, 0.5, halfW - 0.5, halfH - 0.5, 64)

  -- Restore formats, masks and the default blend
  fbuf.width, fbuf.height = w, h
  zbuf.width, zbuf.height = w, h
  zbuf.bpp = 26
  Screen.setBuffer(Screen.DEPTH_BUFFER, zbuf, 0)
  Screen.setBuffer(Screen.DRAW_BUFFER, fbuf, 0)
  Screen.setParam(Screen.ALPHA_BLEND_EQUATION, Screen.BLEND_DEFAULT)
end

local function heatmapLUT(divisor)
  return Screen.createShuffleLUT(function(b)
    if b == 0 then return Color.new(15, 18, 35, 115) end
    local norm = min(1.0, b / divisor)
    local r = min(255, floor(norm * 510.0))
    local g = min(255, max(0, floor((norm - 0.25) * 360.0)))
    local bl = min(255, max(40, floor((1.0 - norm) * 255.0)))
    return Color.new(r, g, bl, 118)
  end)
end
local depthHeatmapLUT = heatmapLUT(64.0)
local depthNormalizedLUT = heatmapLUT(160.0)

Screen.setParam(Screen.ALPHA_TEST_ENABLE, false)
Screen.setParam(Screen.ALPHA_TEST_METHOD, Screen.ALPHA_LESS)
Screen.setParam(Screen.ALPHA_TEST_REF, 0x80)
Screen.setParam(Screen.DEPTH_TEST_ENABLE, true)
Screen.setParam(Screen.DEPTH_TEST_METHOD, Screen.DEPTH_GEQUAL)

local function anim(name) return skin_anims[name] end

while true do
  Screen.clear(gray)
  pad:update()

  if ui:homeRequested(pad) then
    fxSystem:dispose()
    shadowRT:unlock()
    shadowRT:free()
    shadowMaskLUT:free()
    shadowIdentityLUT:free()
    os.chdir("..")
    app.exitToShell()
  end

  if not BENCHMARK and pad:justPressed(Pads.R3) then fxReset((fxLevel + 1) % FX_LEVEL_COUNT) end
  if pad:justPressed(Pads.L3) then shadowMode = (shadowMode + 1) % 3 end

  if pad:justPressed(Pads.START) then
    resetSandbox()
    fxReset(fxLevel)
  end

  if pad:justPressed(Pads.SELECT) then
    fogPresetIdx = fogPresetIdx % #FOG_PRESETS + 1
    statusText = "POST-FX: " .. FOG_PRESETS[fogPresetIdx].name
    statusTimer = 110
  end

  -- 1. Camera orbit & zoom (right stick + L2/R2)
  local rx = ((pad.rx > 25 or pad.rx < -25) and pad.rx or 0) / 1024.0
  local ry = ((pad.ry > 25 or pad.ry < -25) and pad.ry or 0) / 1024.0

  camYaw = camYaw - rx * 0.18
  camPitch = min(1.15, max(0.08, camPitch + ry * 0.14))

  if pad:pressed(Pads.R2) then
    camDist = max(2.8, camDist - 0.12)
  elseif pad:pressed(Pads.L2) then
    camDist = min(13.0, camDist + 0.12)
  end

  -- 2. Player movement (left stick or D-pad)
  local stickX = ((pad.lx > 25 or pad.lx < -25) and pad.lx or 0) / 128.0
  local stickY = ((pad.ly > 25 or pad.ly < -25) and pad.ly or 0) / 128.0

  if pad:pressed(Pads.LEFT) then stickX = -1.0 end
  if pad:pressed(Pads.RIGHT) then stickX = 1.0 end
  if pad:pressed(Pads.UP) then stickY = -1.0 end
  if pad:pressed(Pads.DOWN) then stickY = 1.0 end

  local inputMag = sqrt(stickX * stickX + stickY * stickY)
  if inputMag > 1.0 then
    stickX, stickY = stickX / inputMag, stickY / inputMag
    inputMag = 1.0
  end

  local px, py, pz = player_body:getPosition()
  local vx, vy, vz = player_body:getLinearVel()

  -- Respawn safety if the player ever falls out of bounds
  if py < -8.0 then
    resetSandbox()
    px, py, pz = PLAYER_START.x, PLAYER_START.y, PLAYER_START.z
    vx, vy, vz = 0.0, 0.0, 0.0
  end

  if jumpCooldown > 0 then jumpCooldown = jumpCooldown - 1 end
  if kickCooldown > 0 then kickCooldown = kickCooldown - 1 end
  if statusTimer > 0 then statusTimer = statusTimer - 1 end
  if animTimer > 0 then animTimer = animTimer - 1 end

  local onFloor = (groundGrace > 0) or (py <= PLAYER_RADIUS + 0.12 and abs(vy) < 1.8)

  -- Standing / balancing on top of a crate or ball?
  local onProp = false
  if abs(vy) < 2.2 and jumpCooldown < 30 then
    for _, prop in ipairs(allProps) do
      local bx, by, bz = prop.body:getPosition()
      local dx, dz = px - bx, pz - bz
      local dy = (py - PLAYER_RADIUS) - by
      if (dx * dx + dz * dz) < 0.72 * 0.72 and dy > 0.15 and dy < 0.75 then
        onProp = true
        break
      end
    end
  end

  local isGrounded = onFloor or onProp
  local isBalancing = onProp

  local isSprinting = pad:pressed(Pads.R1)
  local moveSpeed = isSprinting and 5.2 or 2.2

  -- Camera-relative horizontal basis vectors
  local fwdX, fwdZ = -sin(camYaw), -cos(camYaw)
  local rightX, rightZ = cos(camYaw), -sin(camYaw)

  local moveX = stickX * rightX + (-stickY) * fwdX
  local moveZ = stickX * rightZ + (-stickY) * fwdZ

  if inputMag > 0.08 then
    local accel = isGrounded and 0.35 or 0.12
    vx = vx + (moveX * moveSpeed - vx) * accel
    vz = vz + (moveZ * moveSpeed - vz) * accel

    -- Smoothly rotate the character toward the movement direction
    local diff = atan(-moveX, moveZ) - playerYaw
    while diff > pi do diff = diff - 2.0 * pi end
    while diff < -pi do diff = diff + 2.0 * pi end
    playerYaw = playerYaw + diff * 0.22

    -- Gentle camera auto-align when not manually orbiting
    if rx == 0.0 and ry == 0.0 then camYaw = camYaw - stickX * 0.012 end

    walkCycle = walkCycle + (isSprinting and 0.32 or 0.20) * inputMag
    emoteActive = false
  elseif isGrounded then
    vx, vz = vx * 0.72, vz * 0.72
  end

  -- 3. Actions: jump (CROSS), kick (SQUARE), throw (CIRCLE), emote (L1)
  if pad:justPressed(Pads.CROSS) and isGrounded and jumpCooldown == 0 and jumpWindup == 0 then
    jumpWindup = 26 -- ~0.43s squat/wind-up before leaving the ground
    jumpCooldown = 50
    emoteActive = false
    local jumpClip = anim("jump") and "jump" or "capoeira"
    skin_object:playAnim(anim(jumpClip), false)
    currentAnim = jumpClip
    animTimer = 68
    statusText = "CHARGING JUMP..."
    statusTimer = 45
  end

  if jumpWindup > 0 then
    jumpWindup = jumpWindup - 1
    if jumpWindup == 0 then
      vy = 7.4
      groundGrace = 0
      statusText = "JUMP!"
      statusTimer = 45
      fxBurst(px, py - PLAYER_RADIUS + 0.025, pz, 8)
    end
  end

  -- SQUARE: standing melee kick with wind-up & live right-foot bone collision
  if pad:justPressed(Pads.SQUARE) and kickCooldown == 0 and kickPhase == 0 then
    kickCooldown = 58
    kickPhase = 1
    kickHits = 0
    kickHitBodies = {}
    emoteActive = false
    local kickClip = anim("kick") and "kick" or "capoeira"
    skin_object:playAnim(anim(kickClip), false)
    currentAnim = kickClip
    animTimer = 64
    statusText = "WINDING UP KICK..."
    statusTimer = 45
  end

  if kickPhase > 0 then
    kickPhase = kickPhase + 1

    if isGrounded and kickPhase < 48 then vx, vz = vx * 0.65, vz * 0.65 end

    -- Active foot strike window (frames 26..45 of the Mixamo kick animation)
    if kickPhase >= 26 and kickPhase <= 45 then
      local cz, sz = cos(playerYaw), sin(playerYaw)
      local faceX, faceZ = -sz, cz

      -- Live RightToeBase & RightFoot transforms (toArray is 1-based in Lua)
      local sx, sy, szBone = -0.06, 0.98, -1.15
      local bones = skin_object.bones
      if bones and bones[RIGHT_TOE_BONE] then
        local mToe = bones[RIGHT_TOE_BONE].transform:toArray()
        sx, sy, szBone = mToe[4], mToe[8], mToe[12]
        if bones[RIGHT_FOOT_BONE] then
          local mFoot = bones[RIGHT_FOOT_BONE].transform:toArray()
          sx = 0.6 * sx + 0.4 * mFoot[4]
          sy = 0.6 * sy + 0.4 * mFoot[8]
          szBone = 0.6 * szBone + 0.4 * mFoot[12]
        end
      end

      -- Skeleton-space foot -> world space
      local footX = px + (sx * cz - sy * sz)
      local footY = (py - PLAYER_RADIUS) - szBone
      local footZ = pz + (sx * sz + sy * cz)

      for i, prop in ipairs(allProps) do
        if not kickHitBodies[i] then
          local b = prop.body
          local bx, by, bz = b:getPosition()
          local dx, dy, dz = bx - footX, by - footY, bz - footZ
          local horizDistSq = dx * dx + dz * dz

          if horizDistSq < 1.15 * 1.15 and abs(dy) < 1.25 then
            local horizDist = sqrt(horizDistSq) + 0.001
            local dirX = faceX * 0.65 + (dx / horizDist) * 0.35
            local dirZ = faceZ * 0.65 + (dz / horizDist) * 0.35
            local dirLen = sqrt(dirX * dirX + dirZ * dirZ) + 0.001
            dirX, dirZ = dirX / dirLen, dirZ / dirLen

            local strength = 13.5
            local bvx, bvy, bvz = b:getLinearVel()
            b:setLinearVel(bvx + dirX * strength, bvy + 5.2, bvz + dirZ * strength)
            b:setAngularVel(dirZ * 10.0, 4.0, -dirX * 10.0)

            kickHitBodies[i] = true
            kickHits = kickHits + 1
            fxBurst(footX, footY, footZ, 20, i <= #crates and 1 or 2)
            statusText = "KICK IMPACT! (" .. kickHits .. " HIT)"
            statusTimer = 60
          end
        end
      end
    end

    if kickPhase >= 52 then
      if kickHits == 0 then
        statusText = "KICK!"
        statusTimer = 30
      end
      kickPhase = 0
    end
  end

  -- CIRCLE: throw a monkey sphere forward
  if pad:justPressed(Pads.CIRCLE) then
    local b = balls[nextBallIdx]
    nextBallIdx = nextBallIdx % #balls + 1

    local faceX, faceZ = -sin(playerYaw), cos(playerYaw)
    b.body:setPosition(px + faceX * 0.95, (py - PLAYER_RADIUS) + 1.05, pz + faceZ * 0.95)
    b.body:setLinearVel(faceX * 15.5 + vx * 0.4, 2.8, faceZ * 15.5 + vz * 0.4)
    b.body:setAngularVel(faceZ * 12.0, 4.0, -faceX * 12.0)

    statusText = "MONKEY BALL LAUNCHED!"
    statusTimer = 60
  end

  -- L1: cycle Mixamo dance emotes
  if pad:justPressed(Pads.L1) then
    emoteIdx = emoteIdx % #emotes + 1
    local chosen = emotes[emoteIdx]
    emoteActive = true
    animTimer = 0
    skin_object:playAnim(anim(chosen), true)
    currentAnim = chosen
    statusText = "DANCE: " .. chosen:upper()
    statusTimer = 60
  end

  -- Keep the animation looping according to the movement state
  if animTimer == 0 then
    local desired = "idle"
    if not isGrounded then
      desired = anim("fall") and "fall" or "idle"
    elseif isBalancing and inputMag < 0.35 and anim("balance") then
      desired = "balance"
    elseif inputMag > 0.08 then
      desired = isSprinting and "run" or "walk"
    elseif emoteActive then
      desired = emotes[emoteIdx]
    end
    if not anim(desired) then desired = "idle" end
    if currentAnim ~= desired or not skin_object:isPlayingAnim() then
      skin_object:playAnim(anim(desired), true)
      currentAnim = desired
    end
  end

  -- Apply player velocity & lock body rotation
  local preStepVy = vy
  player_body:setLinearVel(vx, vy, vz)
  player_body:setAngularVel(0.0, 0.0, 0.0)

  -- 4. Step the world & resolve collisions
  world:stepWithContacts(space, jgroup, 0.016)

  -- Ground contact: free fall changes vy by -0.288 per step; if normal forces
  -- counteract gravity, refresh groundGrace.
  local _, postVy = player_body:getLinearVel()
  if jumpCooldown < 38 and (postVy - preStepVy) > -0.18 and postVy > -2.4 and postVy < 2.8 then
    if not isGrounded and preStepVy < -2 then fxBurst(px, py - PLAYER_RADIUS + 0.025, pz, 14) end
    groundGrace = 6
  elseif groundGrace > 0 then
    groundGrace = groundGrace - 1
  end

  -- Sync the player visual mesh
  px, py, pz = player_body:getPosition()
  if isGrounded and inputMag > 0.08 and walkCycle >= fxNextStep then
    local side = (floor(walkCycle / pi) % 2 == 0) and -1 or 1
    fxBurst(px + cos(playerYaw) * 0.18 * side, py - PLAYER_RADIUS + 0.025,
      pz + sin(playerYaw) * 0.18 * side, isSprinting and 5 or 3)
    fxNextStep = walkCycle + pi
  end
  fxPropImpacts()

  skin_object:setPosition(px, py - PLAYER_RADIUS, pz)
  skin_object:setRotation(pi / 2, 0.0, playerYaw)

  -- 5. Third-person follow camera
  camTargetX = camTargetX + (px - camTargetX) * 0.18
  camTargetY = camTargetY + ((py - PLAYER_RADIUS + 1.15) - camTargetY) * 0.14
  camTargetZ = camTargetZ + (pz - camTargetZ) * 0.18

  local horizDist = camDist * cos(camPitch)
  local camX = camTargetX + sin(camYaw) * horizDist
  local camY = max(0.55, camTargetY + sin(camPitch) * camDist)
  local camZ = camTargetZ + cos(camYaw) * horizDist

  Camera.target(camTargetX, camTargetY, camTargetZ)
  Camera.position(camX, camY, camZ)
  Camera.update()

  -- 6. Render pass
  Render.begin()
  Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)
  sky:draw(0, 0)
  Screen.setParam(Screen.DEPTH_TEST_ENABLE, true)
  Screen.setParam(Screen.DEPTH_TEST_METHOD, Screen.DEPTH_GEQUAL)
  renderOpaqueScene()
  -- Hand the scene to the GPU now (see physics_collision.js).
  Screen.flush()
  renderDynamicShadows(px, py, pz, camX, camY, camZ)
  -- World-space billboards use scene depth, then receive the same fog/DOF.
  fxRender()

  -- 6.5. Hardware depth fog / DOF / channel shuffle post-pass
  local activeFog = FOG_PRESETS[fogPresetIdx]
  if not benchPostfx then
    -- benchmark phase with post-fx off
  elseif activeFog.type == "fog" then
    Screen.depthFog(activeFog.color, activeFog.start, activeFog["end"], activeFog.opts)
  elseif activeFog.type == "dof" then
    renderDepthOfField(activeFog.strength, false)
  elseif activeFog.type == "dof_fog" then
    Screen.depthFog(activeFog.color, activeFog.start, activeFog["end"], activeFog.opts)
    renderDepthOfField(activeFog.strength, true)
  elseif activeFog.type == "lut" then
    Screen.channelShuffle(Screen.DEPTH_BUFFER, depthHeatmapLUT, Screen.CHANNEL_BLUE,
      { outMask = Screen.CHANNEL_RGB, alphaBlend = true, alphaTest = true })
  elseif activeFog.type == "shift" then
    Screen.depthBitShift(activeFog.shift)
    Screen.channelShuffle(Screen.DEPTH_BUFFER, depthNormalizedLUT, Screen.CHANNEL_BLUE,
      { outMask = Screen.CHANNEL_RGB, alphaBlend = true, alphaTest = true })
  end

  -- 7. HUD overlay
  Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)

  local stateLabel
  if not isGrounded then
    stateLabel = currentAnim == "jump" and "JUMPING" or "FALLING"
  elseif kickPhase > 0 then
    stateLabel = "KICKING"
  elseif isBalancing and inputMag < 0.35 then
    stateLabel = "BALANCING"
  elseif inputMag > 0.08 then
    stateLabel = isSprinting and "SPRINTING" or "WALKING"
  else
    stateLabel = emoteActive and "DANCING" or "IDLE"
  end
  local renderStats = Render.stats()
  ui.status = fmt("%.2f FPS", Screen.getFPS(360))

  local prof = Screen.getProfile()
  local mem = System.getMemoryStats()
  local compRatio = mem.compressedBytes > 0 and fmt("%.2fx", mem.uncompressedBytes / mem.compressedBytes) or "1.00x"
  local fxStats = fxSystem:stats()

  -- Left card: what the player is doing and the world toggles.
  local worldLines = {
    stateLabel .. " (" .. currentAnim:upper() .. ")",
    "SELECT  Fog: " .. activeFog.name,
    fmt("R3  FX density: %d  (%d active, %d visible, %d batches)", FX_LEVELS[fxLevel], fxStats.active, fxStats.visible, fxStats.batches),
    fmt("L3  Shadows: %s  (%d casters, %d overlapping)", shadowFrame.MODE_NAMES[shadowMode], shadowDraws,
      shadowFrame.overlapping),
    "START  Reset sandbox  |  Fast clip: " .. renderStats.fastClip,
  }
  if statusTimer > 0 then worldLines[#worldLines + 1] = ">> " .. statusText end
  ui:hud(worldLines)

  -- Right card: frame profile and memory compaction.
  ui:hud({
    fmt("frame %.1f ms", prof.frame),
    fmt("script %.1f  render %.1f", prof.script, prof.render),
    fmt("anim %.1f  phys %.1f", prof.anim, prof.physics),
    fmt("gpu %.1f  vsync %.1f", prof.gpuWait, prof.vsync),
    fmt("RAM %d KB  LZ4 -%.1f KB (%s)", mem.used // 1024, mem.savedBytes / 1024, compRatio),
    fmt("Tex %d/%d  Mesh %d/%d  OOM %d  Lua %d KB", mem.compressedTextures, mem.trackedTextures,
      mem.compressedMeshes, mem.trackedMeshes, mem.oomRescues, mem.luaHeap // 1024),
  }, { right = true, bottom = true })

  ui:chrome()

  if BENCHMARK then benchTick() end
  Screen.flip()
end
