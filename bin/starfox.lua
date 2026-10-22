-- {"name": "Star Fox", "author": "Daniel Santos", "version": "10192026", "file": "starfox.lua", "category": "games", "blurb": "A rail shooter built on the 3D renderer.", "exit": "TRIANGLE"}
--
-- Lua port of starfox.js. The JS version moves its hot math into
-- Native.compile()d MIPS functions; here those are plain Lua functions, and
-- the typed arrays / Native.struct pools are ordinary tables.

local font = Font("default")
font.scale = 0.6
font.outline = 1.0
font.outline_color = Color.new(0, 0, 0)

Screen.setFrameCounter(true)
Screen.setVSync(true)

local canvas = Screen.getMode()
canvas.zbuffering = true
canvas.psm = Screen.CT32
canvas.psmz = Screen.Z24
Screen.setMode(canvas)
Screen.initBuffers()

Render.init()
Render.setView(72.0, 0.5, 900.0)

local CW = canvas.width
local CH = canvas.height

local sin, cos, sqrt, atan, floor, max, min, abs, pi =
  math.sin, math.cos, math.sqrt, math.atan, math.floor, math.max, math.min, math.abs, math.pi
local random = math.random
math.randomseed(os.millis())

local function randomf(lo, hi) return lo + random() * (hi - lo) end
local function randomi(lo, hi) return random(lo, hi) end

-- ---------------------------------------------------------------------------
-- Palette / tuning
-- ---------------------------------------------------------------------------
local SPACE = Color.new(6, 8, 26)
local C_WHITE = Color.new(230, 240, 255)
local C_CYAN = Color.new(120, 220, 255)
local C_YELLOW = Color.new(255, 214, 90)
local C_RED = Color.new(255, 90, 90)
local C_GREEN = Color.new(120, 255, 150)
local C_DIM = Color.new(120, 140, 180)

local STAR_COUNT = 56
local STAR_SPREAD_X = 90.0
local STAR_SPREAD_Y = 55.0
local STAR_Z_NEAR = 150.0
local STAR_Z_FAR = 340.0
local MAX_PARTICLES = 72
local MAX_PLANETS = 4
local BOUNDS_X = 6.0
local BOUNDS_Y_LOW = -2.4
local BOUNDS_Y_HIGH = 3.4
local SHIP_ACCEL = 0.16
local BASE_SPEED = 0.85
local MAX_BULLETS = 40
local MAX_ENEMIES = 10
local VOXEL_GRID = 5
local VOXEL_MAX = 16
local VOXEL_CELL = 0.30
local VOXEL_DESTROY_AT = 0

local STATE_MENU = 0
local STATE_PLAY = 1
local STATE_OVER = 2
local STATE_HANGAR = 3

local function bootFile(name)
  local base = System.boot_path
  return base:sub(-1) == "/" and base .. name or base .. "/" .. name
end

local SAVE_PATH = bootFile("starfox_save.json")

-- ---------------------------------------------------------------------------
-- Roguelike catalogs
-- ---------------------------------------------------------------------------
local SHIPS = {
  scout = {
    id = "scout", name = "Scout", price = 0,
    base = { cooldown = 8, bullets = 1, spread = 0.0, damage = 1, lives = 3, speed = 1.0 },
    body = {
      { cx = 0.0, cy = 0.0, cz = 0.0, sx = 0.5, sy = 0.28, sz = 1.6, accent = 0 },
      { cx = 0.0, cy = 0.14, cz = 0.32, sx = 0.24, sy = 0.14, sz = 0.5, accent = 1 },
      { cx = -0.66, cy = 0.14, cz = -0.35, sx = 0.07, sy = 0.4, sz = 0.42, accent = 0 },
      { cx = 0.66, cy = 0.14, cz = -0.35, sx = 0.07, sy = 0.4, sz = 0.42, accent = 0 },
      { cx = 0.0, cy = 0.28, cz = -0.72, sx = 0.07, sy = 0.5, sz = 0.4, accent = 0 },
    },
  },
  raptor = {
    id = "raptor", name = "Raptor", price = 120,
    base = { cooldown = 6, bullets = 1, spread = 0.0, damage = 1, lives = 2, speed = 1.15 },
    body = {
      { cx = 0.0, cy = 0.0, cz = 0.0, sx = 0.42, sy = 0.22, sz = 1.5, accent = 0 },
      { cx = 0.0, cy = 0.12, cz = 0.28, sx = 0.2, sy = 0.12, sz = 0.45, accent = 1 },
      { cx = 0.0, cy = 0.22, cz = -0.65, sx = 0.06, sy = 0.45, sz = 0.35, accent = 0 },
    },
  },
  titan = {
    id = "titan", name = "Titan", price = 200,
    base = { cooldown = 10, bullets = 1, spread = 0.0, damage = 2, lives = 5, speed = 0.82 },
    body = {
      { cx = 0.0, cy = 0.0, cz = 0.0, sx = 0.62, sy = 0.34, sz = 1.75, accent = 0 },
      { cx = 0.0, cy = 0.16, cz = 0.35, sx = 0.28, sy = 0.16, sz = 0.55, accent = 1 },
      { cx = -0.72, cy = 0.12, cz = -0.2, sx = 0.08, sy = 0.35, sz = 0.5, accent = 0 },
      { cx = 0.72, cy = 0.12, cz = -0.2, sx = 0.08, sy = 0.35, sz = 0.5, accent = 0 },
    },
  },
}

local PARTS = {
  wing = {
    { id = "std", name = "Std Wings", price = 0, mod = {}, mesh = {
      { cx = -1.0, cy = 0.0, cz = -0.1, sx = 1.4, sy = 0.06, sz = 0.6, accent = 0 },
      { cx = 1.0, cy = 0.0, cz = -0.1, sx = 1.4, sy = 0.06, sz = 0.6, accent = 0 },
      { cx = -1.75, cy = 0.0, cz = -0.15, sx = 0.3, sy = 0.1, sz = 0.45, accent = 1 },
      { cx = 1.75, cy = 0.0, cz = -0.15, sx = 0.3, sy = 0.1, sz = 0.45, accent = 1 },
    } },
    { id = "wide", name = "Wide Wings", price = 80, mod = { speed = -0.05, lives = 1 }, mesh = {
      { cx = -1.2, cy = 0.0, cz = -0.05, sx = 1.75, sy = 0.05, sz = 0.55, accent = 0 },
      { cx = 1.2, cy = 0.0, cz = -0.05, sx = 1.75, sy = 0.05, sz = 0.55, accent = 0 },
    } },
    { id = "dart", name = "Dart Wings", price = 65, mod = { speed = 0.12, cooldown = -1 }, mesh = {
      { cx = -0.85, cy = 0.0, cz = -0.2, sx = 1.0, sy = 0.04, sz = 0.7, accent = 0 },
      { cx = 0.85, cy = 0.0, cz = -0.2, sx = 1.0, sy = 0.04, sz = 0.7, accent = 0 },
    } },
  },
  engine = {
    { id = "std", name = "Std Engine", price = 0, mod = {}, mesh = {
      { cx = 0.0, cy = 0.0, cz = -0.86, sx = 0.34, sy = 0.22, sz = 0.2, accent = 0 },
    } },
    { id = "turbo", name = "Turbo", price = 90, mod = { speed = 0.18, cooldown = -2 }, mesh = {
      { cx = 0.0, cy = 0.0, cz = -0.95, sx = 0.42, sy = 0.28, sz = 0.28, accent = 1 },
    } },
    { id = "heavy", name = "Heavy", price = 70, mod = { lives = 1, speed = -0.08 }, mesh = {
      { cx = 0.0, cy = 0.0, cz = -0.88, sx = 0.48, sy = 0.3, sz = 0.32, accent = 0 },
    } },
  },
  cannon = {
    { id = "std", name = "Pulse Laser", price = 0, mod = {}, mesh = {} },
    { id = "twin", name = "Twin Mount", price = 100, mod = { bullets = 1, spread = 0.35 }, mesh = {
      { cx = -0.35, cy = -0.05, cz = 0.5, sx = 0.12, sy = 0.12, sz = 0.35, accent = 1 },
      { cx = 0.35, cy = -0.05, cz = 0.5, sx = 0.12, sy = 0.12, sz = 0.35, accent = 1 },
    } },
    { id = "rail", name = "Rail Gun", price = 110, mod = { damage = 1, cooldown = 2 }, mesh = {
      { cx = 0.0, cy = -0.08, cz = 0.65, sx = 0.18, sy = 0.14, sz = 0.55, accent = 1 },
    } },
  },
  nose = {
    { id = "std", name = "Std Nose", price = 0, mod = {}, mesh = {
      { cx = 0.0, cy = 0.0, cz = 1.0, sx = 0.3, sy = 0.18, sz = 0.55, accent = 0 },
      { cx = 0.0, cy = -0.02, cz = 1.35, sx = 0.14, sy = 0.1, sz = 0.3, accent = 1 },
    } },
    { id = "probe", name = "Probe Nose", price = 55, mod = { damage = 1 }, mesh = {
      { cx = 0.0, cy = 0.0, cz = 1.15, sx = 0.22, sy = 0.14, sz = 0.75, accent = 0 },
      { cx = 0.0, cy = 0.0, cz = 1.55, sx = 0.08, sy = 0.08, sz = 0.35, accent = 1 },
    } },
    { id = "blunt", name = "Blunt Nose", price = 45, mod = { lives = 1 }, mesh = {
      { cx = 0.0, cy = 0.0, cz = 0.95, sx = 0.38, sy = 0.22, sz = 0.5, accent = 0 },
    } },
  },
}

