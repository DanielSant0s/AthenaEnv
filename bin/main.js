//User settings
var char_scale = 0.35;
var char_speed = [1.5, 6.0];

function Vector2(x_pos, y_pos) {
    this.x = x_pos;
    this.y = y_pos;
};

function MapTile(texture, worldpos, scale) {
    this.tex = texture;
    this.pos = worldpos;
    this.size = scale;
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

var time = Timer.new();
var prev = 0;
var cur = 0;
var fps = 0;

var tile02 = Graphics.loadImage("Map/Tiles/Tile_02.png");
var tile14 = Graphics.loadImage("Map/Tiles/Tile_14.png");

var map = new Array(16);
map[00] = new MapTile(tile02, new Vector2(112.0, 380.0), 1.0);
map[01] = new MapTile(tile02, new Vector2(176.0, 380.0), 1.0);
map[02] = new MapTile(tile02, new Vector2(240.0, 380.0), 1.0);
map[03] = new MapTile(tile02, new Vector2(304.0, 380.0), 1.0);
map[04] = new MapTile(tile02, new Vector2(368.0, 380.0), 1.0);
map[05] = new MapTile(tile02, new Vector2(432.0, 380.0), 1.0);
map[06] = new MapTile(tile02, new Vector2(496.0, 380.0), 1.0);
map[07] = new MapTile(tile02, new Vector2(560.0, 380.0), 1.0);

map[08] = new MapTile(tile14, new Vector2(112.0, 444.0), 1.0);
map[09] = new MapTile(tile14, new Vector2(176.0, 444.0), 1.0);
map[10] = new MapTile(tile14, new Vector2(240.0, 444.0), 1.0);
map[11] = new MapTile(tile14, new Vector2(304.0, 444.0), 1.0);
map[12] = new MapTile(tile14, new Vector2(368.0, 444.0), 1.0);
map[13] = new MapTile(tile14, new Vector2(432.0, 444.0), 1.0);
map[14] = new MapTile(tile14, new Vector2(496.0, 444.0), 1.0);
map[15] = new MapTile(tile14, new Vector2(560.0, 444.0), 1.0);

var ram = System.getFreeMemory();

while(true){
    oldpad = pad;
    pad = Pads.get();
    Display.clear(Color.new(192, 192, 192));

    Font.ftPrint(kghappyshadows, 15.0, 15.0, 0, 640.0, 448.0, "Free RAM:" + Math.ceil(ram/1024) + "KB - " + fps + " FPS\n", Color.new(0,0,0));
    Font.ftPrint(kghappysolid, 15.0, 15.0, 0, 640.0, 448.0, "Free RAM:" + Math.ceil(ram/1024) + "KB - " + fps + "FPS \n", Color.new(128,128,128));
    
    if(pad == 0 && oldpad != 0){
        move_state = 0;
    }

    if((Pads.check(pad, PAD_RIGHT) && !Pads.check(oldpad, PAD_RIGHT)) || (Pads.check(pad, PAD_LEFT) && !Pads.check(oldpad, PAD_LEFT))){
        move_state = 1;
    }

    if(Pads.check(pad, PAD_RIGHT)){
        if(!Pads.check(oldpad, PAD_RIGHT)){
            char_side = 0;
        };
        char.x += char_speed[move_state-1];
        camera.x += char_speed[move_state-1];
    }

    if(Pads.check(pad, PAD_LEFT)){
        if(!Pads.check(oldpad, PAD_LEFT)){
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
        fps = System.getFPS(prev, cur);
    }

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
    }

    for (var i = 0; i < map.length; i++) {
        Graphics.drawScaleImage(map[i].tex, World2Screen(map[i].pos).x, World2Screen(map[i].pos).y, 64.0*map[i].size, 64.0*map[i].size);
    };

    prev = cur;
    cur = Timer.getTime(time);

    Display.flip();
}