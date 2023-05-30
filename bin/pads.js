// {"name": "Gamepad tester", "author": "Daniel Santos", "version": "04072023", "icon": "pads_icon.png", "file": "pads.js"}

const canvas = Screen.getMode();

canvas.double_buffering = false;

Screen.setMode(canvas);

Screen.setVSync(false);
Screen.setFrameCounter(true);

var font = new Font();
font.color = Color.new(128, 0, 255);
font.scale = (0.4);

var circle = new Image("pads/circle.png");
var cross = new Image("pads/cross.png");
var square = new Image("pads/square.png");
var triangle = new Image("pads/triangle.png");
var up = new Image("pads/up.png");
var down = new Image("pads/down.png");
var left = new Image("pads/left.png");
var right = new Image("pads/right.png");
var start = new Image("pads/start.png");
var pad_select = new Image("pads/select.png");
var r1 = new Image("pads/r1.png");
var r2 = new Image("pads/r2.png");
var l1 = new Image("pads/l1.png");
var l2 = new Image("pads/l2.png");
var l3 = new Image("pads/l3.png");
var r3 = new Image("pads/r3.png");

var rumble = false;
var new_pad = Pads.get();
var old_pad = new_pad;

for(;;){
    Screen.clear();

    old_pad = new_pad;
    new_pad = Pads.get();

    font.print(220, 25, "\nAthena project: Controls demo\n");
    font.print(100, 370, "\nTips:\n");
    font.print(100, 390, "\nPress R2+L2 to start rumble and press again to stop it.\n");
    font.print(100, 405, "\nButtons transparency varies with the pressure applied to them.\n");
    font.print(100, 420, "\nPress Start+R3+Pad-Up to exit this demo.\n");

    pad_select.color = Color.new(128, 128, 128, (Pads.check(new_pad, Pads.SELECT)? 128 : 60));
    pad_select.draw(260.0, 190.0);

    start.color = Color.new(128, 128, 128, (Pads.check(new_pad, Pads.START)? 128 : 60));
    start.draw(380.0, 190.0);
    
    up.color = Color.new(128, 128, 128, (Pads.check(new_pad, Pads.UP)? Pads.getPressure(Pads.UP) : 60));
    up.draw(120.0, 155.0);

    down.color = Color.new(128, 128, 128, (Pads.check(new_pad, Pads.DOWN)? Pads.getPressure(Pads.DOWN) : 60));
    down.draw(120.0, 225.0);

    left.color = Color.new(128, 128, 128, (Pads.check(new_pad, Pads.LEFT)? Pads.getPressure(Pads.LEFT) : 60));
    left.draw(85.0, 190.0);

    right.color = Color.new(128, 128, 128, (Pads.check(new_pad, Pads.RIGHT)? Pads.getPressure(Pads.RIGHT) : 60));
    right.draw(155.0, 190.0);

    triangle.color = Color.new(128, 128, 128, (Pads.check(new_pad, Pads.TRIANGLE)? Pads.getPressure(Pads.TRIANGLE) : 60));
    triangle.draw(520.0, 155.0);

    cross.color = Color.new(128, 128, 128, (Pads.check(new_pad, Pads.CROSS)? Pads.getPressure(Pads.CROSS) : 60));
    cross.draw(520.0, 225.0);

    square.color = Color.new(128, 128, 128, (Pads.check(new_pad, Pads.SQUARE)? Pads.getPressure(Pads.SQUARE) : 60));
    square.draw(485.0, 190.0);

    circle.color = Color.new(128, 128, 128, (Pads.check(new_pad, Pads.CIRCLE)? Pads.getPressure(Pads.CIRCLE) : 60));
    circle.draw(555.0, 190.0);

    l1.color = Color.new(128, 128, 128, (Pads.check(new_pad, Pads.L1)? Pads.getPressure(Pads.L1) : 60));
    l1.draw(102.0, 100.0);

    r1.color = Color.new(128, 128, 128, (Pads.check(new_pad, Pads.R1)? Pads.getPressure(Pads.R1) : 60));
    r1.draw(502.0, 100.0);

    l2.color = Color.new(128, 128, 128, (Pads.check(new_pad, Pads.L2)? Pads.getPressure(Pads.L2) : 60));
    l2.draw(137.0, 100.0);

    r2.color = Color.new(128, 128, 128, (Pads.check(new_pad, Pads.R2)? Pads.getPressure(Pads.R2) : 60));
    r2.draw(537.0, 100.0);
    
    l3.color = Color.new(128, 128, 128, (Pads.check(new_pad, Pads.L3)? 128 : 60));
    l3.draw(new_pad.lx/4+242.0, new_pad.ly/4+300.0);

    r3.color = Color.new(128, 128, 128, (Pads.check(new_pad, Pads.R3)? 128 : 60));
    r3.draw(new_pad.rx/4+402.0, new_pad.ry/4+300.0);

    Draw.rect(220.0, 280.0, 75, 75, Color.new(128, 128, 128, 32));
    Draw.rect(380.0, 280.0, 75, 75, Color.new(128, 128, 128, 32));

    if((Pads.check(new_pad, Pads.R2) && Pads.check(new_pad, Pads.L2)) && !(Pads.check(old_pad, Pads.R2) && Pads.check(old_pad, Pads.L2))){
            rumble? Pads.rumble(0 ,0) : Pads.rumble(1 ,255);
            rumble ^= 1;
    }

    if(Pads.check(new_pad, Pads.START) && Pads.check(new_pad, Pads.R3) && Pads.check(new_pad, Pads.UP)){
        break;
    }

    font.print(10, 10, Screen.getFPS(360) + " FPS");
    
    Screen.flip();
}

System.loadELF(System.boot_path + "athena_pkd.elf");
