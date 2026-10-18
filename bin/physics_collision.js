// {"name": "Physics demo", "author": "Daniel Santos", "version": "04072023", "icon": "render_icon.png", "file": "physics_collision.js"}

const font = new Font("default");
font.scale = 0.55f;
font.outline = 1.0f;
font.outline_color = Color.new(0, 0, 0);

Screen.setFrameCounter(true);

const canvas = Screen.getMode();
canvas.psm = Screen.CT32;
canvas.zbuffering = true;
canvas.psmz = Screen.Z24;
Screen.setMode(canvas);

Screen.initBuffers();
const fbuf = Screen.getBuffer(Screen.DRAW_BUFFER);
const zbuf = Screen.getBuffer(Screen.DEPTH_BUFFER);

Render.init();
Render.setView(60.0, 2.0, 250.0);

// Change root folder to "render" for asset loading
os.chdir("render");

const sky = new Image("sky.png");
sky.width = canvas.width;
sky.height = canvas.height;

// Native particles: one shared atlas/pool for dust, wood chips and stone grit.
// Emit only on footsteps, landings and impacts; R3 changes the density budget.
const FX_LEVELS = [0, 128, 256, 512, 1024];
const fxAtlas = new Image("fx_atlas.png");
fxAtlas.filter = LINEAR;
const fxSystem = new Particles.System({ texture: fxAtlas, capacity: 1024, space: "world", seed: 42978 });
const fxDust = fxSystem.createEmitter({
    frame: [0, 0, 32, 32], lifetime: [0.45, 0.9], size: [0.14, 0.28], scaleEnd: 3,
    velocityMin: [-0.4, 0.15, -0.4], velocityMax: [0.4, 0.65, 0.4],
    acceleration: [0, -0.12, 0], drag: 1.8, spread: [0.08, 0.02, 0.08],
    color: [128, 118, 100, 45], colorEnd: [128, 118, 100, 0], fadeIn: 0.08
});
const fxWood = fxSystem.createEmitter({
    frame: [32, 0, 32, 32], lifetime: [0.35, 0.8], size: [0.025, 0.065], scaleEnd: 0.65,
    velocityMin: [-1.8, 1, -1.8], velocityMax: [1.8, 3, 1.8], acceleration: [0, -9, 0],
    color: [128, 90, 55, 128], colorEnd: [128, 90, 55, 0]
});
const fxStone = fxSystem.createEmitter({
    frame: [32, 0, 32, 32], lifetime: [0.3, 0.65], size: [0.02, 0.055], scaleEnd: 0.65,
    velocityMin: [-1.5, 0.6, -1.5], velocityMax: [1.5, 2, 1.5], acceleration: [0, -9, 0],
    color: [120, 118, 112, 128], colorEnd: [120, 118, 112, 0]
});
let fxLevel = 1;
let fxNextStep = 0;
function fxReset(level) {
    fxLevel = level;
    fxNextStep = 0;
    fxSystem.clear(42978);
}
// material: 0 = dust, 1 = wood fragments + dust, 2 = stone grit + dust.
function fxBurst(x, y, z, count, material = 0) {
    const budget = FX_LEVELS[fxLevel];
    count = Math.min(budget - fxSystem.activeCount, Math.ceil(count * budget / 128));
    if (count <= 0) return;
    const dust = material === 0 ? count : Math.ceil(count / 3);
    fxDust.burst(dust, x, y, z);
    if (count > dust) (material === 1 ? fxWood : fxStone).burst(count - dust, x, y, z);
}
function fxRender() {
    // Same fixed simulation step as the rigid-body world below.
    fxSystem.update(0.016);
    fxSystem.draw();
}

// --- Physics World & Broadphase Space ---
const world = Physics.World();
const jgroup = Physics.JointGroup();
const space = Physics.Space();

world.setGravity(0.0f, -18.0f, 0.0f);

// --- Static Environment (Crytek/Khronos Sponza Palace Atrium + Walkable North Terrace) ---
const scene = new RenderData("sponza_strip.gltf");
scene.face_culling = Render.CULL_FACE_BACK;
scene.accurate_clipping = true;
scene.pipeline = Render.PL_DEFAULT;
for (let i = 0; i < 8; i++) {
    const tex = scene.getTexture(i);
    if (tex) tex.filter = LINEAR;
}
scene.freeze();

const scene_object = new RenderObject(scene);

// --- Watertight Scene Collision Composed Purely of Native Primitives (Plane + Box) ---
// Every geom must stay referenced. The JS finalizer destroys the native geom,
// so a temporary that nobody holds is removed from the space on the next GC.
const staticGeoms = [];

function addStaticBox(x0, y0, z0, x1, y1, z1) {
    const g = Physics.GeomBox(space, x1 - x0, y1 - y0, z1 - z0);
    g.setPosition(0.5f * (x0 + x1), 0.5f * (y0 + y1), 0.5f * (z0 + z1));
    staticGeoms.push(g);
    return g;
}

// 1. Courtyard floor slab. Top face is Y = 0, matching the visual floor.
// A finite box (not an infinite plane) so the broadphase AABB stays in range.
addStaticBox(-8.53f, -0.80f, -19.10f, 8.53f, 0.0f, 19.10f);

// 2. Outer Sponza Palace Boundary Walls (X = ±8.53m, Z = ±19.10m)
addStaticBox(-9.53f, 0.0f, -20.0f, -8.53f, 12.0f,  20.0f); // West Outer Wall
addStaticBox( 8.53f, 0.0f, -20.0f,  9.53f, 12.0f,  20.0f); // East Outer Wall
addStaticBox(-9.53f, 0.0f, -20.1f,  9.53f, 12.0f, -19.1f); // North Outer Wall
addStaticBox(-9.53f, 0.0f,  19.1f,  9.53f, 12.0f,  20.1f); // South Outer Wall

// 3. 8 Ground-Floor Colonnade Columns + 4 Corner Piers (exact Khronos Sponza coordinates)
const colZ = [-7.775f, -2.534f, 2.610f, 7.775f];
for (let i = 0; i < colZ.length; i++) {
    const cz = colZ[i];
    addStaticBox(-3.735f, 0.0f, cz - 0.36f, -3.015f, 3.20f, cz + 0.36f); // West Column
    addStaticBox( 3.043f, 0.0f, cz - 0.36f,  3.765f, 3.20f, cz + 0.36f); // East Column
}
const pierZ = [-13.139f, 13.114f];
for (let i = 0; i < pierZ.length; i++) {
    const pz = pierZ[i];
    addStaticBox(-3.763f, 0.0f, pz - 0.57f, -2.618f, 3.20f, pz + 0.57f); // West Corner Pier
    addStaticBox( 2.619f, 0.0f, pz - 0.57f,  3.764f, 3.20f, pz + 0.57f); // East Corner Pier
}

