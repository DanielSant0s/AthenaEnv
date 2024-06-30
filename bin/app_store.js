// {"name": "App store", "author": "Daniel Santos", "version": "04102023", "file": "app_store.js"}

function load_app_db(fname) {
    let list_file = std.open(fname, "r");
    let app_list = JSON.parse(list_file.getline());
    list_file.close();
    list_file = null;

    return app_list;
}


function load_network_driver() {
    IOP.reset();

    IOP.loadDefaultModule(IOP.hdd);
    IOP.loadDefaultModule(IOP.cdfs);
    IOP.loadDefaultModule(IOP.memcard);
    IOP.loadDefaultModule(IOP.usb_mass);
    IOP.loadDefaultModule(IOP.pads);
    IOP.loadDefaultModule(IOP.network);
    
    Network.init();
}

let req = new Request();
req.keepalive = true;

const LOADING = 0;
const MAIN_MENU = 1;
const OPTIONS_MENU = 2;
const DLING_MENU = 3;

let app_state = LOADING;

const LD_INITIAL = 0;
const LD_NETWORK = 1;
const LD_PKGLIST = 2;
const LD_PKGQNTD = 3;
const LD_FADE = 4;
const LD_FINAL = 5;

let loading_state = LD_INITIAL;

const MM_INITIAL = 0;
const MM_FINAL = 1;

let mainmenu_state = MM_INITIAL;

let loading_text = "Athena package manager";

let app_list = null;

const buttons = [{inc:Pads.DOWN, dec:Pads.UP}, {inc:Pads.RIGHT, dec:Pads.LEFT}, {inc:Pads.R2, dec:Pads.L2}];

let pad = Pads.get();

const FADE_IN = 0;
const FADE_OUT = 1;

class UI {
    constructor() {
        this.y = 0;
        this.belt_img = new Image("store/belt.png");
        this.belt_img.width = 750;
        this.belt_img.height = 750;
        this.belt_img.color = Color.new(0x80, 0x80, 0x80, 0x20);

        this.font = new Font("fonts/Karla-Regular.ttf");
        this.text_alpha = 0x80;

        this.fading = false;
        this.fade = FADE_IN;

        this.timer = Timer.new();
    }

    println(str) {
        str.split('\n').forEach(text => {
            this.font.print(10, this.y, text);
            this.y += 25;
        });
    }

    fade_text(type) {
        this.fade = type;
        this.fading = true;
    }

    text_fade_process() {
        if (this.fading) {
            if (this.fade == FADE_IN && this.text_alpha > 0) {
                this.text_alpha -= 1;
            } else if(this.fade == FADE_OUT && this.text_alpha < 0x80){
                this.text_alpha += 1;
            } else {
                this.fading = false;
            }

            this.font.color = Color.new(0x80, 0x80, 0x80, this.text_alpha);
            
        }
    }

    render_belt(speed) {
        this.belt_img.angle += speed;
        this.belt_img.draw(0, 256);
    }

    update() {
        this.text_fade_process();
        Screen.flip();
        this.y = 0;
        Screen.clear(0x80101010);
    }

}

let ui = new UI();
ui.font.color = Color.new(0x80, 0x80, 0x80, 0x0);
ui.font.scale = 0.7f;
ui.text_alpha = 0;

class Menu {
    num = 0;
    num_comp = 0;
    pad_mode = 0;
    mode = 0;
    padding = 4;
    centered = false;
    list;
    backup_x;
    x;
    y;

    constructor(x, y, font, list) {
        this.x = x;
        this.y = y;
        this.list = list;
        this.font = font;
    }

    process_input(pad) {
        if(pad.justPressed(buttons[this.pad_mode].dec)) {
            this.num--;
        }
    
        if(pad.justPressed(buttons[this.pad_mode].inc)) {
            this.num++;
        }


        if(this.num >= this.list.length) {
            this.num = 0;
        } else if (this.num < 0) {
            this.num = this.list.length-1;
        }
    }

    calculate_x(idx) {
        let x = 0;

        for(let j = 0; j < idx; j++) {
            x += this.font.getTextSize(this.list[j]).width;
            x += this.padding;
        }

        return x;
    }

    center_x() {
        let x = 0;

        for(let i = 0; i < this.list.length; i++) {
            x += this.font.getTextSize(this.list[i]).width;
            if(i < this.list.length-1) {
                x += this.padding;
            }
        }

        this.x = 320-x/2;
        this.first = false;
    }