local COLORS = {
  { id = "arwing", name = "Arwing", price = 0, primary = { 0.72, 0.76, 0.86 }, accent = { 0.35, 0.78, 0.98 } },
  { id = "crimson", name = "Crimson", price = 40, primary = { 0.82, 0.22, 0.18 }, accent = { 1.0, 0.45, 0.15 } },
  { id = "jade", name = "Jade", price = 40, primary = { 0.22, 0.72, 0.48 }, accent = { 0.55, 1.0, 0.72 } },
  { id = "void", name = "Void", price = 55, primary = { 0.28, 0.26, 0.38 }, accent = { 0.72, 0.35, 0.95 } },
  { id = "gold", name = "Gold", price = 80, primary = { 0.85, 0.72, 0.22 }, accent = { 1.0, 0.92, 0.45 } },
}

local UPGRADE_DEFS = {
  firerate = { name = "Fire Rate", max = 5, baseCost = 45,
    apply = function(s, lv) s.cooldown = max(3, s.cooldown - lv) end },
  multishot = { name = "Multi-Shot", max = 3, baseCost = 70,
    apply = function(s, lv)
      s.bullets = s.bullets + lv
      if lv > 0 and s.bullets > 1 then s.spread = max(s.spread, 0.22 + lv * 0.14) end
    end },
  damage = { name = "Damage", max = 4, baseCost = 55,
    apply = function(s, lv) s.damage = s.damage + lv end },
  hull = { name = "Hull", max = 3, baseCost = 60,
    apply = function(s, lv) s.lives = s.lives + lv end },
}

local SHIP_IDS = { "scout", "raptor", "titan" }
local PART_SLOTS = { "wing", "engine", "cannon", "nose" }
local UPGRADE_KEYS = { "firerate", "multishot", "damage", "hull" }
local HANGAR_TABS = { "SHIPS", "PARTS", "COLORS", "UPGRADES", "LAUNCH" }
local TAB_SHIPS, TAB_PARTS, TAB_COLORS, TAB_UPGRADES, TAB_LAUNCH = 1, 2, 3, 4, 5

local function defaultSave()
  return {
    credits = 0,
    best = 0,
    ownedShips = { "scout" },
    ownedParts = { wing = { "std" }, engine = { "std" }, cannon = { "std" }, nose = { "std" } },
    ownedColors = { "arwing" },
    upgrades = { firerate = 0, multishot = 0, damage = 0, hull = 0 },
    loadout = { ship = "scout", wing = "std", engine = "std", cannon = "std", nose = "std", color = "arwing" },
  }
end

local function mergeSave(raw)
  local d = defaultSave()
  if type(raw) ~= "table" then return d end
  if raw.credits ~= nil then d.credits = raw.credits end
  if raw.best ~= nil then d.best = raw.best end
  if raw.ownedShips then d.ownedShips = raw.ownedShips end
  if raw.ownedParts then
    for _, k in ipairs(PART_SLOTS) do
      if raw.ownedParts[k] then d.ownedParts[k] = raw.ownedParts[k] end
    end
  end
  if raw.ownedColors then d.ownedColors = raw.ownedColors end
  if raw.upgrades then
    for _, k in ipairs(UPGRADE_KEYS) do d.upgrades[k] = raw.upgrades[k] or 0 end
  end
  if raw.loadout then d.loadout = raw.loadout end
  return d
end

local function loadSave()
  local text = io.readfile(SAVE_PATH)
  if not text or text == "" then return defaultSave() end
  local ok, raw = pcall(JSON.parse, text)
  return ok and mergeSave(raw) or defaultSave()
end

local save

local function saveGame()
  pcall(function() io.writefile(SAVE_PATH, JSON.stringify(save)) end)
end

local function findPart(slot, id)
  for _, p in ipairs(PARTS[slot]) do
    if p.id == id then return p end
  end
  return PARTS[slot][1]
end

local function findColor(id)
  for _, c in ipairs(COLORS) do
    if c.id == id then return c end
  end
  return COLORS[1]
end

local function contains(list, id)
  for _, v in ipairs(list) do
    if v == id then return true end
  end
  return false
end

local function ownsShip(id) return contains(save.ownedShips, id) end
local function ownsPart(slot, id) return contains(save.ownedParts[slot], id) end
local function ownsColor(id) return contains(save.ownedColors, id) end

local function upgradeCost(key, level)
  return UPGRADE_DEFS[key].baseCost + level * 35
end

local function computeStats(sv)
  local hull = SHIPS[sv.loadout.ship] or SHIPS.scout
  local b = hull.base
  local s = { cooldown = b.cooldown, bullets = b.bullets, spread = b.spread,
              damage = b.damage, lives = b.lives, speed = b.speed }

  for _, slot in ipairs(PART_SLOTS) do
    local mod = findPart(slot, sv.loadout[slot]).mod
    if mod then
      for k, v in pairs(mod) do s[k] = s[k] + v end
    end
  end

  for _, key in ipairs(UPGRADE_KEYS) do UPGRADE_DEFS[key].apply(s, sv.upgrades[key]) end

  s.cooldown = max(3, s.cooldown)
  s.bullets = max(1, s.bullets)
  s.damage = max(1, s.damage)
  s.lives = max(1, s.lives)
  s.speed = max(0.5, s.speed)
  return s
end

