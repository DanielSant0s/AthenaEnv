// {"name": "2D Draw", "author": "Daniel Santos", "version": "02282025", "icon": "render_icon.png", "file": "prim_draw.js"}

let pad = Pads.get();

const pixelArray = [
    {x: 10, y: 10, color: Color.new(255, 0, 0)},
    {x: 11, y: 10, color: Color.new(255, 165, 0)},
    {x: 12, y: 10, color: Color.new(255, 255, 0)},
    {x: 13, y: 10, color: Color.new(0, 255, 0)},
    {x: 14, y: 10, color: Color.new(0, 255, 165)},
    {x: 15, y: 10, color: Color.new(0, 255, 255)},
    {x: 16, y: 10, color: Color.new(0, 0, 255)},
    {x: 17, y: 10, color: Color.new(128, 0, 255)},
    {x: 18, y: 10, color: Color.new(255, 0, 255)},
];

while(true) {
    Screen.clear(Color.new(255, 255, 255));
    pad.update();

    if(pad.justPressed(Pads.TRIANGLE)) {
        std.reload("main.js");
    }

    pixelArray.forEach(point => Draw.point(point.x, point.y, point.color));

    Draw.line(220, 200, 270, 250, Color.new(0, 255, 0));

    Draw.rect(300, 204, 40, 40, Color.new(255, 0, 0));

    Draw.triangle(400, 400, 400, 430, 430, 400, Color.new(0, 0, 255)); // flat triangle

    Draw.triangle(400, 300, 400, 330, 430, 300, Color.new(255, 0, 0), Color.new(0, 255, 0), Color.new(0, 0, 255)); // gouraud triangle

    Draw.quad(450, 200, 450, 230, 480, 200, 480, 230, Color.new(128, 0, 255)); 

    Draw.quad(450, 300, 450, 330, 480, 300, 480, 330, Color.new(255, 0, 0), Color.new(0, 255, 0), Color.new(0, 0, 255), Color.new(255, 255, 0));  // gouraud quad

    Screen.flip();
}