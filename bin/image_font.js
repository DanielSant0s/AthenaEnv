// {"name": "DejaVu Font Demo", "author": "Daniel Santos", "version": "11292025", "file": "image_font.js"}

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
    font.outline = 0;
    font.dropshadow = 0;
    font.print(10, y_offset, "DejaVu Bitmap Font Demo");
    y_offset += line_height;

    font.scale = 1.0f;
    font.color = Color.new(255, 255, 0, 128);
    font.outline = 2.0f;
    font.outline_color = Color.new(0, 0, 0, 128);
    font.print(10, y_offset, "Text with black outline");
    y_offset += line_height;

    font.color = Color.new(255, 255, 255, 128);
    font.outline = 1.5f;
    font.outline_color = Color.new(255, 0, 0, 128);
    font.print(10, y_offset, "White text with red outline");
    y_offset += line_height;

    font.outline = 0;

    font.scale = 1.0f;
    font.color = Color.new(0, 255, 255, 128);
    font.dropshadow = 3.0f;
    font.dropshadow_color = Color.new(0, 0, 0, 80);
    font.print(10, y_offset, "Cyan text with drop shadow");
    y_offset += line_height;

    font.color = Color.new(255, 128, 0, 128);
    font.dropshadow = 2.0f;
    font.dropshadow_color = Color.new(64, 32, 0, 100);
    font.print(10, y_offset, "Orange with brown shadow");
    y_offset += line_height;

    font.dropshadow = 0;

    font.scale = 0.8f;
    font.color = Color.new(200, 200, 200, 128);
    
    font.align = Font.ALIGN_LEFT;
    font.print(10, y_offset, "Left aligned (starts at x)");
    y_offset += line_height;

    font.align = Font.ALIGN_HCENTER;
    font.print(320, y_offset, "Center aligned (centered at x)");
    y_offset += line_height;

    font.align = Font.ALIGN_RIGHT;
    font.print(630, y_offset, "Right aligned (ends at x)");
    y_offset += line_height;

    font.align = Font.ALIGN_LEFT;

    font.scale = 1.2f;
    font.color = Color.new(255, 255, 255, 128);
    font.outline = 2.0f;
    font.outline_color = Color.new(128, 0, 255, 128);
    font.align = Font.ALIGN_HCENTER;
    font.print(320, y_offset, "Combined: Outline + Center");
    y_offset += line_height * 1.5f;

    font.outline = 0;
    font.dropshadow = 2.5f;
    font.dropshadow_color = Color.new(0, 0, 0, 100);
    font.color = Color.new(0, 255, 128, 128);
    font.print(320, y_offset, "Shadow + Center Align");
    y_offset += line_height * 1.5f;

    font.dropshadow = 0;
    font.align = Font.ALIGN_LEFT;

    const test_text = "Text Size Test";
    const text_size = font.getTextSize(test_text);
    font.scale = 1.0f;
    font.color = Color.new(128, 255, 128, 128);
    font.print(10, y_offset, test_text);
    y_offset += line_height;

    font.scale = 0.7f;
    font.color = Color.new(128, 128, 128, 128);
    font.print(10, y_offset, `Size: ${text_size.width}x${text_size.height} pixels`);

    font.scale = 0.6f;
    font.color = Color.new(128, 128, 128, 128);
    font.print(10, 440, "Press TRIANGLE to return to menu");

    Screen.flip();
}
