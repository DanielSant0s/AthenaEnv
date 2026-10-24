-- Move the emitter with the left stick/D-pad. Cross: burst. Circle: toggle smoke.
local app = require "athenaos.app"

local ui = app.AppUI{ title = "Native 2D particles", category = "2d",
                      hints = { { "L-stick", "Move" }, { "cross", "Sparks" }, { "circle", "Smoke" }, { "R3", "Clear" } } }
local atlas = Image("render/fx_atlas.png")
atlas.filter = LINEAR

local smoke = Particles.System{ texture = atlas, capacity = 512 }
local dust = smoke:createEmitter{
  rate = 45, frame = { 0, 0, 32, 32 }, lifetime = { 0.7, 1.4 }, size = { 10, 20 }, scaleEnd = 3,
  velocityMin = { -15, -60, 0 }, velocityMax = { 15, -25, 0 }, spread = { 5, 2, 0 }, drag = 0.7,
  color = { 100, 110, 128, 60 }, colorEnd = { 100, 110, 128, 0 }, fadeIn = 0.1,
}
local sparks = Particles.System{ texture = atlas, capacity = 512, blend = "additive" }
local burst = sparks:createEmitter{
  frame = { 32, 0, 32, 32 }, lifetime = { 0.25, 0.8 }, size = { 3, 7 }, scaleEnd = 0.3,
  velocityMin = { -180, -230, 0 }, velocityMax = { 180, 50, 0 }, acceleration = { 0, 320, 0 },
  color = { 180, 90, 20, 128 }, colorEnd = { 128, 20, 4, 0 },
}

local pad, mode, timer = Pads.get(0), Screen.getMode(), Timer.new()
local x, y, running = mode.width / 2, mode.height * 0.65, true
dust:setPosition(x, y)
dust:start()
Screen.setFrameCounter(true)

local function clamp(v, lo, hi) return math.max(lo, math.min(hi, v)) end

while true do
  local dt = math.min(timer:getTime() / 1000000, 0.05)
  timer:reset()
  pad:update()
  if ui:homeRequested(pad) then break end

  local dx, dy = pad.lx / 128, pad.ly / 128
  if math.abs(dx) < 0.15 then dx = 0 end
  if math.abs(dy) < 0.15 then dy = 0 end
  if pad:pressed(Pads.LEFT) then dx = -1 end
  if pad:pressed(Pads.RIGHT) then dx = 1 end
  if pad:pressed(Pads.UP) then dy = -1 end
  if pad:pressed(Pads.DOWN) then dy = 1 end
  x = clamp(x + dx * 200 * dt, 0, mode.width)
  y = clamp(y + dy * 200 * dt, 0, mode.height)
  dust:setPosition(x, y)
  if pad:justPressed(Pads.CROSS) then burst:burst(96, x, y) end
  if pad:justPressed(Pads.CIRCLE) then
    running = not running
    if running then dust:start() else dust:stop() end
  end
  if pad:justPressed(Pads.R3) then
    smoke:clear()
    sparks:clear()
  end

  smoke:update(dt)
  sparks:update(dt)
  Screen.clear(Color.new(12, 14, 20))
  smoke:draw()
  sparks:draw()
  Draw.line(x - 5, y, x + 5, y, Color.new(128, 128, 128))
  Draw.line(x, y - 5, x, y + 5, Color.new(128, 128, 128))
  ui.status = Screen.getFPS(60) .. " FPS"
  ui:hud({ (smoke.activeCount + sparks.activeCount) .. " particles", "Smoke " .. (running and "ON" or "OFF") })
  ui:chrome()
  Screen.flip()
end

smoke:dispose()
sparks:dispose()
app.exitToShell()
