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

Camera.position(0, 0, 50);
Camera.rotation(0, 0,  0);

Lights.create(1);

Lights.set(1,  0,  1, -1, 0.9f, 0.5f, 0.5f, DIRECTIONAL);

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

//console.log(Network.get("https://github.com"));

Screen.setVSync(false);

let img_list = new ImageList();

let circle = new Image("pads/circle.png", VRAM, img_list);
let triangle = new Image("pads/triangle.png", RAM, img_list);
let cross = new Image("pads/cross.png", RAM, img_list);
let square = new Image("pads/square.png", RAM, img_list);
let l1 = new Image("pads/l1.png", RAM, img_list);
let l2 = new Image("pads/l2.png", RAM, img_list);
let r1 = new Image("pads/r1.png", RAM, img_list);
let r2 = new Image("pads/r2.png", RAM, img_list);

img_list.process();

while(!circle.ready()){
    console.log("Waiting circle image to be loaded...");
}

circle.width = 100;
circle.height = 100;
circle.color = Color.new(0, 0, 128);
circle.filter = LINEAR;

let float_test = 15.6f;

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

    mine_font.print(20, 20, Screen.getFPS(360) + " FPS");

    circle.draw(200, 200);

    if(triangle.ready()){
        triangle.draw(200, 400);
    }

    if(square.ready()){
        square.draw(250, 400);
    }

    if(cross.ready()){
        cross.draw(300, 400);
    }

    if(l1.ready()){
        l1.draw(350, 400);
    }

    if(l2.ready()){
        l2.draw(400, 400);
    }

    if(r1.ready()){
        r1.draw(450, 400);
    }

    if(r2.ready()){
        r2.draw(500, 400);
    }

    Render.drawOBJ(dragonmesh, 0, 0, 30, 180, 0, 0);
    
    Screen.flip();
};


