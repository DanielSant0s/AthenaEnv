-- {"name": "Task sample", "author": "Daniel Santos", "version": "10182026", "file": "task.lua", "category": "system", "blurb": "Threads, coroutines, timers and pad events beside the main loop.", "exit": "TRIANGLE"}
--
-- Lua counterpart of task.js. Threads are cooperative here (coroutines on the
-- runtime scheduler): they run whenever they yield -- Thread.yield(), sleep()
-- or await() -- so no mutex dance is needed to keep them in step with drawing.

local app = require "athenaos.app"
local COLORS = app.COLORS

Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)

local ui = app.AppUI{ title = "Task sample", category = "system",
                      hints = { { "cross", "Pause counters" }, { "square", "Burst task" } } }

local counter, pulse_counter, burst_counter = 0, 0, 0
local counting = true

-- A long-running worker: counts every 2 seconds while counting is on.
local thread = Thread(function()
  while true do
    sleep(2000)
    if counting then counter = counter + 1 end
  end
end, "Thread: Loop counter")
thread:start()

-- os.setInterval runs on the same scheduler as JS's os.setInterval.
os.setInterval(function()
  if counting then pulse_counter = pulse_counter + 1 end
end, 1000)

local pad = Pads.get()
pad:setEventHandler()

Pads.newEvent(Pads.CROSS, Pads.JUST_PRESSED, function() counting = not counting end)

-- async/await: a short task that counts ten frames' worth of work, then ends.
Pads.newEvent(Pads.SQUARE, Pads.JUST_PRESSED, function()
  async(function()
    for _ = 1, 10 do
      burst_counter = burst_counter + 1
      sleep(100)
    end
  end)
end)

local exit_requested = false
Pads.newEvent(Pads.TRIANGLE, Pads.JUST_PRESSED, function() exit_requested = true end)

console.log(Thread.list())

Screen.display(function()
  ui:background()
  local c = ui.content
  ui:panel(c.x, c.y, c.w, 172)
  ui:heading(c.x + 20, c.y + 22, "Main loop")
  ui:textV(c.x + 20, c.y + 46, "Hello from Screen.display!", 0.56)
  ui:heading(c.x + 20, c.y + 78, "Cooperative tasks")
  ui:textV(c.x + 20, c.y + 102, "Loop counter - Thread + sleep, every 2 s: " .. counter, 0.5)
  ui:textV(c.x + 20, c.y + 124, "Pulse counter - os.setInterval, 1 s: " .. pulse_counter, 0.5)
  ui:textV(c.x + 20, c.y + 146, "Burst counter - async task per Square: " .. burst_counter, 0.5)
  ui:textV(c.x + c.w - 20, c.y + 22, counting and "RUNNING" or "PAUSED", 0.42,
           counting and COLORS.ok or COLORS.warn, Font.ALIGN_RIGHT)
  ui:chrome()

  if exit_requested then
    thread:stop()
    System.reload(app.SHELL)
  end
end)
