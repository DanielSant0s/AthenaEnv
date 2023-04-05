var fntcpy = new Font();
fntcpy.scale = (0.4f);

Screen.setMode(NTSC, 640, 448, CT24, INTERLACED, FIELD, true, Z16S);
Render.init(4/3);

var dragontex = new Image("render/dragon.png");
dragontex.filter = LINEAR;
var dragonmesh = Render.loadOBJ("render/dragon.obj", dragontex);

var monkeytex = new Image("render/monkey.png");
monkeytex.filter = LINEAR;
var monkeymesh = Render.loadOBJ("render/monkey.obj", monkeytex);

var model = [dragonmesh, monkeymesh];

Camera.position(0.0f, 0.0f, 50.0f);
Camera.rotation(0.0f, 0.0f,  0.0f);

Lights.create(1);

//Lights.set(1,  0.0,  0.0,  0.0, 1.0, 1.0, 1.0,     AMBIENT);
//Lights.set(2,  1.0,  0.0, -1.0, 1.0, 1.0, 1.0, DIRECTIONAL);
Lights.set(1,  0.0,  1.0, -1.0, 0.9, 0.5, 0.5, DIRECTIONAL);
//Lights.set(4, -1.0, -1.0, -1.0, 0.5, 0.5, 0.5, DIRECTIONAL);

var pad = null;
var oldpad = null;
var modeltodisplay = 0;
var lx = null;
var ly = null;
var rx = null;
var ry = null;

var savedlx = 0.0f;
var savedly = 180.0f;
var savedrx = 50.0f;
var savedry = 0.0f;

var free_mem = Math.ceilf(System.getFreeMemory()/1024);
var free_vram = Screen.getFreeVRAM();

while(true){
    Screen.clear(Color.new(40, 40, 40, 128));

    oldpad = pad;
    pad = Pads.get();
    lx = ((pad.lx > 25 || pad.lx < -25)? pad.lx : 0) / 1024.0f;
    ly = ((pad.ly > 25 || pad.ly < -25)? pad.ly : 0) / 1024.0f;
    savedlx = savedlx - lx;
    savedly = savedly - ly;

    rx = ((pad.rx > 25 || pad.rx < -25)? pad.rx : 0) / 1024.0f;
    ry = ((pad.ry > 25 || pad.ry < -25)? pad.ry : 0) / 1024.0f;
    savedrx = savedrx - rx;
    savedry = savedry - ry;

    Camera.position(0.0f, savedry,  savedrx);

    if((Pads.check(pad, Pads.LEFT) && !Pads.check(oldpad, Pads.LEFT)) || (Pads.check(pad, Pads.RIGHT) && !Pads.check(oldpad, Pads.RIGHT))){
        modeltodisplay ^= 1
    }

    Render.drawOBJ(model[modeltodisplay], 0.0f, 0.0f, 30.0f, savedly, savedlx, 0.0f);

    fntcpy.print(10, 10, Screen.getFPS(360) + " FPS | Free RAM: " + free_mem + "KB | Free VRAM: " + free_vram + "KB");

    Screen.flip();
}