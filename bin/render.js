// {"name": "Render demo", "author": "Daniel Santos", "version": "04072023", "icon": "render_icon.png", "file": "render.js"}


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
canvas.psmz = Screen.Z16S;

Screen.setMode(canvas);

Screen.clear();
font.print(0, 0, "Loading assets...");
Screen.flip();

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

const vertList = Render.vertexList(triPositions, 
                                   triNormals, 
                                   triTexCoords, 
                                   triColors);

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

const gltf_box = new RenderData("box_bump.gltf");
//gltf_box.texture_mapping = false;
gltf_box.pipeline = Render.PL_BUMPMAP;

const dragontex = new Image("dragon.png");
const dragonmesh = new RenderData("dragon.obj", dragontex);

const monkeytex = new Image("monkey.png");
const monkeymesh = new RenderData("monkey.obj", monkeytex);

monkeymesh.setTexture(1, skytex);

const dragon_materials = monkeymesh.materials;

dragon_materials[0].ref_texture_id = 1;

monkeymesh.materials = dragon_materials;

const moontex = new Image("moon.png");

const car = new RenderData("Car.obj");
car.setTexture(0, skytex);

const car_materials = car.materials;

const new_materials = car_materials.map((mat) => {
    mat.ref_texture_id = 0;
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
dragon_object.position = {x:0.0f, y:4.0f, z:0.0f};

const monkey_object = new RenderObject(dragonmesh);
monkey_object.position = {x:4.0f, y:4.0f, z:0.0f};

const render_object = [ new RenderObject(dragonmesh), 
                        new RenderObject(gltf_box),
                        new RenderObject(monkeymesh), 
                        new RenderObject(car), 
                        new RenderObject(listtest), 
                        new RenderObject(boombox), 
                        new RenderObject(mill)
                    ];

Camera.position(0.0f, 0.0f, 35.0f);

const light = Lights.new();
Lights.set(light, Lights.DIRECTION, 0.0,  0.5, 1.0);
Lights.set(light, Lights.AMBIENT,   0.12, 0.15, 0.2);
Lights.set(light, Lights.DIFFUSE,   0.5, 0.5, 0.5);
Lights.set(light, Lights.SPECULAR,  1.0, 1.0, 1.0);

//Lights.set(1, Lights.DIRECTION, 0.0,  1.0, 1.0);
//Lights.set(1, Lights.DIFFUSE, 0.4, 0.0, 0.8);

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
let free_vram = Screen.getFreeVRAM();

const gray = Color.new(40, 40, 40, 128);

let bbox = false;

let spec = false;

while(true) {
    Screen.clear(gray);
    Camera.update();
    pad.update();

    lx = ((pad.lx > 25 || pad.lx < -25)? pad.lx : 0) / 4096.0f;
    ly = ((pad.ly > 25 || pad.ly < -25)? pad.ly : 0) / 4096.0f;
    savedlx = savedlx - lx;
    savedly = savedly - ly;

    rx = ((pad.rx > 25 || pad.rx < -25)? pad.rx : 0) / 2048.0f;
    ry = ((pad.ry > 25 || pad.ry < -25)? pad.ry : 0) / 2048.0f;
    savedrx = savedrx - rx;
    savedry = savedry + ry;

    if(pad.pressed(Pads.R2)){
        savedrz -= 0.05f;
    } else if(pad.pressed(Pads.L2)){
        savedrz += 0.05f;
    }

    Camera.target(savedrx,  savedry, savedrz);

    if (rx || ry) {
        Lights.set(light, Lights.DIRECTION, savedrx,  savedry, 1.0);
    }

    if(pad.justPressed(Pads.LEFT) && modeltodisplay > 0){
        modeltodisplay -= 1;
    }

    if(pad.justPressed(Pads.RIGHT) && modeltodisplay < render_data.length-1){
        modeltodisplay += 1;
    }

    if(pad.justPressed(Pads.UP) && render_data[modeltodisplay].pipeline > 0){
        render_data[modeltodisplay].pipeline = render_data[modeltodisplay].pipeline-1;
    }

    if(pad.justPressed(Pads.DOWN) && render_data[modeltodisplay].pipeline < pipelines.length-1){
        render_data[modeltodisplay].pipeline = render_data[modeltodisplay].pipeline+1;
    }
    
    if(pad.justPressed(Pads.TRIANGLE)) {
        os.chdir("..");
        std.reload("main.js");
    }

    if(pad.justPressed(Pads.R1)) {
        bbox ^= 1;
    }

    if(pad.justPressed(Pads.SQUARE)) {
        render_data[modeltodisplay].shade_model ^= 1;
    }

    if(pad.justPressed(Pads.CIRCLE)) {
        render_data[modeltodisplay].texture_mapping ^= 1;
    }

    if(pad.justPressed(Pads.CROSS)) {
        render_data[modeltodisplay].accurate_clipping ^= 1;
    }

    //dragon_object.render();
    //monkey_object.render();

    if (lx || ly) {
        render_object[modeltodisplay].rotation = {x:savedly, y:savedlx, z:0.0f};
    }

    render_object[modeltodisplay].render();

    font.print(10, 10, Screen.getFPS(360) + " FPS | " + free_mem + " | Free VRAM: " + free_vram + "KB");
    font.print(10, 25, render_data[modeltodisplay].size + " Vertices | " + "Pipeline: " + pipelines[render_data[modeltodisplay].pipeline]);

    Screen.flip();
}