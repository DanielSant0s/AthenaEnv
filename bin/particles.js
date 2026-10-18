// {"name": "Native 2D particles", "author": "Daniel Santos", "file": "particles.js"}
// Move the emitter with the left stick/D-pad. Cross: burst. Circle: toggle smoke.
const atlas = new Image("render/fx_atlas.png");
atlas.filter = LINEAR;
const smoke = new Particles.System({ texture: atlas, capacity: 512 });
const dust = smoke.createEmitter({
    rate: 45, frame: [0, 0, 32, 32], lifetime: [0.7, 1.4], size: [10, 20], scaleEnd: 3,
    velocityMin: [-15, -60, 0], velocityMax: [15, -25, 0], spread: [5, 2, 0], drag: 0.7,
    color: [100, 110, 128, 60], colorEnd: [100, 110, 128, 0], fadeIn: 0.1
});
const sparks = new Particles.System({ texture: atlas, capacity: 512, blend: "additive" });
const burst = sparks.createEmitter({
    frame: [32, 0, 32, 32], lifetime: [0.25, 0.8], size: [3, 7], scaleEnd: 0.3,
    velocityMin: [-180, -230, 0], velocityMax: [180, 50, 0], acceleration: [0, 320, 0],
    color: [180, 90, 20, 128], colorEnd: [128, 20, 4, 0]
});
const font = new Font("default"); font.scale = 0.6;
const pad = Pads.get(0), mode = Screen.getMode(), timer = Timer.new();
let x = mode.width / 2, y = mode.height * 0.65, running = true;
dust.setPosition(x, y); dust.start();
Screen.setFrameCounter(true);
while (true) {
    const dt = Math.min(Timer.getTime(timer) / 1000000, 0.05);
    Timer.reset(timer); pad.update();
    if (pad.justPressed(Pads.TRIANGLE)) break;
    let dx = pad.lx / 128, dy = pad.ly / 128;
    if (Math.abs(dx) < 0.15) dx = 0;
    if (Math.abs(dy) < 0.15) dy = 0;
    if (pad.pressed(Pads.LEFT)) dx = -1;
    if (pad.pressed(Pads.RIGHT)) dx = 1;
    if (pad.pressed(Pads.UP)) dy = -1;
    if (pad.pressed(Pads.DOWN)) dy = 1;
    x = Math.max(0, Math.min(mode.width, x + dx * 200 * dt));
    y = Math.max(0, Math.min(mode.height, y + dy * 200 * dt));
    dust.setPosition(x, y);
    if (pad.justPressed(Pads.CROSS)) burst.burst(96, x, y);
    if (pad.justPressed(Pads.CIRCLE)) { running = !running; if (running) dust.start(); else dust.stop(); }
    if (pad.justPressed(Pads.R3)) { smoke.clear(); sparks.clear(); }
    smoke.update(dt); sparks.update(dt);
    Screen.clear(Color.new(12, 14, 20));
    smoke.draw(); sparks.draw();
    Draw.line(x - 5, y, x + 5, y, Color.new(128, 128, 128));
    Draw.line(x, y - 5, x, y + 5, Color.new(128, 128, 128));
    font.print(10, 10, "Native particles | Left stick: move | X: sparks | O: smoke | R3: clear");
    font.print(10, 30, `${Screen.getFPS(60)} FPS | ${smoke.activeCount + sparks.activeCount} particles | Triangle: exit`);
    Screen.flip();
}
smoke.dispose(); sparks.dispose();
