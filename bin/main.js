IOP.loadDefaultModule(IOP.mouse);
Mouse.init();

let bg = new Image("dash/bg.png");

let cursor = new Image("cursor/pointer.png");
cursor.width /= 4;
cursor.height /= 4;

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

    if (System.doesFileExist(metadata.icon)) {
        metadata.icon = new Image(metadata.icon);
    } else {
        metadata.icon = no_icon;
    }

    return metadata;
} ).filter(gen_app => gen_app.name != "%not_app%");

let menu_ptr = 0;

let new_pad = Pads.get();
let old_pad = new_pad;

let mouse = Mouse.get();

while(true) {
    old_pad = new_pad;
    new_pad = Pads.get();
    mouse = Mouse.get();

    Screen.clear();

    bg.draw(0, 0);

    // cursor.draw(mouse.x, mouse.y);

    font_bold.print(15, 5, "Athena dash alpha v0.1");

    if(Pads.check(new_pad, Pads.UP) && !Pads.check(old_pad, Pads.UP)) {
        app_table.unshift(app_table.pop());

    }

    if(Pads.check(new_pad, Pads.DOWN) && !Pads.check(old_pad, Pads.DOWN)){
        app_table.push(app_table.shift());
    }

    if(Pads.check(new_pad, Pads.CROSS) && !Pads.check(old_pad, Pads.CROSS)){
        System.loadELF(System.currentDir() + "athena_pkd.elf", [System.currentDir() + app_table[0].file]); // Doing this to reset all the stuff
    }

    font_medium.print(210, 125, app_table[0].name);
    app_table[0].icon.draw(85, 111);

    for(let i = 1; i < (app_table.length < 10? app_table.length : 10); i++) {
        font.print(210, 125+(23*i), app_table[i].name);
    }

    Screen.flip();
}
