
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
        console.log("Width: " + wallpaper.width + "\n");
    }

    Display.flip();
}

wallpaper = null;
img_list = null;
Duktape.gc();

console.log("Finished!\n");

while(true){
    continue;
}

