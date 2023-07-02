// {"name": "Render demo", "author": "Daniel Santos", "version": "04072023", "icon": "render_icon.png", "file": "render.js"}


var fntcpy = new Font();
fntcpy.scale = (0.4f);

Screen.setFrameCounter(true);
Screen.setVSync(false);

const canvas = Screen.getMode();

canvas.zbuffering = true;
canvas.psmz = Z16S;

Screen.setMode(canvas);

Render.init(4/3);

var dragontex = new Image("render/dragon.png");
dragontex.filter = LINEAR;
var dragonmesh = Render.loadOBJ("render/dragon.obj", dragontex);

var monkeytex = new Image("render/monkey.png");
monkeytex.filter = LINEAR;
var monkeymesh = Render.loadOBJ("render/monkey.obj", monkeytex);

var teapot = Render.loadOBJ("render/Car.obj");

var model = [dragonmesh, monkeymesh, teapot];

Camera.position(0.0f, 0.0f, 50.0f);
Camera.rotation(0.0f, 0.0f,  0.0f);

Lights.create(1);

//Lights.set(1,  0.0,  0.0,  0.0, 1.0, 1.0, 1.0,     AMBIENT);
//Lights.set(2,  1.0,  0.0, -1.0, 1.0, 1.0, 1.0, DIRECTIONAL);
Lights.set(1,  0.0,  1.0, -1.0, 0.8, 0.8, 0.8, DIRECTIONAL);
//Lights.set(4, -1.0, -1.0, -1.0, 0.5, 0.5, 0.5, DIRECTIONAL);

var pad = Pads.get();
var modeltodisplay = 2;
var lx = null;
var ly = null;
var rx = null;
var ry = null;

var savedlx = 0.0f;
var savedly = 180.0f;
var savedrx = 50.0f;
var savedry = 0.0f;

var ee_info = System.getCPUInfo();

var free_mem = `RAM Usage: ${Math.floor(System.getMemoryStats().used / 1048576)}MB / ${Math.floor(ee_info.RAMSize / 1048576)}MB`;
var free_vram = Screen.getFreeVRAM();

const gray = Color.new(40, 40, 40, 128);

var bbox = false;

while(true){
    Screen.clear(gray);
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

    if(pad.justPressed(Pads.LEFT) || pad.justPressed(Pads.RIGHT)){
        modeltodisplay ^= 1
    }

    if(pad.justPressed(Pads.TRIANGLE)) {
        break;
    }

    if(pad.justPressed(Pads.SQUARE)) {
        bbox ^= 1;
    }

    Render.drawOBJ(model[modeltodisplay], 0.0f, 0.0f, 30.0f, savedly, savedlx, 0.0f);

    if(bbox) {
        Render.drawBbox(model[modeltodisplay], 0.0f, 0.0f, 30.0f, savedly, savedlx, 0.0f, Color.new(128, 0, 255));
    }

    fntcpy.print(10, 10, Screen.getFPS(360) + " FPS | " + free_mem + " | Free VRAM: " + free_vram + "KB");

    Screen.flip();
}