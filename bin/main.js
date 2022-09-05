
var wallpaper = new Image("owl.png");

wallpaper.width = 512.0;
wallpaper.height = 256.0;
wallpaper.filter = LINEAR;

for(var i = 0; i < 1000; i++){
    Display.clear();

    wallpaper.draw(0.0, 0.0);

    console.log("Counter: " + i + "\n");

    Display.flip();
}

wallpaper = null;
Duktape.gc();

while(true){
    continue;
}

