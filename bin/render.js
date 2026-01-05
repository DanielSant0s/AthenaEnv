// {"name": "Render demo", "author": "Daniel Santos", "version": "04072023", "icon": "render_icon.png", "file": "render.js"}
import { Batch } from 'RenderBatch';
import { SceneNode } from 'RenderSceneNode';
import { AsyncLoader } from 'RenderAsyncLoader';

const pipelines = [
    "NO_LIGHTS",
    "DEFAULT",
    "SPECULAR"
];

const font = new Font("default");
font.scale = 0.6f;

font.outline = 1.0f;
font.outline_color = Color.new(0, 0, 0);

Screen.setFrameCounter(true);
Screen.setVSync(true);

const canvas = Screen.getMode();

canvas.zbuffering = true;
canvas.psm = Screen.CT32;
canvas.psmz = Screen.Z24;

Screen.setMode(canvas);

Screen.initBuffers();

const draw_buffer = Screen.getBuffer(Screen.DRAW_BUFFER);
const depth_buffer = Screen.getBuffer(Screen.DEPTH_BUFFER);

const decal_buffer = new Image();

decal_buffer.filter = LINEAR;

decal_buffer.bpp = 32;
decal_buffer.texWidth = 128;
decal_buffer.texHeight = 128;
decal_buffer.renderable = true;

decal_buffer.width = 128;
decal_buffer.height = 128;
decal_buffer.endx = 128;
decal_buffer.endy = 128;

decal_buffer.lock();

Screen.switchContext();

Screen.setBuffer(Screen.DRAW_BUFFER, decal_buffer);

Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);

Screen.switchContext();

Screen.clear();
font.print(0, 0, "Loading assets...");
Screen.flip();

Render.init();

Render.setView(60.0, 5.0, 4000.0);

// Change your root folder to "render" so we can work with file path magic :p
os.chdir("render");

// x, y, z, adc=1.0f
const triPositions = new Float32Array([
    0.5f, 0.5f, 0.5f, 1.0f,
    0.5f, 0.8f, 0.5f, 1.0f,
    0.8f, 0.8f, 0.5f, 1.0f,
]);

// n1, n2, n3, adc=1.0f
const triNormals = new Float32Array([
    1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f,
]);

// s, t, q, adc=1.0f
const triTexCoords = new Float32Array([
    0.0f, 0.0f, 1.0f, 1.0f,
    1.0f, 0.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f,
]);

// r, g, b, a
const triColors = new Float32Array([
    1.0f, 0.0f, 0.0f, 1.0f,
    0.0f, 1.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 1.0f, 1.0f,
]);

const vertList = Render.vertexList(
    triPositions,
    triNormals,
    triTexCoords,
    triColors,
    undefined,
    undefined,
    { shareBuffers: true }
);

const listtest = new RenderData(vertList);
listtest.face_culling = Render.CULL_FACE_NONE;

const skytex = new Image("env.png");
skytex.filter = LINEAR;

const trilist_materials = listtest.materials;

trilist_materials[0].diffuse.r = 0.0;
trilist_materials[0].diffuse.g = 0.0;
trilist_materials[0].diffuse.b = 0.0;
//trilist_materials[0].diffuse.a = 0.0;

listtest.materials = trilist_materials;
listtest.updateMaterial(0, { shininess: 64.0f, diffuse: { r: 0.7f, g: 0.2f, b: 0.2f } });

const gltf_box = new RenderData("box_bump.gltf");
//gltf_box.texture_mapping = false;

const dragontex = new Image("dragon.png");
const dragonmesh = new RenderData("dragon.obj", dragontex);

const blood_tex = new Image("blood.png");
blood_tex.width = 40;
blood_tex.height = 40;

const dragon_materials = dragonmesh.materials;

dragon_materials[0].decal_texture_id = dragonmesh.pushTexture(decal_buffer);

dragonmesh.materials = dragon_materials;

const monkeytex = new Image("monkey.png");
const monkeymesh = new RenderData("monkey.obj", monkeytex);

const monkey_materials = monkeymesh.materials;

monkey_materials[0].decal_texture_id = monkeymesh.pushTexture(decal_buffer);

monkeymesh.materials = monkey_materials;

const moontex = new Image("moon.png");

const car = new RenderData("Car.obj");
const envmap_tex_id = car.pushTexture(skytex);

const car_materials = car.materials;