-- ---------------------------------------------------------------------------
-- Math helpers (Native.compile'd in the JS version)
-- ---------------------------------------------------------------------------
local function clamp(v, lo, hi)
  if v < lo then return lo elseif v > hi then return hi end
  return v
end

local function lerp(a, b, t) return a + (b - a) * t end

local function normalize3(x, y, z)
  local len = sqrt(x * x + y * y + z * z)
  if len < 0.0001 then return { x = 0.0, y = 0.0, z = 1.0 } end
  return { x = x / len, y = y / len, z = z / len }
end

local function rotateX(v, a)
  local c, s = cos(a), sin(a)
  return { x = v.x, y = v.y * c - v.z * s, z = v.y * s + v.z * c }
end

local function rotateY(v, a)
  local c, s = cos(a), sin(a)
  return { x = v.x * c + v.z * s, y = v.y, z = -v.x * s + v.z * c }
end

local function rotateZ(v, a)
  local c, s = cos(a), sin(a)
  return { x = v.x * c - v.y * s, y = v.x * s + v.y * c, z = v.z }
end

local function worldToLocal(wx, wy, wz, ex, ey, ez, sc, rx, ry, rz)
  local v = { x = (wx - ex) / sc, y = (wy - ey) / sc, z = (wz - ez) / sc }
  v = rotateZ(v, -rz)
  v = rotateY(v, -ry)
  return rotateX(v, -rx)
end

local function localToWorld(lx, ly, lz, ex, ey, ez, sc, rx, ry, rz)
  local v = rotateX({ x = lx, y = ly, z = lz }, rx)
  v = rotateY(v, ry)
  v = rotateZ(v, rz)
  return { x = ex + v.x * sc, y = ey + v.y * sc, z = ez + v.z * sc }
end

-- Integer hash. Lua integers are 32-bit here and wrap like ToInt32 in JS.
local function voxelHash(ix, iy, iz, seed)
  return ((ix * 73856093) ~ (iy * 19349663) ~ (iz * 83492791) ~ seed) & 255
end

local function voxelColor(ix, iy, iz, seed)
  local t = voxelHash(ix, iy, iz, seed) / 255.0
  return 0.40 + 0.24 * t, 0.36 + 0.20 * t, 0.32 + 0.18 * t
end

local LIGHT = normalize3(0.35, 0.9, -0.28)
local LX, LY, LZ = LIGHT.x, LIGHT.y, LIGHT.z

local function faceLight(nx, ny, nz)
  local d = nx * LX + ny * LY + nz * LZ
  return 0.4 + 0.6 * (d > 0.0 and d or 0.0)
end

-- ---------------------------------------------------------------------------
-- Mesh builder with baked directional shading
-- ---------------------------------------------------------------------------
local MeshBuilder = {}
MeshBuilder.__index = MeshBuilder

local function newMeshBuilder()
  return setmetatable({ positions = {}, normals = {}, texcoords = {}, colors = {}, n = 0, lit = true }, MeshBuilder)
end

function MeshBuilder:vertex(x, y, z, nx, ny, nz, r, g, b, a)
  local n = self.n
  local p, nm, t, c = self.positions, self.normals, self.texcoords, self.colors
  p[n + 1], p[n + 2], p[n + 3], p[n + 4] = x, y, z, 1.0
  nm[n + 1], nm[n + 2], nm[n + 3], nm[n + 4] = nx, ny, nz, 0.0
  t[n + 1], t[n + 2], t[n + 3], t[n + 4] = 0.0, 0.0, 1.0, 1.0
  c[n + 1], c[n + 2], c[n + 3], c[n + 4] = r, g, b, a
  self.n = n + 4
end

function MeshBuilder:tri(v0, v1, v2, nx, ny, nz, r, g, b, a)
  if self.lit then
    local l = faceLight(nx, ny, nz)
    r, g, b = clamp(r * l, 0.0, 1.0), clamp(g * l, 0.0, 1.0), clamp(b * l, 0.0, 1.0)
  end
  self:vertex(v0.x, v0.y, v0.z, nx, ny, nz, r, g, b, a)
  self:vertex(v1.x, v1.y, v1.z, nx, ny, nz, r, g, b, a)
  self:vertex(v2.x, v2.y, v2.z, nx, ny, nz, r, g, b, a)
end

function MeshBuilder:quad(v0, v1, v2, v3, nx, ny, nz, r, g, b, a)
  self:tri(v0, v1, v2, nx, ny, nz, r, g, b, a)
  self:tri(v0, v2, v3, nx, ny, nz, r, g, b, a)
end

function MeshBuilder:box(cx, cy, cz, sx, sy, sz, r, g, b, a)
  local hx, hy, hz = sx * 0.5, sy * 0.5, sz * 0.5
  local A = { x = cx - hx, y = cy - hy, z = cz - hz }
  local B = { x = cx + hx, y = cy - hy, z = cz - hz }
  local C = { x = cx + hx, y = cy + hy, z = cz - hz }
  local D = { x = cx - hx, y = cy + hy, z = cz - hz }
  local E = { x = cx - hx, y = cy - hy, z = cz + hz }
  local F = { x = cx + hx, y = cy - hy, z = cz + hz }
  local G = { x = cx + hx, y = cy + hy, z = cz + hz }
  local H = { x = cx - hx, y = cy + hy, z = cz + hz }

  self:quad(E, F, G, H, 0.0, 0.0, 1.0, r, g, b, a)
  self:quad(B, A, D, C, 0.0, 0.0, -1.0, r, g, b, a)
  self:quad(F, B, C, G, 1.0, 0.0, 0.0, r, g, b, a)
  self:quad(A, E, H, D, -1.0, 0.0, 0.0, r, g, b, a)
  self:quad(H, G, C, D, 0.0, 1.0, 0.0, r, g, b, a)
  self:quad(A, B, F, E, 0.0, -1.0, 0.0, r, g, b, a)
end

-- Hemisphere built on the XY disc (z = 0), bulging toward +Z -- billboarded to camera each frame.
local function sph(lat, lon)
  local cl = cos(lat)
  return { x = cl * cos(lon), y = cl * sin(lon), z = sin(lat) }
end

function MeshBuilder:dome(br, bg, bb, hr, hg, hb, rings, sectors)
  for i = 0, rings - 1 do
    local lat0 = (pi * 0.5 * i) / rings
    local lat1 = (pi * 0.5 * (i + 1)) / rings
    for j = 0, sectors - 1 do
      local lon0 = (pi * 2.0 * j) / sectors
      local lon1 = (pi * 2.0 * (j + 1)) / sectors

      local v00, v01, v10, v11 = sph(lat0, lon0), sph(lat0, lon1), sph(lat1, lon0), sph(lat1, lon1)

      -- average of the four corner colors (two rings, so two distinct values)
      local t0, t1 = sin(lat0), sin(lat1)
      local cr = (lerp(br, hr, t0) + lerp(br, hr, t1)) * 0.5
      local cg = (lerp(bg, hg, t0) + lerp(bg, hg, t1)) * 0.5
      local cb = (lerp(bb, hb, t0) + lerp(bb, hb, t1)) * 0.5

      local n0 = normalize3(v00.x, v00.y, v00.z)
      local n1 = normalize3(v11.x, v11.y, v11.z)

      self:tri(v00, v01, v11, n0.x, n0.y, n0.z, cr, cg, cb, 1.0)
      self:tri(v00, v11, v10, n1.x, n1.y, n1.z, cr, cg, cb, 1.0)
    end
  end
end

function MeshBuilder:voxelFace(cx, cy, cz, hs, face, r, g, b, a)
  if face == 0 then
    self:quad({ x = cx + hs, y = cy - hs, z = cz - hs }, { x = cx + hs, y = cy + hs, z = cz - hs },
              { x = cx + hs, y = cy + hs, z = cz + hs }, { x = cx + hs, y = cy - hs, z = cz + hs },
              1.0, 0.0, 0.0, r, g, b, a)
  elseif face == 1 then
    self:quad({ x = cx - hs, y = cy + hs, z = cz - hs }, { x = cx - hs, y = cy - hs, z = cz - hs },
              { x = cx - hs, y = cy - hs, z = cz + hs }, { x = cx - hs, y = cy + hs, z = cz + hs },
              -1.0, 0.0, 0.0, r, g, b, a)
  elseif face == 2 then
    self:quad({ x = cx - hs, y = cy + hs, z = cz - hs }, { x = cx + hs, y = cy + hs, z = cz - hs },
              { x = cx + hs, y = cy + hs, z = cz + hs }, { x = cx - hs, y = cy + hs, z = cz + hs },
              0.0, 1.0, 0.0, r, g, b, a)
  elseif face == 3 then
    self:quad({ x = cx + hs, y = cy - hs, z = cz - hs }, { x = cx - hs, y = cy - hs, z = cz - hs },
              { x = cx - hs, y = cy - hs, z = cz + hs }, { x = cx + hs, y = cy - hs, z = cz + hs },
              0.0, -1.0, 0.0, r, g, b, a)
  elseif face == 4 then
    self:quad({ x = cx - hs, y = cy - hs, z = cz + hs }, { x = cx + hs, y = cy - hs, z = cz + hs },
              { x = cx + hs, y = cy + hs, z = cz + hs }, { x = cx - hs, y = cy + hs, z = cz + hs },
              0.0, 0.0, 1.0, r, g, b, a)
  else
    self:quad({ x = cx + hs, y = cy - hs, z = cz - hs }, { x = cx - hs, y = cy - hs, z = cz - hs },
              { x = cx - hs, y = cy + hs, z = cz - hs }, { x = cx + hs, y = cy + hs, z = cz - hs },
              0.0, 0.0, -1.0, r, g, b, a)
  end
end

local function newMeshData(b, culling)
  local zero = Render.materialColor(0.0, 0.0, 0.0)
  local material = Render.material(zero, zero, zero, zero, zero, 16.0, 1.0, zero, 0.0, -1, -1, -1, -1)
  local vertList = Render.vertexList(b.positions, b.normals, b.texcoords, b.colors,
                                     { material }, { Render.materialIndex(0, b.n // 4) })
  local data = RenderData(vertList)
  data:updateMaterial(0, { diffuse = { r = 0.0, g = 0.0, b = 0.0 } })
  data.pipeline = Render.PL_NO_LIGHTS
  data.face_culling = culling or Render.CULL_FACE_NONE
  return data
end

local function addBoxes(b, boxes, col)
  for _, bx in ipairs(boxes) do
    local c = bx.accent ~= 0 and col.accent or col.primary
    b:box(bx.cx, bx.cy, bx.cz, bx.sx, bx.sy, bx.sz, c[1], c[2], c[3], 1.0)
  end
end

local function buildShipMesh(loadout)
  local hull = SHIPS[loadout.ship] or SHIPS.scout
  local col = findColor(loadout.color)
  local b = newMeshBuilder()
  addBoxes(b, hull.body, col)
  for _, slot in ipairs(PART_SLOTS) do
    local p = findPart(slot, loadout[slot])
    if p.mesh and #p.mesh > 0 then addBoxes(b, p.mesh, col) end
  end
  return newMeshData(b)
end

-- Every built ship RenderObject is cached by loadout signature, so swapping
-- parts back and forth in the hangar does not rebuild meshes.
local shipCache = {}
local ship = nil

local function loadoutKey(l)
  return table.concat({ l.ship, l.wing, l.engine, l.cannon, l.nose, l.color }, "|")
end

local function rebuildShip()
  local key = loadoutKey(save.loadout)
  local obj = shipCache[key]
  if not obj then
    obj = RenderObject(buildShipMesh(save.loadout))
    shipCache[key] = obj
  end
  ship = obj
end

-- ---------------------------------------------------------------------------
-- Particles (declared early: voxel carving spawns debris)
-- ---------------------------------------------------------------------------
-- Struct-of-arrays, 1-based: a particle is alive while pLife[i] > 0.
local pX, pY, pZ, pVX, pVY, pVZ, pLife, pSc, pKind = {}, {}, {}, {}, {}, {}, {}, {}, {}
local particles = {}

local function spawnParticle(kind, x, y, z, vx, vy, vz, life, sc, spin)
  for i = 1, MAX_PARTICLES do
    if pLife[i] <= 0 and pKind[i] == kind then
      pX[i], pY[i], pZ[i] = x, y, z
      pVX[i], pVY[i], pVZ[i] = vx, vy, vz
      pLife[i], pSc[i] = life, sc
      local p = particles[i]
      p.maxLife = life
      p.spin = spin or randomf(-0.4, 0.4)
      return true
    end
  end
  return false
end

local function spawnVoxelDebris(x, y, z)
  local ang = randomf(0.0, 6.28)
  local spd = randomf(0.06, 0.22)
  spawnParticle(2,
    x + randomf(-0.06, 0.06), y + randomf(-0.06, 0.06), z + randomf(-0.06, 0.06),
    cos(ang) * spd, randomf(0.02, 0.14), sin(ang) * spd,
    randomi(12, 24), randomf(0.25, 0.55))
end

-- ---------------------------------------------------------------------------
-- Voxel asteroids -- procedural grid, mesh rebuilt when voxels are carved away
-- ---------------------------------------------------------------------------
local VoxelAsteroid = {}
do
local G, GG = VOXEL_GRID, VOXEL_GRID * VOXEL_GRID
local HALF = VOXEL_GRID * VOXEL_CELL * 0.5
local CORNER = VOXEL_CELL * 0.866

-- Squared distance from a voxel cell's centre to a point in the asteroid's local space.
local function voxelD2(ix, iy, iz, lx, ly, lz)
  local dx = (ix + 0.5) * VOXEL_CELL - HALF - lx
  local dy = (iy + 0.5) * VOXEL_CELL - HALF - ly
  local dz = (iz + 0.5) * VOXEL_CELL - HALF - lz
  return dx * dx + dy * dy + dz * dz
end

-- Radius one cell contributes to the collision sphere.
local function voxelRadius(ix, iy, iz)
  local dx = (ix + 0.5) * VOXEL_CELL - HALF
  local dy = (iy + 0.5) * VOXEL_CELL - HALF
  local dz = (iz + 0.5) * VOXEL_CELL - HALF
  return sqrt(dx * dx + dy * dy + dz * dz) + CORNER
end

VoxelAsteroid.__index = VoxelAsteroid

local DIRS = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } }

-- grid coords are 0-based like the JS version; the table slot is idx + 1
local function vidx(x, y, z) return x + y * G + z * GG + 1 end

function VoxelAsteroid.new(seed)
  local self = setmetatable({ seed = seed, grid = {}, voxelCount = 0, meshData = nil }, VoxelAsteroid)
  self:reset(seed)
  return self
end

function VoxelAsteroid:get(x, y, z)
  if x < 0 or y < 0 or z < 0 or x >= G or y >= G or z >= G then return 0 end
  return self.grid[vidx(x, y, z)]
end

function VoxelAsteroid:set(x, y, z, v)
  if x < 0 or y < 0 or z < 0 or x >= G or y >= G or z >= G then return end
  local i = vidx(x, y, z)
  local cur = self.grid[i]
  if v ~= 0 and cur == 0 then self.voxelCount = self.voxelCount + 1
  elseif v == 0 and cur ~= 0 then self.voxelCount = self.voxelCount - 1 end
  self.grid[i] = v ~= 0 and 1 or 0
end

function VoxelAsteroid:reset(seed)
  self.seed = seed
  local grid = self.grid
  for i = 1, G * GG do grid[i] = 0 end
  self.voxelCount = 0

  local c = G // 2
  local target = 6 + (voxelHash(c, c, c, seed) % (VOXEL_MAX - 5))
  self:set(c, c, c, 1)

  local fails = 0
  while self.voxelCount < target and fails < 80 do
    local pick = voxelHash(seed, self.voxelCount, fails, 0) % self.voxelCount
    local ox, oy, oz
    for z = 0, G - 1 do
      for y = 0, G - 1 do
        for x = 0, G - 1 do
          if grid[vidx(x, y, z)] ~= 0 then
            if pick == 0 then ox, oy, oz = x, y, z break end
            pick = pick - 1
          end
        end
        if ox then break end
      end
      if ox then break end
    end
    if not ox then break end

    local d = DIRS[voxelHash(ox, oy, oz, seed + fails) % #DIRS + 1]
    local nx, ny, nz = ox + d[1], oy + d[2], oz + d[3]

    if self:get(nx, ny, nz) ~= 0 or nx < 0 or ny < 0 or nz < 0 or nx >= G or ny >= G or nz >= G then
      fails = fails + 1
    else
      self:set(nx, ny, nz, 1)
      fails = 0
    end
  end
  self:rebuildMesh()
end

function VoxelAsteroid:collisionRadius()
  if self.voxelCount <= 0 then return 0.0 end
  local maxDist, grid = 0.0, self.grid
  for z = 0, G - 1 do
    for y = 0, G - 1 do
      for x = 0, G - 1 do
        if grid[vidx(x, y, z)] ~= 0 then
          local d = voxelRadius(x, y, z)
          if d > maxDist then maxDist = d end
        end
      end
    end
  end
  return maxDist
end

function VoxelAsteroid:localFromGrid(ix, iy, iz)
  return (ix + 0.5) * VOXEL_CELL - HALF, (iy + 0.5) * VOXEL_CELL - HALF, (iz + 0.5) * VOXEL_CELL - HALF
end

function VoxelAsteroid:buildMeshBuilder()
  local b = newMeshBuilder()
  local hs = VOXEL_CELL * 0.5
  local grid, seed = self.grid, self.seed

  for z = 0, G - 1 do
    local zp = (z + 0.5) * VOXEL_CELL - HALF
    for y = 0, G - 1 do
      local yp = (y + 0.5) * VOXEL_CELL - HALF
      local row = z * GG + y * G + 1
      for x = 0, G - 1 do
        if grid[row + x] ~= 0 then
          local xp = (x + 0.5) * VOXEL_CELL - HALF
          local r, g, bl = voxelColor(x, y, z, seed)

          if x + 1 >= G or grid[row + x + 1] == 0 then b:voxelFace(xp, yp, zp, hs, 0, r, g, bl, 1.0) end
          if x - 1 < 0 or grid[row + x - 1] == 0 then b:voxelFace(xp, yp, zp, hs, 1, r, g, bl, 1.0) end
          if y + 1 >= G or grid[row + G + x] == 0 then b:voxelFace(xp, yp, zp, hs, 2, r, g, bl, 1.0) end
          if y - 1 < 0 or grid[row - G + x] == 0 then b:voxelFace(xp, yp, zp, hs, 3, r, g, bl, 1.0) end
          if z + 1 >= G or grid[row + GG + x] == 0 then b:voxelFace(xp, yp, zp, hs, 4, r, g, bl, 1.0) end
          if z - 1 < 0 or grid[row - GG + x] == 0 then b:voxelFace(xp, yp, zp, hs, 5, r, g, bl, 1.0) end
        end
      end
    end
  end
  return b
end

function VoxelAsteroid:rebuildMesh()
  local b = self:buildMeshBuilder()
  if not self.meshData then
    self.meshData = newMeshData(b)
    return
  end
  self.meshData.vertices = { positions = b.positions, normals = b.normals, texcoords = b.texcoords, colors = b.colors }
  self.meshData.material_indices = { Render.materialIndex(0, b.n // 4) }
end

local function removeAt(self, x, y, z, ex, ey, ez, sc, rx, ry, rz, debris)
  if self:get(x, y, z) == 0 then return 0 end
  if debris.n < 6 then
    local lx, ly, lz = self:localFromGrid(x, y, z)
    local world = localToWorld(lx, ly, lz, ex, ey, ez, sc, rx, ry, rz)
    spawnVoxelDebris(world.x, world.y, world.z)
    debris.n = debris.n + 1
  end
  self:set(x, y, z, 0)
  return 1
end

-- nearest live cell to a local-space point
function VoxelAsteroid:nearest(lx, ly, lz)
  local bx, by, bz, bestD2 = nil, nil, nil, 1e9
  local grid = self.grid
  for z = 0, G - 1 do
    for y = 0, G - 1 do
      for x = 0, G - 1 do
        if grid[vidx(x, y, z)] ~= 0 then
          local d2 = voxelD2(x, y, z, lx, ly, lz)
          if d2 < bestD2 then bx, by, bz, bestD2 = x, y, z, d2 end
        end
      end
    end
  end
  return bx, by, bz
end

-- A shot takes the cell(s) it lands on (blast radius below one cell spacing),
-- plus dmg-1 more nearest cells, so a damage-N shot removes about N voxels.
function VoxelAsteroid:carveWorld(wx, wy, wz, ex, ey, ez, sc, rx, ry, rz, dmg)
  dmg = dmg or 1
  local l = worldToLocal(wx, wy, wz, ex, ey, ez, sc, rx, ry, rz)
  local hitSlop2 = VOXEL_CELL * VOXEL_CELL * 0.8
  local removed = 0
  local debris = { n = 0 }

  local bx, by, bz = self:nearest(l.x, l.y, l.z)
  if not bx then return 0 end

  local grid = self.grid
  for z = 0, G - 1 do
    for y = 0, G - 1 do
      for x = 0, G - 1 do
        if grid[vidx(x, y, z)] ~= 0 and voxelD2(x, y, z, l.x, l.y, l.z) <= hitSlop2 then
          removed = removed + removeAt(self, x, y, z, ex, ey, ez, sc, rx, ry, rz, debris)
        end
      end
    end
  end

  if removed == 0 then
    removed = removed + removeAt(self, bx, by, bz, ex, ey, ez, sc, rx, ry, rz, debris)
  end

  local extra = 1
  while extra < dmg and self.voxelCount > VOXEL_DESTROY_AT do
    local nx, ny, nz = self:nearest(l.x, l.y, l.z)
    if not nx then break end
    removed = removed + removeAt(self, nx, ny, nz, ex, ey, ez, sc, rx, ry, rz, debris)
    extra = extra + 1
  end

  if removed > 0 then self:rebuildMesh() end
  return removed
end
end

-- ---------------------------------------------------------------------------
-- Meshes
-- ---------------------------------------------------------------------------
-- (scoped in a do-block: a Lua function, the main chunk included, is capped
-- at 200 locals)
local thrusterMesh, enemyMesh, bulletMesh, explosionMesh, starMesh, particleMeshes, buildPlanetDome
do
local function unlit()
  local b = newMeshBuilder()
  b.lit = false
  return b
end

local function buildThruster()
  local b = unlit()
  b:box(0.0, 0.0, 0.0, 0.24, 0.18, 0.5, 1.0, 0.6, 0.15, 1.0)
  b:box(0.0, 0.0, -0.28, 0.12, 0.1, 0.4, 1.0, 0.9, 0.5, 1.0)
  return newMeshData(b)
end

local function buildEnemy()
  local b = newMeshBuilder()
  b:box(0.0, 0.0, 0.0, 0.85, 0.42, 0.9, 0.85, 0.2, 0.18, 1.0)     -- body
  b:box(0.0, 0.0, -0.5, 0.32, 0.32, 0.45, 1.0, 0.45, 0.15, 1.0)   -- front intake
  b:box(0.0, 0.14, 0.35, 0.22, 0.16, 0.4, 0.55, 0.12, 0.12, 1.0)  -- hump
  b:box(-0.85, 0.0, 0.0, 0.7, 0.08, 0.5, 0.72, 0.18, 0.16, 1.0)   -- left wing
  b:box(0.85, 0.0, 0.0, 0.7, 0.08, 0.5, 0.72, 0.18, 0.16, 1.0)    -- right wing
  b:box(-1.2, 0.0, 0.0, 0.22, 0.28, 0.3, 0.95, 0.75, 0.15, 1.0)   -- left tip
  b:box(1.2, 0.0, 0.0, 0.22, 0.28, 0.3, 0.95, 0.75, 0.15, 1.0)    -- right tip
  return newMeshData(b)
end

local function buildBullet()
  local b = unlit()
  b:box(0.0, 0.0, 0.0, 0.12, 0.12, 1.1, 0.35, 1.0, 0.55, 1.0)
  b:box(0.0, 0.0, 0.0, 0.05, 0.05, 0.5, 0.9, 1.0, 0.9, 1.0)
  return newMeshData(b)
end

local function buildExplosion()
  local b = unlit()
  b:box(0.0, 0.0, 0.0, 0.28, 0.28, 0.28, 1.0, 0.98, 0.88, 1.0)
  b:box(0.0, 0.0, 0.0, 0.62, 0.62, 0.62, 1.0, 0.82, 0.18, 1.0)
  b:box(0.0, 0.0, 0.0, 0.95, 0.95, 0.95, 0.85, 0.28, 0.08, 1.0)
  return newMeshData(b)
end

local function buildStar()
  local b = unlit()
  b:box(0.0, 0.0, 0.0, 0.42, 0.42, 0.42, 1.0, 1.0, 1.0, 1.0)
  return newMeshData(b)
end

function buildPlanetDome(p)
  local b = unlit()
  b:dome(p.br, p.bg, p.bb, p.hr, p.hg, p.hb, 10, 18)
  return newMeshData(b)
end

local function buildParticle(kind)
  local b = unlit()
  if kind == "boost" then
    b:box(0.0, 0.0, 0.0, 0.06, 0.06, 0.35, 1.0, 0.72, 0.18, 1.0)
    b:box(0.0, 0.0, -0.12, 0.03, 0.03, 0.22, 1.0, 0.95, 0.55, 1.0)
  elseif kind == "spark" then
    b:box(0.0, 0.0, 0.0, 0.025, 0.025, 0.42, 1.0, 0.95, 0.55, 1.0)
    b:box(0.0, 0.0, 0.0, 0.012, 0.012, 0.65, 1.0, 1.0, 0.85, 1.0)
  elseif kind == "flash" then
    b:box(0.0, 0.0, 0.0, 0.07, 0.07, 0.07, 1.0, 0.98, 0.82, 1.0)
    b:box(0.0, 0.0, 0.0, 0.03, 0.03, 0.28, 1.0, 1.0, 0.92, 1.0)
  elseif kind == "smoke" then
    b:box(0.0, 0.0, 0.0, 0.09, 0.09, 0.09, 0.35, 0.32, 0.38, 1.0)
    b:box(0.0, 0.0, 0.0, 0.06, 0.06, 0.06, 0.22, 0.20, 0.24, 1.0)
  else
    b:box(0.0, 0.0, 0.0, 0.06, 0.06, 0.06, 1.0, 0.62, 0.18, 1.0)
    b:box(0.0, 0.0, 0.0, 0.04, 0.04, 0.04, 0.85, 0.45, 0.12, 1.0)
  end
  return newMeshData(b)
end

thrusterMesh = buildThruster()
enemyMesh = buildEnemy()
bulletMesh = buildBullet()
explosionMesh = buildExplosion()
starMesh = buildStar()
-- indexed by particle kind + 1: boost, spark, ember, flash, smoke
particleMeshes = { buildParticle("boost"), buildParticle("spark"), buildParticle("ember"),
                   buildParticle("flash"), buildParticle("smoke") }
end

save = loadSave()

local planetDefs = {
  { br = 0.22, bg = 0.16, bb = 0.42, hr = 0.55, hg = 0.42, hb = 0.82, x = -34.0, y = 3.5, z = 165.0, sc = 2.4 },
  { br = 0.18, bg = 0.22, bb = 0.38, hr = 0.38, hg = 0.48, hb = 0.72, x = 38.0, y = 11.0, z = 235.0, sc = 1.8 },
  { br = 0.32, bg = 0.14, bb = 0.12, hr = 0.72, hg = 0.38, hb = 0.22, x = 14.0, y = 14.5, z = 145.0, sc = 1.4 },
  { br = 0.12, bg = 0.18, bb = 0.28, hr = 0.28, hg = 0.36, hb = 0.58, x = -22.0, y = 7.5, z = 205.0, sc = 1.1 },
}

-- ---------------------------------------------------------------------------
-- Scene objects (RenderObjects are reused; positions are ASSIGNED each frame)
-- ---------------------------------------------------------------------------
rebuildShip()
local thruster = RenderObject(thrusterMesh)

-- Distant starfield: far away, drifts laterally (parallax) instead of flying past the camera.
local stars = {}
for i = 1, STAR_COUNT do
  stars[i] = {
    obj = RenderObject(starMesh),
    x = randomf(-STAR_SPREAD_X, STAR_SPREAD_X),
    y = randomf(-STAR_SPREAD_Y, STAR_SPREAD_Y),
    z = randomf(STAR_Z_NEAR, STAR_Z_FAR),
    par = randomf(0.04, 0.16),
    sc = randomf(0.35, 0.95),
  }
end

local planets = {}
for i = 1, MAX_PLANETS do
  local def = planetDefs[i]
  planets[i] = { obj = RenderObject(buildPlanetDome(def)), x = def.x, y = def.y, z = def.z, sc = def.sc }
end

-- Slots 1-18 boost, 19-40 spark, 41-54 ember, 55-66 flash, 67-72 smoke.
for i = 1, MAX_PARTICLES do
  local kind = 0
  if i > 66 then kind = 4 elseif i > 54 then kind = 3 elseif i > 40 then kind = 2 elseif i > 18 then kind = 1 end
  pX[i], pY[i], pZ[i], pVX[i], pVY[i], pVZ[i], pLife[i], pSc[i], pKind[i] = 0, 0, 0, 0, 0, 0, 0, 0, kind
  particles[i] = { obj = RenderObject(particleMeshes[kind + 1]), maxLife = 1, spin = 0.0 }
end

-- Per-kind drag, gravity and the scale each kind eases toward.
local function stepParticles()
  for i = 1, MAX_PARTICLES do
    local life = pLife[i]
    if life > 0 then
      pLife[i] = life - 1
      pX[i], pY[i], pZ[i] = pX[i] + pVX[i], pY[i] + pVY[i], pZ[i] + pVZ[i]

      local k = pKind[i]
      if k == 0 then
        pVZ[i] = pVZ[i] - 0.02
        pSc[i] = pSc[i] + (0.15 - pSc[i]) * 0.12
      elseif k == 1 then
        pVX[i], pVY[i], pVZ[i] = pVX[i] * 0.90, pVY[i] * 0.90 - 0.008, pVZ[i] * 0.90
        pSc[i] = pSc[i] + (0.08 - pSc[i]) * 0.10
      elseif k == 2 then
        pVX[i], pVY[i], pVZ[i] = pVX[i] * 0.94, pVY[i] * 0.94 - 0.012, pVZ[i] * 0.94
        pSc[i] = pSc[i] + (0.04 - pSc[i]) * 0.06
      elseif k == 3 then
        pVX[i], pVY[i], pVZ[i] = pVX[i] * 0.86, pVY[i] * 0.86, pVZ[i] * 0.86
        pSc[i] = pSc[i] + (0.02 - pSc[i]) * 0.18
      else
        pVX[i], pVY[i], pVZ[i] = pVX[i] * 0.96, pVY[i] * 0.96 + 0.006, pVZ[i] * 0.96
        pSc[i] = pSc[i] + (1.6 - pSc[i]) * 0.04
      end
    end
  end
end

local bullets = {}
for i = 1, MAX_BULLETS do
  bullets[i] = { x = 0.0, y = 0.0, z = 0.0, pz = 0.0, damage = 1, alive = false, obj = RenderObject(bulletMesh) }
end

local function stepBullets()
  for i = 1, MAX_BULLETS do
    local b = bullets[i]
    if b.alive then
      b.pz = b.z
      b.z = b.z + 3.4
      if b.z > 135.0 then b.alive = false end
    end
  end
end

local enemyPool = {}
for i = 1, MAX_ENEMIES do
  local voxel = VoxelAsteroid.new(1000 + (i - 1) * 7919)
  enemyPool[i] = {
    objFighter = RenderObject(enemyMesh),
    objRock = RenderObject(voxel.meshData),
    voxel = voxel,
    active = false,
    type = 0, x = 0, y = 0, z = 0, rx = 0, ry = 0, rz = 0,
    spin = 0, weave = 0, phase = 0, hp = 1, radius = 0.9, sc = 1.0,
  }
end

local MAX_FX = 12
local fx = {}
for i = 1, MAX_FX do
  fx[i] = { obj = RenderObject(explosionMesh), alive = false, x = 0, y = 0, z = 0, life = 0, maxLife = 1, baseSc = 1.0, spin = 0.0 }
end

-- ---------------------------------------------------------------------------
-- Game state
-- ---------------------------------------------------------------------------
local state = STATE_MENU
local score = 0
local lives = 3
local wave = 1
local fireTimer = 0
local spawnTimer = 60
local invuln = 0
local fireSide = 1
local shake = 0.0
local frame = 0
local menuSpin = 0.0
local runStats = computeStats(save)
local runEarned = 0
local hangTab = TAB_SHIPS
local hangSel = 1
local partSlot = 1
local hangNavLock = 0

local shipX, shipY = 0.0, 0.0
local velX, velY = 0.0, 0.0
local roll, pitch = 0.0, 0.0
local speed = BASE_SPEED

local pad = Pads.get()

-- Camera.target() pans by delta each call -- do NOT invoke it every frame.
local CAM_X, CAM_Y, CAM_Z = 0.0, 1.4, -9.0
local LOOK_X, LOOK_Y, LOOK_Z = 0.0, 0.0, 24.0

local function stick(v)
  if v > -28 and v < 28 then return 0.0 end
  return clamp(v / 128.0, -1.0, 1.0)
end

local function resetGame()
  runStats = computeStats(save)
  score = 0
  lives = runStats.lives
  wave = 1
  fireTimer = 0
  spawnTimer = 45
  invuln = 0
  shake = 0.0
  shipX, shipY = 0.0, 0.0
  velX, velY = 0.0, 0.0
  roll, pitch = 0.0, 0.0
  speed = BASE_SPEED * runStats.speed
  runEarned = 0

  for i = 1, MAX_BULLETS do bullets[i].alive = false end
  for i = 1, MAX_ENEMIES do enemyPool[i].active = false end
  for i = 1, MAX_FX do fx[i].alive = false end
  for i = 1, MAX_PARTICLES do pLife[i] = 0 end
  for _, s in ipairs(stars) do
    s.x = randomf(-STAR_SPREAD_X, STAR_SPREAD_X)
    s.y = randomf(-STAR_SPREAD_Y, STAR_SPREAD_Y)
    s.z = randomf(STAR_Z_NEAR, STAR_Z_FAR)
  end
  for i, p in ipairs(planets) do
    local def = planetDefs[i]
    p.x, p.y, p.z = def.x, def.y, def.z
  end
end

local function spawnEnemy()
  for _, e in ipairs(enemyPool) do
    if not e.active then
      e.active = true
      e.type = randomi(0, 100) < 32 and 1 or 0
      e.x = randomf(-BOUNDS_X, BOUNDS_X)
      e.y = randomf(BOUNDS_Y_LOW + 0.5, BOUNDS_Y_HIGH - 0.5)
      e.z = randomf(95.0, 130.0)
      e.rx = randomf(0.0, 6.28)
      e.ry = randomf(0.0, 6.28)
      e.rz = randomf(0.0, 6.28)
      e.spin = randomf(-0.05, 0.05)
      e.weave = randomf(0.008, 0.03)
      e.phase = randomf(0.0, 6.28)
      e.hp = 1
      if e.type == 1 then
        e.sc = randomf(0.85, 1.25)
        e.voxel:reset(randomi(1, 999999))
        e.radius = e.voxel:collisionRadius() * e.sc
      else
        e.radius = 0.9
        e.sc = 1.0
      end
      return
    end
  end
end

local function awardRunCredits()
  runEarned = score // 10 + wave * 25
  save.credits = save.credits + runEarned
  if score > save.best then save.best = score end
  saveGame()
end

local function fireBullet()
  local n = runStats.bullets
  local spread = runStats.spread
  for k = 0, n - 1 do
    local t = n == 1 and 0.5 or k / (n - 1)
    local off = (t - 0.5) * spread * 2.0
    local wx = (fireSide > 0 and 1.6 or -1.6) + off
    for i = 1, MAX_BULLETS do
      local b = bullets[i]
      if not b.alive then
        b.alive = true
        b.x, b.y, b.z, b.pz = shipX + wx, shipY, 1.2, 1.2
        b.damage = runStats.damage
        break
      end
    end
  end
  fireSide = -fireSide
end

local function burstDir(spd)
  local yaw = randomf(0.0, 6.283185)
  local p = randomf(-0.95, 0.95)
  local cp = cos(p)
  return cos(yaw) * cp * spd, sin(p) * spd, sin(yaw) * cp * spd
end

local function jitter(r) return randomf(-r, r) end

local function spawnExplosionParticles(x, y, z, big)
  for _ = 1, big and 8 or 4 do
    local dx, dy, dz = burstDir(randomf(0.04, big and 0.18 or 0.12))
    spawnParticle(3, x + jitter(0.08), y + jitter(0.08), z + jitter(0.08), dx, dy, dz,
                  randomi(6, 12), randomf(0.7, 1.3))
  end

  for _ = 1, big and 18 or 10 do
    local dx, dy, dz = burstDir(randomf(big and 0.22 or 0.14, big and 0.58 or 0.38))
    spawnParticle(1, x + jitter(0.12), y + jitter(0.12), z + jitter(0.12), dx, dy, dz,
                  randomi(14, big and 28 or 22), randomf(0.45, 1.0))
  end

  for _ = 1, big and 10 or 6 do
    local dx, dy, dz = burstDir(randomf(0.08, big and 0.32 or 0.22))
    dy = dy + randomf(0.02, 0.12)
    spawnParticle(2, x + jitter(0.16), y + jitter(0.16), z + jitter(0.16), dx, dy, dz,
                  randomi(18, big and 36 or 28), randomf(0.35, 0.85))
  end

  for _ = 1, big and 6 or 3 do
    local dx, dy, dz = burstDir(randomf(0.03, big and 0.14 or 0.09))
    dy = dy + randomf(0.04, 0.16)
    spawnParticle(4, x + jitter(0.1), y + jitter(0.1), z + jitter(0.1), dx, dy * 0.6, dz,
                  randomi(22, big and 40 or 32), randomf(0.55, big and 1.4 or 1.0))
  end
end

local function spawnFx(x, y, z, big)
  for _, f in ipairs(fx) do
    if not f.alive then
      f.alive = true
      f.x, f.y, f.z = x, y, z
      f.maxLife = big and 30 or 20
      f.life = f.maxLife
      f.baseSc = big and 0.45 or 0.28
      f.spin = randomf(-0.25, 0.25)
      spawnExplosionParticles(x, y, z, big)
      if big then shake = max(shake, 0.55) end
      return
    end
  end
end

-- The ship pivots about its own origin (z = 0), so anything mounted down the
-- tail sweeps an arc in Y as the ship pitches: (0,0,-d) -> (d*sin, -d*cos).
local function engineMount(d)
  return d * sin(pitch), -d * cos(pitch)
end

local function spawnBoostParticles(sx, sy, sz, boostPower)
  for _ = 1, boostPower > 0.5 and 3 or 1 do
    spawnParticle(0,
      sx + jitter(0.12), sy + jitter(0.08), sz + jitter(0.05),
      jitter(0.04), jitter(0.03), -randomf(0.35, 0.75) - boostPower * 0.4,
      randomi(8, 16), randomf(0.5, 1.1) + boostPower * 0.5)
  end
end

local function hit(ax, ay, az, ar, bx, by, bz, br)
  local dx, dy, dz, rr = ax - bx, ay - by, az - bz, ar + br
  return dx * dx + dy * dy + dz * dz < rr * rr
end

-- Sweeps the bullet's last step in 6 sub-steps so fast shots can't tunnel
-- through small asteroids; returns the hit z or -1.
local function hitBulletAlongZ(bx, by, bz, prevZ, rr, ex, ey, ez)
  local dx, dy = bx - ex, by - ey
  local rr2 = rr * rr
  for s = 0, 6 do
    local z = s == 6 and bz or lerp(prevZ, bz, s / 6)
    local dz = z - ez
    if dx * dx + dy * dy + dz * dz < rr2 then return z end
  end
  return -1.0
end

local function fxScale(life, maxLife, baseSc)
  local prog = 1.0 - life / maxLife
  local pop = sin(prog * 3.141592)
  return baseSc * (0.35 + pop * 2.4) * (1.0 - prog * 0.35)
end

-- Planet billboards: rotate a +Z-facing hemisphere so its pole looks at the camera.
local function billboardYaw(px, pz, cx, cz)
  return atan(cx - px, cz - pz)
end

local function billboardPitch(px, py, pz, cx, cy, cz)
  local dx, dy, dz = cx - px, cy - py, cz - pz
  return atan(-dy, sqrt(dx * dx + dz * dz))
end

-- ---------------------------------------------------------------------------
-- Update
-- ---------------------------------------------------------------------------
local function updateWorld(scrollSpeed)
  for _, s in ipairs(stars) do
    s.x = s.x - scrollSpeed * s.par
    if s.x < -STAR_SPREAD_X then
      s.x = s.x + STAR_SPREAD_X * 2.0
      s.y = randomf(-STAR_SPREAD_Y, STAR_SPREAD_Y)
      s.z = randomf(STAR_Z_NEAR, STAR_Z_FAR)
    end
  end
  for i, p in ipairs(planets) do
    p.z = p.z - scrollSpeed * 0.35
    if p.z < 60.0 then
      local def = planetDefs[i]
      p.z = p.z + 260.0
      p.x = def.x + randomf(-8.0, 8.0)
      p.y = def.y + randomf(-2.5, 2.5)
    end
  end
end

local function updatePlay()
  local ix = stick(pad.lx)
  local iy = stick(pad.ly)
  local boosting = pad:pressed(Pads.R2)
  local braking = pad:pressed(Pads.L2)

  local targetSpeed = BASE_SPEED * runStats.speed + wave * 0.03
  if boosting then targetSpeed = targetSpeed * 2.1
  elseif braking then targetSpeed = targetSpeed * 0.45 end
  speed = lerp(speed, targetSpeed, 0.08)

  velX = lerp(velX, -ix * SHIP_ACCEL, 0.35)
  velY = lerp(velY, iy * SHIP_ACCEL * 0.85, 0.35)
  shipX = clamp(shipX + velX, -BOUNDS_X, BOUNDS_X)
  shipY = clamp(shipY + velY, BOUNDS_Y_LOW, BOUNDS_Y_HIGH)

  roll = lerp(roll, ix * 0.7, 0.2)
  pitch = lerp(pitch, iy * 0.35, 0.2)

  if fireTimer > 0 then fireTimer = fireTimer - 1 end
  if (pad:pressed(Pads.CROSS) or pad:pressed(Pads.R1)) and fireTimer == 0 then
    fireBullet()
    fireTimer = runStats.cooldown
  end

  updateWorld(speed)

  local boostPower = clamp((speed - BASE_SPEED) / (BASE_SPEED * 1.2), 0.0, 1.0)
  if boosting and state == STATE_PLAY then
    local my, mz = engineMount(1.35)
    spawnBoostParticles(shipX, shipY + my, mz, boostPower)
  end

  spawnTimer = spawnTimer - 1
  if spawnTimer <= 0 then
    spawnEnemy()
    spawnTimer = max(14, 42 - wave * 2)
  end
  if frame % 900 == 0 and frame > 0 then wave = wave + 1 end

  stepBullets()

  for _, e in ipairs(enemyPool) do
    if e.active then
      e.z = e.z - (speed * (e.type == 1 and 0.7 or 1.0) + 0.15)
      e.phase = e.phase + e.weave
      e.x = e.x + sin(e.phase) * 0.04
      e.ry = e.ry + e.spin
      e.rx = e.rx + e.spin * 0.5

      if e.z < -8.0 then
        e.active = false
      elseif invuln <= 0 and hit(shipX, shipY, 0.0, 0.8, e.x, e.y, e.z, e.radius) then
        e.active = false
        lives = lives - 1
        invuln = 90
        shake = 1.0
        spawnFx(shipX, shipY, 0.5, true)
        if lives <= 0 then
          awardRunCredits()
          state = STATE_OVER
          return
        end
      else
        for j = 1, MAX_BULLETS do
          local bd = bullets[j]
          if bd.alive then
            local bx, by, bz = bd.x, bd.y, bd.z
            local hitZ = -1.0

            if e.type == 1 then
              hitZ = hitBulletAlongZ(bx, by, bz, bd.pz, 0.55 + e.radius + 0.25, e.x, e.y, e.z)
            elseif hit(bx, by, bz, 0.35, e.x, e.y, e.z, e.radius) then
              hitZ = bz
            end

            if hitZ >= 0.0 then
              bz = hitZ
              local bulletDamage = bd.damage
              bd.alive = false

              if e.type == 1 then
                local removed = e.voxel:carveWorld(bx, by, bz, e.x, e.y, e.z, e.sc, e.rx, e.ry, e.rz, bulletDamage)
                spawnFx(bx, by, bz, false)
                score = score + removed * 8
                e.radius = e.voxel:collisionRadius() * e.sc

                if e.voxel.voxelCount <= VOXEL_DESTROY_AT then
                  e.active = false
                  spawnFx(e.x, e.y, e.z, true)
                  score = score + 60
                end
              else
                e.hp = e.hp - bulletDamage
                if e.hp <= 0 then
                  e.active = false
                  spawnFx(e.x, e.y, e.z, false)
                  score = score + 100
                else
                  spawnFx(bx, by, bz, false)
                end
              end
              break
            end
          end
        end
      end
    end
  end

  for _, f in ipairs(fx) do
    if f.alive then
      f.life = f.life - 1
      if f.life <= 0 then f.alive = false end
    end
  end

  stepParticles()

  if invuln > 0 then invuln = invuln - 1 end
  frame = frame + 1
end

-- ---------------------------------------------------------------------------
-- Hangar hub
-- ---------------------------------------------------------------------------
local updateHangar, drawHangarUi, say
do
local function hangarListCount()
  if hangTab == TAB_SHIPS then return #SHIP_IDS end
  if hangTab == TAB_PARTS then return #PARTS[PART_SLOTS[partSlot]] end
  if hangTab == TAB_COLORS then return #COLORS end
  if hangTab == TAB_UPGRADES then return #UPGRADE_KEYS end
  return 0
end

local function equip(slot, id)
  save.loadout[slot] = id
  rebuildShip()
  saveGame()
end

local function hangarBuyShip(id)
  local hull = SHIPS[id]
  if hull and not ownsShip(id) then
    if save.credits < hull.price then return end
    save.credits = save.credits - hull.price
    table.insert(save.ownedShips, id)
  end
  equip("ship", id)
end

local function hangarBuyPart(slot, id)
  if not ownsPart(slot, id) then
    local p = findPart(slot, id)
    if save.credits < p.price then return end
    save.credits = save.credits - p.price
    table.insert(save.ownedParts[slot], id)
  end
  equip(slot, id)
end

local function hangarBuyColor(id)
  if not ownsColor(id) then
    local c = findColor(id)
    if save.credits < c.price then return end
    save.credits = save.credits - c.price
    table.insert(save.ownedColors, id)
  end
  equip("color", id)
end

local function hangarBuyUpgrade(key)
  local def = UPGRADE_DEFS[key]
  local lv = save.upgrades[key]
  if lv >= def.max then return end
  local cost = upgradeCost(key, lv)
  if save.credits < cost then return end
  save.credits = save.credits - cost
  save.upgrades[key] = lv + 1
  saveGame()
end

local function hangarConfirm()
  if hangTab == TAB_SHIPS then
    hangarBuyShip(SHIP_IDS[hangSel])
  elseif hangTab == TAB_PARTS then
    local slot = PART_SLOTS[partSlot]
    hangarBuyPart(slot, PARTS[slot][hangSel].id)
  elseif hangTab == TAB_COLORS then
    hangarBuyColor(COLORS[hangSel].id)
  elseif hangTab == TAB_UPGRADES then
    hangarBuyUpgrade(UPGRADE_KEYS[hangSel])
  elseif hangTab == TAB_LAUNCH then
    resetGame()
    state = STATE_PLAY
  end
end

-- 1-based cyclic step
local function wrap(i, delta, n) return (i - 1 + delta) % n + 1 end

function updateHangar()
  if hangNavLock > 0 then hangNavLock = hangNavLock - 1 end

  if pad:justPressed(Pads.L1) then
    hangTab, hangSel, hangNavLock = wrap(hangTab, -1, #HANGAR_TABS), 1, 12
  end
  if pad:justPressed(Pads.R1) then
    hangTab, hangSel, hangNavLock = wrap(hangTab, 1, #HANGAR_TABS), 1, 12
  end

  if hangTab == TAB_PARTS then
    if pad:justPressed(Pads.LEFT) then
      partSlot, hangSel, hangNavLock = wrap(partSlot, -1, #PART_SLOTS), 1, 12
    end
    if pad:justPressed(Pads.RIGHT) then
      partSlot, hangSel, hangNavLock = wrap(partSlot, 1, #PART_SLOTS), 1, 12
    end
  end

  local count = hangarListCount()
  if count > 0 and hangNavLock == 0 then
    if pad:justPressed(Pads.UP) then hangSel, hangNavLock = wrap(hangSel, -1, count), 10 end
    if pad:justPressed(Pads.DOWN) then hangSel, hangNavLock = wrap(hangSel, 1, count), 10 end
  end

  if pad:justPressed(Pads.CROSS) then hangarConfirm() end
end

function say(x, y, color, text)
  font.color = color
  font:print(x, y, text)
end

function drawHangarUi()
  local px = floor(CW * 0.42)
  local pw = CW - px - 10

  Draw.rect(0, 0, px, CH, Color.new(0, 0, 0, 40))
  Draw.rect(px, 30, pw, CH - 38, Color.new(16, 22, 38, 200))

  local tx = px + 6
  for t, name in ipairs(HANGAR_TABS) do
    say(tx, 8, t == hangTab and C_YELLOW or C_DIM, name)
    tx = tx + 86
  end

  say(px + 8, 38, C_CYAN, "CREDITS " .. save.credits)
  say(px + 8, 56, C_DIM, "BEST " .. save.best)

  if hangTab == TAB_LAUNCH then
    local rs = computeStats(save)
    say(px + 8, 88, C_WHITE, "LAUNCH RUN")
    font:print(px + 8, 112, SHIPS[save.loadout.ship].name .. " / " .. findColor(save.loadout.color).name)
    say(px + 8, 136, C_DIM, string.format("Lives %d  Dmg %d  Spd x%g", rs.lives, rs.damage, floor(rs.speed * 100) / 100))
    font:print(px + 8, 156, string.format("Shots %d  Rate %df", rs.bullets, rs.cooldown))
    say(px + 8, 190, C_YELLOW, "CROSS = Launch")
    say(px + 8, 212, C_DIM, "L1/R1 tabs  TRIANGLE quit")
    return
  end

  local listY, rowH = 78, 22
  local maxRows = (CH - listY - 16) // rowH

  if hangTab == TAB_PARTS then
    say(px + 8, listY - 18, C_WHITE, "Slot: " .. PART_SLOTS[partSlot]:upper() .. "  < >")
  end

  for idx = 1, min(maxRows, hangarListCount()) do
    local y = listY + (idx - 1) * rowH
    local sel = idx == hangSel

    if sel then Draw.rect(px + 4, y - 2, pw - 8, rowH - 2, Color.new(40, 60, 100, 160)) end

    local label, price, owned, equipped = "", 0, false, false

    if hangTab == TAB_SHIPS then
      local id = SHIP_IDS[idx]
      local hull = SHIPS[id]
      label, price, owned, equipped = hull.name, hull.price, ownsShip(id), save.loadout.ship == id
    elseif hangTab == TAB_PARTS then
      local slot = PART_SLOTS[partSlot]
      local p = PARTS[slot][idx]
      label, price, owned, equipped = p.name, p.price, ownsPart(slot, p.id), save.loadout[slot] == p.id
    elseif hangTab == TAB_COLORS then
      local c = COLORS[idx]
      label, price, owned, equipped = c.name, c.price, ownsColor(c.id), save.loadout.color == c.id
    elseif hangTab == TAB_UPGRADES then
      local key = UPGRADE_KEYS[idx]
      local def = UPGRADE_DEFS[key]
      local lv = save.upgrades[key]
      label = def.name .. " " .. lv .. "/" .. def.max
      owned = lv >= def.max
      price = owned and 0 or upgradeCost(key, lv)
    end

    say(px + 10, y + 2, sel and C_WHITE or C_DIM, label)

    if equipped then
      say(px + pw - 72, y + 2, C_GREEN, "EQUIPPED")
    elseif hangTab == TAB_UPGRADES and owned then
      say(px + pw - 52, y + 2, C_GREEN, "MAX")
    elseif owned then
      say(px + pw - 52, y + 2, C_CYAN, "OWNED")
    else
      say(px + pw - 52, y + 2, save.credits >= price and C_YELLOW or C_RED, price .. "c")
    end
  end

  say(px + 8, CH - 22, C_DIM, "CROSS buy/equip  L1/R1 tab")
end
end

-- ---------------------------------------------------------------------------
-- Render helpers
-- ---------------------------------------------------------------------------
local function setScale(obj, x, y, z) obj:setScale(x, y, z) end

local function syncScene(camX, camY, camZ)
  for _, s in ipairs(stars) do
    s.obj:setPosition(s.x, s.y, s.z)
    setScale(s.obj, s.sc, s.sc, s.sc)
  end
  for _, p in ipairs(planets) do
    p.obj:setPosition(p.x, p.y, p.z)
    setScale(p.obj, p.sc, p.sc, p.sc)
    p.obj:setRotation(billboardPitch(p.x, p.y, p.z, camX, camY, camZ), billboardYaw(p.x, p.z, camX, camZ), 0.0)
  end
  for i = 1, MAX_BULLETS do
    local b = bullets[i]
    if b.alive then b.obj:setPosition(b.x, b.y, b.z) end
  end
  for _, e in ipairs(enemyPool) do
    if e.active then
      local o = e.type == 1 and e.objRock or e.objFighter
      o:setPosition(e.x, e.y, e.z)
      o:setRotation(e.rx, e.ry, e.rz)
      if e.type == 1 then setScale(o, e.sc, e.sc, e.sc) end
    end
  end
  for _, f in ipairs(fx) do
    if f.alive then
      local prog = 1.0 - f.life / f.maxLife
      local sc = fxScale(f.life, f.maxLife, f.baseSc)
      f.obj:setPosition(f.x, f.y, f.z)
      f.obj:setRotation(prog * 0.8, f.spin + prog * 1.6, 0.0)
      setScale(f.obj, sc, sc, sc)
    end
  end
  for i = 1, MAX_PARTICLES do
    local life = pLife[i]
    if life > 0 then
      local p = particles[i]
      local o = p.obj
      local fade = life / p.maxLife
      local s = pSc[i] * fade * fade
      local age = 1.0 - fade
      o:setPosition(pX[i], pY[i], pZ[i])

      local k = pKind[i]
      if k == 0 then
        o:setRotation(pitch, 0.0, roll)
        setScale(o, s * 0.7, s * 0.7, s * 1.8)
      elseif k == 1 then
        o:setRotation(age * 2.1 + p.spin, age * 3.2, p.spin * 0.5)
        setScale(o, s * 0.35, s * 0.35, s * 2.8)
      elseif k == 2 then
        o:setRotation(age * 1.4, age * 2.0 + p.spin, p.spin)
        setScale(o, s, s, s)
      elseif k == 3 then
        o:setRotation(age * 4.0, age * 5.0, age * 2.0)
        setScale(o, s * 1.2, s * 1.2, s * 1.2)
      else
        o:setRotation(age * 0.6, age * 0.9 + p.spin, 0.0)
        setScale(o, s * 1.1, s * 1.1, s * 0.85)
      end
    end
  end
end

local function renderScene(showShip)
  for _, p in ipairs(planets) do p.obj:render() end
  for _, s in ipairs(stars) do s.obj:render() end

  for _, e in ipairs(enemyPool) do
    if e.active then (e.type == 1 and e.objRock or e.objFighter):render() end
  end
  for i = 1, MAX_BULLETS do
    if bullets[i].alive then bullets[i].obj:render() end
  end
  for _, f in ipairs(fx) do
    if f.alive then f.obj:render() end
  end
  for i = 1, MAX_PARTICLES do
    if pLife[i] > 0 then particles[i].obj:render() end
  end

  if showShip then
    if state == STATE_PLAY then
      local pulse = 0.75 + sin(frame * 0.6) * 0.2 + (pad:pressed(Pads.R2) and 0.6 or 0.0)
      local my, mz = engineMount(1.15)
      thruster:setPosition(shipX, shipY + my, mz)
      thruster:setRotation(pitch, 0.0, roll)
      setScale(thruster, 1.0, 1.0, pulse)
      thruster:render()
    end
    ship:render()
  end
end

-- ---------------------------------------------------------------------------
-- HUD
-- ---------------------------------------------------------------------------
local drawBackdrop, drawHud
do
function drawBackdrop()
  -- Subtle vignette only -- planets are 3D now.
  Draw.rect(0, 0, CW, 28, Color.new(0, 0, 0, 80))
  Draw.rect(0, CH - 28, CW, 28, Color.new(0, 0, 0, 80))
end

local function drawLives()
  for i = 0, lives - 1 do
    local x, y = CW - 40 - i * 26, 26
    Draw.triangle(x, y + 14, x + 10, y + 14, x + 5, y, C_GREEN)
  end
end

local function drawBoost(boosting)
  local bw, x, y = 120, 16, CH - 26
  Draw.rect(x, y, bw, 10, Color.new(30, 40, 60))
  local frac = clamp((speed - BASE_SPEED * 0.4) / (BASE_SPEED * 2.1), 0.0, 1.0)
  Draw.rect(x, y, bw * frac, 10, boosting and C_YELLOW or C_CYAN)
  say(x, y - 20, C_DIM, "SPEED")
end

function drawHud()
  say(16, 14, C_WHITE, "SCORE " .. score)
  say(16, 34, C_CYAN, "WAVE " .. wave)

  local cx = CW * 0.5
  if state == STATE_MENU then
    say(cx - 70, 150, C_WHITE, "S T A R   F O X")
    say(cx - 95, 190, C_YELLOW, "Press CROSS for Hangar")
    say(cx - 130, 230, C_DIM, "Left Stick: steer   CROSS/R1: fire")
    font:print(cx - 130, 250, "R2: boost   L2: brake   TRIANGLE: quit")
  elseif state == STATE_HANGAR then
    drawHangarUi()
  elseif state == STATE_OVER then
    say(cx - 55, 140, C_RED, "RUN OVER")
    say(cx - 65, 170, C_WHITE, "Score " .. score .. "  Wave " .. wave)
    say(cx - 75, 195, C_CYAN, "+" .. runEarned .. " credits")
    say(cx - 55, 218, C_DIM, "Bank " .. save.credits)
    say(cx - 85, 245, C_YELLOW, "CROSS = Hangar")
  else
    drawLives()
    drawBoost(pad:pressed(Pads.R2))
    say(CW - 110, 34, C_CYAN, save.credits .. "c")
  end

  say(CW - 96, 14, C_DIM, string.format("%.0f FPS", Screen.getFPS(60)))
end
end

-- ---------------------------------------------------------------------------
-- Main loop
-- ---------------------------------------------------------------------------
Screen.setParam(Screen.DEPTH_TEST_METHOD, Screen.DEPTH_GEQUAL)

Camera.position(CAM_X, CAM_Y, CAM_Z)
Camera.target(LOOK_X, LOOK_Y, LOOK_Z)
Camera.update()

while true do
  pad:update()

  if shake > 0.0 then shake = shake - 0.06 end

  if pad:justPressed(Pads.TRIANGLE) then System.reload("main.lua") end

  if state == STATE_MENU then
    menuSpin = menuSpin + 0.02
    updateWorld(BASE_SPEED * 0.4)
    if pad:justPressed(Pads.CROSS) or pad:justPressed(Pads.START) then
      state = STATE_HANGAR
      hangTab, hangSel, partSlot = TAB_SHIPS, 1, 1
      rebuildShip()
    end
  elseif state == STATE_HANGAR then
    menuSpin = menuSpin + 0.02
    updateWorld(BASE_SPEED * 0.25)
    updateHangar()
  elseif state == STATE_OVER then
    updateWorld(BASE_SPEED * 0.4)
    if pad:justPressed(Pads.CROSS) or pad:justPressed(Pads.START) then
      state = STATE_HANGAR
      hangTab, hangSel = TAB_LAUNCH, 1
      rebuildShip()
    end
  else
    updatePlay()
  end

  -- Ship transform
  if state == STATE_PLAY then
    ship:setPosition(shipX, shipY, 0.0)
    ship:setRotation(pitch, 0.0, roll)
  else
    ship:setPosition(0.0, 0.0, 0.0)
    ship:setRotation(0.0, menuSpin, 0.0)
  end

  -- Fixed chase camera -- only nudge position for hit-shake (never Camera.target per frame).
  local sx, sy = 0.0, 0.0
  if shake > 0.0 then
    sx = randomf(-shake, shake) * 0.6
    sy = randomf(-shake, shake) * 0.6
  end

  local camX, camY, camZ = CAM_X + sx, CAM_Y + sy, CAM_Z

  syncScene(camX, camY, camZ)

  Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)
  Screen.clear(SPACE)
  drawBackdrop()

  Screen.setParam(Screen.DEPTH_TEST_ENABLE, true)
  Screen.setParam(Screen.DEPTH_TEST_METHOD, Screen.DEPTH_GEQUAL)
  Render.begin()

  Camera.position(camX, camY, camZ)
  Camera.update()

  local showShip = state ~= STATE_PLAY or invuln <= 0 or (invuln // 6) % 2 == 0
  renderScene(showShip)

  Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)
  drawHud()
  Screen.flip()
end
