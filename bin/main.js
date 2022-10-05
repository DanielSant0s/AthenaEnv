console.log("Free memory: " + System.getFreeMemory() + "\n");

var osdsys_font = new Font();
osdsys_font.color = Color.new(255, 0, 0);

var dejavu_font = new Font("dejavu.bmp");
dejavu_font.color = Color.new(0, 255, 0);
dejavu_font.scale = 2.0;

var antihero_font = new Font("minecraft.ttf");
antihero_font.color = Color.new(0, 0, 255);
antihero_font.scale = 3.0;

var p1 = new Point(0.0, 400.0, Color.new(255, 0, 0));

var img_list = new ImageList();

var wallpaper = new Image("owl.png", RAM); //TODO: ASYNC LOADING IS COMPLETELY FUCKED

//img_list.process();

wallpaper.filter = LINEAR;

console.log("Free memory: " + System.getFreeMemory() + "\n");

for(var i = 0; i < 500; i++){
    Display.clear();

    for(var j = 0.0; j < 640.0; j += 1.0){
        p1.x = j;
        p1.draw();
    }

    if(wallpaper.ready()) {
        wallpaper.width = 512.0;
        wallpaper.height = 256.0;
        wallpaper.draw(0.0, 0.0);
        dejavu_font.print(10, 10, "Width: " + wallpaper.width);
        osdsys_font.print(10, 50, "Width: " + wallpaper.width);
        antihero_font.print(10, 120, "Width: " + wallpaper.width);
    }
    
    Display.flip();
}

wallpaper = null;
img_list = null;
osdsys_font = null;
dejavu_font = null;
antihero_font = null;
Duktape.gc();

console.log("Finished!\n");

while(true){
    continue;
}