// 4. 8 Courtyard Stone Planters
const planterPos = [
    [-2.62f, -12.58f], [ 2.68f, -12.58f],
    [-2.64f,  -2.52f], [ 2.63f,  -2.52f],
    [-2.65f,   2.58f], [ 2.63f,   2.58f],
    [-2.65f,  12.58f], [ 2.63f,  12.58f]
];
for (let i = 0; i < planterPos.length; i++) {
    const px = planterPos[i][0], pz = planterPos[i][1];
    addStaticBox(px - 0.42f, 0.0f, pz - 0.42f, px + 0.42f, 0.92f, pz + 0.42f);
}

// 5. North Terrace Multi-Level Stone Course (Steps + Platforms + Trophy Pedestal)
// Central Approach Staircase (5 stone steps from Z = -4.20 to -7.80, Y = 0.0 to 1.05m)
for (let s = 0; s < 5; s++) {
    const zFront = -4.20f - s * (3.60f / 5.0f);
    const yTop   = (s + 1) * (1.05f / 5.0f);
    addStaticBox(-1.60f, 0.0f, -7.80f, 1.60f, yTop, zFront);
}
// Tier-1 Lower North Terrace Platform (Y = 1.05m)
addStaticBox(-3.10f, 0.0f, -10.20f, 3.10f, 1.05f, -7.80f);

// Twin Side Staircases up to Tier-2 (4 stone steps from Z = -9.60 to -12.10, Y = 1.05 to 1.95m)
for (let s = 0; s < 4; s++) {
    const zFront = -9.60f - s * (2.50f / 4.0f);
    const yTop   = 1.05f + (s + 1) * (0.90f / 4.0f);
    addStaticBox(-2.95f, 0.0f, -12.10f, -1.65f, yTop, zFront);
    addStaticBox( 1.65f, 0.0f, -12.10f,  2.95f, yTop, zFront);
}
// Tier-2 Upper North Balcony (Y = 1.95m)
addStaticBox(-3.10f, 0.0f, -13.60f, 3.10f, 1.95f, -12.10f);

// Tier-3 Central Trophy Pedestal (Y = 2.55m)
addStaticBox(-1.15f, 1.95f, -13.55f, 1.15f, 2.55f, -12.25f);

// --- Player Character (3rd-Person Walking Simulator) ---
const skin_anims = new AnimCollection("Twerk.gltf");
const gltf_skin = new RenderData("Twerk.gltf");
gltf_skin.accurate_clipping = true;
gltf_skin.face_culling = Render.CULL_FACE_BACK;
// Specular (Blinn-Phong per vertex) on the dynamic props and the player: smooth,
// dense meshes where the highlights read well. The Sponza stays diffuse -- its
// huge low-poly triangles only turn per-vertex highlights into stray bright faces.
gltf_skin.pipeline = Render.PL_SPECULAR;
gltf_skin.freeze();

const skin_object = new RenderObject(gltf_skin);

const PLAYER_RADIUS = 0.34f;
const PLAYER_START = { x: 0.0f, y: 1.0f, z: 6.5f };

const player_geom = Physics.GeomSphere(space, PLAYER_RADIUS);
const player_body = Physics.Body(world);
player_body.setMassSphere(2.5f, PLAYER_RADIUS);
player_geom.setBody(player_body);
player_body.setPosition(PLAYER_START.x, PLAYER_START.y, PLAYER_START.z);

let playerYaw = Math.PI; // Facing -Z down the Sponza atrium toward the crate pyramid & North Terrace
let walkCycle = 0.0f;
let currentAnim = "idle";
const emotes = ["samba", "gangnam", "hiphop", "twerk", "capoeira"];
let emoteIdx = -1;
let emoteActive = false;
const RIGHT_FOOT_BONE = 22; // mixamorig:RightFoot in Twerk.gltf
const RIGHT_TOE_BONE  = 23; // mixamorig:RightToeBase in Twerk.gltf
let animTimer = 0;
let jumpWindup = 0;
let jumpCooldown = 0;
let groundGrace = 6;
let kickPhase = 0;
let kickHits = 0;
let kickCooldown = 0;
const kickHitBodies = [false, false, false, false, false, false, false, false, false, false, false, false, false, false];
let statusText = "SPONZA ATRIUM READY";
let statusTimer = 120;

skin_object.position = { x: PLAYER_START.x, y: PLAYER_START.y - PLAYER_RADIUS, z: PLAYER_START.z };
skin_object.rotation = { x: Math.PI / 2, y: 0.0f, z: playerYaw };
skin_object.playAnim(skin_anims["idle"] || skin_anims["twerk"], true);

// --- Dynamic Physics Crates (Courtyard Pyramid + North Terrace Balcony Crates) ---
const box = new RenderData("box_bump.gltf");
box.face_culling = Render.CULL_FACE_BACK;
box.pipeline = Render.PL_SPECULAR;
box.freeze();

const CRATE_HALF = 0.38f;
const CRATE_SIZE = CRATE_HALF * 2.0f;
const crateSpawns = [
    // Bottom row of courtyard pyramid
    { x: -0.82f, y: CRATE_HALF + 0.02f, z: 0.5f },
    { x:  0.00f, y: CRATE_HALF + 0.02f, z: 0.5f },
    { x:  0.82f, y: CRATE_HALF + 0.02f, z: 0.5f },
    // Middle row of courtyard pyramid
    { x: -0.41f, y: CRATE_HALF * 3.0f + 0.05f, z: 0.5f },
    { x:  0.41f, y: CRATE_HALF * 3.0f + 0.05f, z: 0.5f },
    // Top crate of courtyard pyramid
    { x:  0.00f, y: CRATE_HALF * 5.0f + 0.08f, z: 0.5f },
    // Tier-1 North Terrace Ledge Crates (Y = 1.05m, kickable off the balcony!)
    { x: -2.25f, y: 1.05f + CRATE_HALF + 0.03f, z: -8.5f },
    { x:  2.25f, y: 1.05f + CRATE_HALF + 0.03f, z: -8.5f },
    // Tier-2 Upper Terrace Pedestal Crates (Y = 1.95m & Y = 2.55m)
    { x: -2.10f, y: 1.95f + CRATE_HALF + 0.03f, z: -12.6f },
    { x:  2.10f, y: 1.95f + CRATE_HALF + 0.03f, z: -12.6f },
    { x:  0.00f, y: 2.55f + CRATE_HALF + 0.03f, z: -12.95f }
];

