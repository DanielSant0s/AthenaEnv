// {"name": "DejaVu Font Demo", "author": "Daniel Santos", "version": "11192025", "file": "image_font.js"}

const pad = Pads.get();

const font = new Font("dejavu.bmp");

Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);

let y_offset = 20;
const line_height = 30;

while(true) {
    Screen.clear();
    pad.update();

    if(pad.justPressed(Pads.TRIANGLE)) {
        std.reload("main.js");
    }

    y_offset = 20;

    font.scale = 1.0f;
    font.color = Color.new(255, 255, 255, 128);
    font.print(10, y_offset, "DejaVu Bitmap Font Demo");
    y_offset += line_height;

    font.scale = 0.5f;
    font.color = Color.new(200, 200, 200, 128);
    font.print(10, y_offset, "Small text (scale 0.5)");
    y_offset += line_height;

    font.scale = 1.5f;
    font.color = Color.new(255, 255, 0, 128);
    font.print(10, y_offset, "Large text (scale 1.5)");
    y_offset += line_height * 2;

    font.scale = 1.0f;
    font.color = Color.new(255, 0, 0, 128);
    font.print(10, y_offset, "Red text");
    y_offset += line_height;

    font.color = Color.new(0, 255, 0, 128);
    font.print(10, y_offset, "Green text");
    y_offset += line_height;

    font.color = Color.new(0, 0, 255, 128);
    font.print(10, y_offset, "Blue text");
    y_offset += line_height;

    font.color = Color.new(255, 255, 0, 128);
    font.print(10, y_offset, "Yellow text");
    y_offset += line_height;

    font.color = Color.new(255, 0, 255, 128);
    font.print(10, y_offset, "Magenta text");
    y_offset += line_height;

    font.color = Color.new(0, 255, 255, 128);
    font.print(10, y_offset, "Cyan text");
    y_offset += line_height * 2;

    font.color = Color.new(255, 255, 255, 128);
    font.print(10, y_offset, "Multi-line text:\nLine 1\nLine 2\nLine 3");
    y_offset += line_height * 4;

    font.scale = 0.8f;
    font.color = Color.new(200, 200, 200, 128);
    font.print(10, y_offset, "ASCII: !\"#$%&'()*+,-./0123456789:;<=>?");
    y_offset += line_height;

    font.print(10, y_offset, "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_");
    y_offset += line_height;

    font.print(10, y_offset, "`abcdefghijklmnopqrstuvwxyz{|}~");
    y_offset += line_height * 2;

    const test_text = "Text Size Test";
    const text_size = font.getTextSize(test_text);
    font.scale = 1.0f;
    font.color = Color.new(128, 255, 128, 128);
    font.print(10, y_offset, test_text);
    y_offset += line_height;

    font.scale = 0.7f;
    font.color = Color.new(128, 128, 128, 128);
    font.print(10, y_offset, `Size: ${text_size.width}x${text_size.height} pixels`);
    y_offset += line_height * 2;

    font.scale = 0.6f;
    font.color = Color.new(128, 128, 128, 128);
    font.print(10, 440, "Press TRIANGLE to return to menu");

    Screen.flip();
}

