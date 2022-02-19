//User settings
var char_scale = 0.35;
var char_speed = [1.5, 6.0];

function Vector2(x_pos, y_pos) {
    this.x = x_pos;
    this.y = y_pos;
};

var camera = new Vector2(70.0, 50.0);

function loadAnimGroup(name) {
    var framelist = System.listDirectory("Anim/" + name);
    var slot = new Array(framelist.length);

    for (var i = 0; i < framelist.length; i++) {
        slot[i] = Graphics.loadImage("Anim/" + name + "/" + name + "-" + i + ".png");
    };
    console.log("dummy_game: Free Memory after loading " + name + " framelist: " + Math.ceil(System.getFreeMemory()/1024) + " KB\n");
    return {sprite:slot, width:Graphics.getImageWidth(slot[0]), height:Graphics.getImageHeight(slot[0]), count:framelist.length};
};

function World2Screen(worldcoords) {
    return new Vector2(worldcoords.x-camera.x, worldcoords.y-camera.y);
};

function Screen2World(worldcoords) {
    return new Vector2(worldcoords.x+camera.x, worldcoords.y+camera.y);
};

console.log("dummy_game: Free Memory during boot: " + Math.ceil(System.getFreeMemory()/1024) + " KB\n");

Font.ftInit();
var kghappy = Font.ftLoad("Font/KGHAPPY.ttf");
var kghappyshadows = Font.ftLoad("Font/KGHAPPYShadows.ttf");
var kghappysolid = Font.ftLoad("Font/KGHAPPYSolid.ttf");
Font.ftSetPixelSize(kghappy,        10.0, 10.0);
Font.ftSetPixelSize(kghappy,        25.0, 25.0);
Font.ftSetPixelSize(kghappyshadows, 25.0, 25.0);
Font.ftSetPixelSize(kghappysolid,   25.0, 25.0);

console.log("dummy_game: Free Memory after loading fntsys: " + Math.ceil(System.getFreeMemory()/1024) + " KB\n");

var move_set = new Array(4);
move_set[0] = loadAnimGroup("Idle");
move_set[1] = loadAnimGroup("Walk");
move_set[2] = loadAnimGroup("Run");
move_set[3] = loadAnimGroup("Jump_Start");

var char = new Vector2(224.0, 250.0);

var oldpad = Pads.get();
var pad = Pads.get();
var frame = 0;
var fr_mult = 0;

var move_state = 0; 

//0 = RIGHT, 1 = LEFT
var char_side = 0;

var fps = 0;

var ram = System.getFreeMemory();

while(true){
    oldpad = pad;
    pad = Pads.get();
    Display.clear(Color.new(192, 192, 192));

    Font.ftPrint(kghappyshadows, 15.0, 15.0, 0, 640.0, 448.0, "Free RAM:" + Math.ceil(ram/1024) + "KB - " + fps + " FPS\n", Color.new(0,0,0));
    Font.ftPrint(kghappysolid, 15.0, 15.0, 0, 640.0, 448.0, "Free RAM:" + Math.ceil(ram/1024) + "KB - " + fps + "FPS \n", Color.new(128,128,128));
    
    if(pad.btns == 0 && oldpad.btns != 0 || pad.lx == 0 && oldpad.lx != 0){
        move_state = 0;
    }

    if((Pads.check(pad, PAD_RIGHT) && !Pads.check(oldpad, PAD_RIGHT)) || (Pads.check(pad, PAD_LEFT) && !Pads.check(oldpad, PAD_LEFT)) || (pad.lx != 0  && oldpad.lx == 0)){
        move_state = 1;
    }
    
    if(Pads.check(pad, PAD_RIGHT) || pad.lx > 100){
        if(!Pads.check(oldpad, PAD_RIGHT) || oldpad.lx < -100){
            char_side = 0;
        };
        char.x += char_speed[move_state-1];
        camera.x += char_speed[move_state-1];
    }

    if(Pads.check(pad, PAD_LEFT) || pad.lx < -100){
        if(!Pads.check(oldpad, PAD_LEFT) || oldpad.lx > 100){
            char_side = 1;
        };
        char.x -= char_speed[move_state-1];
        camera.x -= char_speed[move_state-1];
    }

    if(Pads.check(pad, PAD_CROSS) && !Pads.check(oldpad, PAD_CROSS)){
        if(move_state == 1) {
            move_state = 2
        } else if(move_state == 2){
            move_state = 1
        }
    }

    if(Pads.check(pad, PAD_SQUARE) && !Pads.check(oldpad, PAD_SQUARE)){
        move_state = 3
    }

    if(Pads.check(pad, PAD_L1)){
        camera.x += 2.0;
    };

    if(Pads.check(pad, PAD_R1)){
        camera.x -= 2.0;
    };

    fr_mult++;
    if(fr_mult > 1) {
        frame++;
        fr_mult = 0;
    }

    if(frame > move_set[move_state].count-1) {
        frame = 0;
    }

    fps = Display.getFPS(240);

    if(char_side == 0){
        Graphics.drawScaleImage(move_set[move_state].sprite[frame], 
            World2Screen(char).x-move_set[move_state].width/2, 
            World2Screen(char).y, 
            move_set[move_state].width*char_scale, 
            move_set[move_state].height*char_scale);
    } else {
        Graphics.drawScaleImage(move_set[move_state].sprite[frame], 
            World2Screen(char).x+move_set[move_state].width-move_set[move_state].width/2, 
            World2Screen(char).y, 
            -move_set[move_state].width*char_scale, 
            move_set[move_state].height*char_scale);
    };

    Display.flip();
}