// {"name": "Hello world", "author": "Daniel Santos", "version": "03112025", "file": "hello.js", "category": "2d", "blurb": "Font basics: colors, outline, drop shadow and scaling.", "exit": "TRIANGLE"}

import { AppUI, exitToShell } from 'athenaos/app.js';

const pad = Pads.get();

const font = new Font("default");

const ui = new AppUI({ title: "Hello world", category: "2d" });

Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);

// Prints the four variants (plain, color, outline, shadow) at one scale.
function variants(x, y, scale, step) {
    font.scale = scale;

    font.color = Color.new(128, 128, 128);
    font.print(x, y, "Hello World!");

    font.color = Color.new(128, 0, 0);
    font.print(x, y + step, "Hello World(with custom color)!");

    font.color = Color.new(128, 128, 128);
    font.outline = 1.0f;
    font.outline_color = Color.new(128, 0, 0);
    font.print(x, y + step * 2, "Hello World(with outline)!");

    font.outline = 0.0f;
    font.outline_color = Color.new(0, 0, 0);

    font.color = Color.new(128, 128, 128);
    font.dropshadow = 1.0f;
    font.dropshadow_color = Color.new(128, 0, 0);
    font.print(x, y + step * 3, "Hello World(with drop shadow)!");

    font.dropshadow = 0.0f;
    font.dropshadow_color = Color.new(0, 0, 0);
}

while(true) {
    pad.update();

    if(ui.homeRequested(pad)) {
        break;
    }

    ui.background();
    const { x, y, w } = ui.content;

    ui.panel(x, y, w, 168);
    ui.heading(x + 20, y + 20, "Scale 1.0");
    variants(x + 20, y + 32, 1.0f, 32);

    ui.panel(x, y + 184, w, 120);
    ui.heading(x + 20, y + 204, "Scale 0.6");
    variants(x + 20, y + 216, 0.6f, 20);

    ui.chrome();
    Screen.flip();
}

exitToShell();
