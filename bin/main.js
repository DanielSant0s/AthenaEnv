//User settings
var char_scale = 0.6;
var char_speed = [1.5, 6.0];

var idle_frame_num = 30;
var walk_frame_num = 30;
var run_frame_num = 24;

var idle = new Array(idle_frame_num);
var walk = new Array(walk_frame_num);
var run =  new Array(run_frame_num);

for (var i = 0; i < idle_frame_num; i++) {
    idle[i] = Graphics.loadImage("Idle/Idle-" + i + ".png");
};

for (var i = 0; i < walk_frame_num; i++) {
    walk[i] = Graphics.loadImage("Walk/Walk-" + i + ".png");
};


for (var i = 0; i < run_frame_num; i++) {
    run[i]  = Graphics.loadImage("Run/Run-"   + i + ".png");
};

var idle_obj = {sprite:idle, width:Graphics.getImageWidth(idle[0]), height:Graphics.getImageHeight(idle[0]), count:idle_frame_num};
var walk_obj = {sprite:walk, width:Graphics.getImageWidth(walk[0]), height:Graphics.getImageHeight(walk[0]), count:walk_frame_num};
var run_obj = {sprite:run, width:Graphics.getImageWidth(run[0]), height:Graphics.getImageHeight(run[0]), count:run_frame_num};

var move_set = new Array(3);
move_set[0] = idle_obj;
move_set[1] = walk_obj;
move_set[2] = run_obj;

Font.ftInit();
var kghappy = Font.ftLoad("Font/KGHAPPY.ttf");
var kghappyshadows = Font.ftLoad("Font/KGHAPPYShadows.ttf");
var kghappysolid = Font.ftLoad("Font/KGHAPPYSolid.ttf");
Font.ftSetPixelSize(kghappy,        10.0, 10.0);
Font.ftSetPixelSize(kghappy,        25.0, 25.0);
Font.ftSetPixelSize(kghappyshadows, 25.0, 25.0);
Font.ftSetPixelSize(kghappysolid,   25.0, 25.0);

var x = 50.0;
var oldpad = Pads.get();
var pad = Pads.get();
var frame = 0;
var fr_mult = 0;

var move_state = 0; 

//0 = RIGHT, 1 = LEFT
var char_side = 0;

var ram = System.getFreeMemory();

var time = Timer.new();
var prev = 0;
var cur = 0;
var fps = 0;

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
            char_side = 0
        }
        x += char_speed[move_state-1]
    }

    if(Pads.check(pad, PAD_LEFT)){
        if(!Pads.check(oldpad, PAD_LEFT)){
            char_side = 1
        }
        x -= char_speed[move_state-1]
    }

    if(Pads.check(pad, PAD_CROSS) && !Pads.check(oldpad, PAD_CROSS)){
        if(move_state == 1) {
            move_state = 2
        } else if(move_state == 2){
            move_state = 1
        }
    }

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
            x-move_set[move_state].width/2, 
            200.0, 
            move_set[move_state].width*char_scale, 
            move_set[move_state].height*char_scale);
    } else {
        Graphics.drawScaleImage(move_set[move_state].sprite[frame], 
            x+move_set[move_state].width-move_set[move_state].width/2, 
            200.0, 
            -move_set[move_state].width*char_scale, 
            move_set[move_state].height*char_scale);
    }

    prev = cur;
    cur = Timer.getTime(time);

    Display.flip();
}