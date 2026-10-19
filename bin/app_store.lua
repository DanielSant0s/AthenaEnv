-- {"name": "App store", "author": "Daniel Santos", "version": "10192026", "file": "app_store.lua", "category": "network", "blurb": "Download homebrew from the online Brew Store catalog.", "exit": "TRIANGLE"}

require "athena_network.erl" -- registers the NETMAN/SMAP drivers loaded below
local app = require "athenaos.app"
local COLORS, rgba = app.COLORS, app.rgba

local ui = app.AppUI{ title = "App store", category = "network" }

Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)

local function bootFile(name)
  local base = System.boot_path
  return base:sub(-1) == "/" and base .. name or base .. "/" .. name
end

local function endsWith(s, suffix) return s:sub(-#suffix) == suffix end

local function load_app_db(fname)
  return JSON.parse(io.readfile(fname))
end

local function load_network_driver()
  IOP.reset()

  IOP.loadModule("usbmass_bd")
  local ok, err = pcall(IOP.loadModule, "ps2fs")
  if not ok then console.log(err) end
  IOP.loadModule("cdfs")
  IOP.loadModule("mcserv")

  IOP.loadModule("padman")
  IOP.loadModule("SMAP")

  Network.init()
end

local req = Request()
req.keepalive = true

local LOADING, MAIN_MENU, DLING_MENU = 0, 1, 3
local app_state = LOADING

local LD_INITIAL, LD_NETWORK, LD_PKGLIST, LD_PKGQNTD = 0, 1, 2, 3
local loading_state = LD_INITIAL
local loading_log = { "Athena package manager" }

local app_list = nil
local icons = {}
local sel, top = 1, 1

local pad = Pads.get()

local DETAILS, TODOWNLOAD, DOWNLOADING, DOWNLOADED, EXTRACTING, EXTRACTED = 0, 1, 2, 3, 4, 5
local dl_state = DETAILS
local dling_log = {}
local terminate = false
local transfering = false

local NOT_UPDATED, UPDATING, UPDATED = 0, 1, 2
local update_state = NOT_UPDATED

local ROW_H = 48

-- Views -------------------------------------------------------------------------

local function drawLog(lines, x, y, w, h)
  ui:panel(x, y, w, h)
  for i, line in ipairs(lines) do
    ui:textV(x + 16, y + 20 + (i - 1) * 20, ui:fit(line, 0.46, w - 32), 0.46, i == #lines and COLORS.text or COLORS.textDim)
  end
end

local function drawIcon(i, x, cy, size)
  local icn = icons[i]
  if not icn then return end
  icn.width, icn.height = size, size
  icn:draw(x, cy - size / 2)
end

local function drawDetails(x, y, w, h)
  local pkg = app_list[sel]
  ui:panel(x, y, w, h)
  drawIcon(sel, x + 20, y + 52, 64)
  ui:textV(x + 100, y + 38, ui:fit(pkg.name, 0.8, w - 120), 0.8, COLORS.text)
  ui:textV(x + 100, y + 66, pkg.category, 0.46, COLORS.textDim)

  ui:heading(x + 20, y + 112, "Package")
  ui:textV(x + 20, y + 132, ui:fit(pkg.fname, 0.44, w - 40), 0.44, COLORS.text)
  ui:heading(x + 20, y + 162, "Source")
  for i, line in ipairs(ui:wrap(pkg.link, 0.40, w - 40)) do
    if i > 3 then break end
    ui:textV(x + 20, y + 182 + (i - 1) * 16, ui:fit(line, 0.40, w - 40), 0.40, COLORS.textDim)
  end

  if app_state == DLING_MENU then
    local first = math.max(1, #dling_log - 3)
    for i = first, #dling_log do
      ui:textV(x + 20, y + 250 + (i - first) * 20, ui:fit(dling_log[i], 0.46, w - 40), 0.46, COLORS.text)
    end
  end
end

local function drawCatalog()
  local c = ui.content
  local x, y, w, h = c.x, c.y, c.w, c.h
  local listW = 260
  local rows = (h - 12) // ROW_H
  if sel < top then top = sel end
  if sel >= top + rows then top = sel - rows + 1 end

  ui:panel(x, y, listW, h)
  for i = top, math.min(#app_list, top + rows - 1) do
    local cy = y + 6 + (i - top) * ROW_H + ROW_H / 2
    if i == sel then
      Draw.rect(x + 6, cy - ROW_H / 2 + 2, listW - 12, ROW_H - 4, rgba(255, 255, 255, 0.10))
      Draw.rect(x + 6, cy - ROW_H / 2 + 2, 3, ROW_H - 4, ui:accent())
    end
    drawIcon(i, x + 18, cy, 36)
    ui:textV(x + 64, cy - 7, ui:fit(app_list[i].name, 0.5, listW - 80), 0.5, i == sel and COLORS.text or COLORS.textDim)
    ui:textV(x + 64, cy + 10, app_list[i].category, 0.38, COLORS.textFaint)
  end

  drawDetails(x + listW + 16, y, w - listW - 16, h)
end

-- Main loop ---------------------------------------------------------------------

while true do
  pad:update()

  local hints = {}

  if app_state == LOADING then
    if loading_state == LD_INITIAL then
      loading_log[#loading_log + 1] = "Loading network driver..."
      loading_state = LD_NETWORK
    elseif loading_state == LD_NETWORK then
      load_network_driver()
      loading_log[#loading_log + 1] = "Loading package list, it's gonna take a while..."
      loading_state = LD_PKGLIST
    elseif loading_state == LD_PKGLIST then
      if update_state == NOT_UPDATED then
        req:asyncDownload("https://raw.githubusercontent.com/DanielSant0s/brewstore-db/main/brew_data.json", "brew_data.json")
        update_state = UPDATING
        transfering = true
      elseif update_state == UPDATING then
        if req:ready(5000) then
          transfering = false
          update_state = UPDATED
        end
      else
        app_list = load_app_db("brew_data.json")
        loading_log[#loading_log + 1] = #app_list .. " packages found."
        print("ATHENA_STORE_PACKAGES", #app_list)
        loading_state = LD_PKGQNTD
      end
    elseif loading_state == LD_PKGQNTD then
      for i, pkg in ipairs(app_list) do
        local ok, icn = pcall(Image, "store/icons/" .. pkg.icon)
        if ok then
          icn.filter = LINEAR
          icons[i] = icn
        end
      end
      app_state = MAIN_MENU
    end

    ui:background()
    drawLog(loading_log, ui.content.x, ui.content.y, ui.content.w, 40 + #loading_log * 20)

  elseif app_state == MAIN_MENU then
    local n = #app_list
    if pad:justPressed(Pads.DOWN) then sel = sel % n + 1 end
    if pad:justPressed(Pads.UP) then sel = (sel - 2) % n + 1 end
    if pad:justPressed(Pads.CROSS) then app_state = DLING_MENU end
    if ui:homeRequested(pad) then terminate = true end

    ui:background()
    drawCatalog()
    hints = { { "D-pad", "Browse" }, { "cross", "Details" } }

  elseif app_state == DLING_MENU then
    local pkg = app_list[sel]
    if dl_state == DETAILS then
      if pad:justPressed(Pads.CROSS) then
        dling_log[#dling_log + 1] = "Downloading package..."
        dling_log[#dling_log + 1] = "This is an alpha version, it will take a long time"
        dl_state = TODOWNLOAD
      end
      if pad:justPressed(Pads.CIRCLE) then app_state = MAIN_MENU end
      if ui:homeRequested(pad) then terminate = true end
      hints = { { "cross", "Download" }, { "circle", "Back" } }
    elseif dl_state == TODOWNLOAD then
      req:asyncDownload(pkg.link, bootFile(pkg.fname))
      transfering = true
      dl_state = DOWNLOADING
    elseif dl_state == DOWNLOADING then
      local ok, done = pcall(req.ready, req, 30)
      if not ok then
        dling_log[#dling_log + 1] = tostring(done)
        dling_log[#dling_log + 1] = "Restarting application..."
        for _ = 1, 300 do
          ui:background()
          drawCatalog()
          ui:chrome({})
          Screen.flip()
        end
        Network.deinit()
        System.reload("app_store.lua")
      elseif done then
        transfering = false
        dl_state = DOWNLOADED
      end
    elseif dl_state == DOWNLOADED then
      if not endsWith(pkg.fname:lower(), ".elf") then
        dl_state = EXTRACTING
        dling_log[#dling_log + 1] = "Extracting files..."
      else
        dl_state = EXTRACTED
      end
    elseif dl_state == EXTRACTING then
      local pkg_path = bootFile(pkg.fname)
      if endsWith(pkg.fname, ".tar.gz") then
        Archive.untar(pkg_path)
      elseif endsWith(pkg.fname, ".zip") then
        local arc = Archive.open(pkg_path)
        arc:extractAll()
        arc:close()
      end
      dl_state = EXTRACTED
    elseif dl_state == EXTRACTED then
      app_state = MAIN_MENU
      dl_state = DETAILS
      dling_log = {}
    end

    ui:background()
    drawCatalog()
  end

  if terminate then break end

  ui.status = transfering and (req:getAsyncSize() .. " bytes") or ""
  -- Home is only offered where leaving is safe (not mid-download).
  ui.home = (app_state == MAIN_MENU or (app_state == DLING_MENU and dl_state == DETAILS)) and { "triangle", "Home" } or nil
  ui:chrome(hints)
  Screen.flip()
end

Network.deinit()

app.exitToShell()
