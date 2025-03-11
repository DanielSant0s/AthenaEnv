// {"name": "Hello world", "author": "Daniel Santos", "version": "03112025", "file": "hello.js"}

const pad = Pads.get();

const font = new Font("default");

while(true) {
    Screen.clear(Color.new(0, 0, 0));
    pad.update();

    if(pad.justPressed(Pads.TRIANGLE)) {
        std.reload("main.js");
    }

    font.scale = 1.0f;

    font.print(10, 10, "Hello World!");

    font.color = Color.new(128, 0, 0);
    font.print(10, 40, "Hello World(with custom color)!");

    font.color = Color.new(128, 128, 128);
    font.outline = 1.0f;
    font.outline_color = Color.new(128, 0, 0);
    font.print(10, 70, "Hello World(with outline)!");

    font.outline = 0.0f;
    font.outline_color = Color.new(0, 0, 0);

    font.color = Color.new(128, 128, 128);
    font.dropshadow = 1.0f;
    font.dropshadow_color = Color.new(128, 0, 0);
    font.print(10, 100, "Hello World(with drop shadow)!");

    font.dropshadow = 0.0f;
    font.dropshadow_color = Color.new(0, 0, 0);

    // lets draw it smaller
    font.scale = 0.6f;

    font.print(10, 130, "Hello World!");

    font.color = Color.new(128, 0, 0);
    font.print(10, 150, "Hello World(with custom color)!");

    font.color = Color.new(128, 128, 128);
    font.outline = 1.0f;
    font.outline_color = Color.new(128, 0, 0);
    font.print(10, 170, "Hello World(with outline)!");

    font.outline = 0.0f;
    font.outline_color = Color.new(0, 0, 0);

    font.color = Color.new(128, 128, 128);
    font.dropshadow = 1.0f;
    font.dropshadow_color = Color.new(128, 0, 0);
    font.print(10, 190, "Hello World(with drop shadow)!");

    font.dropshadow = 0.0f;
    font.dropshadow_color = Color.new(0, 0, 0);

    Screen.flip();
}