-- {"name": "DejaVu Font Demo", "author": "Daniel Santos", "version": "10182026", "file": "image_font.lua", "category": "2d", "blurb": "Bitmap image fonts with alignment, outline and shadows.", "exit": "TRIANGLE"}

local app = require "athenaos.app"

local pad = Pads.get()
local font = Font("dejavu.bmp")
local ui = app.AppUI{ title = "DejaVu Font Demo", category = "2d" }

Screen.setParam(Screen.DEPTH_TEST_ENABLE, false)

local line_height = 27

-- Each line: style fields applied on top of the previous state, then printed.
local function line(y, x, text, style)
  for k, v in pairs(style or {}) do font[k] = v end
  font:print(x, y, text)
end

while true do
  pad:update()
  if ui:homeRequested(pad) then break end

  ui:background()
  local left, right, center = ui.content.x, ui.content.x + ui.content.w, ui.W / 2
  local y = ui.content.y

  line(y, left, "DejaVu Bitmap Font Demo",
       { scale = 1.0, color = Color.new(255, 255, 255, 128), outline = 0, dropshadow = 0 })
  y = y + line_height
  line(y, left, "Text with black outline",
       { color = Color.new(255, 255, 0, 128), outline = 2.0, outline_color = Color.new(0, 0, 0, 128) })
  y = y + line_height
  line(y, left, "White text with red outline",
       { color = Color.new(255, 255, 255, 128), outline = 1.5, outline_color = Color.new(255, 0, 0, 128) })
  y = y + line_height
  line(y, left, "Cyan text with drop shadow",
       { outline = 0, color = Color.new(0, 255, 255, 128), dropshadow = 3.0, dropshadow_color = Color.new(0, 0, 0, 80) })
  y = y + line_height
  line(y, left, "Orange with brown shadow",
       { color = Color.new(255, 128, 0, 128), dropshadow = 2.0, dropshadow_color = Color.new(64, 32, 0, 100) })
  y = y + line_height

  line(y, left, "Left aligned - starts at x",
       { dropshadow = 0, scale = 0.8, color = Color.new(200, 200, 200, 128), align = Font.ALIGN_LEFT })
  y = y + line_height
  line(y, center, "Center aligned - centered at x", { align = Font.ALIGN_HCENTER })
  y = y + line_height
  line(y, right, "Right aligned - ends at x", { align = Font.ALIGN_RIGHT })
  y = y + line_height

  line(y, center, "Combined: Outline + Center",
       { scale = 1.2, color = Color.new(255, 255, 255, 128), outline = 2.0,
         outline_color = Color.new(128, 0, 255, 128), align = Font.ALIGN_HCENTER })
  y = y + line_height * 1.4
  line(y, center, "Shadow + Center Align",
       { outline = 0, dropshadow = 2.5, dropshadow_color = Color.new(0, 0, 0, 100), color = Color.new(0, 255, 128, 128) })
  y = y + line_height * 1.4

  font.dropshadow = 0
  font.align = Font.ALIGN_LEFT

  local test_text = "Text Size Test"
  local size = font:getTextSize(test_text)
  line(y, left, test_text, { scale = 1.0, color = Color.new(128, 255, 128, 128) })
  font.scale = 0.7
  font.color = Color.new(128, 128, 128, 128)
  font:print(left + 220, y + 4, string.format("Size: %dx%d pixels", size.width, size.height))

  ui:chrome()
  Screen.flip()
end

app.exitToShell()
