-- AthenaOS drawing kit: palette, shapes, animation helpers and button glyphs.
-- Lua port of athenaos/gfx.js. Colors use the GS convention: RGB 0-255,
-- alpha 0-128 (128 = opaque).

local M = {}

local floor, min, max, abs, cos, sin, pi = math.floor, math.min, math.max, math.abs, math.cos, math.sin, math.pi

local function round(v) return floor(v + 0.5) end
M.round = round

function M.rgba(r, g, b, a)
  return Color.new(r, g, b, round((a or 1.0) * 128))
end
local rgba = M.rgba

M.ACCENTS = {
  { name = "Indigo", rgb = { 104, 112, 255 } },
  { name = "Azure",  rgb = {  40, 160, 255 } },
  { name = "Mint",   rgb = {  40, 210, 160 } },
  { name = "Amber",  rgb = { 255, 170,  40 } },
  { name = "Rose",   rgb = { 255,  80, 130 } },
  { name = "Violet", rgb = { 170,  90, 255 } },
}

-- index is 0-based, like the settings value shared with the JS shell
function M.accent(index, a)
  local c = M.ACCENTS[(index % #M.ACCENTS) + 1].rgb
  return rgba(c[1], c[2], c[3], a)
end

function M.mix(c1, c2, t)
  return {
    round(c1[1] + (c2[1] - c1[1]) * t),
    round(c1[2] + (c2[2] - c1[2]) * t),
    round(c1[3] + (c2[3] - c1[3]) * t),
  }
end

M.COLORS = {
  text      = rgba(255, 255, 255, 1.0),
  textDim   = rgba(255, 255, 255, 0.62),
  textFaint = rgba(255, 255, 255, 0.38),
  glass     = rgba( 18,  20,  32, 0.62),
  glassHi   = rgba( 40,  44,  64, 0.70),
  stroke    = rgba(255, 255, 255, 0.10),
  shadow    = rgba(  0,   0,   0, 0.30),
  scrim     = rgba(  0,   0,   0, 0.55),
  ok        = rgba( 70, 210, 120, 1.0),
  warn      = rgba(255, 190,  60, 1.0),
  bad       = rgba(255,  90,  90, 1.0),
}
local COLORS = M.COLORS

-- Animation ---------------------------------------------------------------

function M.clamp(v, lo, hi) return v < lo and lo or (v > hi and hi or v) end
local clamp = M.clamp
function M.lerp(a, b, t) return a + (b - a) * t end
function M.easeOut(t) return 1 - (1 - clamp(t, 0, 1)) ^ 3 end

-- Critically-damped follower: call step() once per frame, read .v.
local Spring = {}
Spring.__index = Spring
M.Spring = Spring

function Spring.new(v, speed)
  v = v or 0
  return setmetatable({ v = v, target = v, speed = speed or 0.25 }, Spring)
end
setmetatable(Spring, { __call = function(_, ...) return Spring.new(...) end })

function Spring:set(target) self.target = target return self end
function Spring:snap(v) self.v = v self.target = v return self end
function Spring:step()
  self.v = self.v + (self.target - self.v) * self.speed
  if abs(self.target - self.v) < 0.01 then self.v = self.target end
  return self.v
end

-- Shapes ------------------------------------------------------------------

-- Unit quarter-arcs, one table per segment count.
local ARCS = {}
for steps = 2, 8 do
  local arc = {}
  for i = 0, steps do
    local a = (i / steps) * (pi / 2)
    arc[#arc + 1] = { cos(a), sin(a) }
  end
  ARCS[steps] = arc
end
local function arcFor(r) return ARCS[clamp(round(r / 4), 2, 6)] end

-- Quarter-circle fan centered at (cx, cy), sx/sy pick the quadrant (+1/-1).
local function corner(cx, cy, r, sx, sy, color)
  local arc = arcFor(r)
  for i = 1, #arc - 1 do
    Draw.triangle(cx, cy,
      cx + sx * arc[i][1] * r, cy + sy * arc[i][2] * r,
      cx + sx * arc[i + 1][1] * r, cy + sy * arc[i + 1][2] * r,
      color)
  end
end

-- Pieces meet on whole pixels; fractional edges leave 1px seams on the GS.
local function snapBox(x, y, w, h, r)
  x, y, w, h = round(x), round(y), round(w), round(h)
  r = round(min(r, w / 2, h / 2))
  return x, y, w, h, r
end

-- Non-overlapping pieces, so translucent fills blend evenly.
function M.roundRect(x, y, w, h, r, color)
  x, y, w, h, r = snapBox(x, y, w, h, r)
  if r < 1 then Draw.rect(x, y, w, h, color) return end
  Draw.rect(x + r, y, w - 2 * r, h, color)
  Draw.rect(x, y + r, r, h - 2 * r, color)
  Draw.rect(x + w - r, y + r, r, h - 2 * r, color)
  corner(x + r,     y + r,     r, -1, -1, color)
  corner(x + w - r, y + r,     r,  1, -1, color)
  corner(x + r,     y + h - r, r, -1,  1, color)
  corner(x + w - r, y + h - r, r,  1,  1, color)
end
local roundRect = M.roundRect

-- Rounded rect with a vertical gradient: solid caps + gradient middle band.
function M.roundGradient(x, y, w, h, r, top, bottom)
  x, y, w, h, r = snapBox(x, y, w, h, r)
  Draw.rect(x + r, y, w - 2 * r, r, top)
  corner(x + r,     y + r, r, -1, -1, top)
  corner(x + w - r, y + r, r,  1, -1, top)
  Draw.quad(x, y + r, x + w, y + r, x, y + h - r, x + w, y + h - r, top, top, bottom, bottom)
  Draw.rect(x + r, y + h - r, w - 2 * r, r, bottom)
  corner(x + r,     y + h - r, r, -1, 1, bottom)
  corner(x + w - r, y + h - r, r,  1, 1, bottom)
end

function M.panel(x, y, w, h, r, fill)
  r = r or 12
  roundRect(x + 2, y + 4, w, h, r, COLORS.shadow)
  roundRect(x, y, w, h, r, fill or COLORS.glass)
  Draw.line(x + r, y, x + w - r, y, COLORS.stroke)
end

-- Focus ring drawn as an outer rounded rect behind the item.
function M.ring(x, y, w, h, r, color, t)
  t = t or 3
  roundRect(x - t, y - t, w + 2 * t, h + 2 * t, r + t, color)
end

function M.vgradient(x, y, w, h, top, bottom)
  Draw.quad(x, y, x + w, y, x, y + h, x + w, y + h, top, top, bottom, bottom)
end

function M.gradient4(x, y, w, h, tl, tr, bl, br)
  Draw.quad(x, y, x + w, y, x, y + h, x + w, y + h, tl, tr, bl, br)
end

function M.bar(x, y, w, h, t, fill, track)
  roundRect(x, y, w, h, h / 2, track or rgba(255, 255, 255, 0.12))
  local fw = max(h, w * clamp(t, 0, 1))
  roundRect(x, y, fw, h, h / 2, fill)
end

function M.toggle(x, y, on, color)
  roundRect(x, y, 36, 20, 10, on and color or rgba(255, 255, 255, 0.20))
  Draw.circle(x + (on and 26 or 10), y + 10, 7, COLORS.text, true)
end

-- Text --------------------------------------------------------------------
-- Font.print() takes the top of the em box; cap-height center lands about
-- 15.5px * scale - 2 under y, so this returns the y that centers on cy.
function M.textTop(cy, scale) return round(cy - 15.5 * scale + 2) end
local textTop = M.textTop

-- Controller glyphs -------------------------------------------------------

local GLYPH = {
  cross    = { 124, 178, 232 },
  circle   = { 255, 102, 102 },
  square   = { 255, 105, 248 },
  triangle = {  64, 226, 160 },
}

local PILL_SCALE = 0.40
local LABEL_SCALE = 0.46

local function glyphWidth(kind, font)
  if GLYPH[kind] then return 16 end
  font.scale = PILL_SCALE
  return font:getTextSize(kind).width + 12
end

-- Draws a PlayStation button glyph vertically centered on cy, returns its width.
function M.glyph(kind, x, cy, font)
  x, cy = round(x), round(cy)
  local c = GLYPH[kind]
  if c then
    local col = rgba(c[1], c[2], c[3], 1.0)
    Draw.circle(x + 8, cy, 8, rgba(255, 255, 255, 0.16), true)
    if kind == "cross" then
      Draw.line(x + 5, cy - 3, x + 11, cy + 3, col)
      Draw.line(x + 11, cy - 3, x + 5, cy + 3, col)
    elseif kind == "circle" then
      Draw.circle(x + 8, cy, 4, col, false)
    elseif kind == "square" then
      Draw.line(x + 5, cy - 3, x + 11, cy - 3, col)
      Draw.line(x + 11, cy - 3, x + 11, cy + 3, col)
      Draw.line(x + 11, cy + 3, x + 5, cy + 3, col)
      Draw.line(x + 5, cy + 3, x + 5, cy - 3, col)
    elseif kind == "triangle" then
      Draw.line(x + 8, cy - 4, x + 12, cy + 3, col)
      Draw.line(x + 12, cy + 3, x + 4, cy + 3, col)
      Draw.line(x + 4, cy + 3, x + 8, cy - 4, col)
    end
    return 16
  end
  -- Text pill: L1, R1, START, SELECT, D-pad...
  local w = glyphWidth(kind, font)
  roundRect(x, cy - 8, w, 16, 8, rgba(255, 255, 255, 0.16))
  font.scale = PILL_SCALE
  font.color = COLORS.text
  font:print(x + 6, textTop(cy, PILL_SCALE), kind)
  return w
end
local glyph = M.glyph

-- Row of {glyph, label} hints, right-aligned at `right`, centered on cy.
-- With maxW, gaps and then label size shrink until the row fits.
function M.hints(list, right, cy, font, maxW)
  maxW = maxW or math.huge
  local gap, labelScale = 18, LABEL_SCALE
  local glyphs = {}
  for i, h in ipairs(list) do glyphs[i] = glyphWidth(h[1], font) end

  local function measureRow()
    font.scale = labelScale
    local widths, total = {}, -gap
    for i, h in ipairs(list) do
      widths[i] = glyphs[i] + 6 + font:getTextSize(h[2]).width
      total = total + widths[i] + gap
    end
    return widths, total
  end

  local widths, total = measureRow()
  if total > maxW then gap = 10 widths, total = measureRow() end
  while total > maxW and labelScale > 0.34 do
    labelScale = labelScale - 0.02
    widths, total = measureRow()
  end

  local x = right - total
  for i, h in ipairs(list) do
    local gw = glyph(h[1], x, cy, font)
    font.scale = labelScale
    font.color = COLORS.textDim
    font:print(round(x + gw + 6), textTop(cy, labelScale), h[2])
    x = x + widths[i] + gap
  end
end

return M
