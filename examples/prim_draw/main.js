
import { AppUI, exitToShell } from 'athenaos/app.js';

let pad = Pads.get();

const ui = new AppUI({ title: "2D Draw", category: "2d" });

Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);

const pixelColors = [
    Color.new(255, 0, 0), Color.new(255, 165, 0), Color.new(255, 255, 0),
    Color.new(0, 255, 0), Color.new(0, 255, 165), Color.new(0, 255, 255),
    Color.new(0, 0, 255), Color.new(128, 0, 255), Color.new(255, 0, 255),
];

// Each sample draws centered on (cx, cy) inside its card.
const samples = [
    ["Draw.point", (cx, cy) => {
        for (let row = -4; row <= 4; row++)
            pixelColors.forEach((c, i) => Draw.point(cx - 16 + i * 4, cy + row * 4, c));
    }],
    ["Draw.line", (cx, cy) => {
        for (let i = 0; i < 6; i++) Draw.line(cx - 30, cy + 25, cx - 30 + i * 12, cy - 25, pixelColors[i]);
    }],
    ["Draw.rect", (cx, cy) => Draw.rect(cx - 20, cy - 20, 40, 40, Color.new(255, 0, 0))],
    ["Draw.circle", (cx, cy) => {
        Draw.circle(cx, cy, 26, Color.new(64, 64, 64), true);
        Draw.circle(cx, cy, 26, Color.new(255, 0, 0), false);
    }],
    ["Flat triangle", (cx, cy) => Draw.triangle(cx - 22, cy + 20, cx - 22, cy - 20, cx + 22, cy + 20, Color.new(0, 0, 255))],
    ["Gouraud triangle", (cx, cy) => Draw.triangle(cx - 22, cy + 20, cx - 22, cy - 20, cx + 22, cy + 20,
                                                  Color.new(255, 0, 0), Color.new(0, 255, 0), Color.new(0, 0, 255))],
    ["Flat quad", (cx, cy) => Draw.quad(cx - 20, cy - 20, cx - 20, cy + 20, cx + 20, cy - 20, cx + 20, cy + 20, Color.new(128, 0, 255))],
    ["Gouraud quad", (cx, cy) => Draw.quad(cx - 20, cy - 20, cx - 20, cy + 20, cx + 20, cy - 20, cx + 20, cy + 20,
                                          Color.new(255, 0, 0), Color.new(0, 255, 0), Color.new(0, 0, 255), Color.new(255, 255, 0))],
];

const COLS = 4, GAP = 12;

while(true) {
    pad.update();

    if(ui.homeRequested(pad)) {
        break;
    }

    ui.background();

    const { x, y, w, h } = ui.content;
    const cw = (w - GAP * (COLS - 1)) / COLS;
    const ch = (h - GAP) / 2;

    samples.forEach(([label, draw], i) => {
        const cx = x + (i % COLS) * (cw + GAP), cy = y + Math.floor(i / COLS) * (ch + GAP);
        ui.panel(cx, cy, cw, ch);
        ui.heading(cx + 14, cy + 18, label);
        draw(Math.round(cx + cw / 2), Math.round(cy + ch / 2 + 10));
    });

    ui.chrome();
    Screen.flip();
}

exitToShell();
