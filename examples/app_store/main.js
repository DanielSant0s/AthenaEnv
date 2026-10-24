
import { Network, Request } from 'athena_network.erl';
import * as Archive from 'athena_archive.erl';
import { AppUI, COLORS, rgba, exitToShell } from 'athenaos/app.js';

const ui = new AppUI({ title: "App store", category: "network" });

Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);

function load_app_db(fname) {
    let list_file = std.open(fname, "r");
    let app_list = JSON.parse(list_file.getline());
    list_file.close();
    list_file = null;

    return app_list;
}


function load_network_driver() {
    IOP.reset();

    IOP.loadModule("usbmass_bd");

    try {
        IOP.loadModule("ps2fs");
    } catch (error) {
        console.log(error);
    }

    IOP.loadModule("cdfs");
    IOP.loadModule("mcserv");

    IOP.loadModule("padman");
    IOP.loadModule("SMAP");

    Network.init();
}

let req = new Request();
req.keepalive = true;

const LOADING = 0;
const MAIN_MENU = 1;
const DLING_MENU = 3;

let app_state = LOADING;

const LD_INITIAL = 0;
const LD_NETWORK = 1;
const LD_PKGLIST = 2;
const LD_PKGQNTD = 3;

let loading_state = LD_INITIAL;

let loading_log = ["Athena package manager"];

let app_list = null;
let icons = [];
let sel = 0;
let top = 0;

let pad = Pads.get();

const DETAILS = 0;
const TODOWNLOAD = 1;
const DOWNLOADING = 2;
const DOWNLOADED = 3;
const EXTRACTING = 4;
const EXTRACTED = 5;

let dl_state = DETAILS;
let dling_log = [];
let terminate = false;

let transfering = false;

const NOT_UPDATED = 0;
const UPDATING = 1;
const UPDATED = 2;
let update_state = NOT_UPDATED;

const ROW_H = 48;

// --- Views --------------------------------------------------------------------

function drawLog(lines, x, y, w, h) {
    ui.panel(x, y, w, h);
    lines.forEach((line, i) => ui.textV(x + 16, y + 20 + i * 20, ui.fit(line, 0.46, w - 32), 0.46, i === lines.length - 1 ? COLORS.text : COLORS.textDim));
}

function drawIcon(i, x, cy, size) {
    const icn = icons[i];
    icn.width = size;
    icn.height = size;
    icn.draw(x, cy - size / 2);
}

function drawCatalog() {
    const { x, y, w, h } = ui.content;
    const listW = 260;
    const rows = Math.floor((h - 12) / ROW_H);
    if (sel < top) top = sel;
    if (sel >= top + rows) top = sel - rows + 1;

    ui.panel(x, y, listW, h);
    for (let i = top; i < Math.min(app_list.length, top + rows); i++) {
        const cy = y + 6 + (i - top) * ROW_H + ROW_H / 2;
        if (i === sel) {
            Draw.rect(x + 6, cy - ROW_H / 2 + 2, listW - 12, ROW_H - 4, rgba(255, 255, 255, 0.10));
            Draw.rect(x + 6, cy - ROW_H / 2 + 2, 3, ROW_H - 4, ui.accent());
        }
        drawIcon(i, x + 18, cy, 36);
        ui.textV(x + 64, cy - 7, ui.fit(app_list[i].name, 0.5, listW - 80), 0.5, i === sel ? COLORS.text : COLORS.textDim);
        ui.textV(x + 64, cy + 10, app_list[i].category, 0.38, COLORS.textFaint);
    }

    drawDetails(x + listW + 16, y, w - listW - 16, h);
}

function drawDetails(x, y, w, h) {
    const pkg = app_list[sel];
    ui.panel(x, y, w, h);
    drawIcon(sel, x + 20, y + 52, 64);
    ui.textV(x + 100, y + 38, ui.fit(pkg.name, 0.8, w - 120), 0.8, COLORS.text);
    ui.textV(x + 100, y + 66, pkg.category, 0.46, COLORS.textDim);

    ui.heading(x + 20, y + 112, "Package");
    ui.textV(x + 20, y + 132, ui.fit(pkg.fname, 0.44, w - 40), 0.44, COLORS.text);
    ui.heading(x + 20, y + 162, "Source");
    ui.wrap(pkg.link, 0.40, w - 40).slice(0, 3).forEach((line, i) => ui.textV(x + 20, y + 182 + i * 16, ui.fit(line, 0.40, w - 40), 0.40, COLORS.textDim));

    if (app_state === DLING_MENU) {
        dling_log.slice(-4).forEach((line, i) => ui.textV(x + 20, y + 250 + i * 20, ui.fit(line, 0.46, w - 40), 0.46, COLORS.text));
    }
}

