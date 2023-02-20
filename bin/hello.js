var sysfnt = new Font();

var fntcpy = new Font();
fntcpy.scale = (0.4);

var dejavu = new Font("dejavu.bmp");
dejavu.scale = (2.0);

var mine = new Font("minecraft.ttf");
mine.scale = (1.0);

var changed_colors = false;
var changed_sizes = false;

for(var i = 0; i < 1250; i++){
    Screen.clear();
    fntcpy.print(10, 10, Screen.getFPS(360) + " FPS");
    fntcpy.print(10, 25, "Don't worry! This is a timed demo.");

    sysfnt.print(10, 50, "Hello World! - Default PS2 Font");
    dejavu.print(10, 90, "Hello World! - BMP Dejavu Font");
    mine.print(10, 130, "Hello World! - TTF Minecraft Font");

    if(i > 333){
        fntcpy.print(10, 400, "Changed font colors");
        if(!changed_colors){
            sysfnt.color = Color.new(255, 0, 0);
            dejavu.color = Color.new(0, 255, 0);
            mine.color = Color.new(0, 0, 255);
            changed_colors = true;
        }
    }

    if(i > 666){
        fntcpy.print(10, 380, "Changed font sizes");
        if(!changed_sizes){
            sysfnt.scale = (0.4);
            dejavu.scale = (1.5);
            mine.scale = (1.5);
            changed_sizes = true;
        }

    }

    fntcpy.print(10, 420, i + " out of 1250 until the end of this demo");
    Screen.flip();
}

dejavu = null;
mine = null;
