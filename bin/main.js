console.log(System.getFreeMemory() + "\n");

var osdsys_font = new Font();
osdsys_font.color = Color.new(255, 0, 0);

var dejavu_font = new Font("dejavu.bmp");
dejavu_font.color = Color.new(0, 255, 0);
dejavu_font.scale = 2.0;

var antihero_font = new Font("minecraft.ttf");
antihero_font.color = Color.new(0, 0, 255);
antihero_font.scale = 3.0;

var p1 = new Point(0.0, 400.0, Color.new(255, 0, 0));
var l1 = new Line(20.0, 20.0, 150.0, 448.0, Color.new(64, 0, 128));
var t1 = new Triangle(29.0, 96.0, 120.0, 10.0, 170, 150.0, Color.new(0, 128, 0));
var t2 = new Triangle(150.0, 20.0, 150.0, 100.0, 250.0, 150.0, Color.new(0, 128, 0), Color.new(128, 0, 0), Color.new(0, 0, 128));
var q1 = new Quad(550.0, 100.0, 350.0, 100.0, 420.0, 350.0, 220.0, 420.0, Color.new(0, 128, 0), Color.new(128, 0, 0), Color.new(0, 0, 128), Color.new(64, 0, 128));
var r1 = new Rect(200.0, 200.0, 50, 50, Color.new(255, 150, 0));
var c1 = new Circle(300.0, 300.0, 50, Color.new(128, 0, 255));

console.log(System.getFreeMemory() + "\n");

//var img_list = new ImageList();

var wallpaper = new Image("owl.png", RAM); //TODO: ASYNC LOADING IS COMPLETELY FUCKED

//img_list.process();

wallpaper.filter = LINEAR;

console.log("Free VRAM: " + Display.getFreeVRAM() + "\n");

for(var i = 0; i < 10000; i++){
    Display.clear();

    for(var j = 0.0; j < 640.0; j++){
        p1.x = j;
        p1.draw();
    }

    for(var j = 0.0, k = 0.0; j < 448.0 && k < 640.0; j++, k++){
        new Point(k, j).draw();
    }

    if(wallpaper.ready()) {
        wallpaper.width = 512.0;
        wallpaper.height = 256.0;
        wallpaper.draw(0.0, 0.0);
        dejavu_font.print(10, 10, "Counter: " + i);
        osdsys_font.print(10, 50, "Free memory: " + System.getFreeMemory());
        antihero_font.print(10, 120, "Counter: " + i);
    }

    l1.draw();
    t1.draw();
    t2.draw();
    q1.draw();
    r1.draw();
    c1.draw();
    
    Display.flip();
}

wallpaper = null;
//img_list = null;
osdsys_font = null;
dejavu_font = null;
antihero_font = null;
Duktape.gc();

console.log("Finished!\n");

while(true){
    continue;
}

