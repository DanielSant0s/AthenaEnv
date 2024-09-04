import { Element, Component, Menu, MenuList, Interface } from 'UI.js'

const unsel_color = Color.new(255, 255, 255, 64);
const sel_color = Color.new(255, 255, 255);

const font = new Font("fonts/LEMONMILK-Light.otf");
const font_medium = new Font("fonts/LEMONMILK-Medium.otf");
const font_bold = new Font("fonts/LEMONMILK-Bold.otf");
font.color = unsel_color;
font_bold.scale = 0.7f
font_medium.scale = 1.0f;
font.scale = 0.44f;

let exit_to = null;

const background = new Component([
    new Element( 
        (ctx) => { 
            ctx.bg = new Image("dash/bg.png"); 

            const bg_palette = new Uint8Array(ctx.bg.palette);

            for (let i = 0; i < bg_palette.length; i += 4) {
                bg_palette[i+0] = Math.trunc(bg_palette[i+0] * 0.2f);
                bg_palette[i+1] = Math.trunc(bg_palette[i+1] * 0.0f);
                bg_palette[i+2] = Math.trunc(bg_palette[i+2] * 1.0f);
            }
        }, 
        (ctx) => { 
            ctx.bg.draw(0, 0) 
        }, 
        (ctx, pad) => {}
    )
]);

const stats = new Component([
    new Element( 
        (ctx) => { 
            ctx.mem = undefined;
            ctx.ee_info = System.getCPUInfo();
        }, 
        (ctx) => { 
            ctx.mem = System.getMemoryStats();
            font.color = unsel_color;
            font.print(15, 420, `Temp: ${System.getTemperature() === undefined? "NaN" : System.getTemperature()} C | RAM Usage: ${Math.floor(ctx.mem.used / 1024)}KB / ${Math.floor(ctx.ee_info.RAMSize / 1024)}KB`);
        }, 
        (ctx, pad) => {}
    )
]);

const no_icon = new Image("no_icon.png");
const js_apps = System.listDir().map(file => file.name).filter(str => str.endsWith(".js")).map( app => {
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

const main_menu = new Menu("JS Apps", js_apps,
    (ctx) => { }, 
    (ctx) => { 
        font_medium.print(210, 125, ctx.entries[0].name);
        ctx.entries[0].icon.draw(85, 111);
    
        for(let i = 1; i < (ctx.entries.length < 10? ctx.entries.length : 10); i++) {
            font.color = unsel_color;
            font.print(210, 125+(23*i), ctx.entries[i].name);
        }
    }, 
    [(ctx, pad) => {
        if(pad.justPressed(Pads.UP)){
            ctx.entries.unshift(ctx.entries.pop());
        }
    },
    (ctx, pad) => {
        if(pad.justPressed(Pads.DOWN)){
            ctx.entries.push(ctx.entries.shift());
        }
    },
    (ctx, pad) => {
        if(pad.justPressed(Pads.CROSS)){
            exit_to = ctx.entries[0].file;
        }
    }]
);

const settings_menu = new Menu("Settings", ["Background image", "Background color", "SFX Volume"],
    (ctx) => { }, 
    (ctx) => { 
        font_medium.print(210, 125, ctx.entries[0]);
    
        for(let i = 1; i < (ctx.entries.length < 10? ctx.entries.length : 10); i++) {
            font.color = unsel_color;
            font.print(210, 125+(23*i), ctx.entries[i]);
        }
    }, 
    [(ctx, pad) => {
        if(pad.justPressed(Pads.UP)){
            ctx.entries.unshift(ctx.entries.pop());
        }
    },
    (ctx, pad) => {
        if(pad.justPressed(Pads.DOWN)){
            ctx.entries.push(ctx.entries.shift());
        }
    },
    (ctx, pad) => {
        if(pad.justPressed(Pads.CROSS)){
            //exit_to = ctx.entries[0].file;
        }
    }]
);

const menus = new MenuList([main_menu, settings_menu],
    (ctx) => {
        ctx.curr_menu = 0;
    },
    (ctx) => {
        ctx.menus.reduce((title_x, menu, index, array) => {
            font.color = (index == ctx.curr_menu? sel_color : unsel_color);
            font.print(title_x, 40, menu.title);
            return title_x + font.getTextSize(menu.title).width + 15;
        }, 85);
    },
    (ctx, pad) => {
        if(pad.justPressed(Pads.L2) && ctx.curr_menu > 0){
            ctx.curr_menu--;
        }

        if(pad.justPressed(Pads.R2) && ctx.curr_menu < ctx.menus.length-1){
            ctx.curr_menu++;
        }
    },
    (ctx, pad) => {
        ctx.menus[ctx.curr_menu].run(pad);
    }
);

const pad = Pads.get();

pad.setEventHandler();

const dashboard = new Interface(pad, [background, menus, stats]);

Screen.display(() => {
    dashboard.run();

    if (exit_to) {
        Screen.clear();
        Screen.flip();
        std.reload(exit_to); // Always run std.reload on the outer scope, otherwise it can lead to object leaking
    }
});