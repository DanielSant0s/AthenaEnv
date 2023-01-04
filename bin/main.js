console.log('Hello, QuickJS!');
let s = Date.now();
let ti = Date.now();

Screen.setMode(NTSC, 640, 448, CT24, INTERLACED, FIELD, true, Z16S);
Render.init(4/3);

let dragontex = new Image("render/dragon.png");
dragontex.filter = LINEAR;
let dragonmesh = Render.loadOBJ("render/dragon.obj", dragontex);

let test = Color.new(128, 0, 255);
console.log('Color module test - R:' + Color.getR(test) + ' G: ' + Color.getG(test) + ' B: ' + Color.getB(test));

Camera.position(0.0, 0.0, 50.0);
Camera.rotation(0.0, 0.0,  0.0);

Lights.create(1);

Lights.set(1,  0.0,  1.0, -1.0, 0.9, 0.5, 0.5, DIRECTIONAL);

let pad = Pads.get();
let oldpad = pad;
let c_x = 300;
let c_y = 300;

var mine_font = new Font("minecraft.ttf");
mine_font.scale = 2;
mine_font.color = Color.new(255, 255, 0);

Network.init();
let netcfg = Network.getConfig();
console.log("Network config\n" + 
            "\nIP: " + netcfg.ip + 
            "\nNetmask: " + netcfg.netmask + 
            "\nGateway: " + netcfg.gateway + 
            "\nDNS: " + netcfg.dns);

console.log(Network.get("https://github.com"));

Screen.setVSync(false);

let circle = new Image("pads/circle.png", VRAM);
circle.width = 100.0;
circle.height = 100.0;
circle.color = Color.new(0, 0, 128);
circle.filter = LINEAR;

while (true){
    Screen.clear(test);
    oldpad = pad;
    pad = Pads.get();

    if(Pads.check(pad, Pads.LEFT)){
        c_x--;
    }
    if(Pads.check(pad, Pads.RIGHT)){
        c_x++;
    }
    if(Pads.check(pad, Pads.UP)){
        c_y--;
    }
    if(Pads.check(pad, Pads.DOWN)){
        c_y++;
    }

    Draw.rect(50, 50, 150, 150, Color.new(128, 128, 128));

    Draw.circle(c_x, c_y, 25, Color.new(255, 0, 0));

    let fps = Screen.getFPS(360);
    if(Date.now() - ti > 360){
        console.log(fps + " FPS");
        ti = Date.now();
    }

    mine_font.print(10, 10, "Hello world!");

    circle.draw(200, 200);

    Render.drawOBJ(dragonmesh, 0.0, 0.0, 30.0, 180.0, 0.0, 0.0);
    
    Screen.flip();
};


