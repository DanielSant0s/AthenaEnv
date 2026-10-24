
local app = require "athenaos.app"

local pad = Pads.get()
local font = Font("default")
local ui = app.AppUI{ title = "Hello world", category = "2d" }

Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)

-- Prints the four variants (plain, color, outline, shadow) at one scale.
local function variants(x, y, scale, step)
  font.scale = scale

  font.color = Color.new(128, 128, 128)
  font:print(x, y, "Hello World!")

  font.color = Color.new(128, 0, 0)
  font:print(x, y + step, "Hello World(with custom color)!")

  font.color = Color.new(128, 128, 128)
  font.outline = 1.0
  font.outline_color = Color.new(128, 0, 0)
  font:print(x, y + step * 2, "Hello World(with outline)!")

  font.outline = 0.0
  font.outline_color = Color.new(0, 0, 0)

  font.color = Color.new(128, 128, 128)
  font.dropshadow = 1.0
  font.dropshadow_color = Color.new(128, 0, 0)
  font:print(x, y + step * 3, "Hello World(with drop shadow)!")

  font.dropshadow = 0.0
  font.dropshadow_color = Color.new(0, 0, 0)
end

while true do
  pad:update()
  if ui:homeRequested(pad) then break end

  ui:background()
  local c = ui.content

  ui:panel(c.x, c.y, c.w, 168)
  ui:heading(c.x + 20, c.y + 20, "Scale 1.0")
  variants(c.x + 20, c.y + 32, 1.0, 32)

  ui:panel(c.x, c.y + 184, c.w, 120)
  ui:heading(c.x + 20, c.y + 204, "Scale 0.6")
  variants(c.x + 20, c.y + 216, 0.6, 20)

  ui:chrome()
  Screen.flip()
end

app.exitToShell()
