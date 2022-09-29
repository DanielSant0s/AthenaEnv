
var img_list = new ImageList();

var wallpaper = new Image("owl.png", VRAM, img_list);

img_list.process();

wallpaper.width = 512.0;
wallpaper.height = 256.0;
wallpaper.filter = LINEAR;

for(var i = 0; i < 500; i++){
    Display.clear();

    if(wallpaper.ready()) {
        wallpaper.draw(0.0, 0.0);
    }

    console.log("Counter: " + i + "\n");

    Display.flip();
}

wallpaper = null;
img_list = null;
Duktape.gc();

console.log("Finished!\n");

while(true){
    continue;
}

