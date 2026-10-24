
local app = require "athenaos.app"

local pad = Pads.get()
local ui = app.AppUI{ title = "2D Draw", category = "2d" }

Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)

local pixelColors = {
  Color.new(255, 0, 0), Color.new(255, 165, 0), Color.new(255, 255, 0),
  Color.new(0, 255, 0), Color.new(0, 255, 165), Color.new(0, 255, 255),
  Color.new(0, 0, 255), Color.new(128, 0, 255), Color.new(255, 0, 255),
}

-- Each sample draws centered on (cx, cy) inside its card.
local samples = {
  { "Draw.point", function(cx, cy)
    for row = -4, 4 do
      for i, c in ipairs(pixelColors) do Draw.point(cx - 16 + (i - 1) * 4, cy + row * 4, c) end
    end
  end },
  { "Draw.line", function(cx, cy)
    for i = 0, 5 do Draw.line(cx - 30, cy + 25, cx - 30 + i * 12, cy - 25, pixelColors[i + 1]) end
  end },
  { "Draw.rect", function(cx, cy) Draw.rect(cx - 20, cy - 20, 40, 40, Color.new(255, 0, 0)) end },
  { "Draw.circle", function(cx, cy)
    Draw.circle(cx, cy, 26, Color.new(64, 64, 64), true)
    Draw.circle(cx, cy, 26, Color.new(255, 0, 0), false)
  end },
  { "Flat triangle", function(cx, cy)
    Draw.triangle(cx - 22, cy + 20, cx - 22, cy - 20, cx + 22, cy + 20, Color.new(0, 0, 255))
  end },
  { "Gouraud triangle", function(cx, cy)
    Draw.triangle(cx - 22, cy + 20, cx - 22, cy - 20, cx + 22, cy + 20,
                  Color.new(255, 0, 0), Color.new(0, 255, 0), Color.new(0, 0, 255))
  end },
  { "Flat quad", function(cx, cy)
    Draw.quad(cx - 20, cy - 20, cx - 20, cy + 20, cx + 20, cy - 20, cx + 20, cy + 20, Color.new(128, 0, 255))
  end },
  { "Gouraud quad", function(cx, cy)
    Draw.quad(cx - 20, cy - 20, cx - 20, cy + 20, cx + 20, cy - 20, cx + 20, cy + 20,
              Color.new(255, 0, 0), Color.new(0, 255, 0), Color.new(0, 0, 255), Color.new(255, 255, 0))
  end },
}

local COLS, GAP = 4, 12

while true do
  pad:update()
  if ui:homeRequested(pad) then break end

  ui:background()

  local c = ui.content
  local cw = (c.w - GAP * (COLS - 1)) / COLS
  local ch = (c.h - GAP) / 2

  for i, sample in ipairs(samples) do
    local idx = i - 1
    local cx, cy = c.x + (idx % COLS) * (cw + GAP), c.y + (idx // COLS) * (ch + GAP)
    ui:panel(cx, cy, cw, ch)
    ui:heading(cx + 14, cy + 18, sample[1])
    sample[2](math.floor(cx + cw / 2 + 0.5), math.floor(cy + ch / 2 + 10.5))
  end

  ui:chrome()
  Screen.flip()
end

app.exitToShell()
