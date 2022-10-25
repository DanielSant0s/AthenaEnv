function randint(min, max) {
    min = Math.ceil(min);
    max = Math.floor(max);
    return Math.floor(Math.random() * (max - min) + min);
}

console.log(System.getFreeMemory() + "\n");

var osdsys_font = new Font();
osdsys_font.color = Color.new(255, 0, 0);

var dejavu_font = new Font("dejavu.bmp");
dejavu_font.color = Color.new(0, 255, 0);
dejavu_font.scale = 2.0;

var antihero_font = new Font("minecraft.ttf");
antihero_font.color = Color.new(0, 0, 255);
antihero_font.scale = 2.0;

console.log(System.getFreeMemory() + "\n");

var img_list = new ImageList();

var wallpaper = new Image("owl.png", RAM); //TODO: ASYNC LOADING IS COMPLETELY FUCKED

//img_list.process();

wallpaper.filter = LINEAR;

console.log("Free VRAM: " + Display.getFreeVRAM() + "\n");

//Display.setVSync(false);

for(var i = 0; i < 10000; i++){
    Display.clear();

    for(var j = 400.0; j < 410.0; j++){
        for(var k = 315.0; k < 325.0; k++){
            drawPoint(k, j, Color.new(randint(0, 256), randint(0, 256), randint(0, 256)));
        }
    }

    if(wallpaper.ready()) {
        wallpaper.width = 512.0;
        wallpaper.height = 256.0;
        wallpaper.draw(0.0, 0.0);
    }

    drawLine(20.0, 20.0, 150.0, 448.0, Color.new(64, 0, 128));
    drawTriangle(29.0, 96.0, 120.0, 10.0, 170, 150.0, Color.new(0, 128, 0));
    drawTriangle(150.0, 20.0, 150.0, 100.0, 250.0, 150.0, Color.new(0, 128, 0), Color.new(128, 0, 0), Color.new(0, 0, 128));
    drawQuad(550.0, 100.0, 350.0, 100.0, 420.0, 350.0, 220.0, 420.0, Color.new(0, 128, 0), Color.new(128, 0, 0), Color.new(0, 0, 128), Color.new(64, 0, 128));
    drawRect(200.0, 200.0, 50, 50, Color.new(255, 150, 0));
    drawCircle(300.0, 300.0, 50, Color.new(128, 0, 255));

    dejavu_font.print(10, 10, "Counter: " + i);
    osdsys_font.print(10, 50, "Free memory: " + System.getFreeMemory());
    osdsys_font.print(400, 10, Display.getFPS() + " FPS");
    antihero_font.print(10, 120, "Counter: " + i);
    
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

