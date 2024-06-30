// {"name": "Render demo", "author": "Daniel Santos", "version": "04072023", "icon": "render_icon.png", "file": "render.js"}


const pipelines = [
    "NO_LIGHTS_COLORS",
    "NO_LIGHTS_COLORS_TEX",
    "NO_LIGHTS",
    "NO_LIGHTS_TEX",
    "DEFAULT",
    "DEFAULT_NO_TEX"
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

let listtest = new RenderObject(vertList);

let dragontex = new Image("dragon.png");
let dragonmesh = new RenderObject("dragon.obj", dragontex);

let monkeytex = new Image("monkey.png");
let monkeymesh = new RenderObject("monkey.obj", monkeytex);
monkeymesh.setPipeline(Render.PL_NO_LIGHTS_COLORS);

let car = new RenderObject("Car.obj");

let car_vertices = car.vertices;

car_vertices.forEach(vertex => {
    if (vertex.r > 0.45f && vertex.g < 0.2f && vertex.b < 0.2f) {
        vertex.r = 0.4f;
        vertex.g = 0.0f;
        vertex.b = 0.8f;
    }
});

car.vertices = car_vertices;

let mill = new RenderObject("cubes.obj");

let boombox = new RenderObject("Boombox.obj");
let boomboxtex = boombox.getTexture(0);
boomboxtex.filter = LINEAR;

let model = [dragonmesh, monkeymesh, car, boombox, mill, listtest];

Camera.position(0.0f, 0.0f, 50.0f);
Camera.rotation(0.0f, 0.0f,  0.0f);

Lights.set(0, Lights.DIRECTION, 0.0,  1.0, -1.0);
Lights.set(0, Lights.DIFFUSE, 0.8, 0.8, 0.8);

Lights.set(1, Lights.DIRECTION, 0.0,  1.0, 1.0);
Lights.set(1, Lights.DIFFUSE, 0.4, 0.0, 0.8);

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

while(true){
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

    if(pad.justPressed(Pads.DOWN) && model[modeltodisplay].getPipeline() < 5){
        model[modeltodisplay].setPipeline(model[modeltodisplay].getPipeline()+1);
    }
    
    if(pad.justPressed(Pads.TRIANGLE)) {
        System.loadELF(System.boot_path + "/athena.elf");
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