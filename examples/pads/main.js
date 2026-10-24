
import { AppUI, COLORS, exitToShell } from 'athenaos/app.js';

const canvas = Screen.getMode();

canvas.psm = Screen.CT24;
canvas.zbuffering = true;
canvas.double_buffering = true;

Screen.setMode(canvas);
Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);

Screen.setVSync(false);
Screen.setFrameCounter(true);

// Every button is under test here, so Home is a combo instead of Triangle.
const ui = new AppUI({ title: "Gamepad tester", category: "input",
                       hints: [["L2+R2", "Rumble"]], home: ["START+R3+UP", "Home"] });

const dark_gray = Color.new(128, 128, 128, 32);
const opaque = Color.new(128, 128, 128, 128);
const translucent = Color.new(128, 128, 128, 60);

const circle = new Image("pads/circle.png");
const cross = new Image("pads/cross.png");
const square = new Image("pads/square.png");
const triangle = new Image("pads/triangle.png");
const up = new Image("pads/up.png");
const down = new Image("pads/down.png");
const left = new Image("pads/left.png");
const right = new Image("pads/right.png");
const start = new Image("pads/start.png");
const pad_select = new Image("pads/select.png");
const r1 = new Image("pads/R1.png");
const r2 = new Image("pads/R2.png");
const l1 = new Image("pads/L1.png");
const l2 = new Image("pads/L2.png");
const l3 = new Image("pads/L3.png");
const r3 = new Image("pads/R3.png");

// Controller layout is shifted down to clear the info card under the top bar.
const OY = 24;

var rumble = false;
var pad = Pads.get(0);

// Pressure-sensitive buttons fade in with the pressure applied.
function pressure(img, btn, x, y) {
    img.color = Color.new(128, 128, 128, ((pad.btns & btn)? Pads.getPressure(btn) : 60));
    img.draw(x, y + OY);
}

function digital(img, btn, x, y) {
    img.color = ((pad.btns & btn)? opaque : translucent);
    img.draw(x, y + OY);
}

while(true) {
    const connectedCount = Pads.getConnectedCount();
    const connectedPorts = Pads.getConnected();
    const isActivePort0 = Pads.isActive(0);
    const isActivePort1 = Pads.isActive(1);

    pad.update();

    if((pad.btns & Pads.START) && (pad.btns & Pads.R3) && (pad.btns & Pads.UP)){
        break;
    }

    ui.background();

    ui.hud([
        `Connected pads: ${connectedCount}`,
        `Active ports: [${connectedPorts.join(', ')}]`,
        `Port 0: ${isActivePort0 ? "ACTIVE" : "INACTIVE"}  |  Port 1: ${isActivePort1 ? "ACTIVE" : "INACTIVE"}`,
        `Rumble: ${rumble ? "ON" : "OFF"}`,
    ]);

    digital(pad_select, Pads.SELECT, 260, 190);
    digital(start, Pads.START, 380, 190);

    pressure(up, Pads.UP, 120, 155);
    pressure(down, Pads.DOWN, 120, 225);
    pressure(left, Pads.LEFT, 85, 190);
    pressure(right, Pads.RIGHT, 155, 190);

    pressure(triangle, Pads.TRIANGLE, 520, 155);
    pressure(cross, Pads.CROSS, 520, 225);
    pressure(square, Pads.SQUARE, 485, 190);
    pressure(circle, Pads.CIRCLE, 555, 190);

    pressure(l1, Pads.L1, 102, 100);
    pressure(r1, Pads.R1, 502, 100);
    pressure(l2, Pads.L2, 137, 100);
    pressure(r2, Pads.R2, 537, 100);

    Draw.rect(220, 280 + OY, 75, 75, dark_gray);
    Draw.rect(380, 280 + OY, 75, 75, dark_gray);

    l3.color = ((pad.btns & Pads.L3)? opaque : translucent);
    l3.draw(pad.lx/4.0f+242.0f, pad.ly/4.0f+300.0f+OY);

    r3.color = ((pad.btns & Pads.R3)? opaque : translucent);
    r3.draw(pad.rx/4.0f+402.0f, pad.ry/4.0f+300.0f+OY);

    if(((pad.btns & Pads.R2) && (pad.btns & Pads.L2)) && ((pad.old_btns & Pads.R2) && !(pad.old_btns & Pads.L2))){
        rumble? Pads.rumble(0 ,0) : Pads.rumble(1 ,255);
        rumble ^= 1;
    }

    ui.status = `${Screen.getFPS(360)} FPS`;
    ui.chrome([["L2+R2", "Rumble"]], "Buttons fade in with pressure");
    Screen.flip();
}

Pads.rumble(0, 0);
exitToShell();
