
import { Terminal, exitToShell } from 'athenaos/app.js';

const term = new Terminal({ title: "DVD Remote demo", category: "input",
                            subtitle: "Point the DVD remote at the console -- STOP also leaves" });
const pad = Pads.get(0);

IOP.loadModule("rmman");

Remote.init();

// The remote control shows up on controller port 2 (index 1), whether it's
// the external IR dongle on a fat PS2 or the built-in receiver on a slim.
let remote = Remote.get(1);

const buttonNames = {
    [Remote.DVD_PLAY]: "PLAY",
    [Remote.DVD_PAUSE]: "PAUSE",
    [Remote.DVD_STOP]: "STOP",
    [Remote.DVD_NEXT]: "NEXT",
    [Remote.DVD_PREV]: "PREV",
    [Remote.DVD_SCAN_FORW]: "SCAN_FORW",
    [Remote.DVD_SCAN_BACK]: "SCAN_BACK",
};

while (true) {
    remote.update();
    pad.update();

    if (term.homeRequested(pad)) {
        break;
    }

    if (!remote.released() && remote.button !== remote.old_button) {
        const name = buttonNames[remote.button] || ("0x" + remote.button.toString(16));
        term.log(`remote button: ${name}`);
    }

    if (remote.pressed(Remote.DVD_PLAY)) {
        term.log("-> play requested");
    }

    if (remote.pressed(Remote.DVD_STOP)) {
        break;
    }

    term.draw();
}

exitToShell();
