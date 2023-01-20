Mouse.init();
Mouse.setBoundary(0, 640, 0, 448);
Mouse.setMode(1);

let cursor = new Image("cursor/pointer.png", VRAM);
cursor.width /= 4;
cursor.height /= 4;

Keyboard.init();

let pad = Pads.get();
let oldpad = pad;
let c_x = 300;
let c_y = 300;

var sce = new Font();

let char = "";
let text = "";

let mouse = Mouse.get();

while (true){
    Screen.clear();
    oldpad = pad;
    pad = Pads.get();

    char = Keyboard.get();
    if (char != 0){
        console.log(char);
        if (char == 7){
            text = text.slice(0, -1);
        } else if (char == 12) {
            text += "\n";
        } else {
            text += String.fromCharCode(char);
        }
    }

    sce.print(20, 350, text);

    mouse = Mouse.get();

    cursor.draw(mouse.x, mouse.y);
    
    Screen.flip();
};


