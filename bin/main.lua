-- AthenaOS shell (Lua) -- desktop, launcher and system UI for the Lua samples.
-- Port of main.js for the athena_lua.elf build.
--
-- Apps are regular scripts started with System.reload(); they come back here
-- with System.reload("main.lua"). The shell remembers what it launched
-- (athenaos/session_lua.json) so returning from an app lands on the desktop
-- with a "closed" toast instead of the lock screen.

local gfx = require "athenaos.gfx"
local registry = require "athenaos.registry"
local storage = require "athenaos.storage"
local uikit = require "athenaos.ui"

local rgba, ACCENTS, accent, mix, COLORS, Spring, clamp, lerp, easeOut =
  gfx.rgba, gfx.ACCENTS, gfx.accent, gfx.mix, gfx.COLORS, gfx.Spring, gfx.clamp, gfx.lerp, gfx.easeOut
local roundRect, roundGradient, panel, ring, vgradient, bar, toggle, hints =
  gfx.roundRect, gfx.roundGradient, gfx.panel, gfx.ring, gfx.vgradient, gfx.bar, gfx.toggle, gfx.hints
local categoryOf, initials = registry.categoryOf, registry.initials
local text, textV, measure, fit, wrap, pad2 = uikit.text, uikit.textV, uikit.measure, uikit.fit, uikit.wrap, uikit.pad2
local round, floor, max, min, sin, cos = gfx.round, math.floor, math.max, math.min, math.sin, math.cos

local function contains(list, value)
  for i, v in ipairs(list) do if v == value then return i end end
  return nil
end

local function filter(list, pred)
  local out = JSON.array()
  for _, v in ipairs(list) do if pred(v) then out[#out + 1] = v end end
  return out
end

-- Video setup -------------------------------------------------------------------

local canvas = Screen.getMode()
canvas.zbuffering = false
canvas.psmz = Screen.Z16S
Screen.setMode(canvas)
Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)

local W, H = canvas.width, canvas.height

-- State -------------------------------------------------------------------------

local settings = storage.loadSettings()
local session = storage.loadSession()
local apps = registry.scanApps()
local function appByFile(file)
  for _, a in ipairs(apps) do if a.file == file then return a end end
  return nil
end

local launchingFile = session.launching ~= JSON.null and session.launching or nil
local returnedFrom = launchingFile and appByFile(launchingFile) or nil
local coldBoot = launchingFile == nil
session.launching = JSON.null
session.pinned = filter(session.pinned, appByFile)
session.recents = filter(session.recents, appByFile)
storage.saveSession(session)

local ui = Font("fonts/Segoe UI.ttf")
local display = Font("fonts/LEMONMILK-Light.otf")
local heavy = Font("fonts/LEMONMILK-Medium.otf")

for _, app in ipairs(apps) do
  app.fx = Spring(0, 0.3)
  if app.iconPath then
    app.icon = Image(app.iconPath)
    app.icon.filter = LINEAR
  end
end

local pad = Pads.get()
local cpuInfo = System.getCPUInfo()
local gpuInfo = System.getGPUInfo()
local uptime = Timer.new()

Screen.setFrameCounter(settings.showFps)

local frame = 0

-- Clock & system stats ----------------------------------------------------------

local function clockText(d) return uikit.clockText(settings, d) end
local function dateText(d) return uikit.dateText(settings, d) end

local stats = { ram = 0, ramUsed = 0, iop = 0, iopUsed = 0, vram = 0, vramUsed = 0, vramSize = 0, temp = nil, pads = {} }

local function refreshStats()
  local mem = System.getMemoryStats()
  stats.ramUsed = mem.used
  stats.ram = mem.used / cpuInfo.RAMSize
  local iop = IOP.getMemoryStats()
  stats.iopUsed = iop.used
  stats.iop = iop.used / (2048 * 1024)
  stats.vramUsed = Screen.getMemoryStats(Screen.VRAM_USED_TOTAL)
  stats.vramSize = Screen.getMemoryStats(Screen.VRAM_SIZE)
  stats.vram = stats.vramSize > 0 and stats.vramUsed / stats.vramSize or 0
  stats.temp = System.getTemperature()
  stats.pads = Pads.getConnected()
end
refreshStats()

local function kb(bytes) return round(bytes / 1024) .. " KB" end

-- Input -------------------------------------------------------------------------
-- D-pad and left stick merged, with key repeat for held directions.

