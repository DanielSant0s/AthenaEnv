// {"name": "Mouse demo", "author": "Daniel Santos", "version": "04102023", "file": "mouse.js"}

import * as Mouse from "athena_mouse.erl"

IOP.loadModule(System.boot_path + "/iop/ps2mouse.irx");

Mouse.init();

let mouse_data = Mouse.get();

while(true) {
    mouse_data = Mouse.get();
    if (mouse_data.x || mouse_data.y || mouse_data.wheel || mouse_data.buttons) {
        console.log(`mouse output ${JSON.stringify(mouse_data)}`);
    }
}