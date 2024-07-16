IOP.loadDefaultModule(IOP.keyboard);
Keyboard.init();

let bg = new Image("dash/bg.png");

const bg_palette = new Uint8Array(bg.palette);

for (let i = 0; i < bg_palette.length; i += 4) {
    bg_palette[i+0] = Math.trunc(bg_palette[i+0] * 0.2f);
    bg_palette[i+1] = Math.trunc(bg_palette[i+1] * 0.0f);
    bg_palette[i+2] = Math.trunc(bg_palette[i+2] * 1.0f);
}

const unsel_color = Color.new(255, 255, 255, 64);
const sel_color = Color.new(255, 255, 255);

let font = new Font("fonts/LEMONMILK-Light.otf");
let font_medium = new Font("fonts/LEMONMILK-Medium.otf");
let font_bold = new Font("fonts/LEMONMILK-Bold.otf");
font.color = unsel_color;
font_bold.scale = 0.7f
font_medium.scale = 1.0f;
font.scale = 0.44f;

let no_icon = new Image("no_icon.png");

let app_table = System.listDir().map(file => file.name).filter(str => str.endsWith(".js")).map( app => {
    const app_fd = std.open(app, "r");
    const metadata_str = app_fd.getline().replace("// ", "");
    app_fd.close();

    if (metadata_str[0] != "{") {
        return {name: "%not_app%"};
    }

    let metadata = JSON.parse(metadata_str);

    if (std.exists(metadata.icon)) {
        metadata.icon = new Image(metadata.icon);
    } else {
        metadata.icon = no_icon;
    }

    return metadata;
} ).filter(gen_app => gen_app.name != "%not_app%");

let menu_ptr = 0;

let pad = Pads.get();

let old_kbd_char = 0;
let kbd_char = 0;

const VK_OLD_UP = 27;
const VK_NEW_UP = 44;
const VK_OLD_DOWN = 27;
const VK_NEW_DOWN = 43;
const VK_RETURN = 10;

const ee_info = System.getCPUInfo();

let mem = undefined;

os.setInterval(() => {
    pad.update();

    old_kbd_char = kbd_char;
    kbd_char = Keyboard.get();

    Screen.clear();

    bg.draw(0, 0);

    font_bold.print(15, 5, "Athena dash");

    mem = System.getMemoryStats();

    font.print(15, 420, `Temp: ${System.getTemperature() === undefined? "NaN" : System.getTemperature()} C | RAM Usage: ${Math.floor(mem.used / 1024)}KB / ${Math.floor(ee_info.RAMSize / 1024)}KB`);

    if(pad.justPressed(Pads.UP) || old_kbd_char == VK_OLD_UP && kbd_char == VK_NEW_UP) {
        app_table.unshift(app_table.pop());

    }

    if(pad.justPressed(Pads.DOWN) || old_kbd_char == VK_OLD_DOWN && kbd_char == VK_NEW_DOWN){
        app_table.push(app_table.shift());
    }

    if(pad.justPressed(Pads.CROSS) || kbd_char == VK_RETURN){
        let bin = "athena.elf";
        if ("bin" in app_table[0]) {
            bin = app_table[0].bin;
        }

        System.loadELF(System.boot_path + "/" + bin, [app_table[0].file, ]); // Doing this to reset all the stuff
    }

    font_medium.print(210, 125, app_table[0].name);
    app_table[0].icon.draw(85, 111);

    for(let i = 1; i < (app_table.length < 10? app_table.length : 10); i++) {
        font.print(210, 125+(23*i), app_table[i].name);
    }
    
    Screen.flip();
}, 0);