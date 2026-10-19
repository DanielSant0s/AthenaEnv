-- AthenaOS app SDK -- gives every app the same chrome as the shell.
-- Lua port of athenaos/app.js.
--
--   local app = require "athenaos.app"
--   local ui = app.AppUI{ title = "Render demo", category = "3d",
--                         hints = { {"cross", "Light"}, {"D-pad", "Model"} } }
--   while true do
--     pad:update()
--     if ui:homeRequested(pad) then break end  -- Triangle, same in every app
--     ui:background()                           -- skip in full-screen apps
--     ...draw the app inside ui.content...
--     ui:hud({ "60 FPS", "1234 tris" })        -- optional stats card
--     ui:chrome()                               -- top bar + footer hints
--     Screen.flip()
--   end
--   app.exitToShell()                          -- never let the script end
--
-- Layout (640x448): top bar 0-26, content ui.content, footer H-34..H.

local gfx = require "athenaos.gfx"
local registry = require "athenaos.registry"
local storage = require "athenaos.storage"
local ui = require "athenaos.ui"

local rgba, COLORS, ACCENTS, roundRect, panel, vgradient, hints, mix =
  gfx.rgba, gfx.COLORS, gfx.ACCENTS, gfx.roundRect, gfx.panel, gfx.vgradient, gfx.hints, gfx.mix
local BAR_H, FOOTER_H, round = ui.BAR_H, ui.FOOTER_H, gfx.round

local M = { COLORS = COLORS, rgba = rgba }

M.SHELL = "main.lua"

-- System.reload() never returns; call this from the outermost scope of the app.
function M.exitToShell()
  Screen.clear(Color.new(0, 0, 0))
  Screen.flip()
  System.reload(M.SHELL)
end

local sharedFont
function M.uiFont()
  if not sharedFont then sharedFont = Font("fonts/Segoe UI.ttf") end
  return sharedFont
end

-- AppUI ----------------------------------------------------------------------

local AppUI = {}
AppUI.__index = AppUI
M.AppUI = AppUI

-- opts.title    - shown in the top bar
-- opts.category - registry category id, colors the top bar dot
-- opts.hints    - { {glyph, label}, ... } for the footer; Home is appended
-- opts.home     - footer hint for the home control, or false to hide it
function AppUI.new(opts)
  local self = setmetatable({}, AppUI)
  local mode = Screen.getMode()
  self.W, self.H = mode.width, mode.height
  self.M = ui.margin(self.W)
  self.settings = storage.loadSettings()
  self.font = M.uiFont()
  self.title = opts.title or ""
  self.category = registry.categoryOf(opts.category or "system")
  self.hintList = opts.hints or {}
  if opts.home == nil then self.home = { "triangle", "Home" } else self.home = opts.home or nil end
  self.status = ""
  self.content = { x = self.M, y = BAR_H + 14, w = self.W - 2 * self.M, h = self.H - BAR_H - FOOTER_H - 28 }
  return self
end
setmetatable(AppUI, { __call = function(_, opts) return AppUI.new(opts) end })

function AppUI:accent(a) return gfx.accent(self.settings.accent, a) end

function AppUI:homeRequested(pad) return pad:justPressed(Pads.TRIANGLE) end

