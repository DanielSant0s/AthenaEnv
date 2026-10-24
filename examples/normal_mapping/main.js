import { AppUI, exitToShell } from 'athenaos/app.js';

// DOT3 normal mapping after "DOT3 Normal Mapping on the PS2" (Mikkelsen, 2004).
// Dungeon: a corner of brick walls and stone slabs, plus an octagonal pillar --
// a few big triangles that Gouraud can only light flat, while DOT3 brings out
// every brick, chip and crack as the light sweeps across. Side by side: Gouraud | 2-pass | 4-pass, as crates and as low-poly spheres --
// a flat face gets the same light vector at every vertex, so only a curved
// surface shows the 2-pass light losing length between vertices. Stress: nine
// crates, all in one mode, for the cost of each. The light circles the
// objects; the left stick moves it by hand.

const ui = new AppUI({ title: "Normal mapping", category: "3d",
                       hints: [["L-stick", "Light"], ["cross", "Layout"], ["square", "Mode"], ["circle", "Spin"]] });

Screen.setFrameCounter(true);

const canvas = Screen.getMode();
canvas.zbuffering = true;
canvas.psm = Screen.CT32;
canvas.psmz = Screen.Z24; // DOT3 keeps its scratch byte in the top of Z24
Screen.setMode(canvas);
Screen.initBuffers();

ui.loading("Loading assets...", "Bricks, slabs and crates");

Render.init();
Render.setView(60.0, 1.0, 400.0);

os.chdir("render");

const MODES = [Render.DOT3_OFF, Render.DOT3_2PASS, Render.DOT3_4PASS];
const MODE_NAMES = ["Gouraud + emboss", "DOT3 2-pass", "DOT3 4-pass"];

// One RenderData per mode: the mode is a property of the data.
const crates = MODES.map((mode) => {
    const data = new RenderData("box_bump.gltf");
    data.normal_mapping = mode;
    return data;
});

const diffuseMap = new Image("box_diffuse.png");
const normalMap = new Image("box_normalmap.png");
diffuseMap.filter = LINEAR;
normalMap.filter = LINEAR;

// A UV sphere as a triangle list, the crate maps wrapped twice around it.
function makeSphere(rings, segments, radius, mode) {
    const count = rings * segments * 6;
    const positions = new Float32Array(count * 4);
    const normals = new Float32Array(count * 4);
    const texcoords = new Float32Array(count * 4);
    const colors = new Float32Array(count * 4);
    let v = 0;
    const put = (r, s) => {
        const theta = Math.PI * r / rings, phi = 2 * Math.PI * s / segments;
        const x = Math.sin(theta) * Math.cos(phi), y = Math.cos(theta), z = Math.sin(theta) * Math.sin(phi);
        positions.set([x * radius, y * radius, z * radius, 1], v * 4);
        normals.set([x, y, z, 1], v * 4);
        texcoords.set([2 * s / segments, r / rings, 1, 1], v * 4);
        colors.set([0, 0, 0, 0], v * 4);
        v++;
    };
    for (let r = 0; r < rings; r++) {
        for (let s = 0; s < segments; s++) {
            put(r, s); put(r + 1, s); put(r + 1, s + 1);
            put(r, s); put(r + 1, s + 1); put(r, s + 1);
        }
    }
    const data = new RenderData(Render.vertexList(positions, normals, texcoords, colors));
    data.face_culling = Render.CULL_FACE_NONE;
    const materials = data.materials;
    materials[0].diffuse = { r: 1, g: 1, b: 1, a: 1 };
    materials[0].texture_id = data.pushTexture(diffuseMap);
    materials[0].normal_texture_id = data.pushTexture(normalMap);
    data.materials = materials;
    data.normal_mapping = mode;
    return data;
}

const spheres = MODES.map((mode) => makeSphere(8, 12, 1.5, mode));

// --- Dungeon ---
// Procedural, tileable 256x256 maps (bricks and stone slabs) whose relief is
// what Gouraud cannot show: flat albedo, all the shape in the normal map.
const brickDiffuse = new Image("bricks_diffuse.png");
const brickNormal = new Image("bricks_normal.png");
const tileDiffuse = new Image("tiles_diffuse.png");
const tileNormal = new Image("tiles_normal.png");
for (const img of [brickDiffuse, brickNormal, tileDiffuse, tileNormal]) img.filter = LINEAR;

// A mesh from triangles given as [position, normal, uv] corners.
function makeMesh(corners, diffuse, normalMap, mode) {
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
    data.face_culling = Render.CULL_FACE_NONE;
    const materials = data.materials;
    materials[0].diffuse = { r: 1, g: 1, b: 1, a: 1 };
    materials[0].texture_id = data.pushTexture(diffuse);
    materials[0].normal_texture_id = data.pushTexture(normalMap);
    data.materials = materials;
    data.normal_mapping = mode;
    return data;
}

