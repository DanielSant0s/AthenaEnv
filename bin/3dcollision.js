// {"name": "3D collision", "author": "Daniel Santos", "version": "04072023", "icon": "render_icon.png", "file": "3dcollision.js"}

let fntcpy = new Font("default");
fntcpy.scale = (0.4f);

const canvas = Screen.getMode();

canvas.zbuffering = true;
canvas.psmz = Z16S;

Screen.setMode(canvas);

Render.setView(4/3);

Screen.setFrameCounter(true);
Screen.setVSync(false);

// Change your root folder to "render" so we can work with file path magic :p
os.chdir("render");

let dragontex = new Image("dragon.png");
let dragonmesh = new RenderObject("dragon.obj", dragontex);
let dragonbounds = Physics.createBox(4, 4, 4);

let car = new RenderObject("Car.obj");
let carbounds = Physics.createBox(2, 2, 2);

Camera.position(0.0f, 0.0f, 40.0f);
Camera.type(Camera.LOOKAT);

Lights.set(0, Lights.DIRECTION, 0.0,  1.0, -1.0);
Lights.set(0, Lights.DIFFUSE, 0.8, 0.8, 0.8);

Lights.set(1, Lights.DIRECTION, 0.0,  1.0, 1.0);
Lights.set(1, Lights.DIFFUSE, 0.4, 0.0, 0.8);

let pad = Pads.get();

let lx = null;
let ly = null;
let rx = null;
let ry = null;

let savedlx = 0.0f;
let savedly = 0.0f;
let savedrx = 0.0f;
let savedry = 0.0f;

const gray = Color.new(40, 40, 40, 128);

let bbox = false;

let collision = undefined; 

while(true){
    Screen.clear(gray);
    Camera.update();
    pad.update();

    lx = ((pad.lx > 25 || pad.lx < -25)? pad.lx : 0) / 1024.0f;
    ly = ((pad.ly > 25 || pad.ly < -25)? pad.ly : 0) / 1024.0f;
    savedlx = savedlx - lx;
    savedly = savedly - ly;

    rx = ((pad.rx > 25 || pad.rx < -25)? pad.rx : 0) / 10240.0f;
    ry = ((pad.ry > 25 || pad.ry < -25)? pad.ry : 0) / 10240.0f;
    savedrx = savedrx - rx;
    savedry = savedry - ry;
    
    if(pad.justPressed(Pads.TRIANGLE)) {
        System.loadELF(System.boot_path + "/athena.elf");
    }

    if(pad.justPressed(Pads.SQUARE)) {
        bbox ^= 1;
    }

    collision = Physics.boxBoxCollide(dragonbounds, savedrx, savedry, 25.0f, carbounds, 0.0f, 0.0f, 25.0f);

    Camera.target(savedrx, savedry, 25.0f);

    car.draw(savedrx, savedry, 25.0f, 3.14f, 0.0f, 0.0f);
    if (collision) {
        car.drawBounds(savedrx, savedry, 25.0f, 3.14f, 0.0f, 0.0f, Color.new(0, 255, 0));
    } else {
        car.drawBounds(savedrx, savedry, 25.0f, 3.14f, 0.0f, 0.0f, Color.new(255, 0, 0));
    }
    
    dragonmesh.draw(0.0f, 0.0f, 25.0f, 3.14f, 0.0f, 0.0f);

    dragonmesh.drawBounds(0.0f, 0.0f, 25.0f, 3.14f, 0.0f, 0.0f, Color.new(128, 0, 255));

    fntcpy.print(10, 10, Screen.getFPS(360) + " FPS | " + `RAM: ${Math.floor(System.getMemoryStats().used / 1048576)}MB` + 
     ` | Collision: ${JSON.stringify(collision)}`);

    Screen.flip();
}