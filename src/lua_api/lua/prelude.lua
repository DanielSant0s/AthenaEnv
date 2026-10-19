-- Runtime prelude for AthenaEnv Lua: timers and the coroutine scheduler.
-- Embedded into the runtime by scripts/lua2c.py and run before the script.
--
-- The C main loop calls the function registered with __athena_set_tick once
-- per iteration; it returns true while timers or tasks are pending, which
-- keeps the loop alive after the main chunk returns (like JS's event loop).

local set_tick = ...
local millis, unpack, pack = os.millis, table.unpack, table.pack
local co_create, co_resume, co_status, co_running, co_yield =
  coroutine.create, coroutine.resume, coroutine.status, coroutine.running, coroutine.yield

-- timers ------------------------------------------------------------------

local timers, next_id = {}, 1

local function add_timer(fn, ms, interval, ...)
  assert(type(fn) == "function", "timer callback must be a function")
  local id = next_id
  next_id = id + 1
  timers[id] = { at = millis() + (ms or 0), fn = fn, interval = interval and (ms or 0) or nil, args = pack(...) }
  return id
end

function os.setTimeout(fn, ms, ...) return add_timer(fn, ms, false, ...) end
function os.setInterval(fn, ms, ...) return add_timer(fn, ms, true, ...) end
function os.clearTimeout(id) if id then timers[id] = nil end end
os.clearInterval = os.clearTimeout

local function run_timers()
  if next(timers) == nil then return false end
  local now = millis()
  for id, t in pairs(timers) do
    if now >= t.at then
      if t.interval then t.at = now + t.interval else timers[id] = nil end
      t.fn(unpack(t.args, 1, t.args.n))
    end
  end
  return next(timers) ~= nil
end

-- coroutine scheduler -----------------------------------------------------
--
--   async(fn, ...)   runs fn as a task; returns the task (task:ready(), task.result)
--   await(x)         inside a task: x = ms, a function returning truthy, or an
--                    object with :ready() (task, Request, ...); returns the
--                    truthy value that woke it up
--   sleep(ms)        await(ms)

local tasks = {}

local function step(task, ...)
  local res = pack(co_resume(task.co, ...))
  if not res[1] then
    -- non-string errors (System.reload's sentinel) pass through untouched
    if type(res[2]) ~= "string" then error(res[2], 0) end
    error(debug.traceback(task.co, res[2]), 0)
  end
  if co_status(task.co) == "dead" then
    task.done = true
    task.cond = nil
    task.result = pack(unpack(res, 2, res.n))
  else
    task.cond = res[2]
  end
end

function async(fn, ...)
  local task = { co = co_create(fn) }
  function task:ready() return self.done end
  tasks[#tasks + 1] = task
  step(task, ...)
  return task
end

function await(cond)
  local _, main = co_running()
  if main then error("await must be called inside async()", 2) end
  local kind = type(cond)
  if kind == "number" then
    local deadline = millis() + cond
    cond = function() return millis() >= deadline end
  elseif kind == "table" or kind == "userdata" then
    -- errors from ready() (a failed transfer...) are re-raised in the task
    local obj = cond
    cond = function()
      local ok, r = pcall(obj.ready, obj)
      if not ok then return { await_error = r } end
      return r
    end
  end
  local v = co_yield(cond)
  if type(v) == "table" and v.await_error ~= nil then error(v.await_error, 2) end
  return v
end

function sleep(ms) return await(ms) end

local function run_tasks()
  local i = 1
  while i <= #tasks do
    local task = tasks[i]
    if not task.done then
      local v = task.cond == nil or task.cond()
      if v then step(task, v) end
    end
    if task.done then table.remove(tasks, i) else i = i + 1 end
  end
  return #tasks > 0
end

set_tick(function()
  local a = run_timers()
  local b = run_tasks()
  return a or b
end)

-- small conveniences ------------------------------------------------------

-- console.log parity: print tables as JSON-ish text
console = {
  log = function(...)
    local parts = pack(...)
    for i = 1, parts.n do
      local v = parts[i]
      if type(v) == "table" then
        local ok, s = pcall(JSON.stringify, v)
        parts[i] = ok and s or tostring(v)
      else
        parts[i] = tostring(v)
      end
    end
    print(table.concat(parts, " ", 1, parts.n))
  end,
}

-- io.readfile / io.writefile: std.loadFile / std.open(...).puts parity
function io.readfile(path)
  local f = io.open(path, "rb")
  if not f then return nil end
  local data = f:read("a")
  f:close()
  return data
end

function io.writefile(path, data)
  local f = io.open(path, "wb")
  if not f then return false end
  f:write(data)
  f:close()
  return true
end

function io.exists(path)
  local f = io.open(path, "rb")
  if f then f:close() return true end
  return false
end

-- lazy subsystems -------------------------------------------------------------
-- Network/Request/Socket/WebSocket, Archive and Video live in relocatable
-- modules (unless built in with STATIC_*): touching the global loads them,
-- so scripts use them exactly like the JS globals, with no explicit require.

local lazy = {
  Network = "athena_network.erl", Request = "athena_network.erl",
  Socket = "athena_network.erl", WebSocket = "athena_network.erl",
  Archive = "athena_archive.erl", Video = "athena_mpeg.erl",
}

setmetatable(_G, {
  __index = function(t, key)
    local erl = lazy[key]
    if erl == nil then return nil end
    for k, v in pairs(lazy) do
      if v == erl then lazy[k] = nil end
    end
    require(erl)
    return rawget(t, key)
  end,
})
