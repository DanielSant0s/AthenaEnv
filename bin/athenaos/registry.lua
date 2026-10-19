-- AthenaOS app registry (Lua port of athenaos/registry.js).
--
-- Any .lua file in the boot folder whose first line is a JSON comment is an app:
--   -- {"name": "Pong", "author": "...", "version": "...", "file": "pong.lua",
--   --  "icon": "pong_icon.png", "category": "games", "blurb": "...", "exit": "START"}
-- `icon`, `category`, `blurb` and `exit` are optional.

local M = {}

M.CATEGORIES = {
  { id = "all",     label = "All",     rgb = { 104, 112, 255 } },
  { id = "games",   label = "Games",   rgb = { 255,  95, 120 } },
  { id = "3d",      label = "3D",      rgb = {  40, 160, 255 } },
  { id = "2d",      label = "2D",      rgb = {  40, 210, 160 } },
  { id = "media",   label = "Media",   rgb = { 255, 170,  40 } },
  { id = "network", label = "Network", rgb = {  90, 200, 255 } },
  { id = "input",   label = "Input",   rgb = { 170,  90, 255 } },
  { id = "system",  label = "System",  rgb = { 150, 160, 180 } },
}

function M.categoryOf(id)
  for _, c in ipairs(M.CATEGORIES) do
    if c.id == id then return c end
  end
  return M.CATEGORIES[#M.CATEGORIES]
end

local function readHeader(file)
  local f = io.open(file, "r")
  if not f then return nil end
  local line = f:read("l")
  f:close()
  if not line then return nil end
  local json = line:gsub("^%s*%-%-%s*", "")
  if json:sub(1, 1) ~= "{" then return nil end
  local ok, meta = pcall(JSON.parse, json)
  if not ok then
    print("[athenaos] bad app header in " .. file .. ": " .. tostring(meta))
    return nil
  end
  return meta
end

local SHELL_FILES = { ["main.lua"] = true }

function M.scanApps()
  local apps = {}
  for _, entry in ipairs(System.listDir()) do
    local file = entry.name
    if not entry.dir and file:lower():sub(-4) == ".lua" and not SHELL_FILES[file] then
      local meta = readHeader(file)
      if meta and meta.name then
        apps[#apps + 1] = {
          name = meta.name,
          author = meta.author or "Unknown",
          version = meta.version or "",
          file = meta.file or file,
          iconPath = (meta.icon and io.exists(meta.icon)) and meta.icon or nil,
          icon = nil, -- Image, loaded lazily by the shell
          category = M.categoryOf(meta.category or "system").id,
          blurb = meta.blurb or "",
          exit = meta.exit or "TRIANGLE",
        }
      end
    end
  end
  table.sort(apps, function(a, b) return a.name:lower() < b.name:lower() end)
  return apps
end

function M.initials(name)
  local words = {}
  for w in name:gsub("[^%w ]", " "):gmatch("%S+") do words[#words + 1] = w end
  if #words == 0 then return "?" end
  if #words == 1 then return words[1]:sub(1, 2):upper() end
  return (words[1]:sub(1, 1) .. words[2]:sub(1, 1)):upper()
end

return M
