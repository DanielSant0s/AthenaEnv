var font = new Font();
font.color = Color.new(128, 0, 255);
font.setScale(0.4);

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
    Display.clear();

    old_pad = new_pad;
    new_pad = Pads.get();

    font.print(220, 25, "\nAthena project: Controls demo\n");
    font.print(100, 370, "\nTips:\n");
    font.print(100, 390, "\nPress R2+L2 to start rumble and press again to stop it.\n");
    font.print(100, 405, "\nButtons transparency varies with the pressure applied to them.\n");
    font.print(100, 420, "\nPress Start+R3+Pad-Up to exit this demo.\n");

    pad_select.color = Color.new(128, 128, 128, (Pads.check(new_pad, PAD_SELECT)? 128 : 60));
    pad_select.draw(260.0, 190.0);

    start.color = Color.new(128, 128, 128, (Pads.check(new_pad, PAD_START)? 128 : 60));
    start.draw(380.0, 190.0);

    up.color = Color.new(128, 128, 128, (Pads.check(new_pad, PAD_UP)? Pads.getPressure(PAD_UP) : 60));
    up.draw(120.0, 155.0);

    down.color = Color.new(128, 128, 128, (Pads.check(new_pad, PAD_DOWN)? Pads.getPressure(PAD_DOWN) : 60));
    down.draw(120.0, 225.0);

    left.color = Color.new(128, 128, 128, (Pads.check(new_pad, PAD_LEFT)? Pads.getPressure(PAD_LEFT) : 60));
    left.draw(85.0, 190.0);

    right.color = Color.new(128, 128, 128, (Pads.check(new_pad, PAD_RIGHT)? Pads.getPressure(PAD_RIGHT) : 60));
    right.draw(155.0, 190.0);

    triangle.color = Color.new(128, 128, 128, (Pads.check(new_pad, PAD_TRIANGLE)? Pads.getPressure(PAD_TRIANGLE) : 60));
    triangle.draw(520.0, 155.0);

    cross.color = Color.new(128, 128, 128, (Pads.check(new_pad, PAD_CROSS)? Pads.getPressure(PAD_CROSS) : 60));
    cross.draw(520.0, 225.0);

    square.color = Color.new(128, 128, 128, (Pads.check(new_pad, PAD_SQUARE)? Pads.getPressure(PAD_SQUARE) : 60));
    square.draw(485.0, 190.0);

    circle.color = Color.new(128, 128, 128, (Pads.check(new_pad, PAD_CIRCLE)? Pads.getPressure(PAD_CIRCLE) : 60));
    circle.draw(555.0, 190.0);

    l1.color = Color.new(128, 128, 128, (Pads.check(new_pad, PAD_L1)? Pads.getPressure(PAD_L1) : 60));
    l1.draw(102.0, 100.0);

    r1.color = Color.new(128, 128, 128, (Pads.check(new_pad, PAD_R1)? Pads.getPressure(PAD_R1) : 60));
    r1.draw(502.0, 100.0);

    l2.color = Color.new(128, 128, 128, (Pads.check(new_pad, PAD_L2)? Pads.getPressure(PAD_L2) : 60));
    l2.draw(137.0, 100.0);

    r2.color = Color.new(128, 128, 128, (Pads.check(new_pad, PAD_R2)? Pads.getPressure(PAD_R2) : 60));
    r2.draw(537.0, 100.0);
    
    l3.color = Color.new(128, 128, 128, (Pads.check(new_pad, PAD_L3)? 128 : 60));
    l3.draw(new_pad.lx/4+242.0, new_pad.ly/4+300.0);

    r3.color = Color.new(128, 128, 128, (Pads.check(new_pad, PAD_R3)? 128 : 60));
    r3.draw(new_pad.rx/4+402.0, new_pad.ry/4+300.0);

    drawRect(220.0, 280.0, 75, 75, Color.new(128, 128, 128, 32))
    drawRect(380.0, 280.0, 75, 75, Color.new(128, 128, 128, 32))

    if((Pads.check(new_pad, PAD_R2) && Pads.check(new_pad, PAD_L2)) && !(Pads.check(old_pad, PAD_R2) && Pads.check(old_pad, PAD_L2))){
            rumble? Pads.rumble(0 ,0) : Pads.rumble(1 ,255);
            rumble ^= 1;
    }

    if(Pads.check(new_pad, PAD_START) && Pads.check(new_pad, PAD_R3) && Pads.check(new_pad, PAD_UP)){
        break;
    }
    
    Display.flip();
}

circle = null;
cross = null;
square = null;
triangle = null;
up = null; 
down = null;
left = null;
right = null;
start = null;
pad_select = null;
r1 = null; 
r2 = null; 
l1 = null; 
l2 = null; 
l3 = null; 
r3 = null;
rumble = null; 
font = null;
new_pad = null;
old_pad = null;

Duktape.gc();