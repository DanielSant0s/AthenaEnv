
var img_list = new AsyncImage();

var wallpaper = new Image("owl.png", VRAM, img_list.handle);

img_list.process();

wallpaper.width = 512.0;
wallpaper.height = 256.0;
wallpaper.filter = LINEAR;

for(var i = 0; i < 1000; i++){
    Display.clear();

    if(wallpaper.isLoaded()) {
        wallpaper.draw(0.0, 0.0);
    }

    console.log("Counter: " + i + "\n");

    Display.flip();
}

wallpaper = null;
Duktape.gc();

console.log("Finished!\n");

while(true){
    continue;
}

