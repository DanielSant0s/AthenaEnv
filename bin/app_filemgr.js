// {"name": "File manager", "author": "Daniel Santos", "version": "04072023", "icon": "lfm_icon.png", "file": "app_filemgr.js", "category": "system", "blurb": "Browse memory cards, USB and HDD; copy, move and extract archives.", "exit": "TRIANGLE"}

import * as Archive from 'athena_archive.erl';
import { AppUI, COLORS, rgba, exitToShell } from 'athenaos/app.js';

const ui = new AppUI({ title: "File manager", category: "system",
                       hints: [["cross", "Open"], ["circle", "Up"], ["R1", "Actions"]] });

Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);

const root = [{name:"cdfs:/", size:0, dir:true},
              {name:"host:/", size:0, dir:true},
              {name:"mass:/", size:0, dir:true},
              {name:"mc0:/", size:0, dir:true},
              {name:"mc1:/", size:0, dir:true}];

const ACTIONS = ["Copy", "Move", "Paste", "Rename", "Delete"];

let path = "";
let file = root;
let sel = 0;          // selected entry
let top = 0;          // first visible entry
let menu = false;     // actions menu open
let action = 0;

// Clipboard for copy/move -> paste
let src = "";
let srcfile = "";
let fileop = 0;       // 0 = copy, 1 = move

const ROW_H = 24;

function list(newPath) {
    path = newPath;
    file = path === "" ? root : System.listDir(path);
    sel = Math.min(sel, Math.max(0, file.length - 1));
}

function goUp() {
    var idxof = path.lastIndexOf("/");
    if(path[idxof-1] != ":" && idxof != -1){
        path = path.slice(0, idxof);
        idxof = path.lastIndexOf("/");
        list(path.slice(0, idxof+1));
    } else {
        list("");
    }
    sel = 0;
}

function open(entry) {
    if(entry.dir){
        sel = 0;
        list(path == "" ? entry.name : path + entry.name + "/");
    } else if(entry.name.endsWith(".js")){
        System.loadELF(System.boot_path + "/athena.elf", [path + "/" + entry.name]);
    } else if(entry.name.endsWith(".elf") || entry.name.endsWith(".ELF")){
        System.loadELF(path + "/"+ entry.name);
    } else if(entry.name.endsWith(".zip")){
        Archive.extractAll(path + "/"+ entry.name);
        list(path); // Update file list
    } else if(entry.name.endsWith(".tar.gz")){
        Archive.untar(path + "/"+ entry.name);
        list(path); // Update file list
    }
}

function runAction(entry) {
    switch (action) {
        case 0:
        case 1:
            if (!entry) return;
            fileop = action;
            src = path + "/" + entry.name;
            srcfile = entry.name;
            break;
        case 2: {
            if (!srcfile) return;
            const dst = path + "/" + srcfile;
            if (fileop === 0) System.copyFile(src, dst);
            else System.moveFile(src, dst);
            break;
        }
        case 3:
            break;
        case 4:
            if (!entry) return;
            System.removeFile(path + "/" + entry.name);
            break;
    }
    menu = false;
    list(path);
}

function formatSize(bytes) {
    if (bytes < 1024) return `${bytes} B`;
    if (bytes < 1048576) return `${(bytes / 1024).toFixed(1)} KB`;
    return `${(bytes / 1048576).toFixed(1)} MB`;
}

const pad = Pads.get();

while(true){
    pad.update();

    if(ui.homeRequested(pad)) {
        break;
    }

    if (pad.justPressed(Pads.R1)) {
        menu = !menu;
        action = 0;
    }

    if (menu) {
        if (pad.justPressed(Pads.DOWN)) action = (action + 1) % ACTIONS.length;
        if (pad.justPressed(Pads.UP)) action = (action + ACTIONS.length - 1) % ACTIONS.length;
        if (pad.justPressed(Pads.CROSS)) runAction(file[sel]);
        if (pad.justPressed(Pads.CIRCLE)) menu = false;
    } else {
        if (pad.justPressed(Pads.DOWN) && file.length) sel = (sel + 1) % file.length;
        if (pad.justPressed(Pads.UP) && file.length) sel = (sel + file.length - 1) % file.length;
        if (pad.justPressed(Pads.CROSS) && file[sel]) open(file[sel]);
        if (pad.justPressed(Pads.CIRCLE)) goUp();
    }

    ui.background();
    const { x, y, w, h } = ui.content;

    // Location bar
    ui.heading(x, y + 6, "Location");
    ui.textV(x + 72, y + 6, ui.fit(path || "Devices", 0.5, w - 72), 0.5, COLORS.text);

    // File list
    const ly = y + 20, lh = h - 20;
    const rows = Math.floor((lh - 12) / ROW_H);
    if (sel < top) top = sel;
    if (sel >= top + rows) top = sel - rows + 1;
    ui.panel(x, ly, w, lh);

    if (!file.length) ui.textV(x + w / 2, ly + lh / 2, "Empty folder", 0.5, COLORS.textDim, Font.ALIGN_HCENTER);

    for (let i = top; i < Math.min(file.length, top + rows); i++) {
        const entry = file[i];
        const cy = ly + 6 + (i - top) * ROW_H + ROW_H / 2;
        if (i === sel) {
            Draw.rect(x + 6, cy - ROW_H / 2, w - 12, ROW_H, rgba(255, 255, 255, menu ? 0.06 : 0.10));
            Draw.rect(x + 6, cy - ROW_H / 2, 3, ROW_H, ui.accent());
        }
        // Folder / file marker
        if (entry.dir) Draw.rect(x + 18, cy - 5, 14, 10, ui.accent(0.85));
        else Draw.rect(x + 20, cy - 6, 10, 12, rgba(255, 255, 255, 0.45));
        ui.textV(x + 42, cy, ui.fit(entry.name, 0.46, w - 160), 0.46, i === sel ? COLORS.text : COLORS.textDim);
        ui.textV(x + w - 16, cy, entry.dir ? "Folder" : formatSize(entry.size), 0.42, COLORS.textFaint, Font.ALIGN_RIGHT);
    }

    // Actions menu
    if (menu) {
        const mw = 150, mh = ACTIONS.length * 30 + 16, mx = x + w - mw - 12, my = ly + 12;
        ui.panel(mx, my, mw, mh);
        ACTIONS.forEach((label, i) => {
            const cy = my + 8 + i * 30 + 15;
            const disabled = (i === 2 && !srcfile) || i === 3;
            if (i === action) Draw.rect(mx + 6, cy - 13, mw - 12, 26, ui.accent(0.85));
            ui.textV(mx + 18, cy, label, 0.48, disabled ? COLORS.textFaint : COLORS.text);
        });
    }

    ui.status = srcfile ? `${fileop === 0 ? "Copy" : "Move"}: ${ui.fit(srcfile, 0.44, 140)}` : "";
    ui.chrome(menu ? [["cross", "Run"], ["circle", "Close"]] : ui.hintList, `${file.length} items`);
    Screen.flip();
};

exitToShell();