const crates = [];
for (let i = 0; i < crateSpawns.length; i++) {
    const sp = crateSpawns[i];
    const obj = new RenderObject(box);
    obj.scale = { x: CRATE_HALF, y: CRATE_HALF, z: CRATE_HALF };
    obj.position = { x: sp.x, y: sp.y, z: sp.z };

    const geom = Physics.GeomBox(space, CRATE_SIZE, CRATE_SIZE, CRATE_SIZE);
    const body = Physics.Body(world);
    body.setMassBox(0.8f, CRATE_SIZE, CRATE_SIZE, CRATE_SIZE);
    geom.setBody(body);
    body.setPosition(sp.x, sp.y, sp.z);
    obj.setPhysics(body);

    crates.push({ obj, geom, body, spawn: sp });
}

// --- Dynamic Rolling / Throwable Monkey Spheres ---
const monkeyTex = new Image("monkey.png");
const monkeyData = new RenderData("monkey_strip.gltf", monkeyTex);
monkeyData.face_culling = Render.CULL_FACE_BACK;
monkeyData.pipeline = Render.PL_SPECULAR;
monkeyData.freeze();

const BALL_RADIUS = 0.45f;
const ballSpawns = [
    { x: -1.8f, y: 1.5f, z: 3.5f },
    { x:  1.8f, y: 1.5f, z: 3.5f },
    { x:  0.0f, y: 2.2f, z: -9.0f }
];

const balls = [];
let nextBallIdx = 0;
for (let i = 0; i < ballSpawns.length; i++) {
    const sp = ballSpawns[i];
    const obj = new RenderObject(monkeyData);
    obj.scale = { x: BALL_RADIUS, y: BALL_RADIUS, z: BALL_RADIUS };
    obj.position = { x: sp.x, y: sp.y, z: sp.z };

    const geom = Physics.GeomSphere(space, BALL_RADIUS);
    const body = Physics.Body(world);
    body.setMassSphere(0.7f, BALL_RADIUS);
    geom.setBody(body);
    body.setPosition(sp.x, sp.y, sp.z);
    obj.setPhysics(body);

    balls.push({ obj, geom, body, spawn: sp });
}

const allProps = crates.concat(balls);
for (let i = 0; i < allProps.length; i++) {
    allProps[i].fxPreviousVy = 0;
    allProps[i].fxImpactCooldown = 0;
}

function fxPropImpacts() {
    for (let i = 0; i < allProps.length; i++) {
        const prop = allProps[i];
        if (fxLevel === 0) {
            prop.fxPreviousVy = 0;
            continue;
        }
        prop.fxImpactCooldown -= 0.016;
        const velocity = prop.body.getLinearVel();
        // A falling prop abruptly loses downward speed at a real collision.
        // Resting contacts, gravity and the upward throw impulse do not emit.
        if (prop.fxPreviousVy < -2.2 && velocity[1] - prop.fxPreviousVy > 1.8 && prop.fxImpactCooldown <= 0) {
            const position = prop.body.getPosition();
            const radius = i < crates.length ? CRATE_HALF : BALL_RADIUS;
            fxBurst(position[0], position[1] - radius + 0.025, position[2], 16, i < crates.length ? 1 : 2);
            prop.fxImpactCooldown = 0.2;
        }
        prop.fxPreviousVy = velocity[1];
    }
}

function resetSandbox() {
    const identityRot = [
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f
    ];

    player_body.setPosition(PLAYER_START.x, PLAYER_START.y, PLAYER_START.z);
    player_body.setLinearVel(0.0f, 0.0f, 0.0f);
    player_body.setAngularVel(0.0f, 0.0f, 0.0f);
    player_body.setRotation(identityRot);
    playerYaw = Math.PI;
    jumpWindup = 0;
    groundGrace = 6;
    kickPhase = 0;
    for (let i = 0; i < allProps.length; i++) {
        allProps[i].fxPreviousVy = 0;
        allProps[i].fxImpactCooldown = 0;
    }

    for (let i = 0; i < crates.length; i++) {
        const c = crates[i];
        c.body.setPosition(c.spawn.x, c.spawn.y, c.spawn.z);
        c.body.setLinearVel(0.0f, 0.0f, 0.0f);
        c.body.setAngularVel(0.0f, 0.0f, 0.0f);
        c.body.setRotation(identityRot);
    }

    for (let i = 0; i < balls.length; i++) {
        const b = balls[i];
        b.body.setPosition(b.spawn.x, b.spawn.y, b.spawn.z);
        b.body.setLinearVel(0.0f, 0.0f, 0.0f);
        b.body.setAngularVel(0.0f, 0.0f, 0.0f);
        b.body.setRotation(identityRot);
    }

    statusText = "SANDBOX RESET!";
    statusTimer = 90;
}

// --- Third-Person Follow Camera State ---
let camYaw = 0.0f;
let camPitch = 0.34f;
let camDist = 5.8f;
let camTargetX = PLAYER_START.x;
let camTargetY = PLAYER_START.y + 0.8f;
let camTargetZ = PLAYER_START.z;

// --- Architectural Atrium Lighting (Sunlight + Warm Colonnade Fill) ---
const light = Lights.new();
Lights.set(light, Lights.DIRECTION, 0.35f, 0.90f, 0.45f);
Lights.set(light, Lights.AMBIENT,   0.26f, 0.27f, 0.30f);
Lights.set(light, Lights.DIFFUSE,   0.78f, 0.74f, 0.66f);
Lights.set(light, Lights.SPECULAR,  1.00f, 1.00f, 1.00f); // white sun highlight

const fillLight = Lights.new();
Lights.set(fillLight, Lights.DIRECTION, -0.45f, 0.55f, -0.70f);
Lights.set(fillLight, Lights.AMBIENT,    0.05f, 0.04f, 0.03f);
Lights.set(fillLight, Lights.DIFFUSE,    0.34f, 0.27f, 0.20f);
Lights.set(fillLight, Lights.SPECULAR,   0.70f, 0.50f, 0.30f); // warm second highlight

