
import { AppUI, exitToShell } from 'athenaos/app.js';

const pad = Pads.get();

const font = new Font("dejavu.bmp");

const ui = new AppUI({ title: "DejaVu Font Demo", category: "2d" });

Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);

const line_height = 27;

while(true) {
    pad.update();

    if(ui.homeRequested(pad)) {
        break;
    }

    ui.background();
    const left = ui.content.x, right = ui.content.x + ui.content.w, center = ui.W / 2;
    let y_offset = ui.content.y;

    font.scale = 1.0f;
    font.color = Color.new(255, 255, 255, 128);
    font.outline = 0;
    font.dropshadow = 0;
    font.print(left, y_offset, "DejaVu Bitmap Font Demo");
    y_offset += line_height;

    font.scale = 1.0f;
    font.color = Color.new(255, 255, 0, 128);
    font.outline = 2.0f;
    font.outline_color = Color.new(0, 0, 0, 128);
    font.print(left, y_offset, "Text with black outline");
    y_offset += line_height;

    font.color = Color.new(255, 255, 255, 128);
    font.outline = 1.5f;
    font.outline_color = Color.new(255, 0, 0, 128);
    font.print(left, y_offset, "White text with red outline");
    y_offset += line_height;

    font.outline = 0;

    font.scale = 1.0f;
    font.color = Color.new(0, 255, 255, 128);
    font.dropshadow = 3.0f;
    font.dropshadow_color = Color.new(0, 0, 0, 80);
    font.print(left, y_offset, "Cyan text with drop shadow");
    y_offset += line_height;

    font.color = Color.new(255, 128, 0, 128);
    font.dropshadow = 2.0f;
    font.dropshadow_color = Color.new(64, 32, 0, 100);
    font.print(left, y_offset, "Orange with brown shadow");
    y_offset += line_height;

    font.dropshadow = 0;

    font.scale = 0.8f;
    font.color = Color.new(200, 200, 200, 128);

    font.align = Font.ALIGN_LEFT;
    font.print(left, y_offset, "Left aligned (starts at x)");
    y_offset += line_height;

    font.align = Font.ALIGN_HCENTER;
    font.print(center, y_offset, "Center aligned (centered at x)");
    y_offset += line_height;

    font.align = Font.ALIGN_RIGHT;
    font.print(right, y_offset, "Right aligned (ends at x)");
    y_offset += line_height;

    font.align = Font.ALIGN_LEFT;

    font.scale = 1.2f;
    font.color = Color.new(255, 255, 255, 128);
    font.outline = 2.0f;
    font.outline_color = Color.new(128, 0, 255, 128);
    font.align = Font.ALIGN_HCENTER;
    font.print(center, y_offset, "Combined: Outline + Center");
    y_offset += line_height * 1.4f;

    font.outline = 0;
    font.dropshadow = 2.5f;
    font.dropshadow_color = Color.new(0, 0, 0, 100);
    font.color = Color.new(0, 255, 128, 128);
    font.print(center, y_offset, "Shadow + Center Align");
    y_offset += line_height * 1.4f;

    font.dropshadow = 0;
    font.align = Font.ALIGN_LEFT;

    const test_text = "Text Size Test";
    const text_size = font.getTextSize(test_text);
    font.scale = 1.0f;
    font.color = Color.new(128, 255, 128, 128);
    font.print(left, y_offset, test_text);

    font.scale = 0.7f;
    font.color = Color.new(128, 128, 128, 128);
    font.print(left + 220, y_offset + 4, `Size: ${text_size.width}x${text_size.height} pixels`);

    ui.chrome();
    Screen.flip();
}

exitToShell();
