Display.setMode(NTSC, 640, 448, CT24, INTERLACED, FIELD, true, Z16S);
Render.init(4/3);

var dragontex = new Image("render/dragon.png");
dragontex.filter = LINEAR;
var dragonmesh = Render.loadOBJ("render/dragon.obj", dragontex);

var monkeytex = new Image("render/monkey.png");
monkeytex.filter = LINEAR;
var monkeymesh = Render.loadOBJ("render/monkey.obj", monkeytex);

var model = [dragonmesh, monkeymesh];

Camera.position(0.0, 0.0, 50.0);
Camera.rotation(0.0, 0.0,  0.0);

Lights.create(4);

//Lights.set(1,  0.0,  0.0,  0.0, 1.0, 1.0, 1.0,     AMBIENT);
//Lights.set(2,  1.0,  0.0, -1.0, 1.0, 1.0, 1.0, DIRECTIONAL);
Lights.set(3,  0.0,  1.0, -1.0, 0.9, 0.5, 0.5, DIRECTIONAL);
Lights.set(4, -1.0, -1.0, -1.0, 0.5, 0.5, 0.5, DIRECTIONAL);

var pad = null;
var oldpad = null;
var modeltodisplay = 0;
var lx = null;
var ly = null;
var rx = null;
var ry = null;

var savedlx = 0.0;
var savedly = 180.0;
var savedrx = 50.0;
var savedry = 0.0;

while(true){
    oldpad = pad;
    pad = Pads.get();
    lx = pad.lx / 1024.0;
    ly = pad.ly / 1024.0;
    savedlx = savedlx - lx;
    savedly = savedly - ly;

    rx = pad.rx / 1024.0;
    ry = pad.ry / 1024.0;
    savedrx = savedrx - rx;
    savedry = savedry - ry;

    Camera.position(0.0, savedry,  savedrx);

    Display.clear(Color.new(40, 40, 40, 128));

    if((Pads.check(pad, PAD_LEFT) && !Pads.check(oldpad, PAD_LEFT)) || (Pads.check(pad, PAD_RIGHT) && !Pads.check(oldpad, PAD_RIGHT))){
        modeltodisplay ^= 1
    }

    Render.drawOBJ(model[modeltodisplay], 0.0, 0.0, 30.0, savedly, savedlx, 0.0);

    Display.flip();
}