local DIRS = { up = Pads.UP, down = Pads.DOWN, left = Pads.LEFT, right = Pads.RIGHT }
local dirHeld = { up = 0, down = 0, left = 0, right = 0 }
local nav = { up = false, down = false, left = false, right = false }

local function pollInput()
  pad:update()
  local stick = { up = pad.ly < -64, down = pad.ly > 64, left = pad.lx < -64, right = pad.lx > 64 }
  for d, btn in pairs(DIRS) do
    local down = pad:pressed(btn) or stick[d]
    dirHeld[d] = down and dirHeld[d] + 1 or 0
    nav[d] = dirHeld[d] == 1 or (dirHeld[d] > 18 and dirHeld[d] % 5 == 0)
  end
end

local function pressed(btn) return pad:justPressed(btn) end
local function anyButton() return pad.btns ~= 0 and pad.btns ~= pad.old_btns end

-- Wallpapers --------------------------------------------------------------------

local WALLPAPERS = { [0] = "Aurora", "Midnight", "Classic" }
local WALLPAPER_COUNT = 3
local classicBg = nil

local function accentRgb() return ACCENTS[(settings.accent % #ACCENTS) + 1].rgb end

local function blob(x, y, r, rgb, a)
  for i = 0, 3 do
    Draw.circle(x, y, r * (1 - i * 0.22), rgba(rgb[1], rgb[2], rgb[3], a), true)
  end
end

local function drawWallpaper()
  local ac = accentRgb()
  local wp = settings.wallpaper % WALLPAPER_COUNT
  if wp == 0 then -- Aurora: gradient with slowly drifting light blobs
    local top, bottom = mix({ 10, 12, 28 }, ac, 0.18), { 6, 6, 14 }
    vgradient(0, 0, W, H, rgba(top[1], top[2], top[3]), rgba(bottom[1], bottom[2], bottom[3]))
    local t = settings.animations and frame / 240 or 0
    blob(W * 0.22 + cos(t) * 60, H * 0.30 + sin(t * 1.3) * 30, 190, ac, 0.05)
    blob(W * 0.80 + sin(t * 0.8) * 50, H * 0.65 + cos(t) * 40, 220, mix(ac, { 255, 80, 160 }, 0.6), 0.045)
    blob(W * 0.55 + cos(t * 0.6) * 80, H * 1.05, 200, mix(ac, { 40, 220, 200 }, 0.5), 0.04)
  elseif wp == 1 then -- Midnight: plain, cheap
    vgradient(0, 0, W, H, rgba(22, 24, 34), rgba(4, 4, 8))
  else -- Classic: the original dashboard picture, tinted with the accent
    if not classicBg then
      classicBg = Image("dash/bg.png")
      local pal = classicBg.palette
      if pal then
        for i = 0, #pal - 4, 4 do
          local l = (pal:getU8(i) + pal:getU8(i + 1) + pal:getU8(i + 2)) / 765
          pal:setU8(i, floor(ac[1] * l))
          pal:setU8(i + 1, floor(ac[2] * l))
          pal:setU8(i + 2, floor(ac[3] * l))
        end
      end
    end
    classicBg.width, classicBg.height = W, H
    classicBg:draw(0, 0)
  end
end

-- App tiles ---------------------------------------------------------------------

local function drawTile(app, cx, cy, size, focus, dim)
  focus, dim = focus or 0, dim or 1.0
  local s = size * (1 + 0.10 * focus)
  local x, y, r = cx - s / 2, cy - s / 2, s * 0.22
  local c = categoryOf(app.category).rgb
  local hi, lo = mix(c, { 255, 255, 255 }, 0.25), mix(c, { 0, 0, 0 }, 0.45)

  if focus > 0.02 then ring(x, y, s, s, r, accent(settings.accent, focus * dim), 3) end
  roundGradient(x, y, s, s, r, rgba(hi[1], hi[2], hi[3], dim), rgba(lo[1], lo[2], lo[3], dim))

  if app.icon then
    local is = s * 0.70
    app.icon.width, app.icon.height = is, is
    app.icon.color = Color.new(255, 255, 255, round(128 * dim))
    app.icon:draw(cx - is / 2, cy - is / 2)
  else
    textV(heavy, cx, cy, initials(app.name), s / 110, rgba(255, 255, 255, 0.92 * dim), Font.ALIGN_HCENTER)
  end
end

-- Toasts ------------------------------------------------------------------------

local TOAST_HIDDEN, TOAST_SHOWN = H + 10, H - 70
local toast = { msg = "", ttl = 0, y = Spring(TOAST_HIDDEN, 0.2) }

local function notify(msg)
  toast.msg = msg
  toast.ttl = 150
  toast.y:set(TOAST_SHOWN)
end

local function drawToast()
  if toast.ttl > 0 then
    toast.ttl = toast.ttl - 1
    if toast.ttl == 0 then toast.y:set(TOAST_HIDDEN) end
  end
  local y = toast.y:step()
  if y >= TOAST_HIDDEN - 1 then return end
  local w = measure(ui, toast.msg, 0.5) + 36
  panel(W / 2 - w / 2, y, w, 28, 14, COLORS.glassHi)
  Draw.circle(W / 2 - w / 2 + 14, y + 14, 4, accent(settings.accent), true)
  textV(ui, W / 2 - w / 2 + 24, y + 14, toast.msg, 0.5, COLORS.text)
end

-- Layout ------------------------------------------------------------------------

local M = uikit.margin(W)
local GRID = { cols = 5, rows = 2, tile = 80, pitchY = 102, top = 70 }
GRID.pitchX = (W - 2 * M - GRID.tile) / (GRID.cols - 1)
GRID.left = M + GRID.tile / 2
local LAYOUT = { topCY = 32, nameCY = 292, metaCY = 318, blurbCY = 342, hintsCY = 422 }

-- Desktop -----------------------------------------------------------------------

-- Tabs group registry categories; "Home" is the user's pinned + recent apps.
local TABS = {
  { label = "Home" },
  { label = "All" },
  { label = "Games",    cats = { "games" } },
  { label = "Graphics", cats = { "3d", "2d" } },
  { label = "Media",    cats = { "media" } },
  { label = "Network",  cats = { "network" } },
  { label = "System",   cats = { "system", "input" } },
}

-- tab and focus are 0-based, like the JS shell
local desk = { tab = 0, focus = 0, topRow = 0, tabX = Spring(M, 0.25), tabW = Spring(30, 0.25), scroll = Spring(0, 0.25) }

local state, overlay, launching, pendingReload
local launch, openOverlay

local function homeApps()
  local out = {}
  local function add(f)
    local a = appByFile(f)
    if a and #out < GRID.cols * GRID.rows then out[#out + 1] = a end
  end
  for _, f in ipairs(session.pinned) do add(f) end
  for _, f in ipairs(session.recents) do if not contains(session.pinned, f) then add(f) end end
  return out
end

local function visibleApps()
  local tab = TABS[desk.tab + 1]
  if tab.label == "Home" then return homeApps() end
  if not tab.cats then return apps end
  return filter(apps, function(a) return contains(tab.cats, a.category) end)
end

local function focusedApp() return visibleApps()[desk.focus + 1] end

local function setTab(i)
  desk.tab = (i + #TABS) % #TABS
  desk.focus = 0
  desk.topRow = 0
end

local function togglePin(app)
  if not app then return end
  local i = contains(session.pinned, app.file)
  if i then
    table.remove(session.pinned, i)
    notify("Unpinned " .. app.name)
  elseif #session.pinned >= 8 then
    notify("Home is full - unpin something first")
    return
  else
    session.pinned[#session.pinned + 1] = app.file
    notify("Pinned " .. app.name .. " to Home")
  end
  storage.saveSession(session)
  desk.focus = min(desk.focus, max(0, #visibleApps() - 1))
end

local function desktopInput()
  if pressed(Pads.L1) then setTab(desk.tab - 1) end
  if pressed(Pads.R1) then setTab(desk.tab + 1) end

  local n = #visibleApps()
  local f = desk.focus
  if nav.right and f < n - 1 then f = f + 1 end
  if nav.left and f > 0 then f = f - 1 end
  if nav.down and f + GRID.cols < n then
    f = f + GRID.cols
  elseif nav.down and f // GRID.cols < (n - 1) // GRID.cols then
    f = n - 1
  end
  if nav.up and f - GRID.cols >= 0 then f = f - GRID.cols end
  desk.focus = f

  local row = f // GRID.cols
  if row < desk.topRow then desk.topRow = row end
  if row >= desk.topRow + GRID.rows then desk.topRow = row - GRID.rows + 1 end

  local app = focusedApp()
  if pressed(Pads.CROSS) and app then launch(app) end
  if pressed(Pads.TRIANGLE) and app then openOverlay("details") end
  if pressed(Pads.SQUARE) and app then togglePin(app) end
  if pressed(Pads.START) then openOverlay("control") end
  if pressed(Pads.SELECT) then openOverlay("monitor") end
end

-- Plain text tabs with a sliding accent underline; clock on the right.
local function drawTopRow()
  local cy, scale, gap = LAYOUT.topCY, 0.52, 22
  local x, selX, selW = M, M, 0
  for i, tab in ipairs(TABS) do
    local w = round(measure(ui, tab.label, scale))
    local sel = (i - 1) == desk.tab
    if sel then selX, selW = x, w end
    textV(ui, x, cy, tab.label, scale, sel and COLORS.text or COLORS.textFaint)
    x = x + w + gap
  end

  desk.tabX:set(selX)
  desk.tabW:set(selW)
  if not settings.animations then desk.tabX:snap(selX) desk.tabW:snap(selW) end
  Draw.rect(round(desk.tabX:step()), cy + 12, round(desk.tabW:step()), 2, accent(settings.accent))

  local clock = clockText()
  textV(ui, W - M - measure(ui, clock, 0.52), cy, clock, 0.52, COLORS.text)
end

local function drawGrid()
  local list = visibleApps()
  desk.scroll:set(desk.topRow)
  if not settings.animations then desk.scroll:snap(desk.topRow) end
  local scroll = desk.scroll:step()

  if #list == 0 then
    local msg = TABS[desk.tab + 1].label == "Home" and "Press Square on any app to pin it here" or "Nothing here yet"
    textV(ui, W / 2, GRID.top + GRID.pitchY / 2 + GRID.tile / 2, msg, 0.52, COLORS.textFaint, Font.ALIGN_HCENTER)
    return
  end

  for idx, app in ipairs(list) do
    local i = idx - 1
    local col, row = i % GRID.cols, i // GRID.cols - scroll
    local focused = i == desk.focus
    app.fx:set(focused and 1 or 0)
    local f = settings.animations and app.fx:step() or app.fx:snap(focused and 1 or 0).v
    if row >= -0.6 and row <= GRID.rows - 0.4 then
      local dim = clamp(1 - max(0, -row, row - (GRID.rows - 1)) * 1.6, 0, 1)
      local cx = GRID.left + col * GRID.pitchX
      local cy = GRID.top + GRID.tile / 2 + row * GRID.pitchY
      drawTile(app, cx, cy, GRID.tile, f, dim * (focused and 1 or 0.8))
    end
  end

  -- Thin scroll indicator in the right gutter, only when there is more to see.
  local rows = math.ceil(#list / GRID.cols)
  if rows > GRID.rows then
    local trackH, y0 = GRID.pitchY * (GRID.rows - 1) + GRID.tile, GRID.top
    local tx = W - M / 2 - 1
    Draw.rect(tx, y0, 2, trackH, rgba(255, 255, 255, 0.10))
    local thumb = trackH * GRID.rows / rows
    Draw.rect(tx, round(y0 + (trackH - thumb) * (scroll / (rows - GRID.rows))), 2, round(thumb), accent(settings.accent))
  end
end

-- The focused app, described once and large, instead of a label under every tile.
local function drawFocused()
  local app = focusedApp()
  if not app then return end
  textV(ui, M, LAYOUT.nameCY, fit(ui, app.name, 0.95, W - 2 * M), 0.95, COLORS.text)
  textV(ui, M, LAYOUT.metaCY, categoryOf(app.category).label .. "  -  " .. app.author, 0.46, accent(settings.accent))
  if app.blurb ~= "" then
    for i, line in ipairs(wrap(ui, app.blurb, 0.48, W - 2 * M)) do
      if i > 2 then break end
      textV(ui, M, LAYOUT.blurbCY + (i - 1) * 20, line, 0.48, COLORS.textDim)
    end
  end
end

local function drawDesktop()
  drawTopRow()
  drawGrid()
  drawFocused()
  hints({ { "cross", "Open" }, { "triangle", "Options" }, { "L1/R1", "Switch" }, { "START", "Menu" } }, W - M, LAYOUT.hintsCY, ui)
end

-- Lock screen ---------------------------------------------------------------------

local lock = { y = Spring(0, 0.18), fade = Spring(coldBoot and 0 or 1, 0.04), leaving = false }

local function lockInput()
  if not lock.leaving and frame > 20 and anyButton() then
    lock.leaving = true
    lock.y:set(-H)
  end
end

local function drawLock()
  local y = settings.animations and lock.y:step() or (lock.leaving and -H or 0)
  if lock.leaving and y <= -H + 2 then
    state = "desktop"
    lock.leaving = false
    lock.y:snap(0)
    return
  end
  local d = uikit.now(settings)
  Draw.rect(0, y, W, H, rgba(0, 0, 0, 0.25))
  text(display, W / 2, y + 120, clockText(d), 2.6, COLORS.text, Font.ALIGN_HCENTER)
  text(ui, W / 2, y + 200, dateText(d), 0.7, COLORS.textDim, Font.ALIGN_HCENTER)

  local pulse = 0.45 + 0.35 * sin(frame / 22)
  text(ui, W / 2, y + H - 70, "Press any button to unlock", 0.55, rgba(255, 255, 255, pulse), Font.ALIGN_HCENTER)
  text(heavy, W / 2, y + H - 40, "ATHENA OS", 0.38, COLORS.textFaint, Font.ALIGN_HCENTER)
end

-- Overlays --------------------------------------------------------------------------

local sheet = Spring(0, 0.25)
local ccFocus, setFocus, confirmPower = 0, 0, false

openOverlay = function(name)
  overlay = name
  sheet:snap(0):set(1)
  ccFocus, setFocus, confirmPower = 0, 0, false
  if name == "monitor" then refreshStats() end
end

local function closeOverlay()
  if overlay == "settings" then
    storage.saveSettings(settings)
    notify("Settings saved")
  end
  overlay = nil
end

local function scrim() Draw.rect(0, 0, W, H, rgba(0, 0, 0, 0.55 * sheet.v)) end

-- Details ---------------------------------------------------------------------

local function detailsInput()
  local app = focusedApp()
  if not app or pressed(Pads.CIRCLE) or pressed(Pads.TRIANGLE) then closeOverlay() return end
  if pressed(Pads.CROSS) then closeOverlay() launch(app) end
  if pressed(Pads.SQUARE) then togglePin(app) end
end

local function drawDetails()
  local app = focusedApp()
  if not app then return end
  local t = easeOut(sheet:step())
  scrim()
  local w, h = 440, 250
  local x, y = (W - w) / 2, (H - h) / 2 + (1 - t) * 30
  panel(x, y, w, h, 18, COLORS.glassHi)

  drawTile(app, x + 70, y + 70, 92)
  local cat = categoryOf(app.category)
  textV(ui, x + 136, y + 42, fit(ui, app.name, 0.85, w - 160), 0.85, COLORS.text)
  textV(ui, x + 136, y + 72, app.author .. (app.version ~= "" and ("  -  v" .. app.version) or ""), 0.46, COLORS.textDim)
  local chipW = round(measure(ui, cat.label, 0.42)) + 16
  roundRect(x + 136, y + 89, chipW, 18, 9, rgba(cat.rgb[1], cat.rgb[2], cat.rgb[3], 0.45))
  textV(ui, x + 144, y + 98, cat.label, 0.42, COLORS.text)

  local blurb = app.blurb ~= "" and app.blurb or "An AthenaEnv sample."
  for i, line in ipairs(wrap(ui, blurb, 0.5, w - 48)) do
    if i > 2 then break end
    textV(ui, x + 24, y + 146 + (i - 1) * 20, line, 0.5, COLORS.text)
  end

  local runs = session.launches[app.file] or 0
  textV(ui, x + 24, y + 190, string.format("%s   -   Home: %s   -   Opened %d time%s", app.file, app.exit, runs, runs == 1 and "" or "s"),
        0.42, COLORS.textFaint)

  Draw.rect(x + 24, y + h - 42, w - 48, 1, COLORS.stroke)
  local pinned = contains(session.pinned, app.file)
  hints({ { "cross", "Open" }, { "square", pinned and "Unpin" or "Pin" }, { "circle", "Close" } }, x + w - 24, y + h - 21, ui)
end

-- Control center ----------------------------------------------------------------

local CC_ITEMS = {
  { label = "Settings",       sub = "Theme, clock, wallpaper", run = function() openOverlay("settings") end },
  { label = "System monitor", sub = "Memory, video, hardware", run = function() openOverlay("monitor") end },
  { label = "File manager",   sub = "Browse devices", run = function()
      local a = appByFile("app_filemgr.lua")
      if a then closeOverlay() launch(a) else notify("File manager not installed") end
    end },
  { label = "Lock",           sub = "Back to the lock screen", run = function() closeOverlay() state = "lock" lock.fade:snap(1) end },
  { label = "Restart shell",  sub = "Reload AthenaOS",         run = function() pendingReload = "main.lua" end },
  { label = "Power",          sub = "Exit to the PS2 browser", run = function() confirmPower = true end },
}

local function controlInput()
  if confirmPower then
    if pressed(Pads.CROSS) then storage.saveSession(session) System.exitToBrowser() end
    if pressed(Pads.CIRCLE) then confirmPower = false end
    return
  end
  if pressed(Pads.CIRCLE) or pressed(Pads.START) then closeOverlay() return end
  if nav.down then ccFocus = (ccFocus + 1) % #CC_ITEMS end
  if nav.up then ccFocus = (ccFocus + #CC_ITEMS - 1) % #CC_ITEMS end
  if pressed(Pads.CROSS) then CC_ITEMS[ccFocus + 1].run() end
end

local function drawControl()
  local t = easeOut(sheet:step())
  scrim()
  local w = 270
  local x = W - w * t
  Draw.rect(x, 0, w, H, rgba(16, 18, 30, 0.92))
  Draw.rect(x, 0, 1, H, COLORS.stroke)

  text(display, x + 22, 22, clockText(), 1.25, COLORS.text)
  text(ui, x + 24, 72, dateText(), 0.46, COLORS.textDim)

  for i, item in ipairs(CC_ITEMS) do
    local iy = 104 + (i - 1) * 44
    local sel = (i - 1) == ccFocus
    if sel then roundRect(x + 12, iy, w - 24, 38, 10, accent(settings.accent, 0.85)) end
    textV(ui, x + 26, iy + 13, item.label, 0.54, COLORS.text)
    textV(ui, x + 26, iy + 28, item.sub, 0.40, sel and COLORS.text or COLORS.textFaint)
  end

  local sy = H - 70
  for i, row in ipairs({ { "RAM", stats.ram }, { "IOP", stats.iop }, { "VRAM", stats.vram } }) do
    local cy = sy + 8 + (i - 1) * 18
    textV(ui, x + 24, cy, row[1], 0.40, COLORS.textDim)
    bar(x + 70, cy - 3, w - 94, 6, row[2], accent(settings.accent))
  end

  if confirmPower then
    Draw.rect(0, 0, W, H, rgba(0, 0, 0, 0.5))
    local pw, ph = 320, 120
    local px, py = (W - pw) / 2, (H - ph) / 2
    panel(px, py, pw, ph, 16, COLORS.glassHi)
    textV(ui, W / 2, py + 32, "Exit AthenaOS?", 0.75, COLORS.text, Font.ALIGN_HCENTER)
    textV(ui, W / 2, py + 60, "You will return to the PS2 browser.", 0.46, COLORS.textDim, Font.ALIGN_HCENTER)
    hints({ { "cross", "Exit" }, { "circle", "Cancel" } }, px + pw - 24, py + ph - 21, ui)
  end
end

-- Settings ------------------------------------------------------------------------

local SETTINGS_ROWS = {
  { label = "Accent color", value = function() return ACCENTS[settings.accent + 1].name end,
    change = function(d) settings.accent = (settings.accent + d + #ACCENTS) % #ACCENTS classicBg = nil end },
  { label = "Wallpaper", value = function() return WALLPAPERS[settings.wallpaper % WALLPAPER_COUNT] end,
    change = function(d) settings.wallpaper = (settings.wallpaper + d + WALLPAPER_COUNT) % WALLPAPER_COUNT end },
  { label = "24-hour clock", bool = "clock24" },
  { label = "Clock offset", value = function() return string.format("UTC %s%dh", settings.utcOffset >= 0 and "+" or "", settings.utcOffset) end,
    change = function(d) settings.utcOffset = clamp(settings.utcOffset + d, -12, 14) end },
  { label = "Lock screen on boot", bool = "lockScreen" },
  { label = "Show FPS", bool = "showFps", after = function() Screen.setFrameCounter(settings.showFps) end },
  { label = "Animations", bool = "animations" },
}

local function settingsInput()
  if pressed(Pads.CIRCLE) or pressed(Pads.TRIANGLE) then closeOverlay() return end
  if nav.down then setFocus = (setFocus + 1) % #SETTINGS_ROWS end
  if nav.up then setFocus = (setFocus + #SETTINGS_ROWS - 1) % #SETTINGS_ROWS end
  local row = SETTINGS_ROWS[setFocus + 1]
  local d = (nav.right or pressed(Pads.CROSS)) and 1 or (nav.left and -1 or 0)
  if d == 0 then return end
  if row.bool then settings[row.bool] = not settings[row.bool] else row.change(d) end
  if row.after then row.after() end
end

local function drawSettings()
  local t = easeOut(sheet:step())
  scrim()
  local w, h = 420, 104 + #SETTINGS_ROWS * 36
  local x, y = (W - w) / 2, (H - h) / 2 + (1 - t) * 30
  panel(x, y, w, h, 18, COLORS.glassHi)
  textV(ui, x + 24, y + 30, "Settings", 0.8, COLORS.text)

  for i, row in ipairs(SETTINGS_ROWS) do
    local cy = y + 74 + (i - 1) * 36
    local sel = (i - 1) == setFocus
    local right = x + w - 26
    if sel then roundRect(x + 12, cy - 16, w - 24, 32, 10, rgba(255, 255, 255, 0.10)) end
    textV(ui, x + 26, cy, row.label, 0.52, sel and COLORS.text or COLORS.textDim)
    if row.bool then
      toggle(right - 36, cy - 10, settings[row.bool], accent(settings.accent))
    else
      local v = row.value()
      local vw = round(measure(ui, v, 0.5))
      local arrowW = 14
      textV(ui, right - arrowW - vw, cy, v, 0.5, COLORS.text)
      if sel then
        textV(ui, right - arrowW - vw - 16, cy, "<", 0.5, accent(settings.accent))
        textV(ui, right - 8, cy, ">", 0.5, accent(settings.accent))
      end
      if row.label == "Accent color" then
        Draw.circle(right - arrowW - vw - (sel and 30 or 14), cy, 6, accent(settings.accent), true)
      end
    end
  end
  Draw.rect(x + 24, y + h - 42, w - 48, 1, COLORS.stroke)
  hints({ { "D-pad", "Change" }, { "circle", "Done" } }, x + w - 24, y + h - 21, ui)
end

-- System monitor --------------------------------------------------------------------

local function upText()
  local s = uptime:getTime() // 1000000
  return string.format("%d:%s", s // 60, pad2(s % 60))
end

local function monitorInput()
  if pressed(Pads.CIRCLE) or pressed(Pads.SELECT) or pressed(Pads.TRIANGLE) then closeOverlay() end
end

local function drawMonitor()
  local t = easeOut(sheet:step())
  scrim()
  local w, h = 520, 350
  local x, y = (W - w) / 2, (H - h) / 2 + (1 - t) * 30
  panel(x, y, w, h, 18, COLORS.glassHi)
  textV(ui, x + 24, y + 30, "System monitor", 0.8, COLORS.text)
  local up = "Up " .. upText()
  textV(ui, x + w - 24 - measure(ui, up, 0.46), y + 30, up, 0.46, COLORS.textDim)

  local rows = {
    { "EE RAM",  stats.ram,  kb(stats.ramUsed) .. " / " .. kb(cpuInfo.RAMSize) },
    { "IOP RAM", stats.iop,  kb(stats.iopUsed) .. " / 2048 KB" },
    { "VRAM",    stats.vram, kb(stats.vramUsed) .. " / " .. kb(stats.vramSize) },
  }
  for i, r in ipairs(rows) do
    local cy = y + 68 + (i - 1) * 40
    textV(ui, x + 24, cy, r[1], 0.5, COLORS.text)
    textV(ui, x + w - 24 - measure(ui, r[3], 0.44), cy, r[3], 0.44, COLORS.textDim)
    bar(x + 24, cy + 12, w - 48, 8, r[2], r[2] > 0.85 and COLORS.warn or accent(settings.accent))
  end

  local padList = {}
  for _, p in ipairs(stats.pads) do padList[#padList + 1] = "Port " .. (p + 1) end
  local info = {
    { "CPU", string.format("R5900 rev %d  -  I$ %s  D$ %s", cpuInfo.revision, kb(cpuInfo.ICacheSize), kb(cpuInfo.DCacheSize)) },
    { "GPU", string.format("GS id %d rev %d", gpuInfo.id, gpuInfo.revision) },
    { "Video", string.format("%dx%d  %s%s", W, H, canvas.interlace ~= 0 and "interlaced" or "progressive",
                             settings.showFps and ("  -  " .. Screen.getFPS(60) .. " FPS") or "") },
    { "Temp", stats.temp and (stats.temp .. " C") or "Sensor not available" },
    { "Pads", #padList > 0 and table.concat(padList, ", ") or "None" },
    { "Script", "Lua " .. _VERSION:match("%d+%.%d+") .. "  -  heap " .. kb(collectgarbage("count") * 1024) },
    { "Boot", System.boot_path ~= "" and System.boot_path or "unknown" },
  }
  for i, row in ipairs(info) do
    local cy = y + 196 + (i - 1) * 19
    textV(ui, x + 24, cy, row[1], 0.44, COLORS.textDim)
    textV(ui, x + 90, cy, fit(ui, tostring(row[2]), 0.44, w - 114), 0.44, COLORS.text)
  end
  Draw.rect(x + 24, y + h - 42, w - 48, 1, COLORS.stroke)
  hints({ { "circle", "Close" } }, x + w - 24, y + h - 21, ui)
end

local OVERLAYS = {
  details  = { detailsInput,  drawDetails },
  control  = { controlInput,  drawControl },
  settings = { settingsInput, drawSettings },
  monitor  = { monitorInput,  drawMonitor },
}

-- Launching -------------------------------------------------------------------------

launch = function(app)
  if launching then return end
  launching = { app = app, t = 0 }
  session.launching = app.file
  local recents = JSON.array({ app.file })
  for _, f in ipairs(session.recents) do
    if f ~= app.file and #recents < 8 then recents[#recents + 1] = f end
  end
  session.recents = recents
  session.launches[app.file] = (session.launches[app.file] or 0) + 1
  storage.saveSession(session)
end

local function drawLaunch()
  local LEN = settings.animations and 22 or 2
  launching.t = launching.t + 1
  local t = easeOut(launching.t / LEN)
  Draw.rect(0, 0, W, H, rgba(0, 0, 0, t))
  drawTile(launching.app, W / 2, H / 2 - 20, lerp(GRID.tile, 120, t), 0, 1)
  text(ui, W / 2, H / 2 + 56, launching.app.name, 0.7, rgba(255, 255, 255, t), Font.ALIGN_HCENTER)
  if launching.t >= LEN + 4 then pendingReload = launching.app.file end
end

-- Main loop -------------------------------------------------------------------------

state = (coldBoot and settings.lockScreen) and "lock" or "desktop"

-- Start on Home (it holds the app just closed, as the most recent), or All when empty.
if #homeApps() == 0 then setTab(1) end

if returnedFrom then
  notify("Closed " .. returnedFrom.name)
  for i, a in ipairs(visibleApps()) do
    if a == returnedFrom then
      desk.focus = i - 1
      desk.topRow = max(0, (i - 1) // GRID.cols - GRID.rows + 1)
      desk.scroll:snap(desk.topRow)
      break
    end
  end
end

while true do
  frame = frame + 1
  pollInput()
  if frame % 30 == 0 then refreshStats() end

  if not launching then
    if state == "lock" then lockInput()
    elseif overlay then OVERLAYS[overlay][1]()
    else desktopInput() end
  end

  Screen.clear(Color.new(0, 0, 0))
  drawWallpaper()

  if state == "lock" then
    drawLock()
  else
    drawDesktop()
    if overlay then OVERLAYS[overlay][2]() end
  end

  drawToast()
  if launching then drawLaunch() end

  -- Cold-boot fade-in from black
  local fade = lock.fade:step()
  if fade < 0.99 then Draw.rect(0, 0, W, H, rgba(0, 0, 0, 1 - fade)) end
  if coldBoot then lock.fade:set(1) end

  Screen.flip()

  if pendingReload then
    -- Outer scope on purpose: System.reload() never returns.
    if pendingReload == "main.lua" then
      session.launching = "main.lua"
      storage.saveSession(session)
    end
    Screen.clear(Color.new(0, 0, 0))
    Screen.flip()
    System.reload(pendingReload)
  end
end
