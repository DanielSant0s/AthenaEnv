// {"name": "Mouse demo", "author": "Daniel Santos", "version": "04102023", "file": "mouse.js", "category": "input", "blurb": "USB mouse pointer, buttons and wheel.", "exit": "TRIANGLE"}

import { Terminal, exitToShell } from 'athenaos/app.js';

const term = new Terminal({ title: "Mouse demo", category: "input", subtitle: "Plug a USB mouse -- move, click and scroll" });
const pad = Pads.get();
const mode = Screen.getMode();

IOP.loadModule("ps2mouse");

Mouse.init();

let mouse_data = Mouse.get();
let cursor = { x: mode.width / 2, y: mode.height / 2 };

while(true) {
    pad.update();
    if (term.homeRequested(pad)) break;

    mouse_data = Mouse.get();
    if (mouse_data.x || mouse_data.y || mouse_data.wheel || mouse_data.buttons) {
        term.log(`mouse output ${JSON.stringify(mouse_data)}`);
    }

    // Default read mode reports relative movement.
    cursor.x = Math.max(0, Math.min(mode.width, cursor.x + mouse_data.x));
    cursor.y = Math.max(0, Math.min(mode.height, cursor.y + mouse_data.y));

    term.render();
    Draw.triangle(cursor.x, cursor.y, cursor.x, cursor.y + 16, cursor.x + 11, cursor.y + 11,
                  mouse_data.buttons ? term.accent() : Color.new(255, 255, 255));
    Screen.flip();
}

exitToShell();