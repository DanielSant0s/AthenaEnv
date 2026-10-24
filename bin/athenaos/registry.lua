-- AthenaOS app registry (Lua port of athenaos/registry.js).
--
-- Every folder under apps/ with an app.json and a main.lua is an app:
--   apps/pong/app.json  {"name": "Pong", "author": "...", "version": "...",
--                        "icon": "pong_icon.png", "category": "games", "blurb": "...", "exit": "START"}
--   apps/pong/main.lua
-- `icon`, `category`, `blurb` and `exit` are optional; "js": {...} / "lua": {...}
-- override fields for one language. Apps run from the boot folder, like the
-- shell: `icon` and every asset path are relative to it.

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

M.APPS_DIR = "apps"
-- Every C app, run with --app=<id> (examples/common/demos_main.c).
M.DEMOS_ELF = "athena_demos.elf"

local function readMeta(dir)
  local f = io.open(dir .. "/app.json", "r")
  if not f then return nil end
  local text = f:read("a")
  f:close()
  local ok, meta = pcall(JSON.parse, text)
  if not ok or type(meta) ~= "table" then
    print("[athenaos] bad " .. dir .. "/app.json: " .. tostring(meta))
    return nil
  end
  if type(meta.lua) == "table" then
    for k, v in pairs(meta.lua) do meta[k] = v end
  end
  return meta
end

function M.scanApps()
  local apps = {}
  for _, entry in ipairs(System.listDir(M.APPS_DIR)) do
    local dir = M.APPS_DIR .. "/" .. entry.name
    local file = dir .. "/main.lua"
    if entry.dir and entry.name:sub(1, 1) ~= "." and io.exists(file) then
      local meta = readMeta(dir)
      if meta and meta.name then
        apps[#apps + 1] = {
          id = entry.name,
          name = meta.name,
          author = meta.author or "Unknown",
          version = meta.version or "",
          file = file,
          c = io.exists(dir .. "/main.c") and io.exists(M.DEMOS_ELF),
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
