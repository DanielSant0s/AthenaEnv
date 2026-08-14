/**
 * AthenaEnv — Input devices: `Pads`, `Keyboard`, `Mouse`.
 * Verified against src/js_api/ath_pads.c, ath_keyboard.c, ath_mouse.c.
 */

// ---------------------------------------------------------------------------
// Pads
// ---------------------------------------------------------------------------

/** Object returned by {@link Pads.get}. All properties are read/write. */
declare class Pad {
    /** Button state on the current check (bitmask of `Pads.*` button constants). */
    btns: number;
    /** Button state on the last check. */
    old_btns: number;
    /** Left analog horizontal position (left = -127, center = 0, right = 128). */
    lx: number;
    /** Left analog vertical position (up = -127, center = 0, down = 128). */
    ly: number;
    /** Right analog horizontal position. */
    rx: number;
    /** Right analog vertical position. */
    ry: number;
    /** Previous-check value of {@link lx}. */
    old_lx: number;
    /** Previous-check value of {@link ly}. */
    old_ly: number;
    /** Previous-check value of {@link rx}. */
    old_rx: number;
    /** Previous-check value of {@link ry}. */
    old_ry: number;

    /** Refreshes this pad's button/stick state. */
    update(): void;
    /** True while `button` is held down. */
    pressed(button: number): boolean;
    /** True only on the single check where `button` transitioned to pressed. */
    justPressed(button: number): boolean;
    /** Registers this pad object as the listener for events created with {@link Pads.newEvent}, so it doesn't need manual {@link update} calls. */
    setEventHandler(): void;
}

declare namespace Pads {
    // -- buttons (bitmask) --------------------------------------------------
    const SELECT: number;
    const START: number;
    const UP: number;
    const RIGHT: number;
    const DOWN: number;
    const LEFT: number;
    const TRIANGLE: number;
    const CIRCLE: number;
    const CROSS: number;
    const SQUARE: number;
    const L1: number;
    const R1: number;
    const L2: number;
    const R2: number;
    const L3: number;
    const R3: number;

    // -- pad type ------------------------------------------------------------
    const TYPE_DIGITAL: number;
    const TYPE_ANALOG: number;
    const TYPE_DUALSHOCK: number;
    const TYPE_NEJICON: number;
    const TYPE_KONAMIGUN: number;
    const TYPE_NAMCOGUN: number;
    const TYPE_JOGCON: number;
    const TYPE_EX_TSURICON: number;
    const TYPE_EX_JOGCON: number;

    // -- digital/analog mode lock ---------------------------------------------
    const MMODE_DIGITAL: number;
    const MMODE_DUALSHOCK: number;

    // -- low-level connection state (Pads.getState) --------------------------
    const STATE_DISCONN: number;
    const STATE_FINDPAD: number;
    const STATE_FINDCTP1: number;
    const STATE_EXECCMD: number;
    const STATE_STABLE: number;
    const STATE_ERROR: number;

    // -- event kinds -----------------------------------------------------------
    const PRESSED: number;
    const JUST_PRESSED: number;
    /** @remarks No underscore between "NON" and "PRESSED". */
    const NONPRESSED: number;

    /** @param port Defaults to 0. */
    function get(port?: number): Pad;
    /** Gamepad type reported at mode-table slot 0 (baseline digital identity). @param port Defaults to 0. */
    function getType(port?: number): number;
    /** Gamepad type currently active (reflects {@link setMode} and the pad's own toggling). @param port Defaults to 0. */
    function getActiveType(port?: number): number;
    /** Low-level connection/negotiation state (`STATE_*`). @param port Defaults to 0. */
    function getState(port?: number): number;

    /**
     * Forces the pad's digital/analog mode.
     * @param port Defaults to 0 when omitted (2-arg call form).
     * @param mode `Pads.MMODE_DIGITAL` or `Pads.MMODE_DUALSHOCK`.
     * @param lock True = user cannot toggle mode themselves (e.g. via the Analog button).
     */
    function setMode(port: number | undefined, mode: number, lock: boolean): number;
    function setMode(mode: number, lock: boolean): number;

    function getPressure(port: number | undefined, button: number): number;
    function getPressure(button: number): number;

    /** @param port Defaults to 0 when omitted (2-arg call form). */
    function rumble(port: number | undefined, big: number, small: number): void;
    function rumble(big: number, small: number): void;

    /** Sets the pad's indicator LED colour. @param port Defaults to 0 when omitted (3-arg call form). */
    function setLED(port: number | undefined, r: number, g: number, b: number): void;
    function setLED(r: number, g: number, b: number): void;

    function isActive(port: number): boolean;
    /** Ports of all currently connected gamepads, e.g. `[0]`, `[1]`, or `[0, 1]`. */
    function getConnected(): number[];
    function getConnectedCount(): number;

