// Hello world: the starting point of the AthenaEnv JavaScript package.
// athena.elf runs the script named by default_script in athena.ini -- this one.
// Edit it and run athena.elf again. The API is documented in docs/, and the
// demos package has a sample of every module.

const font = new Font("default");
const pad = Pads.get();
const background = Color.new(16, 20, 36);

Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);

let presses = 0;

while (true) {
    pad.update();
    if (pad.justPressed(Pads.CROSS)) presses++;

    Screen.clear(background);

    font.scale = 1.0f;
    font.color = Color.new(128, 128, 128);
    font.print(40, 40, "Hello world!");

    font.scale = 0.6f;
    font.color = Color.new(90, 110, 128);
    font.print(40, 90, "Edit main.js to make it yours.");
    font.print(40, 120, `Cross pressed ${presses} time${presses === 1 ? "" : "s"}.`);

    Screen.flip();
}
