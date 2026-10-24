import { AppUI, exitToShell } from 'athenaos/app.js';

// Unified lighting, PS2 style: one flickering torch moves through a closed room.
// Every surface is DOT3 normal mapped against it, and the pillars and crates
// cast stencil shadow volumes from it -- onto the walls, the floor and each
// other. The light is a point light, so both the relief and the shadows swing
// as it passes.

const ui = new AppUI({ title: "Torchlight", category: "3d",
                       hints: [["L-stick", "Look"], ["cross", "Shadows"], ["square", "Mode"], ["circle", "Torch"]] });

Screen.setFrameCounter(true);
Screen.setVSync(true);

const canvas = Screen.getMode();
canvas.zbuffering = true;
canvas.psm = Screen.CT32;
canvas.psmz = Screen.Z24; // DOT3 and the shadow volumes both keep scratch in the top of Z24
Screen.setMode(canvas);
Screen.initBuffers();

ui.loading("Loading assets...", "Bricks, slabs and crates");

Render.init();
Render.setView(60.0, 0.5, 200.0);

os.chdir("render");

const MODES = [Render.DOT3_OFF, Render.DOT3_2PASS, Render.DOT3_4PASS];
const MODE_NAMES = ["Gouraud", "DOT3 2-pass", "DOT3 4-pass"];
let mode = 2;

const brickDiffuse = new Image("bricks_diffuse.png");
const brickNormal = new Image("bricks_normal.png");
const tileDiffuse = new Image("tiles_diffuse.png");
const tileNormal = new Image("tiles_normal.png");
for (const img of [brickDiffuse, brickNormal, tileDiffuse, tileNormal]) img.filter = LINEAR;

// --- Geometry ---

// A mesh from triangles given as [position, normal, uv] corners, drawn with
// diffuse modulated by the torch and normal mapped with normalMap.
function makeMesh(corners, diffuse, normalMap) {
    const count = corners.length;
    const positions = new Float32Array(count * 4);
    const normals = new Float32Array(count * 4);
    const texcoords = new Float32Array(count * 4);
    const colors = new Float32Array(count * 4);
    corners.forEach(([p, n, uv], i) => {
        positions.set([p[0], p[1], p[2], 1], i * 4);
        normals.set([n[0], n[1], n[2], 1], i * 4);
        texcoords.set([uv[0], uv[1], 1, 1], i * 4);
    });
    const data = new RenderData(Render.vertexList(positions, normals, texcoords, colors));
    const materials = data.materials;
    materials[0].diffuse = { r: 1, g: 1, b: 1, a: 1 };
    materials[0].texture_id = data.pushTexture(diffuse);
    materials[0].normal_texture_id = data.pushTexture(normalMap);
    data.materials = materials;
    return data;
}

// A rectangle from its top-left corner o and the edges u (rightwards) and v
// (downwards), seen from the front; the texture repeated ru x rv times.
function quad(o, u, v, ru, rv) {
    const n = cross(v, u);
    const at = (s, t) => [[o[0] + u[0] * s + v[0] * t, o[1] + u[1] * s + v[1] * t, o[2] + u[2] * s + v[2] * t], n, [s * ru, t * rv]];
    return [at(0, 0), at(0, 1), at(1, 1), at(0, 0), at(1, 1), at(1, 0)];
}