    draw(pad) {
        this.process_input(pad);
        let t_size = this.font.getTextSize(this.list[this.num]);

        for(let i = 0; i < this.list.length; i++) {
            this.font.color = Color.new(0x80, 0x80, 0x80, (i == this.num? 0x80 : 0x20));
            if(this.mode == 0) {
                this.font.print(this.x-(this.centered? this.font.getTextSize(this.list[i]).width/2 : 0), this.y+((t_size.height+this.padding)*i), this.list[i]);
            } else {
                this.font.print(this.x+this.calculate_x(i), this.y, this.list[i]);
            }
        }

    }
};


class IconMenu extends Menu {
    constructor(x, y, font, list) {
        super(x, y, font, list)
        
        let icons = [];
        for(let i = 0; i < list.length; i++) {
            let icn = new Image("store/icons/" + list[i].icon);
            icn.width = 40;
            icn.height = 40;
            icons.push(icn);
        }
        this.icons = icons;
        this.icon_padding = 5;
        this.target_size = 50;
        this.scr_limit = 6;
        this.font = font;
    }

    calculate_x(idx) {
        let x = 0;

        for(let j = 0; j < idx; j++) {
            x += this.font.getTextSize(this.list[j].name).width;
            x += this.padding;
        }

        return x;
    }

    center_x() {
        let x = 0;

        for(let i = 0; i < this.list.length; i++) {
            x += this.font.getTextSize(this.list[i].name).width;
            if(i < this.list.length-1) {
                x += this.padding;
            }
        }

        this.x = 320-x/2;
        this.first = false;
    }

    draw(pad) {
        this.process_input(pad);

        if(this.num > this.scr_limit-1){
            this.num_comp = -(this.num-(this.scr_limit-1));
        } else {
            this.num_comp = 0;
        }

        let t_size = this.font.getTextSize(this.list[this.num]);

        for(let i = 0; i < this.list.length; i++) {
            this.font.color = Color.new(0x80, 0x80, 0x80, (i == this.num? 0x80 : 0x20));
            if(i+this.num_comp < this.scr_limit && i+this.num_comp >= 0) {
                if(this.mode == 0) {
                    this.icons[i].draw(this.x, this.y+((this.target_size-this.icons[i].height)/2) + ((this.target_size+this.padding)*(i+this.num_comp)));
                    this.font.print(this.x+this.icons[i].width+this.icon_padding, this.y+(this.target_size/2-t_size.height/2)+((this.target_size+this.padding)*(i+this.num_comp)), this.list[i].name);
                } else {
                    this.font.print(this.x+this.calculate_x(i), this.y, this.list[i].name);
                }
            }

        }
    }
};

let main_menu = new Menu(320, 20, ui.font, ["Explore", "Manage", "Settings"]);
main_menu.padding = 30;
main_menu.pad_mode = 2;
main_menu.mode = 1;
main_menu.centered = true;
main_menu.center_x();

let explore_menu = undefined;

const DETAILS = 0;
const TODOWNLOAD = 1;
const DOWNLOADING = 2;
const DOWNLOADED = 3;
const EXTRACTING = 4;
const EXTRACTED = 5;
const LEAVING = 6;

let dl_state = DETAILS;
let dling_text = "";
let terminate = false;

let transfering = false;

const NOT_UPDATED = 0;
const UPDATING = 1;
const UPDATED = 2;
let update_state = NOT_UPDATED;