const new_materials = car_materials.map((mat) => {
    mat.ref_texture_id = envmap_tex_id;
    if (mat.diffuse.r > 0.4 && mat.diffuse.g == 0.0 && mat.diffuse.b == 0.0) {
        mat.diffuse.r = 0.25;
        mat.diffuse.b = 0.5;
    }

    return mat;
});

car.materials = new_materials;

//const car_vertices = new Float32Array(car.vertices.positions); // Pointer to a float array

//car_vertices.forEach((item, i, positions) => positions[i] = item + (Math.random() * 0.1f));

const mill = new RenderData("cubes.obj");
mill.setTexture(1, moontex);

const boombox = new RenderData("Boombox.obj");
boombox.getTexture(0).filter = LINEAR;

const render_data = [dragonmesh, gltf_box, monkeymesh, car, listtest, boombox, mill];

const dragon_object = new RenderObject(dragonmesh);
dragon_object.position = { x: 0.0f, y: 4.0f, z: 0.0f };

const monkey_object = new RenderObject(dragonmesh);
monkey_object.position = { x: 4.0f, y: 4.0f, z: 0.0f };

const render_object = [new RenderObject(dragonmesh),
new RenderObject(gltf_box),
new RenderObject(monkeymesh),
new RenderObject(car),
new RenderObject(listtest),
new RenderObject(boombox),
new RenderObject(mill)
];

const sceneRoot = new SceneNode();
const objectNodes = render_object.map(obj => {
    const node = new SceneNode();
    node.attach(obj);
    sceneRoot.addChild(node);
    return node;
});

const sceneBatch = new Batch({ autoSort: true });
render_object.forEach(obj => sceneBatch.add(obj));

const asyncLoader = new AsyncLoader({ jobsPerStep: 2 });
const streamedObjects = [];

console.log("[Render] enqueue BoxTextured.gltf");
asyncLoader.enqueue("BoxTextured.gltf", (path, renderData) => {
    console.log("[Render] loaded:", path, "size:", renderData.size);
    const obj = new RenderObject(renderData);
    streamedObjects.push(obj);

    const node = new SceneNode();
    node.position = { x: 0.0f, y: 6.0f + streamedObjects.length * 2.5f, z: 0.0f
};
node.attach(obj);
sceneRoot.addChild(node);
sceneBatch.add(obj);
});

Camera.position(0.0f, 0.0f, 35.0f);

const light = Lights.new();
Lights.set(light, Lights.DIRECTION, 0.0, 0.5, 1.0);
Lights.set(light, Lights.AMBIENT, 0.12, 0.15, 0.2);
Lights.set(light, Lights.DIFFUSE, 0.5, 0.5, 0.5);
Lights.set(light, Lights.SPECULAR, 1.0, 1.0, 1.0);

//const light1 = Lights.new();
//Lights.set(light1, Lights.DIRECTION, 0.0,  1.0, 1.0);
//Lights.set(light1, Lights.AMBIENT,   0.12, 0.15, 0.2);
//Lights.set(light1, Lights.DIFFUSE, 0.4, 0.0, 0.8);

let pad = Pads.get();
let modeltodisplay = 0;
let lx = null;
let ly = null;
let rx = null;
let ry = null;

let savedlx = 0.0f;
let savedly = 0.0f;

let savedrx = 0.0f;
let savedry = 0.0f;
let savedrz = 0.0f;

let ee_info = System.getCPUInfo();

let free_mem = `RAM: ${Math.floor(System.getMemoryStats().used / 1048576)}MB / ${Math.floor(ee_info.RAMSize / 1048576)}MB`;

const gray = Color.new(40, 40, 40, 128);

let bbox = false;

let spec = false;

let decal_x = 32;
let decal_y = 0;

