-- {"name": "DVD Remote demo", "author": "Daniel Santos", "version": "10182026", "file": "remote.lua", "category": "input", "blurb": "DVD remote control buttons via the IR receiver.", "exit": "TRIANGLE"}

local app = require "athenaos.app"

local term = app.Terminal{ title = "DVD Remote demo", category = "input",
                           subtitle = "Point the DVD remote at the console - STOP also leaves" }
local pad = Pads.get(0)

IOP.loadModule("rmman")
Remote.init()

-- The remote shows up on controller port 2 (index 1), whether it's the
-- external IR dongle on a fat PS2 or the built-in receiver on a slim.
local remote = Remote.get(1)

local buttonNames = {
  [Remote.DVD_PLAY] = "PLAY", [Remote.DVD_PAUSE] = "PAUSE", [Remote.DVD_STOP] = "STOP",
  [Remote.DVD_NEXT] = "NEXT", [Remote.DVD_PREV] = "PREV",
  [Remote.DVD_SCAN_FORW] = "SCAN_FORW", [Remote.DVD_SCAN_BACK] = "SCAN_BACK",
}

while true do
  remote:update()
  pad:update()

  if term:homeRequested(pad) then break end

  if not remote:released() and remote.button ~= remote.old_button then
    term:log("remote button: " .. (buttonNames[remote.button] or string.format("0x%x", remote.button)))
  end

  if remote:pressed(Remote.DVD_PLAY) then term:log("-> play requested") end
  if remote:pressed(Remote.DVD_STOP) then break end

  term:draw()
end

app.exitToShell()