    /**
     * Creates an asynchronous pad event, evaluated automatically once a pad
     * has {@link Pad.setEventHandler} set.
     * @remarks All three arguments are required — the native binding does
     * not validate argument count for this call.
     * @returns the event id, or -1 if the internal event table (64 slots) is full.
     */
    function newEvent(button: number, kind: number, callback: () => void): number;
    /** @remarks `id` is required — the native binding does not validate argument count for this call. */
    function deleteEvent(id: number): void;
}

// ---------------------------------------------------------------------------
// Keyboard
// ---------------------------------------------------------------------------

/** Event returned by {@link Keyboard.getRaw} (only valid in `READMODE_RAW`). */
interface KeyboardRawEvent {
    /** Raw USB HID Keyboard/Keypad Usage Page (0x07) code. */
    code: number;
    /** True on key-down, false on key-up. */
    pressed: boolean;
}

declare namespace Keyboard {
    const READMODE_NORMAL: number;
    const READMODE_RAW: number;
    /** Byte size required for {@link setKeymap}'s `keymap`/`shiftKeymap`/`keycap` buffers. */
    const KEYMAP_SIZE: number;

    // Modifier key raw codes — the only KEY_* constants exposed (non-modifier
    // codes must be looked up against the USB HID Keyboard/Keypad Usage Page).
    const KEY_LEFT_CTRL: number;
    const KEY_LEFT_SHIFT: number;
    const KEY_LEFT_ALT: number;
    const KEY_LEFT_GUI: number;
    const KEY_RIGHT_CTRL: number;
    const KEY_RIGHT_SHIFT: number;
    const KEY_RIGHT_ALT: number;
    const KEY_RIGHT_GUI: number;

    function init(): number;
    /**
     * Reads the current key as ASCII (translated via the active keymap).
     * Modifier keys and non-printable keys (arrows, F1-F12, Insert/Delete/
     * Home/End/PgUp/PgDn, ...) have no ASCII form and are silently dropped
     * — use {@link getRaw} for those.
     */
    function get(): number;
    /** Reads one raw key event; only valid in `READMODE_RAW`. `null` if none pending. */
    function getRaw(): KeyboardRawEvent | null;
    function setRepeatRate(msec: number): number;
    /** Blocks the calling thread while awaiting the next key press when enabled. */
    function setBlockingMode(mode: boolean): number;
    /** Switches between `Keyboard.READMODE_NORMAL` and `Keyboard.READMODE_RAW`. */
    function setReadMode(mode: number): number;
    /**
     * Replaces the whole ASCII translation table used in normal read mode.
     * @param keymap `KEYMAP_SIZE`-byte ArrayBuffer, indexed by USB HID usage code.
     * @param shiftKeymap `KEYMAP_SIZE`-byte ArrayBuffer, used while Shift is held.
     * @param keycap Optional `KEYMAP_SIZE`-byte ArrayBuffer of booleans marking keys affected by Caps Lock.
     */
    function setKeymap(keymap: ArrayBuffer, shiftKeymap: ArrayBuffer, keycap?: ArrayBuffer): number;
    /** Restores the default keymap baked into `ps2kbd.irx`. */
    function resetKeymap(): number;
    function deinit(): number;
}

// ---------------------------------------------------------------------------
// Mouse
// ---------------------------------------------------------------------------

interface MouseState {
    x: number;
    y: number;
    wheel: number;
    /** Bitmask — compare against `Mouse.BTN1`/`BTN2`/`BTN3` and their `_DBL` (double-click) variants. */
    buttons: number;
}

interface MouseBoundary {
    minX: number;
    maxX: number;
    minY: number;
    maxY: number;
}

declare namespace Mouse {
    const READMODE_DIFF: number;
    const READMODE_ABS: number;
    const BTN1: number;
    const BTN2: number;
    const BTN3: number;
    const BTN1_DBL: number;
    const BTN2_DBL: number;
    const BTN3_DBL: number;

    function init(): number;
    function get(): MouseState;
    /** Only relevant in `Mouse.READMODE_ABS`. */
    function setBoundary(minx: number, maxx: number, miny: number, maxy: number): number;
    function getBoundary(): MouseBoundary;
    /** `Mouse.READMODE_DIFF` (relative movement) or `Mouse.READMODE_ABS` (absolute position). */
    function getMode(): number;
    function setMode(mode: number): number;
    function getAccel(): number;
    function setAccel(val: number): number;
    function setPosition(x: number, y: number): number;
    /** Movement threshold, in mickeys, before a move is reported. */
    function getThreshold(): number;
    function setThreshold(thres: number): number;
    /** Max interval between clicks counted as a double-click, in milliseconds. */
    function getDoubleClickTime(): number;
    function setDoubleClickTime(msec: number): number;
    /** `ps2mouse.irx` driver version. */
    function getVersion(): number;
    /** Driver-defined bitmask describing the connected mice (see ps2sdk's `PS2MouseEnum`). */
    function enumerate(): number;
    /** Resets the mouse driver state (already called once internally by {@link init}). */
    function reset(): number;
}
