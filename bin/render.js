// {"name": "Render demo", "author": "Daniel Santos", "version": "04072023", "icon": "render_icon.png", "file": "render.js"}


const pipelines = [
    "NO_LIGHTS",
    "DEFAULT",
    "SPECULAR",

    "PVC",
];

const font = new Font("default");
font.scale = 0.4f;

Screen.setFrameCounter(true);
Screen.setVSync(false);

const canvas = Screen.getMode();

canvas.zbuffering = true;
canvas.psmz = Z16S;

Screen.setMode(canvas);

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

const listtest = new RenderObject(vertList);

let dragontex = new Image("dragon.png");
const dragonmesh = new RenderObject("dragon.obj", dragontex);

let monkeytex = new Image("monkey.png");
const monkeymesh = new RenderObject("monkey.obj", monkeytex);

let moontex = new Image("moon.png");

const car = new RenderObject("Car.obj");

const car_materials = car.materials;

const new_materials = car_materials.map((mat) => {
    if (mat.diffuse.r > 0.4 && mat.diffuse.g == 0.0 && mat.diffuse.b == 0.0) {
        mat.diffuse.r = 0.25;
        mat.diffuse.b = 0.5;
    }

    return mat;
});

car.materials = new_materials;

//const car_vertices = new Float32Array(car.vertices.positions); // Pointer to a float array

//car_vertices.forEach((item, i, positions) => positions[i] = item + (Math.random() * 0.1f));

const mill = new RenderObject("cubes.obj");
mill.setTexture(1, moontex);

const boombox = new RenderObject("Boombox.obj");
boombox.getTexture(0).filter = LINEAR;

const model = [dragonmesh, monkeymesh, car, listtest, boombox, mill];

Camera.type(Camera.LOOKAT)
Camera.position(0.0f, 0.0f, 35.0f);
Camera.rotation(0.0f, 0.0f,  0.0f);

const light = Lights.new();
Lights.set(light, Lights.DIRECTION, 0.0,  0.5, 1.0);
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
let savedly = 180.0f;

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

    lx = ((pad.lx > 25 || pad.lx < -25)? pad.lx : 0) / 1024.0f;
    ly = ((pad.ly > 25 || pad.ly < -25)? pad.ly : 0) / 1024.0f;
    savedlx = savedlx - lx;
    savedly = savedly - ly;

    rx = ((pad.rx > 25 || pad.rx < -25)? pad.rx : 0) / 1024.0f;
    ry = ((pad.ry > 25 || pad.ry < -25)? pad.ry : 0) / 1024.0f;
    savedrx = savedrx - rx;
    savedry = savedry + ry;

    if(pad.pressed(Pads.R2)){
        savedrz -= 0.05f;
    } else if(pad.pressed(Pads.L2)){
        savedrz += 0.05f;
    }

    Camera.target(savedrx, savedry, savedrz);

    if(pad.justPressed(Pads.LEFT) && modeltodisplay > 0){
        modeltodisplay -= 1;
    }

    if(pad.justPressed(Pads.RIGHT) && modeltodisplay < model.length-1){
        modeltodisplay += 1;
    }

    if(pad.justPressed(Pads.UP) && model[modeltodisplay].getPipeline() > 0){
        model[modeltodisplay].setPipeline(model[modeltodisplay].getPipeline()-1);
    }

    if(pad.justPressed(Pads.DOWN) && model[modeltodisplay].getPipeline() < pipelines.length){
        model[modeltodisplay].setPipeline(model[modeltodisplay].getPipeline()+1);
    }
    
    if(pad.justPressed(Pads.TRIANGLE)) {
        os.chdir("..");
        std.reload("main.js");
    }

    if(pad.justPressed(Pads.SQUARE)) {
        bbox ^= 1;
    }

    if(pad.justPressed(Pads.CIRCLE)) {
        model[modeltodisplay].texture_mapping ^= 1;
    }

    if(pad.justPressed(Pads.CROSS)) {
        model[modeltodisplay].accurate_clipping ^= 1;
    }


    model[modeltodisplay].draw(0.0f, 0.0f, 0.0f, savedly, savedlx, 0.0f);

    if(bbox) {
        model[modeltodisplay].drawBounds(0.0f, 0.0f, 30.0f, savedly, savedlx, 0.0f, Color.new(128, 0, 255));
    }

    font.print(10, 10, Screen.getFPS(360) + " FPS | " + free_mem + " | Free VRAM: " + free_vram + "KB");
    font.print(10, 25, model[modeltodisplay].size + " Vertices | " + "Pipeline: " + pipelines[model[modeltodisplay].getPipeline()]);

    Screen.flip();
}