// A rectangle from its top-left corner o and the edges u (rightwards) and v
// (downwards): texture u along u, texture v along v, repeated ru x rv times.
function quad(o, u, v, n, ru, rv) {
    const at = (s, t) => [[o[0] + u[0] * s + v[0] * t, o[1] + u[1] * s + v[1] * t, o[2] + u[2] * s + v[2] * t], n, [s * ru, t * rv]];
    return [at(0, 0), at(0, 1), at(1, 1), at(0, 0), at(1, 1), at(1, 0)];
}

// The pillar: 8 sides, smooth normals, the bricks once around.
function pillar(x, z, radius, height) {
    const corners = [];
    const side = (k, t) => {
        const a = 2 * Math.PI * k / 8;
        const n = [Math.sin(a), 0, Math.cos(a)];
        return [[x + n[0] * radius, height * (1 - t), z + n[2] * radius], n, [k / 8, t * height / 4]];
    };
    for (let k = 0; k < 8; k++)
        corners.push(side(k, 0), side(k, 1), side(k + 1, 1), side(k, 0), side(k + 1, 1), side(k + 1, 0));
    return corners;
}

const ROOM = 4.0, WALL = 4.0;
const dungeon = MODES.map((mode) => [
    makeMesh([
        ...quad([-ROOM, WALL, -ROOM], [2 * ROOM, 0, 0], [0, -WALL, 0], [0, 0, 1], 2, 1),   // back wall
        ...quad([-ROOM, WALL, ROOM], [0, 0, -2 * ROOM], [0, -WALL, 0], [1, 0, 0], 2, 1),  // left wall
        ...pillar(1.4, -1.2, 0.6, WALL),
    ], brickDiffuse, brickNormal, mode),
    makeMesh(quad([-ROOM, 0, -ROOM], [2 * ROOM, 0, 0], [0, 0, 2 * ROOM], [0, 1, 0], 2, 2),   // floor
             tileDiffuse, tileNormal, mode),
]);
const dungeonObjects = dungeon.map((datas) => datas.map((data) => new RenderObject(data)));

const DUNGEON = 0, SIDE = 1, SPHERES = 2, STRESS = 3;
const LAYOUT_NAMES = ["dungeon", "crates side by side", "spheres side by side", "9 crates"];
let layout = DUNGEON;
let shownMode = 2;
let spin = true;

const sideObjects = crates.map((data, i) => {
    const obj = new RenderObject(data);
    obj.position = { x: (i - 1) * 3.2, y: 0.0, z: 0.0 };
    return obj;
});

// 3x3 crates, the same mode, rebound when the mode changes.
const sphereObjects = spheres.map((data, i) => {
    const obj = new RenderObject(data);
    obj.position = { x: (i - 1) * 3.4, y: 0.0, z: 0.0 };
    return obj;
});

let stressObjects = [];
function buildStress() {
    stressObjects = [];
    for (let i = 0; i < 9; i++) {
        const obj = new RenderObject(crates[shownMode]);
        obj.position = { x: ((i % 3) - 1) * 3.0, y: (Math.floor(i / 3) - 1) * 2.6, z: -2.0 };
        stressObjects.push(obj);
    }
}
buildStress();

Camera.position(0.0, 0.0, 9.0);
Camera.target(0.0, 0.0, 0.0);

const light = Lights.new();
Lights.set(light, Lights.AMBIENT, 0.18, 0.18, 0.18);
Lights.set(light, Lights.DIFFUSE, 0.9, 0.9, 0.9);

const pad = Pads.get();
const gray = Color.new(40, 40, 40, 128);

let lightAngle = 0.6;
let lightPitch = 0.5;
let crateAngle = 0.4;
let sweep = 0.01; // dungeon: the light swings across the walls, low and grazing