while (true) {
    Screen.clear(gray);
    Render.begin();
    Camera.update();
    pad.update();
    const processed = asyncLoader.process(2);
    if (processed) console.log("[Render] processed", processed, "queue:", asyncLoader.size());

    lx = ((pad.lx > 25 || pad.lx < -25) ? pad.lx : 0) / 4096.0f;
    ly = ((pad.ly > 25 || pad.ly < -25) ? pad.ly : 0) / 4096.0f;
    savedlx = savedlx - lx;
    savedly = savedly - ly;

    rx = ((pad.rx > 25 || pad.rx < -25) ? pad.rx : 0) / 2048.0f;
    ry = ((pad.ry > 25 || pad.ry < -25) ? pad.ry : 0) / 2048.0f;
    savedrx = savedrx - rx;
    savedry = savedry + ry;

    if (pad.pressed(Pads.R2)) {
        savedrz -= 0.05f;
    } else if (pad.pressed(Pads.L2)) {
        savedrz += 0.05f;
    }

    if (rx || ry) {
        decal_x += (rx * 32.0f);
        decal_y += (ry * 32.0f);
    }

    Camera.target(0, 0, savedrz);

    //if (rx || ry) {
    //    Lights.set(light, Lights.DIRECTION, savedrx,  savedry, 1.0);
    //}

    if (pad.justPressed(Pads.LEFT) && modeltodisplay > 0) {
        modeltodisplay -= 1;
    }

    if (pad.justPressed(Pads.RIGHT) && modeltodisplay < render_data.length - 1) {
        modeltodisplay += 1;
    }

    if (pad.justPressed(Pads.UP) && render_data[modeltodisplay].pipeline > 0) {
        render_data[modeltodisplay].pipeline = render_data[modeltodisplay].pipeline - 1;
    }

    if (pad.justPressed(Pads.DOWN) && render_data[modeltodisplay].pipeline < pipelines.length - 1) {
        render_data[modeltodisplay].pipeline = render_data[modeltodisplay].pipeline + 1;
    }

    if (pad.justPressed(Pads.TRIANGLE)) {
        asyncLoader.clear();
        asyncLoader.destroy();
        os.chdir("..");
        std.reload("main.js");
    }

    if (pad.justPressed(Pads.R1)) {
        bbox ^= 1;
    }

    if (pad.justPressed(Pads.SQUARE)) {
        render_data[modeltodisplay].shade_model ^= 1;
    }

    if (pad.justPressed(Pads.CIRCLE)) {
        render_data[modeltodisplay].texture_mapping ^= 1;
    }

    if (pad.justPressed(Pads.CROSS)) {
        render_data[modeltodisplay].accurate_clipping ^= 1;
    }

    //dragon_object.render();
    //monkey_object.render();

    if (lx || ly) {
        objectNodes[modeltodisplay].rotation = { x: savedly, y: savedlx, z: 0.0f };
    }

    Screen.switchContext();

    Draw.rect(0, 0, 128, 128, Color.new(0, 0, 0, 0));

    blood_tex.draw(decal_x, decal_y);

    Screen.switchContext();

    decal_buffer.draw(0, 100);

    Screen.setParam(Screen.DEPTH_TEST_ENABLE, true);
    Screen.setParam(Screen.DEPTH_TEST_METHOD, Screen.DEPTH_GEQUAL);

    sceneRoot.update();
    sceneBatch.render();

    Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);
    //
    //Image.copyVRAMBlock(draw_buffer, 0, 0, depth_buffer, 0, 0);
    //
    //Screen.setParam(Screen.ALPHA_BLEND_EQUATION, additive_alpha);

    //depth_buffer.color = Color.new(128, 100, 100, 32);
    //depth_buffer.draw(2, 2);
    //
    //depth_buffer.color = Color.new(100, 100, 128, 32);
    //depth_buffer.draw(-2, 2);
    //
    //depth_buffer.color = Color.new(100, 128, 100, 32);
    //depth_buffer.draw(2, -2);

    //Screen.setParam(Screen.ALPHA_BLEND_EQUATION, default_alpha);
    //
    //Image.copyVRAMBlock(draw_buffer, 150, 150, temp_buffer, 0, 0);
    //
    //temp_buffer.color = Color.new(128, 0, 0);
    //temp_buffer.draw(0, 0);
    //

    const renderStats = Render.stats();
    font.print(10, 10, Screen.getFPS(360) + " FPS | " + free_mem + " | Static VRAM: " + Screen.getMemoryStats(Screen.VRAM_USED_STATIC) / 1024 + "KB" + " | Dynamic VRAM: " + Screen.getMemoryStats(Screen.VRAM_USED_DYNAMIC) / 1024 + "KB");
    font.print(10, 25, render_data[modeltodisplay].size + " Vertices | " + "Pipeline: " + pipelines[render_data[modeltodisplay].pipeline] + " | Draws: " + renderStats.drawCalls + " | Tris: " + renderStats.triangles);
    font.print(10, 40, `Batch: ${sceneBatch.size} objects | Loader queue: ${asyncLoader.size()} | Streamed: ${streamedObjects.length}`);

    Screen.flip();
}
