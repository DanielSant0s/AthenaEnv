
local app = require "athenaos.app"

local canvas = Screen.getMode()
canvas.psm = Screen.CT24
canvas.zbuffering = true
canvas.double_buffering = true
Screen.setMode(canvas)
Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)

Screen.setVSync(false)
Screen.setFrameCounter(true)

-- Every button is under test here, so Home is a combo instead of Triangle.
local ui = app.AppUI{ title = "Gamepad tester", category = "input",
                      hints = { { "L2+R2", "Rumble" } }, home = { "START+R3+UP", "Home" } }

local dark_gray = Color.new(128, 128, 128, 32)
local opaque = Color.new(128, 128, 128, 128)
local translucent = Color.new(128, 128, 128, 60)

local img = {}
for _, name in ipairs({ "circle", "cross", "square", "triangle", "up", "down", "left", "right",
                        "start", "select", "R1", "R2", "L1", "L2", "L3", "R3" }) do
  img[name] = Image("pads/" .. name .. ".png")
end

-- Controller layout is shifted down to clear the info card under the top bar.
local OY = 24
local rumble = false
local pad = Pads.get(0)

-- Pressure-sensitive buttons fade in with the pressure applied.
local function pressure(image, btn, x, y)
  image.color = Color.new(128, 128, 128, (pad.btns & btn) ~= 0 and Pads.getPressure(btn) or 60)
  image:draw(x, y + OY)
end

local function digital(image, btn, x, y)
  image.color = (pad.btns & btn) ~= 0 and opaque or translucent
  image:draw(x, y + OY)
end

local function held(btn) return (pad.btns & btn) ~= 0 end

while true do
  pad:update()

  if held(Pads.START) and held(Pads.R3) and held(Pads.UP) then break end

  ui:background()

  ui:hud({
    "Connected pads: " .. Pads.getConnectedCount(),
    "Active ports: [" .. table.concat(Pads.getConnected(), ", ") .. "]",
    string.format("Port 0: %s  |  Port 1: %s", Pads.isActive(0) and "ACTIVE" or "INACTIVE", Pads.isActive(1) and "ACTIVE" or "INACTIVE"),
    "Rumble: " .. (rumble and "ON" or "OFF"),
  })

  digital(img.select, Pads.SELECT, 260, 190)
  digital(img.start, Pads.START, 380, 190)

  pressure(img.up, Pads.UP, 120, 155)
  pressure(img.down, Pads.DOWN, 120, 225)
  pressure(img.left, Pads.LEFT, 85, 190)
  pressure(img.right, Pads.RIGHT, 155, 190)

  pressure(img.triangle, Pads.TRIANGLE, 520, 155)
  pressure(img.cross, Pads.CROSS, 520, 225)
  pressure(img.square, Pads.SQUARE, 485, 190)
  pressure(img.circle, Pads.CIRCLE, 555, 190)

  pressure(img.L1, Pads.L1, 102, 100)
  pressure(img.R1, Pads.R1, 502, 100)
  pressure(img.L2, Pads.L2, 137, 100)
  pressure(img.R2, Pads.R2, 537, 100)

  Draw.rect(220, 280 + OY, 75, 75, dark_gray)
  Draw.rect(380, 280 + OY, 75, 75, dark_gray)

  img.L3.color = held(Pads.L3) and opaque or translucent
  img.L3:draw(pad.lx / 4.0 + 242.0, pad.ly / 4.0 + 300.0 + OY)

  img.R3.color = held(Pads.R3) and opaque or translucent
  img.R3:draw(pad.rx / 4.0 + 402.0, pad.ry / 4.0 + 300.0 + OY)

  if held(Pads.R2) and held(Pads.L2) and (pad.old_btns & Pads.R2) ~= 0 and (pad.old_btns & Pads.L2) == 0 then
    if rumble then Pads.rumble(0, 0) else Pads.rumble(1, 255) end
    rumble = not rumble
  end

  ui.status = Screen.getFPS(360) .. " FPS"
  ui:chrome({ { "L2+R2", "Rumble" } }, "Buttons fade in with pressure")
  Screen.flip()
end

Pads.rumble(0, 0)
app.exitToShell()
