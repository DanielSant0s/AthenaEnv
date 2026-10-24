// {"name": "Shadow volumes", "author": "Daniel Santos", "version": "23102026", "file": "shadow_volumes.js", "category": "3d", "blurb": "Stencil shadow volumes on the GS, with the red channel as the stencil.", "exit": "TRIANGLE"}
import { AppUI, exitToShell } from 'athenaos/app.js';

// Stencil shadow volumes: each crate's silhouette, pushed away from the light,
// counted into the red channel (+1 front faces, -1 back faces). Unlike the
// projected shadows, a shadow lands on anything inside the volume -- the floor,
// another crate, the crate itself.

const ui = new AppUI({ title: "Shadow volumes", category: "3d",
                       hints: [["L-stick", "Light"], ["cross", "Shadows"], ["circle", "Spin"]] });

Screen.setFrameCounter(true);
Screen.setVSync(true);

const canvas = Screen.getMode();
canvas.zbuffering = true;
canvas.psm = Screen.CT32;
canvas.psmz = Screen.Z24; // the scene's red waits in the top byte of Z24
Screen.setMode(canvas);
Screen.initBuffers();

ui.loading("Loading assets...", "Crates");

Render.init();
Render.setView(60.0, 1.0, 400.0);

os.chdir("render");

// Floor: two triangles, facing up.
const FLOOR = 9.0, FLOOR_Y = -1.0;
const floorData = new RenderData(Render.vertexList(
    new Float32Array([
        -FLOOR, FLOOR_Y, -FLOOR, 1,  -FLOOR, FLOOR_Y, FLOOR, 1,  FLOOR, FLOOR_Y, FLOOR, 1,
        -FLOOR, FLOOR_Y, -FLOOR, 1,   FLOOR, FLOOR_Y, FLOOR, 1,  FLOOR, FLOOR_Y, -FLOOR, 1,
    ]),
    new Float32Array([0, 1, 0, 1,  0, 1, 0, 1,  0, 1, 0, 1,  0, 1, 0, 1,  0, 1, 0, 1,  0, 1, 0, 1]),
    new Float32Array(24).fill(0),
    new Float32Array([
        0.55, 0.55, 0.6, 1,  0.55, 0.55, 0.6, 1,  0.55, 0.55, 0.6, 1,
        0.55, 0.55, 0.6, 1,  0.55, 0.55, 0.6, 1,  0.55, 0.55, 0.6, 1,
    ])));
floorData.texture_mapping = false;
floorData.face_culling = Render.CULL_FACE_NONE;
const floorObject = new RenderObject(floorData);

// Not frozen: a volume reads the crate's positions once, to find its edges.
const crate = new RenderData("box_bump.gltf");
const CRATES = [
    { x: -2.6, y: 0.0, z: 0.8, ry: 0.3 },
    { x: 2.4, y: 0.0, z: -1.2, ry: -0.5 },
    { x: 0.0, y: 0.0, z: -0.2, ry: 0.0 },
    { x: 0.3, y: 2.7, z: 0.4, ry: 0.7 }, // floats over the middle one
];
const crates = CRATES.map((c) => {
    const obj = new RenderObject(crate);
    obj.position = { x: c.x, y: c.y, z: c.z };
    obj.rotation = { x: 0.0, y: c.ry, z: 0.0 };
    return obj;
});
const volumes = crates.map((obj) => {
    const volume = new Shadows.Volume(obj);
    volume.extrusion = 30.0;
    return volume;
});

Camera.position(0.0, 6.0, 12.0);
Camera.target(0.0, 0.0, 0.0);

const light = Lights.new();
Lights.set(light, Lights.AMBIENT, 0.25, 0.25, 0.28);
Lights.set(light, Lights.DIFFUSE, 0.85, 0.82, 0.75);

const pad = Pads.get();
const sky = Color.new(52, 58, 70, 128);

let shadowsOn = true;
let spin = true;
let lightAngle = 0.8;
let lightPitch = 0.9;
let floatAngle = 0.7;

while (true) {
    Screen.clear(sky);
    Render.begin();
    pad.update();

    if (ui.homeRequested(pad)) {
        os.chdir("..");
        exitToShell();
    }
    if (pad.justPressed(Pads.CROSS)) shadowsOn = !shadowsOn;
    if (pad.justPressed(Pads.CIRCLE)) spin = !spin;

    const lx = (pad.lx > 25 || pad.lx < -25) ? pad.lx / 2048.0 : 0;
    const ly = (pad.ly > 25 || pad.ly < -25) ? pad.ly / 2048.0 : 0;
    if (lx || ly) {
        lightAngle += lx;
        lightPitch = Math.max(0.25, Math.min(1.45, lightPitch - ly));
    } else {
        lightAngle += 0.01;
    }
    if (spin) floatAngle += 0.012;
    crates[3].rotation = { x: 0.4, y: floatAngle, z: 0.2 };

    // Direction towards the light.
    const cp = Math.cos(lightPitch);
    Lights.set(light, Lights.DIRECTION, Math.sin(lightAngle) * cp, Math.sin(lightPitch), Math.cos(lightAngle) * cp);

    Camera.update();
    Screen.setParam(Screen.DEPTH_TEST_ENABLE, true);
    Screen.setParam(Screen.DEPTH_TEST_METHOD, Screen.DEPTH_GEQUAL);

    floorObject.render();
    for (const obj of crates) obj.render();

    if (shadowsOn && Shadows.beginVolumes()) {
        for (const volume of volumes) volume.render();
        Shadows.endVolumes(0.55);
    }

    Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);

    const prof = Screen.getProfile();
    ui.status = `${Screen.getFPS(360)} FPS`;
    ui.hud([
        `CROSS  Shadows: ${shadowsOn ? "ON" : "OFF"}  (${volumes.length} volumes, ${volumes[0].edges} edges each)`,
        `frame ${prof.frame.toFixed(1)} ms  |  render ${prof.render.toFixed(1)}  |  gpu ${prof.gpuWait.toFixed(1)}`,
    ]);
    ui.chrome();

    Screen.flip();
}