// --- Automatic benchmark ---
// Cycles the stress layout through the three modes, BENCH_PHASE_MS each, and
// logs the averaged Screen.getProfile() of every phase to the console. VSync
// is off meanwhile, so the frame time is what the frame really costs.
const BENCHMARK = false;
Screen.setVSync(!BENCHMARK);
const BENCH_PHASE_MS = 10000;
const BENCH_WARMUP_MS = 2000;
const BENCH_KEYS = ["frame", "script", "render", "gpuWait", "vsync"];
let benchPhase = 0;
let benchSum = null;
let benchSamples = 0;
let benchFrames = 0;
const benchTimer = Timer.new();
function benchApply() {
    layout = STRESS;
    shownMode = benchPhase;
    buildStress();
    benchSum = {};
    for (const k of BENCH_KEYS) benchSum[k] = 0;
    benchSamples = benchFrames = 0;
    Timer.reset(benchTimer);
}
function benchTick() {
    const elapsed = Timer.getTime(benchTimer) / 1000;
    benchFrames++;
    if (elapsed >= BENCH_WARMUP_MS && benchFrames > 60) {
        const prof = Screen.getProfile();
        for (const k of BENCH_KEYS) benchSum[k] += prof[k];
        benchSamples++;
    }
    if (elapsed < BENCH_PHASE_MS || benchSamples === 0) return;
    const avg = {};
    for (const k of BENCH_KEYS) avg[k] = benchSum[k] / benchSamples;
    console.log(
        `[bench] ${MODE_NAMES[benchPhase]} x9 | ${(1000 / avg.frame).toFixed(1)} fps | ` +
        `frame ${avg.frame.toFixed(1)} | script ${avg.script.toFixed(1)} | render ${avg.render.toFixed(1)} | ` +
        `gpu wait ${avg.gpuWait.toFixed(1)} | vsync ${avg.vsync.toFixed(1)} (ms)`);
    benchPhase = (benchPhase + 1) % MODES.length;
    benchApply();
}
if (BENCHMARK) {
    console.log("[bench] automatic benchmark on: 3 phases, " + (BENCH_PHASE_MS / 1000) + " s each");
    benchApply();
}

while (true) {
    Screen.clear(gray);
    Render.begin();
    pad.update();

    if (ui.homeRequested(pad)) {
        os.chdir("..");
        exitToShell();
    }

    if (!BENCHMARK) {
        if (pad.justPressed(Pads.CROSS)) layout = (layout + 1) % LAYOUT_NAMES.length;
        if (pad.justPressed(Pads.SQUARE)) {
            shownMode = (shownMode + 1) % MODES.length;
            buildStress();
        }
        if (pad.justPressed(Pads.CIRCLE)) spin = !spin;
    }

    const lx = (pad.lx > 25 || pad.lx < -25) ? pad.lx / 2048.0 : 0;
    const ly = (pad.ly > 25 || pad.ly < -25) ? pad.ly / 2048.0 : 0;
    if (lx || ly) {
        lightAngle += lx;
        lightPitch = Math.max(-1.2, Math.min(1.2, lightPitch - ly));
    } else if (layout === DUNGEON) {
        if (Math.abs(lightAngle) > 1.25) sweep = lightAngle > 0 ? -0.01 : 0.01;
        lightAngle += sweep;
    } else {
        lightAngle += 0.012;
    }
    if (spin) crateAngle += 0.006;

    // Direction towards the light.
    const cp = Math.cos(lightPitch);
    Lights.set(light, Lights.DIRECTION, Math.sin(lightAngle) * cp, Math.sin(lightPitch), Math.cos(lightAngle) * cp);

    if (layout === DUNGEON) {
        Camera.position(1.0, 2.2, 6.0);
        Camera.target(-0.6, 1.4, -2.0);
    } else {
        Camera.position(0.0, 0.0, 9.0);
        Camera.target(0.0, 0.0, 0.0);
    }
    Camera.update();
    Screen.setParam(Screen.DEPTH_TEST_ENABLE, true);
    Screen.setParam(Screen.DEPTH_TEST_METHOD, Screen.DEPTH_GEQUAL);

    if (layout === DUNGEON) {
        for (const obj of dungeonObjects[shownMode]) obj.render();
    } else {
        const objects = layout === SIDE ? sideObjects : layout === SPHERES ? sphereObjects : stressObjects;
        for (const obj of objects) {
            obj.rotation = { x: 0.35, y: crateAngle, z: 0.0 };
            obj.render();
        }
    }

    Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);

    const prof = Screen.getProfile();
    ui.status = `${Screen.getFPS(360)} FPS`;
    const lines = (layout === SIDE || layout === SPHERES)
        ? [`CROSS  Layout: ${LAYOUT_NAMES[layout]}`, `${MODE_NAMES[0]}  |  ${MODE_NAMES[1]}  |  ${MODE_NAMES[2]}`]
        : [`CROSS  Layout: ${LAYOUT_NAMES[layout]}`, `SQUARE  Mode: ${MODE_NAMES[shownMode]}`];
    lines.push(`frame ${prof.frame.toFixed(1)} ms  |  render ${prof.render.toFixed(1)}  |  gpu ${prof.gpuWait.toFixed(1)}`);
    ui.hud(lines);
    ui.chrome();

    Screen.flip();

    if (BENCHMARK) benchTick();
}
