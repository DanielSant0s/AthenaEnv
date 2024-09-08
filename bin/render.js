// {"name": "Render demo", "author": "Daniel Santos", "version": "04072023", "icon": "render_icon.png", "file": "render.js"}


const pipelines = [
    "NO_LIGHTS_COLORS",
    "NO_LIGHTS_COLORS_TEX",
    "NO_LIGHTS",
    "NO_LIGHTS_TEX",
    "DEFAULT",
    "DEFAULT_NO_TEX",
    "SPECULAR",
    "SPECULAR_NO_TEX"
];

let fntcpy = new Font("default");
fntcpy.scale = (0.4f);

Screen.setFrameCounter(true);
Screen.setVSync(false);

const canvas = Screen.getMode();

canvas.zbuffering = true;
canvas.psmz = Z16S;

Screen.setMode(canvas);

Render.setView(4/3);

// Change your root folder to "render" so we can work with file path magic :p
os.chdir("render");

const vertList = [
    Render.vertex(0.5f, 0.5f, 0.5f,         // Position - X Y Z
                  1.0f, 1.0f, 1.0f,         // Normal - N1 N2 N3
                  0.0f, 0.0f,               // Texture coordinate - S T
                  1.0f, 0.0f, 0.0f, 1.0f),  // Color - R G B A

    Render.vertex(0.5f, 0.8f, 0.5f, 
                  1.0f, 1.0f, 1.0f, 
                  1.0f, 0.0f, 
                  0.0f, 1.0f, 0.0f, 1.0f),

    Render.vertex(0.8f, 0.8f, 0.5f, 
                  1.0f, 1.0f, 1.0f, 
                  1.0f, 1.0f, 
                  0.0f, 0.0f, 1.0f, 1.0f),
];

const listtest = new RenderObject(vertList);

let dragontex = new Image("dragon.png");
const dragonmesh = new RenderObject("dragon.obj", dragontex);

let monkeytex = new Image("monkey.png");
const monkeymesh = new RenderObject("monkey.obj", monkeytex);

const car = new RenderObject("Car.obj");

let car_vertices = car.vertices;

car_vertices.forEach(vertex => {
    if (vertex.r > 0.45f && vertex.g < 0.2f && vertex.b < 0.2f) {
        vertex.r = 0.4f;
        vertex.g = 0.0f;
        vertex.b = 0.8f;
    }
});

car.vertices = car_vertices;

const mill = new RenderObject("cubes.obj");

const boombox = new RenderObject("Boombox.obj");
let boomboxtex = boombox.getTexture(0);
boomboxtex.filter = LINEAR;

function generateSphere(radius, latSegments, longSegments) {
    const vertList = [];

    function sphericalToCartesian(radius, theta, phi) {
        const x = radius * Math.sin(theta) * Math.cos(phi);
        const y = radius * Math.sin(theta) * Math.sin(phi);
        const z = radius * Math.cos(theta);
        return { x, y, z };
    }

    for (let lat = 0; lat <= latSegments; lat++) {
        const theta = lat * Math.PI / latSegments;
        for (let lon = 0; lon <= longSegments; lon++) {
            const phi = lon * 2 * Math.PI / longSegments;
            const { x, y, z } = sphericalToCartesian(radius, theta, phi);
            const u = lon / longSegments;
            const v = lat / latSegments;
            const color = { r: 1.0, g: 1.0, b: 1.0, a: 1.0 }; // Exemplo de cor

            vertList.push(
                Render.vertex(x, y, z,     // Posição
                              x, y, z,     // Normal (exemplo de normal, deve ser vetor unitário)
                              u, v,        // Coordenadas de textura
                              color.r, color.g, color.b, color.a) // Cor
            );
        }
    }

    // Gerar triângulos strips
    const finalVertList = [];
    for (let lat = 0; lat < latSegments; lat++) {
        for (let lon = 0; lon <= longSegments; lon++) {
            const current = lat * (longSegments + 1) + lon;
            const next = (lat + 1) * (longSegments + 1) + lon;

            finalVertList.push(vertList[current]);
            finalVertList.push(vertList[next]);
            finalVertList.push(vertList[next + 1]);
            finalVertList.push(vertList[current]);
            finalVertList.push(vertList[next + 1]);
            finalVertList.push(vertList[current + 1]);
        }
    }

    const moontex = new Image("moon.png");
    const listtest = new RenderObject(finalVertList, moontex, true);
    return listtest;
}

const radius = 1.0;
const latSegments = 160;
const longSegments = 160;
const sphere = generateSphere(radius, latSegments, longSegments);

const model = [dragonmesh, monkeymesh, car, listtest, boombox, mill, sphere];

Camera.position(0.0f, 0.0f, 50.0f);
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
let savedrx = 100.0f;
let savedry = 0.0f;

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
    savedry = savedry - ry;

    Camera.position(0.0f, savedry,  savedrx);

    if(pad.justPressed(Pads.LEFT) && modeltodisplay > 0){
        modeltodisplay -= 1;
    }

    if(pad.justPressed(Pads.RIGHT) && modeltodisplay < model.length-1){
        modeltodisplay += 1;
    }

    if(pad.justPressed(Pads.UP) && model[modeltodisplay].getPipeline() > 0){
        model[modeltodisplay].setPipeline(model[modeltodisplay].getPipeline()-1);
    }

    if(pad.justPressed(Pads.DOWN) && model[modeltodisplay].getPipeline() < 7){
        model[modeltodisplay].setPipeline(model[modeltodisplay].getPipeline()+1);
    }
    
    if(pad.justPressed(Pads.TRIANGLE)) {
        os.chdir("..");
        std.reload("main.js");
    }

    if(pad.justPressed(Pads.SQUARE)) {
        bbox ^= 1;
    }

    model[modeltodisplay].draw(0.0f, 0.0f, 30.0f, savedly, savedlx, 0.0f);

    if(bbox) {
        model[modeltodisplay].drawBounds(0.0f, 0.0f, 30.0f, savedly, savedlx, 0.0f, Color.new(128, 0, 255));
    }

    fntcpy.print(10, 10, Screen.getFPS(360) + " FPS | " + free_mem + " | Free VRAM: " + free_vram + "KB");
    fntcpy.print(10, 25, model[modeltodisplay].size + " Vertices | " + "Pipeline: " + pipelines[model[modeltodisplay].getPipeline()]);

    Screen.flip();
}