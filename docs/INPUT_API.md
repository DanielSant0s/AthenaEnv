# Input API

`Pads`, `Keyboard`, `Mouse`.

P.S.: *Italic* parameters refer to optional parameters.

## Pads module

**Buttons list:**
* `Pads.SELECT`
* `Pads.START`
* `Pads.UP`
* `Pads.RIGHT`
* `Pads.DOWN`
* `Pads.LEFT`
* `Pads.TRIANGLE`
* `Pads.CIRCLE`
* `Pads.CROSS`
* `Pads.SQUARE`
* `Pads.L1`
* `Pads.R1`
* `Pads.L2`
* `Pads.R2`
* `Pads.L3`
* `Pads.R3`

`let pad = Pads.get(*port*)` - Returns a pad object:

**Properties:**
* `pad.btns` - Button state on the current check.
* `pad.old_btns` - Button state on the last check.
* `pad.lx` - Left analog horizontal position (left = -127, default = 0, right = 128).
* `pad.ly` - Left analog vertical position (up = -127, default = 0, down = 128).
* `pad.rx` - Right analog horizontal position (left = -127, default = 0, right = 128).
* `pad.ry` - Right analog vertical position (up = -127, default = 0, down = 128).

![analog_graph](https://user-images.githubusercontent.com/47725160/154816009-99d7e5da-badf-409b-9a3b-3618fd372f09.png)

**Methods:**
* `update()` - Updates all pads pressed and stick positions data.
* `pressed(button)` - Checks if a button is being pressed (continuously).
* `justPressed(button)` - Checks if a button was pressed only once.
* `setEventHandler()` - Sets the pad object to listen to events defined by `Pads.newEvent`, so it doesn't need to be updated.

* `let event_id = Pads.newEvent(button, kind, function)` - Creates an asynchronous pad event, returns the event id. Remember to set the pad object event handler first!
* Pad event kinds:
  * `Pads.PRESSED`
  * `Pads.JUST_PRESSED`
  * `Pads.NONPRESSED`
* `Pads.deleteEvent(event_id)` - Deletes the event created by `Pads.newEvent`.
* `let type = Pads.getType(*port*)` - Gets the gamepad type reported at mode-table slot 0 (usually its baseline digital identity, not necessarily what's active right now).
* `let type = Pads.getActiveType(*port*)` - Gets the gamepad type currently active (reflects `Pads.setMode()` changes and the pad's own Analog/Select-button toggling).
* Pad Types:
  * `Pads.TYPE_DIGITAL`
  * `Pads.TYPE_ANALOG`
  * `Pads.TYPE_DUALSHOCK`

* `Pads.setMode(*port*, mode, lock)` - Forces the gamepad's digital/analog mode and whether the player can change it themselves (e.g. via the Analog button). `mode` is `Pads.MMODE_DIGITAL` or `Pads.MMODE_DUALSHOCK`; `lock` is a boolean (true = user can't toggle mode, false = unlocked). Switching to `MMODE_DUALSHOCK` on a pad that doesn't support it is simply rejected by the pad — check `Pads.getActiveType()` afterwards if you need to confirm it took effect. Note this only sets the basic digital/analog mode — it doesn't by itself negotiate pressure-sensitive buttons or rumble, which the engine already does automatically at connect time for capable pads.

* `let press = Pads.getPressure(*port*, button)` - Get button pressure level.
* `Pads.rumble(port, big, small)` - Rumble your gamepad.
* `let count = Pads.getConnectedCount()` – Returns the number of gamepads currently connected.
* `let ports = Pads.getConnected()` – Returns an array containing the ports of all connected gamepads. Example: `[0]`, `[1]`, or `[0, 1]`.
* `let active = Pads.isActive(port)` – Checks whether the specified port is active and ready to receive input.

## Keyboard module

* `Keyboard.init()` - Initialize keyboard routines.
* `let c = Keyboard.get()` - Get keyboard current char, translated to ASCII by the active keymap. Modifier keys (Ctrl/Shift/Alt/GUI) and non-printable keys (arrows, F1-F12, Insert/Delete/Home/End/PgUp/PgDn, ...) have no ASCII representation and are silently dropped in this mode — use `Keyboard.getRaw()` for those.
* `Keyboard.setRepeatRate(msec)` - Set keyboard repeat rate.
* `Keyboard.setBlockingMode(mode)` - Sets keyboard to block (or not) the thread waiting for the next key to be pressed.
* `Keyboard.setReadMode(mode)` - Switches between `Keyboard.READMODE_NORMAL` (default, ASCII via `Keyboard.get()`) and `Keyboard.READMODE_RAW` (raw USB HID events via `Keyboard.getRaw()`).
* `let event = Keyboard.getRaw()` - Reads one raw key event (only valid in `READMODE_RAW`), or null if none is pending. Returns `{ code, pressed }`, where `code` is the raw USB HID Keyboard/Keypad Usage Page (0x07) code and `pressed` is true on key-down, false on key-up. Modifier keys are reported as ordinary events in this range: `Keyboard.KEY_LEFT_CTRL`, `KEY_LEFT_SHIFT`, `KEY_LEFT_ALT`, `KEY_LEFT_GUI`, `KEY_RIGHT_CTRL`, `KEY_RIGHT_SHIFT`, `KEY_RIGHT_ALT`, `KEY_RIGHT_GUI`.
* `Keyboard.setKeymap(keymap, shiftKeymap, *keycap*)` - Replaces the whole ASCII translation table used in normal read mode (e.g. to switch keyboard language). `keymap` and `shiftKeymap` are required ArrayBuffers of `Keyboard.KEYMAP_SIZE` (256) bytes, indexed by USB HID usage code; `keycap` is an optional 256-byte ArrayBuffer of booleans marking which keys are affected by Caps Lock. There's no way to patch a single key — always pass full tables. See `bin/keyboard.js` for a worked example.
* `Keyboard.resetKeymap()` - Restores the default keymap baked into `ps2kbd.irx`.
* `Keyboard.deinit()` - Destroy keyboard routines.

## Mouse module

* `Mouse.init()` - Initialize mouse routines.
* `let mouse = Mouse.get()` - Returns mouse actual properties on the object format below:
  * `mouse.x`
  * `mouse.y`
  * `mouse.wheel`
  * `mouse.buttons` — bitmask, compare against `Mouse.BTN1`/`BTN2`/`BTN3` and `Mouse.BTN1_DBL`/`BTN2_DBL`/`BTN3_DBL`
* `Mouse.setBoundary(minx, maxx, miny, maxy)` - Set mouse x and y bounds (only relevant in `Mouse.READMODE_ABS`).
* `let bounds = Mouse.getBoundary()` - Returns the current bounds as `{ minX, maxX, minY, maxY }`.
* `let mode = Mouse.getMode()` - Get mouse mode, one of `Mouse.READMODE_DIFF` (relative movement) or `Mouse.READMODE_ABS` (absolute position).
* `Mouse.setMode(mode)` - Set mouse mode.
* `let accel = Mouse.getAccel()` - Get mouse acceleration.
* `Mouse.setAccel(val)` - Set mouse acceleration.
* `Mouse.setPosition(x, y)` - Set mouse pointer position.
* `let thres = Mouse.getThreshold()` - Get the movement threshold (in mickeys) before a move is reported.
* `Mouse.setThreshold(thres)` - Set the movement threshold.
* `let msec = Mouse.getDoubleClickTime()` - Get the max interval between clicks counted as a double-click, in milliseconds.
* `Mouse.setDoubleClickTime(msec)` - Set the double-click interval.
* `let version = Mouse.getVersion()` - Get the `ps2mouse.irx` driver version.
* `let mask = Mouse.enumerate()` - Returns a driver-defined bitmask describing the connected mice (see ps2sdk's `PS2MouseEnum`; not validated against real hardware here).
* `Mouse.reset()` - Resets the mouse driver state (already called once internally by `Mouse.init()`).
