-- AthenaOS persistence: small JSON files next to the shell (Lua port of
-- athenaos/storage.js). Writes are best-effort -- booting from read-only media
-- just keeps everything in memory for the current session.
--
-- settings.json is shared with the JS shell; the session (recents, pins,
-- launch counts) lives in session_lua.json since it names .lua apps.

local M = {}

local DIR = "athenaos/"

local function merge(fallback, values)
  local out = {}
  for k, v in pairs(fallback) do
    if type(v) == "table" and v ~= JSON.null then
      -- copy so defaults are never mutated; keep the metatable (JSON.array)
      local copy = setmetatable({}, getmetatable(v))
      for kk, vv in pairs(v) do copy[kk] = vv end
      out[k] = copy
    else
      out[k] = v
    end
  end
  for k, v in pairs(values or {}) do out[k] = v end
  return out
end

function M.readJSON(name, fallback)
  local text = io.readfile(DIR .. name)
  if text then
    local ok, value = pcall(JSON.parse, text)
    if ok and type(value) == "table" then return merge(fallback, value) end
    print("[athenaos] could not read " .. name .. ": " .. tostring(value))
  end
  return merge(fallback)
end

function M.writeJSON(name, value)
  local ok, text = pcall(JSON.stringify, value)
  if not ok then
    print("[athenaos] could not write " .. name .. ": " .. tostring(text))
    return false
  end
  return io.writefile(DIR .. name, text)
end

M.DEFAULT_SETTINGS = {
  accent = 0,
  wallpaper = 0,     -- index into WALLPAPERS (see main.lua)
  clock24 = true,
  utcOffset = 0,     -- hours added to the console clock
  lockScreen = true, -- show the lock screen on cold boot
  showFps = false,
  animations = true,
}

M.DEFAULT_SESSION = {
  launching = JSON.null, -- app started from the shell; null means cold boot
  recents = JSON.array(),
  launches = {},         -- file -> launch count
  pinned = JSON.array({ "apps/render/main.lua", "apps/physics_collision/main.lua", "apps/sound/main.lua", "apps/pong/main.lua" }),
}

function M.loadSettings() return M.readJSON("settings.json", M.DEFAULT_SETTINGS) end
function M.saveSettings(s) return M.writeJSON("settings.json", s) end
function M.loadSession() return M.readJSON("session_lua.json", M.DEFAULT_SESSION) end
function M.saveSession(s) return M.writeJSON("session_lua.json", s) end

return M
