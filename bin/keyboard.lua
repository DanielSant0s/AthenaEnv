-- {"name": "Keyboard demo", "author": "Daniel Santos", "version": "10182026", "file": "keyboard.lua", "category": "input", "blurb": "Live USB keyboard events with a custom keymap.", "exit": "TRIANGLE"}

local app = require "athenaos.app"

local term = app.Terminal{ title = "Keyboard demo", category = "input",
                           subtitle = "Plug a USB keyboard and press keys - modifiers and arrows included" }
local pad = Pads.get()

IOP.loadModule("ps2kbd")
Keyboard.init()

-- Custom keymap test --------------------------------------------------------
-- setKeymap() replaces the whole translation table (USB HID usage codes,
-- page 0x07). This table matches the default US QWERTY layout in ps2kbd.irx;
-- change the assignments to build another layout (AZERTY, ABNT2...).
local SIZE = Keyboard.KEYMAP_SIZE
local keymap, shiftKeymap = Buffer(SIZE), Buffer(SIZE)

local letters = "abcdefghijklmnopqrstuvwxyz"
for i = 1, #letters do
  keymap:setU8(0x04 + i - 1, letters:byte(i))
  shiftKeymap:setU8(0x04 + i - 1, letters:byte(i) - 0x20) -- uppercase
end

local digits, digitsShifted = "1234567890", "!@#$%^&*()"
for i = 1, #digits do
  keymap:setU8(0x1E + i - 1, digits:byte(i))
  shiftKeymap:setU8(0x1E + i - 1, digitsShifted:byte(i))
end

local punctuation = {
  [0x28] = { "\r", "\r" },     -- Enter
  [0x29] = { "\27", "\27" },   -- Escape
  [0x2A] = { "\b", "\b" },     -- Backspace
  [0x2B] = { "\t", "\t" },     -- Tab
  [0x2C] = { " ", " " },       -- Space
  [0x2D] = { "-", "_" }, [0x2E] = { "=", "+" }, [0x2F] = { "[", "{" }, [0x30] = { "]", "}" },
  [0x31] = { "\\", "|" }, [0x33] = { ";", ":" }, [0x34] = { "'", "\"" }, [0x35] = { "`", "~" },
  [0x36] = { ",", "<" }, [0x37] = { ".", ">" }, [0x38] = { "/", "?" },
}
for code, pair in pairs(punctuation) do
  keymap:setU8(code, pair[1]:byte())
  shiftKeymap:setU8(code, pair[2]:byte())
end

Keyboard.setKeymap(keymap, shiftKeymap)
term:log("Custom keymap applied.")
-- Keyboard.resetKeymap() goes back to the layout baked into ps2kbd.irx.

-- Raw mode: every key as a {code, pressed} event, including modifiers,
-- arrows and function keys that have no ASCII form.
Keyboard.setReadMode(Keyboard.READMODE_RAW)

local modifierNames = {
  [Keyboard.KEY_LEFT_CTRL] = "Left Ctrl", [Keyboard.KEY_LEFT_SHIFT] = "Left Shift",
  [Keyboard.KEY_LEFT_ALT] = "Left Alt", [Keyboard.KEY_LEFT_GUI] = "Left GUI/Win",
  [Keyboard.KEY_RIGHT_CTRL] = "Right Ctrl", [Keyboard.KEY_RIGHT_SHIFT] = "Right Shift",
  [Keyboard.KEY_RIGHT_ALT] = "Right Alt", [Keyboard.KEY_RIGHT_GUI] = "Right GUI/Win",
}

while true do
  pad:update()
  if term:homeRequested(pad) then break end

  local event = Keyboard.getRaw()
  while event do
    local name = modifierNames[event.code]
    if name then
      term:log(name .. (event.pressed and " pressed" or " released"))
    elseif event.pressed then
      term:log(string.format("raw key 0x%x pressed", event.code))
    end
    event = Keyboard.getRaw()
  end

  term:draw()
end

Keyboard.deinit()
app.exitToShell()
