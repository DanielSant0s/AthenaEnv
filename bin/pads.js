// {"name": "Gamepad tester", "author": "Daniel Santos", "version": "04072023", "icon": "pads_icon.png", "file": "pads.js"}

const canvas = Screen.getMode();

//canvas.psm = CT24;
//canvas.zbuffering = false;
//canvas.double_buffering = false;

//Screen.setMode(canvas);

Screen.setVSync(false);
Screen.setFrameCounter(true);

const dark_gray = Color.new(128, 128, 128, 32);
const opaque = Color.new(128, 128, 128, 128);
const translucent = Color.new(128, 128, 128, 60);

const font = new Font("fonts/minecraft.ttf");
font.color = Color.new(128, 0, 255);
font.scale = Math.fround(0.4);

const circle = new Image("pads/circle.png", VRAM);
const cross = new Image("pads/cross.png", VRAM);
const square = new Image("pads/square.png", VRAM);
const triangle = new Image("pads/triangle.png", VRAM);
const up = new Image("pads/up.png", VRAM);
const down = new Image("pads/down.png", VRAM);
const left = new Image("pads/left.png", VRAM);
const right = new Image("pads/right.png", VRAM);
const start = new Image("pads/start.png", VRAM);
const pad_select = new Image("pads/select.png", VRAM);
const r1 = new Image("pads/r1.png", VRAM);
const r2 = new Image("pads/r2.png", VRAM);
const l1 = new Image("pads/l1.png", VRAM);
const l2 = new Image("pads/l2.png", VRAM);
const l3 = new Image("pads/l3.png", VRAM);
const r3 = new Image("pads/r3.png", VRAM);

var rumble = false;
var pad = Pads.get(0);

Screen.display(() => {
    pad.update();

    font.print(220, 25, "\nAthena project: Controls demo\n");
    font.print(100, 370, "\nTips:\n");
    font.print(100, 390, "\nPress R2+L2 to start rumble and press again to stop it.\n");
    font.print(100, 405, "\nButtons transparency varies with the pressure applied to them.\n");
    font.print(100, 420, "\nPress Start+R3+Pad-Up to exit this demo.\n");

    pad_select.color = ((pad.btns & Pads.SELECT)? opaque : translucent);
    pad_select.draw(260.0f, 190.0f);

    start.color = ((pad.btns & Pads.START)? opaque : translucent);
    start.draw(380.0f, 190.0f);
    
    up.color = Color.new(128, 128, 128, ((pad.btns & Pads.UP)? Pads.getPressure(Pads.UP) : 60));
    up.draw(120.0f, 155.0f);

    down.color = Color.new(128, 128, 128, ((pad.btns & Pads.DOWN)? Pads.getPressure(Pads.DOWN) : 60));
    down.draw(120.0f, 225.0f);

    left.color = Color.new(128, 128, 128, ((pad.btns & Pads.LEFT)? Pads.getPressure(Pads.LEFT) : 60));
    left.draw(85.0f, 190.0f);

    right.color = Color.new(128, 128, 128, ((pad.btns & Pads.RIGHT)? Pads.getPressure(Pads.RIGHT) : 60));
    right.draw(155.0f, 190.0f);

    triangle.color = Color.new(128, 128, 128, ((pad.btns & Pads.TRIANGLE)? Pads.getPressure(Pads.TRIANGLE) : 60));
    triangle.draw(520.0f, 155.0f);

    cross.color = Color.new(128, 128, 128, ((pad.btns & Pads.CROSS)? Pads.getPressure(Pads.CROSS) : 60));
    cross.draw(520.0f, 225.0f);

    square.color = Color.new(128, 128, 128, ((pad.btns & Pads.SQUARE)? Pads.getPressure(Pads.SQUARE) : 60));
    square.draw(485.0f, 190.0f);

    circle.color = Color.new(128, 128, 128, ((pad.btns & Pads.CIRCLE)? Pads.getPressure(Pads.CIRCLE) : 60));
    circle.draw(555.0f, 190.0f);

    l1.color = Color.new(128, 128, 128, ((pad.btns & Pads.L1)? Pads.getPressure(Pads.L1) : 60));
    l1.draw(102.0f, 100.0f);

    r1.color = Color.new(128, 128, 128, ((pad.btns & Pads.R1)? Pads.getPressure(Pads.R1) : 60));
    r1.draw(502.0f, 100.0f);

    l2.color = Color.new(128, 128, 128, ((pad.btns & Pads.L2)? Pads.getPressure(Pads.L2) : 60));
    l2.draw(137.0f, 100.0f);

    r2.color = Color.new(128, 128, 128, ((pad.btns & Pads.R2)? Pads.getPressure(Pads.R2) : 60));
    r2.draw(537.0f, 100.0f);
    
    l3.color = ((pad.btns & Pads.L3)? opaque : translucent);
    l3.draw(pad.lx/4.0f+242.0f, pad.ly/4.0f+300.0f);

    r3.color = ((pad.btns & Pads.R3)? opaque : translucent);
    r3.draw(pad.rx/4.0f+402.0f, pad.ry/4.0f+300.0f);

    Draw.rect(220.0f, 280.0f, 75, 75, dark_gray);
    Draw.rect(380.0f, 280.0f, 75, 75, dark_gray);

    if(((pad.btns & Pads.R2) && (pad.btns & Pads.L2)) && !((pad.old_btns & Pads.R2) && !(pad.old_btns & Pads.L2))){
        rumble? Pads.rumble(0 ,0) : Pads.rumble(1 ,255);
        rumble ^= 1;
    }

    if((pad.btns & Pads.START) && (pad.btns & Pads.R3) && (pad.btns & Pads.UP)){
        System.loadELF(System.boot_path + "athena_pkd.elf");
    }

    font.print(10, 10, Screen.getFPS(360) + " FPS");
});