-- Same backdrop as the shell's "Midnight" wallpaper, tinted by the accent.
function AppUI:background()
  local top = mix({ 18, 20, 30 }, ACCENTS[(self.settings.accent % #ACCENTS) + 1].rgb, 0.12)
  Screen.clear(Color.new(0, 0, 0))
  vgradient(0, 0, self.W, self.H, rgba(top[1], top[2], top[3]), rgba(4, 4, 8))
end

function AppUI:text(x, y, str, scale, color, align) ui.text(self.font, x, y, str, scale or 0.5, color or COLORS.text, align) end
function AppUI:textV(x, cy, str, scale, color, align) ui.textV(self.font, x, cy, str, scale or 0.5, color or COLORS.text, align) end
function AppUI:measure(str, scale) return ui.measure(self.font, str, scale or 0.5) end
function AppUI:fit(str, scale, maxW) return ui.fit(self.font, str, scale, maxW) end
function AppUI:wrap(str, scale, maxW) return ui.wrap(self.font, str, scale, maxW) end

function AppUI:panel(x, y, w, h, r) panel(x, y, w, h, r or 14, COLORS.glassHi) end

-- Section heading inside the content area.
function AppUI:heading(x, cy, str) self:textV(x, cy, str:upper(), 0.38, self:accent()) end

-- Translucent stats card; top-left under the bar by default, or pinned to the
-- right edge (opts.right) / just above the footer (opts.bottom).
function AppUI:hud(lines, opts)
  if #lines == 0 then return end
  opts = opts or {}
  local scale, lh = 0.40, 15
  local w = 0
  for _, l in ipairs(lines) do w = math.max(w, self:measure(l, scale)) end
  w = round(w) + 20
  local h = #lines * lh + 10
  local x, y = opts.x or self.M, opts.y or (BAR_H + 10)
  if opts.right then x = self.W - self.M - w end
  if opts.bottom then y = self.H - FOOTER_H - 10 - h end
  roundRect(x, y, w, h, 8, rgba(0, 0, 0, 0.45))
  for i, l in ipairs(lines) do
    self:textV(x + 10, y + 5 + lh / 2 + (i - 1) * lh, l, scale, i == 1 and COLORS.text or COLORS.textDim)
  end
end

function AppUI:header(status)
  status = status or self.status
  local cy = BAR_H / 2
  Draw.rect(0, 0, self.W, BAR_H, rgba(0, 0, 0, 0.45))
  local c = self.category.rgb
  Draw.circle(self.M + 4, cy, 4, rgba(c[1], c[2], c[3]), true)
  self:textV(self.M + 14, cy, self.title, 0.48, COLORS.text)

  local x = self.W - self.M
  local clock = ui.clockText(self.settings)
  x = x - self:measure(clock, 0.48)
  self:textV(x, cy, clock, 0.48, COLORS.text)
  if status and status ~= "" then
    x = x - 16 - self:measure(status, 0.44)
    self:textV(x, cy, status, 0.44, COLORS.textDim)
  end
end

function AppUI:footer(hintList, left)
  hintList, left = hintList or self.hintList, left or ""
  local y = self.H - FOOTER_H
  local cy = y + FOOTER_H / 2
  Draw.rect(0, y, self.W, FOOTER_H, rgba(0, 0, 0, 0.45))
  local list = {}
  for i, h in ipairs(hintList) do list[i] = h end
  if self.home then list[#list + 1] = self.home end
  local leftW = left ~= "" and (round(self:measure(left, 0.44)) + 20) or 0
  hints(list, self.W - self.M, cy, self.font, self.W - 2 * self.M - leftW)
  if left ~= "" then self:textV(self.M, cy, left, 0.44, COLORS.textDim) end
end

-- Chrome for full-screen media: shown while the pad is in use, hidden after
-- ~4 s idle so it does not cover the picture. Call every frame.
function AppUI:autoChrome(pad, hintList, left)
  if pad.btns ~= 0 or math.abs(pad.lx) > 64 or math.abs(pad.ly) > 64 then
    self.idle = 0
  else
    self.idle = (self.idle or 0) + 1
  end
  if self.idle < 240 then self:chrome(hintList, left) end
end

-- Top bar + footer. Disables depth test so it also draws over 3D scenes.
function AppUI:chrome(hintList, left)
  local depth = Screen.getParam(Screen.DEPTH_TEST_ENABLE)
  if depth ~= 0 then Screen.setParam(Screen.DEPTH_TEST_ENABLE, false) end
  self:header()
  self:footer(hintList, left)
  if depth ~= 0 then Screen.setParam(Screen.DEPTH_TEST_ENABLE, depth) end
end

-- One-shot frame for long blocking work (asset loading, network calls).
function AppUI:loading(message, detail)
  self:background()
  local cy = self.H / 2
  self:textV(self.W / 2, cy - 10, message, 0.62, COLORS.text, Font.ALIGN_HCENTER)
  if detail and detail ~= "" then
    self:textV(self.W / 2, cy + 16, detail, 0.44, COLORS.textDim, Font.ALIGN_HCENTER)
  end
  self:chrome({}, "")
  Screen.flip()
end

-- Terminal -------------------------------------------------------------------
-- Scrolling text console with the standard chrome, for demos that mostly log.
--   local term = app.Terminal{ title = "Keyboard demo", category = "input", subtitle = "..." }
--   term:log("hello")
--   while true do pad:update(); if term:homeRequested(pad) then break end; term:draw() end

local Terminal = setmetatable({}, { __index = AppUI })
Terminal.__index = Terminal
M.Terminal = Terminal

function Terminal.new(opts)
  local self = AppUI.new(opts)
  setmetatable(self, Terminal)
  self.subtitle = opts.subtitle or ""
  self.lines = {}
  self.maxLines = 20
  Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)
  return self
end
setmetatable(Terminal, { __index = AppUI, __call = function(_, opts) return Terminal.new(opts) end })

function Terminal:log(...)
  local parts = table.pack(...)
  for i = 1, parts.n do
    local v = parts[i]
    if type(v) == "table" then
      local ok, s = pcall(JSON.stringify, v)
      parts[i] = ok and s or tostring(v)
    else
      parts[i] = tostring(v)
    end
  end
  local str = table.concat(parts, " ", 1, parts.n)
  print(str)
  local maxW = self.content.w - 24
  for line in (str .. "\n"):gmatch("(.-)\n") do
    local wrapped = self:wrap(line:gsub("\t", "    "), 0.42, maxW)
    for _, w in ipairs(wrapped) do
      -- hard-split words longer than a line (JSON, URLs)
      while self:measure(w, 0.42) > maxW and #w > 1 do
        local n = #w
        while n > 1 and self:measure(w:sub(1, n), 0.42) > maxW do n = math.floor(n * 0.9) end
        self.lines[#self.lines + 1] = w:sub(1, n)
        w = w:sub(n + 1)
      end
      self.lines[#self.lines + 1] = w
    end
    if #line == 0 then self.lines[#self.lines + 1] = "" end
  end
  while #self.lines > self.maxLines do table.remove(self.lines, 1) end
end

function Terminal:clear() self.lines = {} end

-- Draws without flipping, so callers can add their own overlay.
function Terminal:render()
  self:background()
  local c = self.content
  if self.subtitle ~= "" then self:textV(c.x, c.y + 6, self.subtitle, 0.46, COLORS.textDim) end
  local top = self.subtitle ~= "" and c.y + 20 or c.y
  roundRect(c.x, top, c.w, c.h - (top - c.y), 10, rgba(0, 0, 0, 0.35))
  for i, line in ipairs(self.lines) do
    self:textV(c.x + 12, top + 14 + (i - 1) * 15, line, 0.42, COLORS.text)
  end
  self:chrome()
end

function Terminal:draw()
  self:render()
  Screen.flip()
end

return M
