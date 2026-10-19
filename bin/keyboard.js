// {"name": "Keyboard demo", "author": "Daniel Santos", "version": "04102023", "file": "keyboard.js", "category": "input", "blurb": "Live USB keyboard events with a custom keymap.", "exit": "TRIANGLE"}

import { Terminal, exitToShell } from 'athenaos/app.js';

const term = new Terminal({ title: "Keyboard demo", category: "input",
                            subtitle: "Plug a USB keyboard and press keys -- modifiers and arrows included" });
const pad = Pads.get();

IOP.loadModule("ps2kbd");

Keyboard.init();

// --- Custom keymap test --------------------------------------------------
// PS2KbdSetKeymap() replaces the *entire* translation table at once (there's
// no "patch a single key" mode), so to test it we build a full table here.
// Codes below follow the standard USB HID Keyboard/Keypad Usage Page (0x07),
// and this particular table is functionally identical to the default US
// QWERTY layout already baked into ps2kbd.irx -- swap the assignments below
// to build a different layout (e.g. AZERTY, ABNT2).
const KEYMAP_SIZE = Keyboard.KEYMAP_SIZE;
const keymap = new Uint8Array(KEYMAP_SIZE);
const shiftKeymap = new Uint8Array(KEYMAP_SIZE);

const letters = "abcdefghijklmnopqrstuvwxyz";
for (let i = 0; i < letters.length; i++) {
    keymap[0x04 + i] = letters.charCodeAt(i);
    shiftKeymap[0x04 + i] = letters.charCodeAt(i) - 0x20; // uppercase
}

const digits = "1234567890";
const digitsShifted = "!@#$%^&*()";
for (let i = 0; i < digits.length; i++) {
    keymap[0x1E + i] = digits.charCodeAt(i);
    shiftKeymap[0x1E + i] = digitsShifted.charCodeAt(i);
}

const punctuation = {
    0x28: ["\r", "\r"], // Enter
    0x29: ["\x1b", "\x1b"], // Escape
    0x2A: ["\b", "\b"], // Backspace
    0x2B: ["\t", "\t"], // Tab
    0x2C: [" ", " "], // Space
    0x2D: ["-", "_"],
    0x2E: ["=", "+"],
    0x2F: ["[", "{"],
    0x30: ["]", "}"],
    0x31: ["\\", "|"],
    0x33: [";", ":"],
    0x34: ["'", "\""],
    0x35: ["`", "~"],
    0x36: [",", "<"],
    0x37: [".", ">"],
    0x38: ["/", "?"],
};

for (const code in punctuation) {
    const [normal, shifted] = punctuation[code];
    keymap[code] = normal.charCodeAt(0);
    shiftKeymap[code] = shifted.charCodeAt(0);
}

Keyboard.setKeymap(keymap.buffer, shiftKeymap.buffer);
term.log("Custom keymap applied.");

// Call Keyboard.resetKeymap() to go back to the layout baked into ps2kbd.irx.

// --- Raw mode: detect modifiers and other non-printable keys -------------
// In normal/"cooked" mode Keyboard.get() only ever returns translated ASCII,
// so pure modifier keys (Ctrl/Shift/Alt/GUI) and non-printable keys (arrows,
// F1-F12, Insert/Delete/Home/End/PgUp/PgDn, ...) never show up -- they have
// no ASCII representation and are silently dropped. Raw mode reports every
// key as a {code, pressed} event instead, using the raw USB HID usage code.
Keyboard.setReadMode(Keyboard.READMODE_RAW);

const modifierNames = {
    [Keyboard.KEY_LEFT_CTRL]: "Left Ctrl",
    [Keyboard.KEY_LEFT_SHIFT]: "Left Shift",
    [Keyboard.KEY_LEFT_ALT]: "Left Alt",
    [Keyboard.KEY_LEFT_GUI]: "Left GUI/Win",
    [Keyboard.KEY_RIGHT_CTRL]: "Right Ctrl",
    [Keyboard.KEY_RIGHT_SHIFT]: "Right Shift",
    [Keyboard.KEY_RIGHT_ALT]: "Right Alt",
    [Keyboard.KEY_RIGHT_GUI]: "Right GUI/Win",
};

while (true) {
    pad.update();
    if (term.homeRequested(pad)) break;

    let event;
    while ((event = Keyboard.getRaw())) {
        const name = modifierNames[event.code];
        if (name) {
            term.log(`${name} ${event.pressed ? "pressed" : "released"}`);
        } else if (event.pressed) {
            term.log(`raw key 0x${event.code.toString(16)} pressed`);
        }
    }

    term.draw();
}

Keyboard.deinit();
exitToShell();