const pad = Pads.get();
const gray = Color.new(180, 180, 220, 128);

// --- Hardware Channel Shuffle, Depth Fog & DOF Presets (SELECT to cycle) ---
const DOF_SHIFT = 3;

const FOG_PRESETS = [
    {
        name: "MIST FOG (LINEAR)",
        type: "fog",
        color: Color.new(182, 198, 225, 112),
        start: 8.5f,
        end: 36.0f,
        opts: { curve: Screen.FOG_LINEAR, skyFog: 0.35f, shift: 4 }
    },
    {
        name: "DEPTH OF FIELD (DOF)",
        type: "dof",
        strength: 124
    },
    {
        name: "DOF + MIST FOG",
        type: "dof_fog",
        strength: 118,
        color: Color.new(182, 198, 225, 108),
        start: 9.0f,
        end: 36.0f,
        opts: { curve: Screen.FOG_LINEAR, skyFog: 0.35f, shift: DOF_SHIFT }
    },
    {
        name: "SILENT HILL (EXP2)",
        type: "fog",
        color: Color.new(195, 200, 208, 124),
        start: 5.5f,
        end: 24.0f,
        opts: { curve: Screen.FOG_EXP2, density: 1.85f, skyFog: 0.85f, shift: 4 }
    },
    {
        name: "GOLDEN DUSK (EXP)",
        type: "fog",
        color: Color.new(235, 165, 105, 110),
        start: 7.5f,
        end: 32.0f,
        opts: { curve: Screen.FOG_EXP, density: 1.6f, skyFog: 0.45f, shift: 4 }
    },
    {
        name: "Z-BUFFER SHUFFLE LUT",
        type: "lut"
    },
    {
        name: "NORMALIZED Z (SHIFT 4)",
        type: "shift",
        shift: 4
    },
    {
        name: "POSTFX OFF",
        type: "off"
    }
];
let fogPresetIdx = 0;

// --- Automatic benchmark ---
// Cycles through lighting on/off x post-fx on/off, BENCH_PHASE_MS each, and
// logs the averaged Screen.getProfile() of every phase to the console (the
// first BENCH_WARMUP_MS and two 30-frame profiler windows of a phase are skipped
// so a report spanning the phase switch cannot leak into the numbers). Set BENCHMARK to
// false to play normally: lighting and post-fx then stay as configured above.
const BENCHMARK = false;
const BENCH_PHASE_MS = 10000;
const BENCH_WARMUP_MS = 2000;
const BENCH_PHASES = [
    { name: "lights ON  | postfx ON ", lights: true,  postfx: true  },
    { name: "lights ON  | postfx OFF", lights: true,  postfx: false },
    { name: "lights OFF | postfx ON ", lights: false, postfx: true  },
    { name: "lights OFF | postfx OFF", lights: false, postfx: false },
];
const BENCH_KEYS = ["frame", "script", "render", "anim", "physics", "gpuWait", "vsync"];
let benchPhase = 0;
let benchPostfx = true;
let benchSum = null;
let benchFastClip = 0;
let benchSamples = 0;
let benchFrames = 0;
const benchTimer = Timer.new();
function benchApply() {
    const phase = BENCH_PHASES[benchPhase];
    scene.pipeline = phase.lights ? Render.PL_DEFAULT : Render.PL_NO_LIGHTS;
    benchPostfx = phase.postfx;
    benchSum = {};
    for (const k of BENCH_KEYS) benchSum[k] = 0;
    benchFastClip = benchSamples = benchFrames = 0;
    Timer.reset(benchTimer);
}
function benchTick() {
    const elapsed = Timer.getTime(benchTimer) / 1000;
    benchFrames++;
    if (elapsed >= BENCH_WARMUP_MS && benchFrames > 60) {
        const prof = Screen.getProfile();
        for (const k of BENCH_KEYS) benchSum[k] += prof[k];
        benchFastClip += Render.stats().fastClip;
        benchSamples++;
    }
    if (elapsed < BENCH_PHASE_MS || benchSamples === 0) return;
    const avg = {};
    for (const k of BENCH_KEYS) avg[k] = benchSum[k] / benchSamples;
    console.log(
        `[bench] ${BENCH_PHASES[benchPhase].name} | ${(1000 / avg.frame).toFixed(1)} fps | ` +
        `frame ${avg.frame.toFixed(1)} | script ${avg.script.toFixed(1)} | render ${avg.render.toFixed(1)} | ` +
        `anim ${avg.anim.toFixed(1)} | phys ${avg.physics.toFixed(1)} | gpu wait ${avg.gpuWait.toFixed(1)} | ` +
        `vsync ${avg.vsync.toFixed(1)} (ms) | fast clip groups ${(benchFastClip / benchSamples).toFixed(1)}`);
    benchPhase = (benchPhase + 1) % BENCH_PHASES.length;
    benchApply();
}

if (BENCHMARK) {
    console.log("[bench] automatic benchmark on: 4 phases, " + (BENCH_PHASE_MS / 1000) + " s each");
    benchApply();
}

// Custom 256-entry CSM1 LUT for Depth of Field (DOF) Alpha Mask:
// Reconstructs eye-space distance d (in meters) from the shifted Z24 MSB byte b.
// Keeps foreground & player (~2m..9.5m) 100% sharp (alpha = 0) and smoothly ramps
// out-of-focus background (9.5m..26m+) up to alpha = 128 (0x80).
const dofMaskLUT = Screen.createShuffleLUT((b) => {
    if (b === 0) return Color.new(0, 0, 0, 128); // Sky / far horizon: full blur

    const byteScale = 65536.0f / (1 << DOF_SHIFT);
    const screenScaleZ = 4194304.0f;
    const nearZ = 2.0f;
    const farZ = 250.0f;

    const zGs = Math.min(screenScaleZ * 2.0f, (b + 0.5f) * byteScale);
    const zNdc = (zGs / screenScaleZ) - 1.0f;
    const denom = (farZ + nearZ) + zNdc * (farZ - nearZ);
    const d = denom > 0.0001f ? (2.0f * farZ * nearZ) / denom : farZ;

    const focusEnd = 9.5f;
    const blurFull = 26.0f;
    if (d <= focusEnd) return Color.new(0, 0, 0, 0);

    const t = Math.min(1.0f, (d - focusEnd) / (blurFull - focusEnd));
    const smooth = t * t * (3.0f - 2.0f * t);
    const alpha = Math.min(128, Math.floor(smooth * 128.0f + 0.5f));
    return Color.new(0, 0, 0, alpha);
});

