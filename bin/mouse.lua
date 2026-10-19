-- {"name": "Mouse demo", "author": "Daniel Santos", "version": "10182026", "file": "mouse.lua", "category": "input", "blurb": "USB mouse pointer, buttons and wheel.", "exit": "TRIANGLE"}

local app = require "athenaos.app"

local term = app.Terminal{ title = "Mouse demo", category = "input", subtitle = "Plug a USB mouse - move, click and scroll" }
local pad = Pads.get()
local mode = Screen.getMode()

IOP.loadModule("ps2mouse")
Mouse.init()

local cursor = { x = mode.width / 2, y = mode.height / 2 }
local white = Color.new(255, 255, 255)

while true do
  pad:update()
  if term:homeRequested(pad) then break end

  local m = Mouse.get()
  if m.x ~= 0 or m.y ~= 0 or m.wheel ~= 0 or m.buttons ~= 0 then
    term:log("mouse output " .. JSON.stringify(m))
  end

  -- Default read mode reports relative movement.
  cursor.x = math.max(0, math.min(mode.width, cursor.x + m.x))
  cursor.y = math.max(0, math.min(mode.height, cursor.y + m.y))

  term:render()
  Draw.triangle(cursor.x, cursor.y, cursor.x, cursor.y + 16, cursor.x + 11, cursor.y + 11,
                m.buttons ~= 0 and term:accent() or white)
  Screen.flip()
end

app.exitToShell()
