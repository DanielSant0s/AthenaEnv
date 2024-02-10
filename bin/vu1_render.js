// {"name": "VU1 Cube", "author": "Daniel Santos", "version": "07022024", "icon": "render_icon.png", "file": "vu1_render.js"}


let fntcpy = new Font();
fntcpy.scale = (0.4f);

Screen.setFrameCounter(true);
Screen.setVSync(false);

const canvas = Screen.getMode();

canvas.zbuffering = true;
canvas.psm = CT24;
canvas.psmz = Z16S;

Screen.setMode(canvas);

Render.init(4/3);

Camera.position(0.0f, 0.0f, 50.0f);
Camera.rotation(0.0f, 0.0f,  0.0f);

Lights.create(1);

//Lights.set(1,  0.0,  0.0,  0.0, 1.0, 1.0, 1.0,     AMBIENT);
//Lights.set(2,  1.0,  0.0, -1.0, 1.0, 1.0, 1.0, DIRECTIONAL);
Lights.set(1,  0.0,  1.0, -1.0, 0.8, 0.8, 0.8, DIRECTIONAL);
//Lights.set(4, -1.0, -1.0, -1.0, 0.5, 0.5, 0.5, DIRECTIONAL);

os.chdir("render");

let fish = Render.loadCube("Boombox.obj");

Camera.position(0.0f, 0.0f, 50.0f);
Camera.rotation(0.0f, 0.0f,  0.0f);

let pad = Pads.get();
let lx = null;
let ly = null;
let rx = null;
let ry = null;

let savedlx = 0.0f;
let savedly = 180.0f;
let savedrx = 100.0f;
let savedry = 0.0f;

let ee_info = System.getCPUInfo();

let free_mem = `RAM Usage: ${Math.floor(System.getMemoryStats().used / 1048576)}MB / ${Math.floor(ee_info.RAMSize / 1048576)}MB`;
let free_vram = Screen.getFreeVRAM();

const gray = Color.new(40, 40, 40, 128);

let bbox = false;

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

    if(pad.justPressed(Pads.TRIANGLE)) {
        System.loadELF(System.boot_path + "/athena.elf");
    }

    if(pad.justPressed(Pads.SQUARE)) {
        bbox ^= 1;
    }

    Render.drawCube(fish, 0.0f, 0.0f, 30.0f, savedly, savedlx, 0.0f);


    if(bbox) {
        Render.drawBbox(fish, 0.0f, 0.0f, 30.0f, savedly, savedlx, 0.0f, Color.new(128, 0, 255));
    }

    fntcpy.print(10, 10, Screen.getFPS(360) + " FPS | " + free_mem + " | Free VRAM: " + free_vram + "KB");

    Screen.flip();
}