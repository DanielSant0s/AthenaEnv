-- {"name": "File manager", "author": "Daniel Santos", "version": "10192026", "icon": "lfm_icon.png", "file": "app_filemgr.lua", "category": "system", "blurb": "Browse memory cards, USB and HDD; copy, move and extract archives.", "exit": "TRIANGLE"}

local app = require "athenaos.app"
local COLORS, rgba = app.COLORS, app.rgba

local ui = app.AppUI{ title = "File manager", category = "system",
                      hints = { { "cross", "Open" }, { "circle", "Up" }, { "R1", "Actions" } } }

Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)

local root = {
  { name = "cdfs:/", size = 0, dir = true },
  { name = "host:/", size = 0, dir = true },
  { name = "mass:/", size = 0, dir = true },
  { name = "mc0:/", size = 0, dir = true },
  { name = "mc1:/", size = 0, dir = true },
}

local ACTIONS = { "Copy", "Move", "Paste", "Rename", "Delete" }
local COPY, MOVE, PASTE, RENAME, DELETE = 1, 2, 3, 4, 5

-- path is "" (device list) or always ends with "/"
local path = ""
local file = root
local sel = 1         -- selected entry
local top = 1         -- first visible entry
local menu = false    -- actions menu open
local action = COPY

-- Clipboard for copy/move -> paste
local src, srcfile, fileop = "", nil, COPY

local ROW_H = 24

local function list(newPath)
  path = newPath
  file = path == "" and root or System.listDir(path)
  sel = math.max(1, math.min(sel, #file))
end

local function goUp()
  -- "mc0:/a/b/" -> "mc0:/a/"; a device root goes back to the device list
  sel = 1
  list(path:match("^(.*/)[^/]+/$") or "")
end

local function endsWith(s, suffix) return s:sub(-#suffix) == suffix end

local function open(entry)
  local full = path .. entry.name
  if entry.dir then
    sel = 1
    list(path == "" and entry.name or full .. "/")
  elseif endsWith(entry.name, ".lua") then
    System.reload(full)
  elseif endsWith(entry.name:lower(), ".elf") then
    System.loadELF(full)
  elseif endsWith(entry.name, ".zip") then
    local archive = Archive.open(full)
    if archive then
      archive:extractAll()
      archive:close()
    end
    list(path) -- update file list
  elseif endsWith(entry.name, ".tar.gz") then
    Archive.untar(full)
    list(path)
  end
end

local function runAction(entry)
  if action == COPY or action == MOVE then
    if not entry or path == "" then return end
    fileop = action
    src = path .. entry.name
    srcfile = entry.name
  elseif action == PASTE then
    if not srcfile or path == "" then return end
    local dst = path .. srcfile
    if fileop == COPY then System.copyFile(src, dst) else System.moveFile(src, dst) end
    if fileop == MOVE then srcfile = nil end
  elseif action == RENAME then
    return
  elseif action == DELETE then
    if not entry or path == "" then return end
    if entry.dir then System.removeDirectory(path .. entry.name) else os.remove(path .. entry.name) end
  end
  menu = false
  list(path)
end

local function formatSize(bytes)
  if bytes < 1024 then return bytes .. " B" end
  if bytes < 1048576 then return string.format("%.1f KB", bytes / 1024) end
  return string.format("%.1f MB", bytes / 1048576)
end

local pad = Pads.get()

while true do
  pad:update()

  if ui:homeRequested(pad) then break end

  if pad:justPressed(Pads.R1) then
    menu = not menu
    action = COPY
  end

  local n = #file
  if menu then
    if pad:justPressed(Pads.DOWN) then action = action % #ACTIONS + 1 end
    if pad:justPressed(Pads.UP) then action = (action - 2) % #ACTIONS + 1 end
    if pad:justPressed(Pads.CROSS) then runAction(file[sel]) end
    if pad:justPressed(Pads.CIRCLE) then menu = false end
  else
    if pad:justPressed(Pads.DOWN) and n > 0 then sel = sel % n + 1 end
    if pad:justPressed(Pads.UP) and n > 0 then sel = (sel - 2) % n + 1 end
    if pad:justPressed(Pads.CROSS) and file[sel] then open(file[sel]) end
    if pad:justPressed(Pads.CIRCLE) then goUp() end
  end

  ui:background()
  local c = ui.content
  local x, y, w, h = c.x, c.y, c.w, c.h

  -- Location bar
  ui:heading(x, y + 6, "Location")
  ui:textV(x + 72, y + 6, ui:fit(path ~= "" and path or "Devices", 0.5, w - 72), 0.5, COLORS.text)

  -- File list
  local ly, lh = y + 20, h - 20
  local rows = (lh - 12) // ROW_H
  if sel < top then top = sel end
  if sel >= top + rows then top = sel - rows + 1 end
  ui:panel(x, ly, w, lh)

  if #file == 0 then ui:textV(x + w / 2, ly + lh / 2, "Empty folder", 0.5, COLORS.textDim, Font.ALIGN_HCENTER) end

  for i = top, math.min(#file, top + rows - 1) do
    local entry = file[i]
    local cy = ly + 6 + (i - top) * ROW_H + ROW_H / 2
    if i == sel then
      Draw.rect(x + 6, cy - ROW_H / 2, w - 12, ROW_H, rgba(255, 255, 255, menu and 0.06 or 0.10))
      Draw.rect(x + 6, cy - ROW_H / 2, 3, ROW_H, ui:accent())
    end
    -- Folder / file marker
    if entry.dir then Draw.rect(x + 18, cy - 5, 14, 10, ui:accent(0.85))
    else Draw.rect(x + 20, cy - 6, 10, 12, rgba(255, 255, 255, 0.45)) end
    ui:textV(x + 42, cy, ui:fit(entry.name, 0.46, w - 160), 0.46, i == sel and COLORS.text or COLORS.textDim)
    ui:textV(x + w - 16, cy, entry.dir and "Folder" or formatSize(entry.size), 0.42, COLORS.textFaint, Font.ALIGN_RIGHT)
  end

  -- Actions menu
  if menu then
    local mw, mh = 150, #ACTIONS * 30 + 16
    local mx, my = x + w - mw - 12, ly + 12
    ui:panel(mx, my, mw, mh)
    for i, label in ipairs(ACTIONS) do
      local cy = my + 8 + (i - 1) * 30 + 15
      local disabled = (i == PASTE and not srcfile) or i == RENAME
      if i == action then Draw.rect(mx + 6, cy - 13, mw - 12, 26, ui:accent(0.85)) end
      ui:textV(mx + 18, cy, label, 0.48, disabled and COLORS.textFaint or COLORS.text)
    end
  end

  ui.status = srcfile and ((fileop == COPY and "Copy" or "Move") .. ": " .. ui:fit(srcfile, 0.44, 140)) or ""
  ui:chrome(menu and { { "cross", "Run" }, { "circle", "Close" } } or ui.hintList, #file .. " items")
  Screen.flip()
end

app.exitToShell()