// --- Main loop ----------------------------------------------------------------

while(true) {
    pad.update();

    let hints = [];

    switch (app_state) {
        case LOADING:
            switch(loading_state) {
                case LD_INITIAL:
                    loading_log.push("Loading network driver...");
                    loading_state++;
                    break;
                case LD_NETWORK:
                    load_network_driver();
                    loading_log.push("Loading package list, it's gonna take a while...");
                    loading_state++;
                    break;
                case LD_PKGLIST:
                    if (update_state == NOT_UPDATED) {
                        req.asyncDownload("https://raw.githubusercontent.com/DanielSant0s/brewstore-db/main/brew_data.json", "brew_data.json");
                        update_state = UPDATING;
                        transfering = true;
                    } else if (update_state == UPDATING) {
                        if(req.ready(5000)) {
                            transfering = false;
                            update_state = UPDATED;
                        }
                    } else {
                        app_list = load_app_db("brew_data.json");
                        loading_log.push(app_list.length + " packages found.");
                        loading_state++;
                    }

                    break;
                case LD_PKGQNTD:
                    icons = app_list.map(pkg => {
                        const icn = new Image("store/icons/" + pkg.icon);
                        icn.filter = LINEAR;
                        return icn;
                    });
                    app_state = MAIN_MENU;
                    break;
            }

            ui.background();
            drawLog(loading_log, ui.content.x, ui.content.y, ui.content.w, 40 + loading_log.length * 20);
            break;
        case MAIN_MENU:
            if(pad.justPressed(Pads.DOWN)) sel = (sel + 1) % app_list.length;
            if(pad.justPressed(Pads.UP)) sel = (sel + app_list.length - 1) % app_list.length;

            if(pad.justPressed(Pads.CROSS)) {
                app_state = DLING_MENU;
            }

            if(ui.homeRequested(pad)) {
                terminate = true;
            }

            ui.background();
            drawCatalog();
            hints = [["D-pad", "Browse"], ["cross", "Details"]];
            break;
        case DLING_MENU:
            switch (dl_state) {
                case DETAILS:
                    if(pad.justPressed(Pads.CROSS)) {
                        dling_log.push("Downloading package...");
                        dling_log.push("This is an alpha version, it will take a long time");
                        dl_state++;
                    }
                    if(pad.justPressed(Pads.CIRCLE)) {
                        app_state = MAIN_MENU;
                    }
                    if(ui.homeRequested(pad)) {
                        terminate = true;
                    }
                    hints = [["cross", "Download"], ["circle", "Back"]];
                    break;
                case TODOWNLOAD:
                    req.asyncDownload(app_list[sel].link, System.boot_path + "/" + app_list[sel].fname);
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
                        dling_log.push(ex.message);
                        dling_log.push("Restarting application...");
                        for (let i = 0; i < 300; i++) {
                            ui.background();
                            drawCatalog();
                            ui.chrome([]);
                            Screen.flip();
                        }
                        Network.deinit();
                        System.loadELF(System.boot_path + "athena_pkd.elf", ["--script=apps/app_store/main.js"]);
                    }

                    break;
                case DOWNLOADED:
                    if(!app_list[sel].fname.endsWith(".elf") && !app_list[sel].fname.endsWith(".ELF")) {
                        dl_state++;
                        dling_log.push("Extracting files...");
                    } else {
                        dl_state = EXTRACTED;
                    }
                    break;
                case EXTRACTING: {
                    let pkg_path = System.boot_path + "/" + app_list[sel].fname;
                    if (app_list[sel].fname.endsWith(".tar.gz")) {
                        Archive.untar(pkg_path);
                    } else if (app_list[sel].fname.endsWith(".zip")) {
                        let arc = Archive.open(pkg_path);
                        Archive.extractAll(arc);
                        Archive.close(arc);
                    }
                    dl_state++;
                    break;
                }
                case EXTRACTED:
                    app_state = MAIN_MENU;
                    dl_state = DETAILS;
                    dling_log = [];
                    break;
            }

            ui.background();
            drawCatalog();
            break;
        default:
            break;
    }

    if(terminate){
        break;
    }

    ui.status = transfering ? `${req.getAsyncSize()} bytes` : "";
    // Home is only offered where leaving is safe (not mid-download).
    ui.home = (app_state === MAIN_MENU || (app_state === DLING_MENU && dl_state === DETAILS)) ? ["triangle", "Home"] : null;
    ui.chrome(hints);
    Screen.flip();
}

Network.deinit();

exitToShell();