// Draws `img` over [0, dstW) x [0, dstH) as vertical strips of `stripW` pixels
// instead of one screen-sized sprite, so the GS stays within one texture page
// and one frame page column at a time. The texel mapping is the same
// continuous [u0, u1] x [v0, v1] a single sprite would use, evaluated at each
// strip edge, so there are no seams.
function drawInStrips(img, dstW, dstH, u0, v0, u1, v1, stripW) {
    const du = (u1 - u0) / dstW;
    img.starty = v0;
    img.endy = v1;
    img.height = dstH;
    for (let x = 0; x < dstW; x += stripW) {
        const x1 = Math.min(x + stripW, dstW);
        img.startx = u0 + x * du;
        img.endx = u0 + x1 * du;
        img.width = x1 - x;
        img.draw(x, 0);
    }
    img.startx = u0;
    img.endx = u1;
}

function renderDepthOfField(strength = 124, alreadyShifted = false) {
    const w = canvas.width;
    const h = canvas.height;
    const halfW = w >> 1;
    const halfH = h >> 1;

    // 1. Shift Z24 up by DOF_SHIFT bits to expand depth precision around the focal plane
    if (!alreadyShifted) {
        Screen.depthBitShift(DOF_SHIFT);
    }

    // 2. Channel Shuffle: map Z-buffer Blue MSB -> Framebuffer Alpha ONLY (RGB untouched)
    Screen.channelShuffle(Screen.DEPTH_BUFFER, dofMaskLUT, Screen.CHANNEL_BLUE, {
        outMask: Screen.CHANNEL_ALPHA,
        alphaBlend: false,
        alphaTest: false,
        unswizzle: true
    });

    // 3. Downsample Framebuffer (RGB + DOF Alpha) 2x2 -> Z-buffer scratch space (half-res)
    // Mask Z-writes (ZMSK = 1) so drawing into zbuf doesn't write Z=0 over the blurred RGB+A!
    Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);
    Screen.setParam(Screen.ALPHA_TEST_ENABLE, false);
    Screen.setBuffer(Screen.DEPTH_BUFFER, zbuf, 1);

    zbuf.bpp = 32; // Treat Z-buffer memory as 32-bit RGBA surface (GS_PSM_CT32)
    Screen.setBuffer(Screen.DRAW_BUFFER, zbuf, 0);

    Screen.setParam(
        Screen.ALPHA_BLEND_EQUATION,
        Screen.alphaEquation(Screen.SRC_RGB, Screen.ZERO_RGB, Screen.ALPHA_FIX, Screen.ZERO_RGB, 0x80)
    );

    fbuf.filter = LINEAR;
    fbuf.color = Color.new(128, 128, 128, 128);
    // 32-pixel strips: each reads one 64-texel source page column (2:1).
    drawInStrips(fbuf, halfW, halfH, 0.5f, 0.5f, w - 0.5f, h - 0.5f, 32);

    // 4. Upsample half-res Z-buffer scratch -> Framebuffer RGB weighted by blurred Alpha mask
    Screen.setBuffer(Screen.DRAW_BUFFER, fbuf, 0xFF000000); // Mask Alpha, write RGB only
    Screen.setParam(
        Screen.ALPHA_BLEND_EQUATION,
        Screen.alphaEquation(Screen.SRC_RGB, Screen.DST_RGB, Screen.SRC_ALPHA, Screen.DST_RGB, 0)
    );

    zbuf.filter = LINEAR;
    zbuf.color = Color.new(128, 128, 128, strength);
    // 64-pixel strips: one frame page column each (reading 32 source texels).
    drawInStrips(zbuf, w, h, 0.5f, 0.5f, halfW - 0.5f, halfH - 0.5f, 64);

    // 5. Restore Z-buffer format (GS_PSMZ_24), Z-writes, Draw buffer mask, and default blend
    fbuf.width = w;
    fbuf.height = h;
    zbuf.width = w;
    zbuf.height = h;
    zbuf.bpp = 26;
    Screen.setBuffer(Screen.DEPTH_BUFFER, zbuf, 0);
    Screen.setBuffer(Screen.DRAW_BUFFER, fbuf, 0);
    Screen.setParam(Screen.ALPHA_BLEND_EQUATION, Screen.BLEND_DEFAULT);
}

// Custom 256-entry CSM1 LUT for raw Depth Channel Shuffle visualization
const depthHeatmapLUT = Screen.createShuffleLUT((b) => {
    if (b === 0) return Color.new(15, 18, 35, 115);
    // In Z24, MSB byte b is in [1..128] (with far_z=48 and near_z=2, courtyard spans b=4..128)
    const norm = Math.min(1.0f, b / 64.0f);
    const r = Math.min(255, Math.floor(norm * 510.0f));
    const g = Math.min(255, Math.max(0, Math.floor((norm - 0.25f) * 360.0f)));
    const bl = Math.min(255, Math.max(40, Math.floor((1.0f - norm) * 255.0f)));
    return Color.new(r, g, bl, 118);
});

const depthNormalizedLUT = Screen.createShuffleLUT((b) => {
    if (b === 0) return Color.new(15, 18, 35, 115);
    const norm = Math.min(1.0f, b / 160.0f);
    const r = Math.min(255, Math.floor(norm * 510.0f));
    const g = Math.min(255, Math.max(0, Math.floor((norm - 0.25f) * 360.0f)));
    const bl = Math.min(255, Math.max(40, Math.floor((1.0f - norm) * 255.0f)));
    return Color.new(r, g, bl, 118);
});

Screen.setParam(Screen.ALPHA_TEST_ENABLE, false);
Screen.setParam(Screen.ALPHA_TEST_METHOD, Screen.ALPHA_LESS);
Screen.setParam(Screen.ALPHA_TEST_REF, 0x80);
Screen.setParam(Screen.DEPTH_TEST_ENABLE, true);
Screen.setParam(Screen.DEPTH_TEST_METHOD, Screen.DEPTH_GEQUAL);