while(true) {
    pad.update();

    switch (app_state) {
        case LOADING:
            switch(loading_state) {
                case LD_INITIAL:
                    if(!ui.fading && ui.text_alpha != 0x80) {
                        ui.fade_text(FADE_OUT);
                    }

                    if (ui.text_alpha == 0x80) {
                        loading_text += "\n\nLoading network driver...";
                        loading_state++;
                    }

                    break;
                case LD_NETWORK:
                    loading_text += "\nLoading package list, it's gonna take a while...";
                    load_network_driver();
                    loading_state++;
                    break;
                case LD_PKGLIST:
                    if (update_state == NOT_UPDATED) {
                        req.asyncDownload("https://raw.githubusercontent.com/DanielSant0s/brewstore-db/main/brew_data.json", "brew_data.json");
                        console.log(JSON.stringify(Tasks.get()));
                        update_state = UPDATING;
                        transfering = true;
                    } else if (update_state == UPDATING) {
                        if(req.ready(5000)) {
                            transfering = false;
                            update_state = UPDATED;
                        }
                    } else {
                        app_list = load_app_db("brew_data.json");
                        loading_text += "\n" + app_list.length + " packages found."
                        //loading_text += "\n" + Math.floor(System.getFreeMemory()/1024) + "KB free for allocation.";
                        loading_state++;
                    }

                    break;
                case LD_PKGQNTD:
                    loading_state++;
                    explore_menu = new IconMenu(120, 110, ui.font, app_list);
                    explore_menu.padding = -5;
                    explore_menu.icon_padding = 20;
                    break;
                case LD_FADE:
                    ui.fade_text(FADE_IN);
                    loading_state++;
                    break;
                case LD_FINAL:
                    if (!ui.fading) {
                        loading_state = 0;
                        app_state = MAIN_MENU;
                    }
            }

            ui.render_belt(0.005f);
            ui.println(loading_text);

            break;
        case MAIN_MENU:
            switch (mainmenu_state) {
                case MM_INITIAL:
                    mainmenu_state++;
                    break;
                case MM_FINAL:
                    break;
            }

            if(pad.justPressed(Pads.CROSS)) {
                app_state = DLING_MENU;
            }

            if(pad.justPressed(Pads.TRIANGLE)) {
                terminate = true;
            }

            ui.render_belt(0.0025f);
            explore_menu.draw(pad);

            break;
        case OPTIONS_MENU:
            break;
        case DLING_MENU:
            ui.font.color = 0x80808080;
            ui.println("Name: " + app_list[explore_menu.num].name);
            ui.println("Category: " + app_list[explore_menu.num].category);
            ui.println("Package: " + app_list[explore_menu.num].fname);
            ui.println("Link: " + app_list[explore_menu.num].link);

            switch (dl_state) {
                case DETAILS:
                    if(pad.justPressed(Pads.CROSS)) {
                        dling_text += "Downloading package...\n";
                        dling_text += "This is an alpha version, it will take a long time\n";
                        dl_state++;
                    }
                    if(pad.justPressed(Pads.TRIANGLE)) {
                        app_state = MAIN_MENU;
                    }
                    break;
                case TODOWNLOAD:
                    req.asyncDownload(app_list[explore_menu.num].link, System.boot_path + "/" + app_list[explore_menu.num].fname);
                    transfering = true;
                    dl_state++;
                    break;
                case DOWNLOADING:
                    try {
                        if(req.ready(30)) {
                            transfering = false;
                            dl_state++;
                        }
                    } catch (ex) {
                        dling_text += ex.message;
                        dling_text += "Restarting application...\n"
                        ui.println(dling_text);
                        for (let i = 0; i < 5000; i++) {
                            Screen.flip();
                        }   
                        Network.deinit();
                        System.loadELF(System.loadELF(System.boot_path + "athena_pkd.elf", ["app_store.js"]) );
                    }

                    break;
                case DOWNLOADED:
                    if(!app_list[explore_menu.num].fname.endsWith(".elf") && !app_list[explore_menu.num].fname.endsWith(".ELF")) {
                        dl_state++;
                        dling_text += "Extracting files...\n";
                    } else {
                        dl_state = EXTRACTED;
                    }
                    break;
                case EXTRACTING:
                    if (app_list[explore_menu.num].fname.endsWith(".tar.gz")) {
                        Archive.untar(app_list[explore_menu.num].fname);
                    } else if (app_list[explore_menu.num].fname.endsWith(".zip")) {
                        let arc = Archive.open(app_list[explore_menu.num].fname);
                        Archive.extractAll(arc);
                        Archive.close(arc);
                    }
                    dl_state++;
                    break;
                case EXTRACTED:
                    app_state = MAIN_MENU;
                    dl_state = DETAILS;
                    dling_text = "";
                    break;
            }

            ui.render_belt(0.005f);
            ui.println(dling_text);

            break;
        default:
            break;
    }

    if(transfering) {
        ui.font.print(0, 400, req.getAsyncSize() + " bytes transfered");
    }

    ui.update();

    if(terminate){
        break;
    }
}

Network.deinit();

System.loadELF(System.boot_path + "athena_pkd.elf");