function cross(a, b) {
    const c = [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
    const l = Math.hypot(c[0], c[1], c[2]);
    return [c[0] / l, c[1] / l, c[2] / l];
}

// An octagonal pillar around the origin, capped at both ends: a shadow volume
// needs a closed mesh. Smooth side normals, the bricks once around.
function pillar(radius, height) {
    const corners = [];
    const ring = (k, y) => {
        const a = 2 * Math.PI * k / 8;
        return [Math.sin(a) * radius, y, Math.cos(a) * radius];
    };
    const side = (k, t) => {
        const a = 2 * Math.PI * k / 8;
        return [ring(k, height * (1 - t)), [Math.sin(a), 0, Math.cos(a)], [k / 8, t * height / 4]];
    };
    for (let k = 0; k < 8; k++) {
        corners.push(side(k, 0), side(k, 1), side(k + 1, 1), side(k, 0), side(k + 1, 1), side(k + 1, 0));
        // Caps, wound to face up and down.
        corners.push([[0, height, 0], [0, 1, 0], [0.5, 0.5]], [ring(k, height), [0, 1, 0], [0.5, 0]], [ring(k + 1, height), [0, 1, 0], [0.6, 0]]);
        corners.push([[0, 0, 0], [0, -1, 0], [0.5, 0.5]], [ring(k + 1, 0), [0, -1, 0], [0.6, 0]], [ring(k, 0), [0, -1, 0], [0.5, 0]]);
    }
    return corners;
}

// A brick tile is 4 x 4 units (8 rows), a slab 2 x 2.
const R = 8.0, H = 5.0;
const walls = makeMesh([
    ...quad([-R, H, -R], [2 * R, 0, 0], [0, -H, 0], 2 * R / 4, H / 4),   // north, facing +z
    ...quad([R, H, R], [-2 * R, 0, 0], [0, -H, 0], 2 * R / 4, H / 4),    // south, facing -z
    ...quad([-R, H, R], [0, 0, -2 * R], [0, -H, 0], 2 * R / 4, H / 4),   // west, facing +x
    ...quad([R, H, -R], [0, 0, 2 * R], [0, -H, 0], 2 * R / 4, H / 4),    // east, facing -x
], brickDiffuse, brickNormal);
const floor = makeMesh(quad([-R, 0, -R], [2 * R, 0, 0], [0, 0, 2 * R], 2 * R / 4, 2 * R / 4), tileDiffuse, tileNormal);
const ceiling = makeMesh(quad([-R, H, R], [2 * R, 0, 0], [0, 0, -2 * R], 2 * R / 4, 2 * R / 4), tileDiffuse, tileNormal);
const pillarData = makeMesh(pillar(0.55, H), brickDiffuse, brickNormal);
const crateData = new RenderData("box_bump.gltf");

const datas = [walls, floor, ceiling, pillarData, crateData];
function applyMode() {
    for (const data of datas) data.normal_mapping = MODES[mode];
}
applyMode();

const room = [walls, floor, ceiling].map((data) => new RenderObject(data));

// Casters: everything inside the room. Not frozen -- each volume reads its
// caster's positions once.
const casters = [];
function place(data, x, y, z, ry) {
    const obj = new RenderObject(data);
    obj.position = { x, y, z };
    obj.rotation = { x: 0, y: ry, z: 0 };
    casters.push(obj);
    return obj;
}
// Pillars within 3.2 of the centre, crates in the corners: the torch circles
// between the two, the camera outside it.
place(pillarData, -2.2, 0, -1.4, 0.0);
place(pillarData, 2.2, 0, 1.4, 0.4);
place(crateData, 6.6, 1.0, -6.6, 0.3);
place(crateData, -6.6, 1.0, 6.6, -0.5);
place(crateData, -6.6, 3.0, 6.6, 0.2);
place(crateData, 6.4, 1.0, 6.2, 0.9);
const floater = place(crateData, 0.3, 3.0, -0.5, 0.0);
floater.scale = { x: 0.6, y: 0.6, z: 0.6 };

const scene = [...room, ...casters];
const volumes = casters.map((obj) => {
    const volume = new Shadows.Volume(obj);
    volume.extrusion = 30.0;
    return volume;
});

// --- The torch ---

const light = Lights.new();
Lights.set(light, Lights.AMBIENT, 0.08, 0.08, 0.1);
Lights.set(light, Lights.ATTENUATION, 0, 0, 0.02);

// Fire: additive camera-facing puffs from the soft cell of the shared FX atlas,
// and a few embers.
const fxAtlas = new Image("fx_atlas.png");
fxAtlas.filter = LINEAR;
const fire = new Particles.System({ texture: fxAtlas, capacity: 256, space: "world", blend: "additive", seed: 1234 });
const flame = fire.createEmitter({
    frame: [0, 0, 32, 32], lifetime: [0.25, 0.5], size: [0.22, 0.34], scaleEnd: 0.2,
    velocityMin: [-0.15, 0.6, -0.15], velocityMax: [0.15, 1.2, 0.15], spread: [0.05, 0.02, 0.05],
    color: [128, 80, 30, 90], colorEnd: [110, 30, 8, 0], rate: 90
});
const embers = fire.createEmitter({
    frame: [0, 0, 32, 32], lifetime: [0.6, 1.2], size: [0.03, 0.05], scaleEnd: 0.3,
    velocityMin: [-0.4, 0.5, -0.4], velocityMax: [0.4, 1.6, 0.4], acceleration: [0, -0.6, 0],
    color: [128, 90, 30, 128], colorEnd: [128, 40, 10, 0], rate: 12
});
flame.start();
embers.start();

// Flicker: a few incommensurate sines, cheap and never repeating visibly.
function flicker(t) {
    return 0.86 + 0.07 * Math.sin(t * 17.0) + 0.05 * Math.sin(t * 23.7 + 1.3) + 0.03 * Math.sin(t * 41.3 + 0.4);
}

// The torch circles the room between the pillars and the crates, weaving in
// and out and low enough to throw long shadows up the walls.
const torchAngle = (t) => t * 0.3;
function torchPath(t) {
    const a = torchAngle(t), r = 4.6 + 0.4 * Math.sin(t * 0.7);
    return [Math.sin(a) * r, 1.3 + 0.3 * Math.sin(t * 0.53), Math.cos(a) * r];
}

// --- Camera ---

// The camera follows the torch around the room from across it, outside its
// path, looking at the flame: the torch stays in view and the pillars throw
// their shadows towards the camera. The stick swings it round.
let yaw = Math.PI - 0.55, pitch = 0.0;
const CAM_RADIUS = 6.6, CAM_HEIGHT = 3.0;

const pad = Pads.get();
const black = Color.new(0, 0, 0, 128);
const timer = Timer.new();
let shadowsOn = true;
let torchMoving = true;
let torchT = 0.0;
let lastTime = 0.0;

while (true) {
    Screen.clear(black);
    Render.begin();
    pad.update();

    if (ui.homeRequested(pad)) {
        fire.dispose();
        os.chdir("..");
        exitToShell();
    }

    if (pad.justPressed(Pads.CROSS)) shadowsOn = !shadowsOn;
    if (pad.justPressed(Pads.SQUARE)) {
        mode = (mode + 1) % MODES.length;
        applyMode();
    }
    if (pad.justPressed(Pads.CIRCLE)) torchMoving = !torchMoving;

    const now = Timer.getTime(timer) / 1000000;
    const dt = Math.min(0.1, Math.max(0, now - lastTime));
    lastTime = now;
    if (torchMoving) torchT += dt;

    const lx = (pad.lx > 25 || pad.lx < -25) ? pad.lx / 128.0 : 0;
    const ly = (pad.ly > 25 || pad.ly < -25) ? pad.ly / 128.0 : 0;
    yaw += lx * dt * 1.6;
    pitch = Math.max(-1.0, Math.min(1.0, pitch - ly * dt * 1.5));

    const [tx, ty, tz] = torchPath(torchT);
    const ca = torchAngle(torchT) + yaw;
    Camera.position(Math.sin(ca) * CAM_RADIUS, CAM_HEIGHT, Math.cos(ca) * CAM_RADIUS);
    Camera.target(tx * 0.7, 1.4 + pitch * 2.0, tz * 0.7);

    const f = flicker(now);
    const jitter = 0.03 * Math.sin(now * 29.0);
    Lights.set(light, Lights.POSITION, tx + jitter, ty, tz - jitter);
    Lights.set(light, Lights.DIFFUSE, 1.7 * f, 1.3 * f, 0.85 * f);

    floater.rotation = { x: 0.5, y: now * 0.4, z: 0.3 };

    Camera.update();
    Screen.setParam(Screen.DEPTH_TEST_ENABLE, true);
    Screen.setParam(Screen.DEPTH_TEST_METHOD, Screen.DEPTH_GEQUAL);

    for (const obj of room) obj.render();
    for (const obj of casters) obj.render();

    // Shadowed means ambient only: those pixels are drawn again without the
    // torch, the way Doom 3 lights a scene.
    if (shadowsOn && Shadows.beginVolumes()) {
        for (const volume of volumes) volume.render();
        Shadows.endVolumes(scene);
    }

    flame.setPosition(tx, ty - 0.05, tz);
    embers.setPosition(tx, ty, tz);
    fire.update(dt);
    fire.draw();

    Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);

    const prof = Screen.getProfile();
    ui.status = `${Screen.getFPS(360)} FPS`;
    ui.hud([
        `SQUARE  ${MODE_NAMES[mode]}   CROSS  Shadows: ${shadowsOn ? "ON" : "OFF"} (${volumes.length} volumes)`,
        `frame ${prof.frame.toFixed(1)} ms  |  render ${prof.render.toFixed(1)}  |  gpu ${prof.gpuWait.toFixed(1)}`,
    ]);
    ui.chrome();

    Screen.flip();
}
