var osdsys_font = new Font();
osdsys_font.color = Color.new(255, 0, 0);

var dejavu_font = new Font("dejavu.bmp");
dejavu_font.color = Color.new(0, 255, 0);

var antihero_font = new Font("minecraft.ttf");
antihero_font.color = Color.new(0, 0, 255);

var img_list = new ImageList();

var wallpaper = new Image("owl.png", VRAM, img_list);

img_list.process();

wallpaper.filter = LINEAR;

for(var i = 0; i < 500; i++){
    Display.clear();

    if(wallpaper.ready()) {
        wallpaper.width = 512.0;
        wallpaper.height = 256.0;
        wallpaper.draw(0.0, 0.0);
        osdsys_font.print(10, 10, 0.6, "Width: " + wallpaper.width);
        dejavu_font.print(10, 30, 1.2, "Width: " + wallpaper.width);
        antihero_font.print(10, 50, 1.2, "Width: " + wallpaper.width);
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

