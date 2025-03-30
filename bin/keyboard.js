// {"name": "Keyboard demo", "author": "Daniel Santos", "version": "04102023", "file": "keyboard.js"}

import * as Keyboard from "athena_keyboard.erl"

IOP.loadModule(System.boot_path + "/iop/ps2kbd.irx");

Keyboard.init();

let cur_char = 0;

while(true) {
    cur_char = Keyboard.get();
    if (cur_char != 0) {
        console.log(`pressed ${String.fromCharCode(cur_char)}`);
    }
    
}