while (true) {
    Screen.clear(gray);
    pad.update();

    if (pad.justPressed(Pads.TRIANGLE)) {
        fxSystem.dispose();
        os.chdir("..");
        std.reload("main.js");
    }

    if (!BENCHMARK && pad.justPressed(Pads.R3)) fxReset((fxLevel + 1) % FX_LEVELS.length);

    if (pad.justPressed(Pads.START)) {
        resetSandbox();
        fxReset(fxLevel);
    }

    if (pad.justPressed(Pads.SELECT)) {
        fogPresetIdx = (fogPresetIdx + 1) % FOG_PRESETS.length;
        statusText = `POST-FX: ${FOG_PRESETS[fogPresetIdx].name}`;
        statusTimer = 110;
    }

    // --- 1. Camera Orbit & Zoom Input (Right Stick + L2/R2) ---
    const rx = ((pad.rx > 25 || pad.rx < -25) ? pad.rx : 0) / 1024.0f;
    const ry = ((pad.ry > 25 || pad.ry < -25) ? pad.ry : 0) / 1024.0f;

    camYaw -= rx * 0.18f;
    camPitch += ry * 0.14f;
    if (camPitch < 0.08f) camPitch = 0.08f;
    if (camPitch > 1.15f) camPitch = 1.15f;

    if (pad.pressed(Pads.R2)) {
        camDist -= 0.12f;
        if (camDist < 2.8f) camDist = 2.8f;
    } else if (pad.pressed(Pads.L2)) {
        camDist += 0.12f;
        if (camDist > 13.0f) camDist = 13.0f;
    }

    // --- 2. Player Movement Input (Left Stick or D-Pad) ---
    let stickX = ((pad.lx > 25 || pad.lx < -25) ? pad.lx : 0) / 128.0f;
    let stickY = ((pad.ly > 25 || pad.ly < -25) ? pad.ly : 0) / 128.0f;

    if (pad.pressed(Pads.LEFT))  stickX = -1.0f;
    if (pad.pressed(Pads.RIGHT)) stickX =  1.0f;
    if (pad.pressed(Pads.UP))    stickY = -1.0f;
    if (pad.pressed(Pads.DOWN))  stickY =  1.0f;

    let inputMag = Math.sqrt(stickX * stickX + stickY * stickY);
    if (inputMag > 1.0f) {
        stickX /= inputMag;
        stickY /= inputMag;
        inputMag = 1.0f;
    }

    const pPos = player_body.getPosition();
    const pVel = player_body.getLinearVel();
    let px = pPos[0], py = pPos[1], pz = pPos[2];
    let vx = pVel[0], vy = pVel[1], vz = pVel[2];

    // Respawn safety if player ever falls out of bounds
    if (py < -8.0f) {
        resetSandbox();
        px = PLAYER_START.x; py = PLAYER_START.y; pz = PLAYER_START.z;
        vx = 0.0f; vy = 0.0f; vz = 0.0f;
    }

    if (jumpCooldown > 0) jumpCooldown--;
    if (kickCooldown > 0) kickCooldown--;
    if (statusTimer > 0) statusTimer--;
    if (animTimer > 0) animTimer--;

    const onFloor = (groundGrace > 0) || (py <= PLAYER_RADIUS + 0.12f && Math.abs(vy) < 1.8f);

    // Check if the character is standing / balancing on top of a crate or ball
    let onProp = false;
    if (Math.abs(vy) < 2.2f && jumpCooldown < 30) {
        for (let i = 0; i < allProps.length; i++) {
            const bp = allProps[i].body.getPosition();
            const dx = px - bp[0];
            const dz = pz - bp[2];
            const dy = (py - PLAYER_RADIUS) - bp[1];
            if ((dx * dx + dz * dz) < 0.72f * 0.72f && dy > 0.15f && dy < 0.75f) {
                onProp = true;
                break;
            }
        }
    }

    const isGrounded = onFloor || onProp;
    const isBalancing = onProp;

    const isSprinting = pad.pressed(Pads.R1);
    const moveSpeed = isSprinting ? 5.2f : 2.2f;

    // Camera-relative horizontal basis vectors
    const fwdX = -Math.sin(camYaw);
    const fwdZ = -Math.cos(camYaw);
    const rightX = Math.cos(camYaw);
    const rightZ = -Math.sin(camYaw);

    const moveX = stickX * rightX + (-stickY) * fwdX;
    const moveZ = stickX * rightZ + (-stickY) * fwdZ;

    if (inputMag > 0.08f) {
        const targetVx = moveX * moveSpeed;
        const targetVz = moveZ * moveSpeed;
        const accel = isGrounded ? 0.35f : 0.12f;
        vx += (targetVx - vx) * accel;
        vz += (targetVz - vz) * accel;

        // Smoothly rotate character toward movement direction
        const targetYaw = Math.atan2(-moveX, moveZ);
        let diff = targetYaw - playerYaw;
        while (diff >  Math.PI) diff -= 2.0f * Math.PI;
        while (diff < -Math.PI) diff += 2.0f * Math.PI;
        playerYaw += diff * 0.22f;

        // Gentle camera auto-align when not manually orbiting
        if (rx === 0.0f && ry === 0.0f) {
            camYaw -= stickX * 0.012f;
        }

        walkCycle += (isSprinting ? 0.32f : 0.20f) * inputMag;
        emoteActive = false;
    } else if (isGrounded) {
        vx *= 0.72f;
        vz *= 0.72f;
    }

    // --- 3. Player Actions: Jump (CROSS), Kick (SQUARE), Throw (CIRCLE), Emote (L1) ---
    if (pad.justPressed(Pads.CROSS) && isGrounded && jumpCooldown === 0 && jumpWindup === 0) {
        jumpWindup = 26; // ~0.43s squat/wind-up before leaving the ground
        jumpCooldown = 50;
        emoteActive = false;
        const jumpClip = skin_anims["jump"] ? "jump" : "capoeira";
        skin_object.playAnim(skin_anims[jumpClip], false);
        currentAnim = jumpClip;
        animTimer = 68;
        statusText = "CHARGING JUMP...";
        statusTimer = 45;
    }

    if (jumpWindup > 0) {
        jumpWindup--;
        if (jumpWindup === 0) {
            vy = 7.4f;
            groundGrace = 0;
            statusText = "JUMP!";
            statusTimer = 45;
            fxBurst(px, py - PLAYER_RADIUS + 0.025, pz, 8);
        }
    }

    // SQUARE: Standing Melee Kick with wind-up & live Right Foot bone collision
    if (pad.justPressed(Pads.SQUARE) && kickCooldown === 0 && kickPhase === 0) {
        kickCooldown = 58;
        kickPhase = 1;
        kickHits = 0;
        for (let i = 0; i < kickHitBodies.length; i++) kickHitBodies[i] = false;
        emoteActive = false;
        const kickClip = skin_anims["kick"] ? "kick" : "capoeira";
        skin_object.playAnim(skin_anims[kickClip], false);
        currentAnim = kickClip;
        animTimer = 64;
        statusText = "WINDING UP KICK...";
        statusTimer = 45;
    }

    if (kickPhase > 0) {
        kickPhase++;

        // Damp horizontal movement during kick wind-up & strike so foot plants cleanly
        if (isGrounded && kickPhase < 48) {
            vx *= 0.65f;
            vz *= 0.65f;
        }

        // Active foot strike window (frames 26..45 of the Mixamo kick animation)
        if (kickPhase >= 26 && kickPhase <= 45) {
            const cz = Math.cos(playerYaw);
            const sz = Math.sin(playerYaw);
            const faceX = -sz;
            const faceZ =  cz;

            // Read live Mixamo RightToeBase (bone 23) & RightFoot (bone 22) transforms
            let sx = -0.06f, sy = 0.98f, szBone = -1.15f;
            if (skin_object.bones && skin_object.bones[RIGHT_TOE_BONE]) {
                const mToe = skin_object.bones[RIGHT_TOE_BONE].transform.toArray();
                sx = mToe[3];
                sy = mToe[7];
                szBone = mToe[11];
                if (skin_object.bones[RIGHT_FOOT_BONE]) {
                    const mFoot = skin_object.bones[RIGHT_FOOT_BONE].transform.toArray();
                    sx = 0.6f * sx + 0.4f * mFoot[3];
                    sy = 0.6f * sy + 0.4f * mFoot[7];
                    szBone = 0.6f * szBone + 0.4f * mFoot[11];
                }
            }

            // Transform skeleton-space foot coordinates into world space
            const footX = px + (sx * cz - sy * sz);
            const footY = (py - PLAYER_RADIUS) - szBone;
            const footZ = pz + (sx * sz + sy * cz);

            for (let i = 0; i < allProps.length; i++) {
                if (kickHitBodies[i]) continue;

                const b = allProps[i].body;
                const bp = b.getPosition();
                const dx = bp[0] - footX;
                const dy = bp[1] - footY;
                const dz = bp[2] - footZ;
                const horizDistSq = dx * dx + dz * dz;

                // Match horizontal foot proximity and leg sweep vertical band
                if (horizDistSq < 1.15f * 1.15f && Math.abs(dy) < 1.25f) {
                    const horizDist = Math.sqrt(horizDistSq) + 0.001f;
                    // Blend kick direction between character's forward kick arc and foot-to-object contact normal
                    let dirX = faceX * 0.65f + (dx / horizDist) * 0.35f;
                    let dirZ = faceZ * 0.65f + (dz / horizDist) * 0.35f;
                    const dirLen = Math.sqrt(dirX * dirX + dirZ * dirZ) + 0.001f;
                    dirX /= dirLen;
                    dirZ /= dirLen;

                    const strength = 13.5f;
                    const bVel = b.getLinearVel();
                    b.setLinearVel(
                        bVel[0] + dirX * strength,
                        bVel[1] + 5.2f,
                        bVel[2] + dirZ * strength
                    );
                    b.setAngularVel(
                        dirZ * 10.0f,
                        4.0f,
                        -dirX * 10.0f
                    );

                    kickHitBodies[i] = true;
                    kickHits++;
                    fxBurst(footX, footY, footZ, 20, i < crates.length ? 1 : 2);
                    statusText = `KICK IMPACT! (${kickHits} HIT)`;
                    statusTimer = 60;
                }
            }
        }

        if (kickPhase >= 52) {
            if (kickHits === 0) {
                statusText = "KICK!";
                statusTimer = 30;
            }
            kickPhase = 0;
        }
    }

    // CIRCLE: Throw / Bowl a Monkey Sphere forward
    if (pad.justPressed(Pads.CIRCLE)) {
        const b = balls[nextBallIdx];
        nextBallIdx = (nextBallIdx + 1) % balls.length;

        const faceX = -Math.sin(playerYaw);
        const faceZ =  Math.cos(playerYaw);
        const spawnX = px + faceX * 0.95f;
        const spawnY = (py - PLAYER_RADIUS) + 1.05f;
        const spawnZ = pz + faceZ * 0.95f;

        b.body.setPosition(spawnX, spawnY, spawnZ);
        b.body.setLinearVel(faceX * 15.5f + vx * 0.4f, 2.8f, faceZ * 15.5f + vz * 0.4f);
        b.body.setAngularVel(faceZ * 12.0f, 4.0f, -faceX * 12.0f);

        statusText = "MONKEY BALL LAUNCHED!";
        statusTimer = 60;
    }

    // L1: Cycle Mixamo Dance Emotes
    if (pad.justPressed(Pads.L1)) {
        emoteIdx = (emoteIdx + 1) % emotes.length;
        const chosenEmote = emotes[emoteIdx];
        emoteActive = true;
        animTimer = 0;
        skin_object.playAnim(skin_anims[chosenEmote], true);
        currentAnim = chosenEmote;
        statusText = `DANCE: ${chosenEmote.toUpperCase()}`;
        statusTimer = 60;
    }

    // Keep animation looping smoothly based on movement state
    if (animTimer === 0) {
        let desiredAnim = "idle";
        if (!isGrounded) {
            desiredAnim = skin_anims["fall"] ? "fall" : "idle";
        } else if (isBalancing && inputMag < 0.35f && skin_anims["balance"]) {
            desiredAnim = "balance";
        } else if (inputMag > 0.08f) {
            desiredAnim = isSprinting ? "run" : "walk";
        } else if (emoteActive) {
            desiredAnim = emotes[emoteIdx];
        }
        if (!skin_anims[desiredAnim]) {
            desiredAnim = "idle";
        }
        if (currentAnim !== desiredAnim || !skin_object.isPlayingAnim()) {
            skin_object.playAnim(skin_anims[desiredAnim], true);
            currentAnim = desiredAnim;
        }
    }

    // Apply player velocity & lock player body rotation
    const preStepVy = vy;
    player_body.setLinearVel(vx, vy, vz);
    player_body.setAngularVel(0.0f, 0.0f, 0.0f);

    // --- 4. Step Physics World & Resolve Collisions ---
    const contacts = world.stepWithContacts(space, jgroup, 0.016f);

    // Multi-level ground & ramp contact detection:
    // Free-fall changes vy by -18.0 * 0.016 = -0.288 per step. If normal forces from
    // the floor, terrace, or ramps counteract gravity, refresh groundGrace.
    const postVel = player_body.getLinearVel();
    const postVy = postVel[1];
    if (jumpCooldown < 38 && (postVy - preStepVy) > -0.18f && postVy > -2.4f && postVy < 2.8f) {
        if (!isGrounded && preStepVy < -2) fxBurst(px, py - PLAYER_RADIUS + 0.025, pz, 14);
        groundGrace = 6;
    } else if (groundGrace > 0) {
        groundGrace--;
    }

    // Sync player visual mesh
    const newPos = player_body.getPosition();
    px = newPos[0]; py = newPos[1]; pz = newPos[2];
    if (isGrounded && inputMag > 0.08 && walkCycle >= fxNextStep) {
        const side = Math.floor(walkCycle / Math.PI) % 2 === 0 ? -1 : 1;
        fxBurst(px + Math.cos(playerYaw) * 0.18 * side, py - PLAYER_RADIUS + 0.025,
            pz + Math.sin(playerYaw) * 0.18 * side, isSprinting ? 5 : 3);
        fxNextStep = walkCycle + Math.PI;
    }
    fxPropImpacts();

    skin_object.position = { x: px, y: py - PLAYER_RADIUS, z: pz };
    skin_object.rotation = { x: Math.PI / 2, y: 0.0f, z: playerYaw };

    // --- 5. Update 3rd-Person Follow Camera ---
    camTargetX += (px - camTargetX) * 0.18f;
    camTargetY += ((py - PLAYER_RADIUS + 1.15f) - camTargetY) * 0.14f;
    camTargetZ += (pz - camTargetZ) * 0.18f;

    const horizDist = camDist * Math.cos(camPitch);
    const camX = camTargetX + Math.sin(camYaw) * horizDist;
    const camY = Math.max(0.55f, camTargetY + Math.sin(camPitch) * camDist);
    const camZ = camTargetZ + Math.cos(camYaw) * horizDist;

    Camera.target(camTargetX, camTargetY, camTargetZ);
    Camera.position(camX, camY, camZ);
    Camera.update();

    // --- 6. Render Pass ---
    Render.begin();

    Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);
    sky.draw(0, 0);

    Screen.setParam(Screen.DEPTH_TEST_ENABLE, true);
    Screen.setParam(Screen.DEPTH_TEST_METHOD, Screen.DEPTH_GEQUAL);

    scene_object.render();

    for (let i = 0; i < crates.length; i++) {
        crates[i].obj.render();
    }

    for (let i = 0; i < balls.length; i++) {
        balls[i].obj.render();
    }

    skin_object.render();
    // World-space billboards use scene depth, then receive the same fog/DOF.
    fxRender();

    // --- 6.5. Hardware Depth Fog / DOF / Channel Shuffle Post-Pass ---
    const activeFog = FOG_PRESETS[fogPresetIdx];
    if (!benchPostfx) {
        // benchmark phase with post-fx off
    } else if (activeFog.type === "fog") {
        Screen.depthFog(activeFog.color, activeFog.start, activeFog.end, activeFog.opts);
    } else if (activeFog.type === "dof") {
        renderDepthOfField(activeFog.strength, false);
    } else if (activeFog.type === "dof_fog") {
        Screen.depthFog(activeFog.color, activeFog.start, activeFog.end, activeFog.opts);
        renderDepthOfField(activeFog.strength, true);
    } else if (activeFog.type === "lut") {
        Screen.channelShuffle(Screen.DEPTH_BUFFER, depthHeatmapLUT, Screen.CHANNEL_BLUE, {
            outMask: Screen.CHANNEL_RGB,
            alphaBlend: true,
            alphaTest: true
        });
    } else if (activeFog.type === "shift") {
        Screen.depthBitShift(activeFog.shift);
        Screen.channelShuffle(Screen.DEPTH_BUFFER, depthNormalizedLUT, Screen.CHANNEL_BLUE, {
            outMask: Screen.CHANNEL_RGB,
            alphaBlend: true,
            alphaTest: true
        });
    }

    // --- 7. HUD Overlay ---
    Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);

    const stateLabel = !isGrounded
        ? (currentAnim === "jump" ? "JUMPING" : "FALLING")
        : (kickPhase > 0
            ? "KICKING"
            : (isBalancing && inputMag < 0.35f
                ? "BALANCING"
                : (inputMag > 0.08f
                    ? (isSprinting ? "SPRINTING" : "WALKING")
                    : (emoteActive ? "DANCING" : "IDLE"))));
    const renderStats = Render.stats();
    font.print(10, 10, `${Screen.getFPS(360)} FPS | ${stateLabel} (${currentAnim.toUpperCase()}) | Fog: ${activeFog.name} | Fast clip: ${renderStats.fastClip}`);
    font.print(10, 26, "L-Stick/DPad: Walk (+R1 Run) | R-Stick: Camera | X: Jump");
    font.print(10, 42, "SQUARE: Kick | CIRCLE: Throw | L1: Dance | SELECT: Fog | START: Reset");

    const prof = Screen.getProfile();
    font.print(10, 400, `frame ${prof.frame.toFixed(1)} | script ${prof.script.toFixed(1)} | render ${prof.render.toFixed(1)} | anim ${prof.anim.toFixed(1)} | phys ${prof.physics.toFixed(1)}`);
    font.print(10, 416, `gpu wait ${prof.gpuWait.toFixed(1)} | vsync ${prof.vsync.toFixed(1)} (ms)`);
    const fxStats = fxSystem.stats();
    font.print(10, 78, `Native particles: ${fxStats.active} active | ${fxStats.visible} visible | ${fxStats.batches} VU batches`);
    font.print(10, 94, `R3: FX density OFF/128/256/512/1024 | current ${FX_LEVELS[fxLevel]}`);

    if (statusTimer > 0) {
        font.print(10, 62, `>> ${statusText}`);
    }

    if (BENCHMARK) benchTick();
    Screen.flip();
}
