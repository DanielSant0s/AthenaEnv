// AthenaOS shell -- desktop, launcher and system UI for the AthenaEnv samples.
//
// Apps are regular scripts started with std.reload(); they come back here with
// std.reload("main.js"). The shell remembers what it launched (athenaos/session.json)
// so returning from an app lands on the desktop with a "closed" toast instead of
// the lock screen.

import { rgba, ACCENTS, accent, mix, COLORS, Spring, clamp, lerp, easeOut,
         roundRect, roundGradient, panel, ring, vgradient, bar, toggle, glyph, hints } from 'athenaos/gfx.js';
import { scanApps, categoryOf, initials, DEMOS_ELF } from 'athenaos/registry.js';
import { loadSettings, saveSettings, loadSession, saveSession } from 'athenaos/storage.js';
import { text, textV, measure, fit, wrap, pad2, margin, now as nowIn,
         clockText as clockIn, dateText as dateIn } from 'athenaos/ui.js';

// --- Video setup ------------------------------------------------------------

const canvas = Screen.getMode();
canvas.zbuffering = false;
canvas.psmz = Screen.Z16S;
Screen.setMode(canvas);
Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);

const W = canvas.width;
const H = canvas.height;

// --- State --------------------------------------------------------------------

const settings = loadSettings();
const session = loadSession();
const apps = scanApps();
const appByFile = (file) => apps.find(a => a.file === file);

const returnedFrom = session.launching ? appByFile(session.launching) : null;
const coldBoot = !session.launching;
session.launching = null;
session.pinned = session.pinned.filter(f => appByFile(f));
session.recents = session.recents.filter(f => appByFile(f));
saveSession(session);

const ui = new Font("fonts/Segoe UI.ttf");
const display = new Font("fonts/LEMONMILK-Light.otf");
const heavy = new Font("fonts/LEMONMILK-Medium.otf");

apps.forEach(app => {
    app.fx = new Spring(0, 0.3);
    if (app.iconPath) {
        app.icon = new Image(app.iconPath);
        app.icon.filter = LINEAR;
    }
});

const pad = Pads.get();
const cpuInfo = System.getCPUInfo();
const gpuInfo = System.getGPUInfo();
const uptime = Timer.new();

Screen.setFrameCounter(settings.showFps);

let frame = 0;

// --- Clock & system stats ---------------------------------------------------------

const now = () => nowIn(settings);
const clockText = (d) => clockIn(settings, d);
const dateText = (d) => dateIn(settings, d);

const stats = { ram: 0, ramUsed: 0, iop: 0, iopUsed: 0, vram: 0, vramUsed: 0, vramSize: 0, temp: undefined, pads: [] };

function refreshStats() {
    const mem = System.getMemoryStats();
    stats.ramUsed = mem.used;
    stats.ram = mem.used / cpuInfo.RAMSize;
    const iop = IOP.getMemoryStats();
    stats.iopUsed = iop.used;
    stats.iop = iop.used / (2048 * 1024);
    stats.vramUsed = Screen.getMemoryStats(Screen.VRAM_USED_TOTAL);
    stats.vramSize = Screen.getMemoryStats(Screen.VRAM_SIZE);
    stats.vram = stats.vramSize ? stats.vramUsed / stats.vramSize : 0;
    stats.temp = System.getTemperature();
    stats.pads = Pads.getConnected();
}
refreshStats();

const kb = (bytes) => `${Math.round(bytes / 1024)} KB`;
const mb = (bytes) => `${(bytes / (1024 * 1024)).toFixed(1)} MB`;

// --- Input ------------------------------------------------------------------------
// D-pad and left stick merged, with key repeat for held directions.

const DIRS = { up: Pads.UP, down: Pads.DOWN, left: Pads.LEFT, right: Pads.RIGHT };
const dirHeld = { up: 0, down: 0, left: 0, right: 0 };
const nav = { up: false, down: false, left: false, right: false };

function pollInput() {
    pad.update();
    const stick = { up: pad.ly < -64, down: pad.ly > 64, left: pad.lx < -64, right: pad.lx > 64 };
    for (const d in DIRS) {
        const down = pad.pressed(DIRS[d]) || stick[d];
        dirHeld[d] = down ? dirHeld[d] + 1 : 0;
        nav[d] = dirHeld[d] === 1 || (dirHeld[d] > 18 && dirHeld[d] % 5 === 0);
    }
}

const pressed = (btn) => pad.justPressed(btn);

function anyButton() {
    return pad.btns !== 0 && pad.btns !== pad.old_btns;
}

// --- Wallpapers ---------------------------------------------------------------------

const WALLPAPERS = ["Aurora", "Midnight", "Classic"];
let classicBg = null;

function accentRgb() { return ACCENTS[settings.accent % ACCENTS.length].rgb; }

function blob(x, y, r, rgb, a) {
    for (let i = 0; i < 4; i++) {
        Draw.circle(x, y, r * (1 - i * 0.22), rgba(rgb[0], rgb[1], rgb[2], a), true);
    }
}

function drawWallpaper() {
    const ac = accentRgb();
    switch (settings.wallpaper % WALLPAPERS.length) {
        case 0: { // Aurora: gradient with slowly drifting light blobs
            const top = mix([10, 12, 28], ac, 0.18), bottom = [6, 6, 14];
            vgradient(0, 0, W, H, rgba(top[0], top[1], top[2]), rgba(bottom[0], bottom[1], bottom[2]));
            const t = settings.animations ? frame / 240 : 0;
            blob(W * 0.22 + Math.cos(t) * 60, H * 0.30 + Math.sin(t * 1.3) * 30, 190, ac, 0.05);
            blob(W * 0.80 + Math.sin(t * 0.8) * 50, H * 0.65 + Math.cos(t) * 40, 220, mix(ac, [255, 80, 160], 0.6), 0.045);
            blob(W * 0.55 + Math.cos(t * 0.6) * 80, H * 1.05, 200, mix(ac, [40, 220, 200], 0.5), 0.04);
            break;
        }
        case 1: { // Midnight: plain, cheap
            vgradient(0, 0, W, H, rgba(22, 24, 34), rgba(4, 4, 8));
            break;
        }
        case 2: { // Classic: the original dashboard picture, tinted with the accent
            if (!classicBg) {
                classicBg = new Image("dash/bg.png");
                if (classicBg.palette) {
                    const pal = new Uint8Array(classicBg.palette);
                    for (let i = 0; i < pal.length; i += 4) {
                        const l = (pal[i] + pal[i + 1] + pal[i + 2]) / 765;
                        pal[i] = Math.trunc(ac[0] * l); pal[i + 1] = Math.trunc(ac[1] * l); pal[i + 2] = Math.trunc(ac[2] * l);
                    }
                }
            }
            classicBg.width = W; classicBg.height = H;
            classicBg.draw(0, 0);
            break;
        }
    }
}

// --- App tiles ---------------------------------------------------------------------

function drawTile(app, cx, cy, size, focus = 0, dim = 1.0) {
    const s = size * (1 + 0.10 * focus);
    const x = cx - s / 2, y = cy - s / 2, r = s * 0.22;
    const c = categoryOf(app.category).rgb;
    const hi = mix(c, [255, 255, 255], 0.25), lo = mix(c, [0, 0, 0], 0.45);

    if (focus > 0.02) ring(x, y, s, s, r, accent(settings.accent, focus * dim), 3);
    roundGradient(x, y, s, s, r, rgba(hi[0], hi[1], hi[2], dim), rgba(lo[0], lo[1], lo[2], dim));

    if (app.icon) {
        const is = s * 0.70;
        app.icon.width = is; app.icon.height = is;
        app.icon.color = Color.new(255, 255, 255, Math.round(128 * dim));
        app.icon.draw(cx - is / 2, cy - is / 2);
    } else {
        textV(heavy, cx, cy, initials(app.name), s / 110, rgba(255, 255, 255, 0.92 * dim), Font.ALIGN_HCENTER);
    }
}

// --- Toasts -------------------------------------------------------------------------

// Toasts rise from the bottom edge into the gap above the hints row.
const TOAST_HIDDEN = H + 10, TOAST_SHOWN = H - 70;
const toast = { msg: "", ttl: 0, y: new Spring(TOAST_HIDDEN, 0.2) };

function notify(msg) {
    toast.msg = msg;
    toast.ttl = 150;
    toast.y.set(TOAST_SHOWN);
}

function drawToast() {
    if (toast.ttl > 0 && --toast.ttl === 0) toast.y.set(TOAST_HIDDEN);
    const y = toast.y.step();
    if (y >= TOAST_HIDDEN - 1) return;
    const w = measure(ui, toast.msg, 0.5) + 36;
    panel(W / 2 - w / 2, y, w, 28, 14, COLORS.glassHi);
    Draw.circle(W / 2 - w / 2 + 14, y + 14, 4, accent(settings.accent), true);
    textV(ui, W / 2 - w / 2 + 24, y + 14, toast.msg, 0.5, COLORS.text);
}

// --- Layout ---------------------------------------------------------------------------

// Four bands, one margin (M = 48 at 640 wide):
//   tabs + clock ~32 | grid 70-252 | focused app 292-364 | hints ~422
const M = margin(W);
const GRID = { cols: 5, rows: 2, tile: 80, pitchY: 102, top: 70 };
GRID.pitchX = (W - 2 * M - GRID.tile) / (GRID.cols - 1);   // outer tiles touch the margins
GRID.left = M + GRID.tile / 2;                               // first column center
const LAYOUT = { topCY: 32, nameCY: 292, metaCY: 318, blurbCY: 342, hintsCY: 422 };

// --- Desktop ------------------------------------------------------------------------

// Tabs group registry categories; "Home" is the user's pinned + recent apps.
const TABS = [
    { label: "Home" },
    { label: "All" },
    { label: "Games",    cats: ["games"] },
    { label: "Graphics", cats: ["3d", "2d"] },
    { label: "Media",    cats: ["media"] },
    { label: "Network",  cats: ["network"] },
    { label: "System",   cats: ["system", "input"] },
];

const desk = {
    tab: 0,
    focus: 0,
    topRow: 0,
    tabX: new Spring(M, 0.25),
    tabW: new Spring(30, 0.25),
    scroll: new Spring(0, 0.25),
};

function homeApps() {
    const files = [...session.pinned, ...session.recents.filter(f => !session.pinned.includes(f))];
    return files.map(appByFile).filter(a => a).slice(0, GRID.cols * GRID.rows);
}

function visibleApps() {
    const tab = TABS[desk.tab];
    if (tab.label === "Home") return homeApps();
    return tab.cats ? apps.filter(a => tab.cats.includes(a.category)) : apps;
}

function focusedApp() {
    return visibleApps()[desk.focus] || null;
}

function setTab(i) {
    desk.tab = (i + TABS.length) % TABS.length;
    desk.focus = 0;
    desk.topRow = 0;
}

function togglePin(app) {
    if (!app) return;
    const i = session.pinned.indexOf(app.file);
    if (i >= 0) { session.pinned.splice(i, 1); notify(`Unpinned ${app.name}`); }
    else if (session.pinned.length >= 8) { notify("Home is full - unpin something first"); return; }
    else { session.pinned.push(app.file); notify(`Pinned ${app.name} to Home`); }
    saveSession(session);
    desk.focus = Math.min(desk.focus, Math.max(0, visibleApps().length - 1));
}

function desktopInput() {
    if (pressed(Pads.L1)) setTab(desk.tab - 1);
    if (pressed(Pads.R1)) setTab(desk.tab + 1);

    const n = visibleApps().length;
    let f = desk.focus;
    if (nav.right && f < n - 1) f++;
    if (nav.left && f > 0) f--;
    if (nav.down && f + GRID.cols < n) f += GRID.cols;
    else if (nav.down && Math.floor(f / GRID.cols) < Math.floor((n - 1) / GRID.cols)) f = n - 1;
    if (nav.up && f - GRID.cols >= 0) f -= GRID.cols;
    desk.focus = f;

    const row = Math.floor(f / GRID.cols);
    if (row < desk.topRow) desk.topRow = row;
    if (row >= desk.topRow + GRID.rows) desk.topRow = row - GRID.rows + 1;

    const app = focusedApp();
    if (pressed(Pads.CROSS) && app) launch(app);
    if (pressed(Pads.TRIANGLE) && app) openOverlay("details");
    if (pressed(Pads.SQUARE) && app) togglePin(app);
    if (pressed(Pads.START)) openOverlay("control");
    if (pressed(Pads.SELECT)) openOverlay("monitor");
}

// Plain text tabs with a sliding accent underline; clock on the right.
function drawTopRow() {
    const cy = LAYOUT.topCY, scale = 0.52, gap = 22;
    let x = M, selX = M, selW = 0;
    TABS.forEach((tab, i) => {
        const w = Math.round(measure(ui, tab.label, scale));
        const sel = i === desk.tab;
        if (sel) { selX = x; selW = w; }
        textV(ui, x, cy, tab.label, scale, sel ? COLORS.text : COLORS.textFaint);
        x += w + gap;
    });

    desk.tabX.set(selX); desk.tabW.set(selW);
    if (!settings.animations) { desk.tabX.snap(selX); desk.tabW.snap(selW); }
    Draw.rect(Math.round(desk.tabX.step()), cy + 12, Math.round(desk.tabW.step()), 2, accent(settings.accent));

    const clock = clockText();
    textV(ui, W - M - measure(ui, clock, 0.52), cy, clock, 0.52, COLORS.text);
}

function drawGrid() {
    const list = visibleApps();
    desk.scroll.set(desk.topRow);
    if (!settings.animations) desk.scroll.snap(desk.topRow);
    const scroll = desk.scroll.step();

    if (list.length === 0) {
        const msg = TABS[desk.tab].label === "Home" ? "Press Square on any app to pin it here" : "Nothing here yet";
        textV(ui, W / 2, GRID.top + GRID.pitchY / 2 + GRID.tile / 2, msg, 0.52, COLORS.textFaint, Font.ALIGN_HCENTER);
        return;
    }

    list.forEach((app, i) => {
        const col = i % GRID.cols, row = Math.floor(i / GRID.cols) - scroll;
        const focused = i === desk.focus;
        app.fx.set(focused ? 1 : 0);
        const f = settings.animations ? app.fx.step() : app.fx.snap(focused ? 1 : 0).v;
        if (row < -0.6 || row > GRID.rows - 0.4) return;

        const dim = clamp(1 - Math.max(0, -row, row - (GRID.rows - 1)) * 1.6, 0, 1);
        const cx = GRID.left + col * GRID.pitchX;
        const cy = GRID.top + GRID.tile / 2 + row * GRID.pitchY;
        drawTile(app, cx, cy, GRID.tile, f, dim * (focused ? 1 : 0.8));
    });

    // Thin scroll indicator in the right gutter, only when there is more to see.
    const rows = Math.ceil(list.length / GRID.cols);
    if (rows > GRID.rows) {
        const trackH = GRID.pitchY * (GRID.rows - 1) + GRID.tile, y0 = GRID.top;
        const tx = W - M / 2 - 1;
        Draw.rect(tx, y0, 2, trackH, rgba(255, 255, 255, 0.10));
        const thumb = trackH * GRID.rows / rows;
        Draw.rect(tx, Math.round(y0 + (trackH - thumb) * (scroll / (rows - GRID.rows))), 2, Math.round(thumb), accent(settings.accent));
    }
}

// The focused app, described once and large, instead of a label under every tile.
function drawFocused() {
    const app = focusedApp();
    if (!app) return;
    textV(ui, M, LAYOUT.nameCY, fit(ui, app.name, 0.95, W - 2 * M), 0.95, COLORS.text);
    textV(ui, M, LAYOUT.metaCY, `${categoryOf(app.category).label}  -  ${app.author}`, 0.46, accent(settings.accent));
    if (app.blurb) {
        wrap(ui, app.blurb, 0.48, W - 2 * M).slice(0, 2)
            .forEach((line, i) => textV(ui, M, LAYOUT.blurbCY + i * 20, line, 0.48, COLORS.textDim));
    }
}

function drawDesktop() {
    drawTopRow();
    drawGrid();
    drawFocused();
    hints([["cross", "Open"], ["triangle", "Options"], ["L1/R1", "Switch"], ["START", "Menu"]], W - M, LAYOUT.hintsCY, ui);
}

// --- Lock screen ------------------------------------------------------------------

const lock = { y: new Spring(0, 0.18), fade: new Spring(coldBoot ? 0 : 1, 0.04), leaving: false };

function lockInput() {
    if (!lock.leaving && frame > 20 && anyButton()) {
        lock.leaving = true;
        lock.y.set(-H);
    }
}

function drawLock() {
    const y = settings.animations ? lock.y.step() : (lock.leaving ? -H : 0);
    if (lock.leaving && y <= -H + 2) { state = "desktop"; lock.leaving = false; lock.y.snap(0); return; }
    const d = now();
    Draw.rect(0, y, W, H, rgba(0, 0, 0, 0.25));
    text(display, W / 2, y + 120, clockText(d), 2.6, COLORS.text, Font.ALIGN_HCENTER);
    text(ui, W / 2, y + 200, dateText(d), 0.7, COLORS.textDim, Font.ALIGN_HCENTER);

    const pulse = 0.45 + 0.35 * Math.sin(frame / 22);
    text(ui, W / 2, y + H - 70, "Press any button to unlock", 0.55, rgba(255, 255, 255, pulse), Font.ALIGN_HCENTER);
    text(heavy, W / 2, y + H - 40, "ATHENA OS", 0.38, COLORS.textFaint, Font.ALIGN_HCENTER);
}

// --- Overlays ------------------------------------------------------------------------

let overlay = null;
const sheet = new Spring(0, 0.25);
let ccFocus = 0, setFocus = 0, confirmPower = false;

function openOverlay(name) {
    overlay = name;
    sheet.snap(0).set(1);
    ccFocus = 0; setFocus = 0; confirmPower = false;
    if (name === "monitor") refreshStats();
}

function closeOverlay() {
    if (overlay === "settings") { saveSettings(settings); notify("Settings saved"); }
    overlay = null;
}

function scrim() {
    Draw.rect(0, 0, W, H, rgba(0, 0, 0, 0.55 * sheet.v));
}

// Details -----------------------------------------------------------------------

function detailsInput() {
    const app = focusedApp();
    if (!app || pressed(Pads.CIRCLE) || pressed(Pads.TRIANGLE)) { closeOverlay(); return; }
    if (pressed(Pads.CROSS)) { closeOverlay(); launch(app); }
    if (app.c && pressed(Pads.R1)) { closeOverlay(); launch(app, "c"); }
    if (pressed(Pads.SQUARE)) togglePin(app);
}

function drawDetails() {
    const app = focusedApp();
    if (!app) return;
    const t = easeOut(sheet.step());
    scrim();
    const w = 440, h = 250, x = (W - w) / 2, y = (H - h) / 2 + (1 - t) * 30;
    panel(x, y, w, h, 18, COLORS.glassHi);

    drawTile(app, x + 70, y + 70, 92);
    const cat = categoryOf(app.category);
    // Tile spans y+24..y+116; the three header lines are spread over the same height.
    textV(ui, x + 136, y + 42, fit(ui, app.name, 0.85, w - 160), 0.85, COLORS.text);
    textV(ui, x + 136, y + 72, `${app.author}${app.version ? "  -  v" + app.version : ""}`, 0.46, COLORS.textDim);
    const chipW = Math.round(measure(ui, cat.label, 0.42)) + 16;
    roundRect(x + 136, y + 89, chipW, 18, 9, rgba(cat.rgb[0], cat.rgb[1], cat.rgb[2], 0.45));
    textV(ui, x + 144, y + 98, cat.label, 0.42, COLORS.text);

    const blurb = app.blurb || "An AthenaEnv sample.";
    wrap(ui, blurb, 0.5, w - 48).slice(0, 2).forEach((line, i) => textV(ui, x + 24, y + 146 + i * 20, line, 0.5, COLORS.text));

    const runs = session.launches[app.file] || 0;
    textV(ui, x + 24, y + 190, `${app.file}${app.c ? " + main.c" : ""}   -   Home: ${app.exit}   -   Opened ${runs} time${runs === 1 ? "" : "s"}`, 0.42, COLORS.textFaint);

    Draw.rect(x + 24, y + h - 42, w - 48, 1, COLORS.stroke);
    const pinned = session.pinned.includes(app.file);
    hints([["cross", "Open"], ...(app.c ? [["R1", "Open in C"]] : []), ["square", pinned ? "Unpin" : "Pin"], ["circle", "Close"]],
          x + w - 24, y + h - 21, ui);
}

// Control center -------------------------------------------------------------------

const CC_ITEMS = [
    { label: "Settings",       sub: "Theme, clock, wallpaper", run: () => openOverlay("settings") },
    { label: "System monitor", sub: "Memory, video, hardware", run: () => openOverlay("monitor") },
    { label: "File manager",   sub: "Browse devices",          run: () => { const a = appByFile("apps/app_filemgr/main.js"); if (a) { closeOverlay(); launch(a); } } },
    { label: "Lock",           sub: "Back to the lock screen", run: () => { closeOverlay(); state = "lock"; lock.fade.snap(1); } },
    { label: "Restart shell",  sub: "Reload AthenaOS",         run: () => { pendingReload = "main.js"; } },
    { label: "Power",          sub: "Exit to the PS2 browser", run: () => { confirmPower = true; } },
];

function controlInput() {
    if (confirmPower) {
        if (pressed(Pads.CROSS)) { saveSession(session); System.exitToBrowser(); }
        if (pressed(Pads.CIRCLE)) confirmPower = false;
        return;
    }
    if (pressed(Pads.CIRCLE) || pressed(Pads.START)) { closeOverlay(); return; }
    if (nav.down) ccFocus = (ccFocus + 1) % CC_ITEMS.length;
    if (nav.up) ccFocus = (ccFocus + CC_ITEMS.length - 1) % CC_ITEMS.length;
    if (pressed(Pads.CROSS)) CC_ITEMS[ccFocus].run();
}

function drawControl() {
    const t = easeOut(sheet.step());
    scrim();
    const w = 270, x = W - w * t, y = 0;
    Draw.rect(x, y, w, H, rgba(16, 18, 30, 0.92));
    Draw.rect(x, y, 1, H, COLORS.stroke);

    text(display, x + 22, 22, clockText(), 1.25, COLORS.text);
    text(ui, x + 24, 72, dateText(), 0.46, COLORS.textDim);

    CC_ITEMS.forEach((item, i) => {
        const iy = 104 + i * 44;
        const sel = i === ccFocus;
        if (sel) roundRect(x + 12, iy, w - 24, 38, 10, accent(settings.accent, 0.85));
        textV(ui, x + 26, iy + 13, item.label, 0.54, COLORS.text);
        textV(ui, x + 26, iy + 28, item.sub, 0.40, sel ? COLORS.text : COLORS.textFaint);
    });

    const sy = H - 70;
    [["RAM", stats.ram], ["IOP", stats.iop], ["VRAM", stats.vram]].forEach(([label, v], i) => {
        const cy = sy + 8 + i * 18;
        textV(ui, x + 24, cy, label, 0.40, COLORS.textDim);
        bar(x + 70, cy - 3, w - 94, 6, v, accent(settings.accent));
    });

    if (confirmPower) {
        Draw.rect(0, 0, W, H, rgba(0, 0, 0, 0.5));
        const pw = 320, ph = 120, px = (W - pw) / 2, py = (H - ph) / 2;
        panel(px, py, pw, ph, 16, COLORS.glassHi);
        textV(ui, W / 2, py + 32, "Exit AthenaOS?", 0.75, COLORS.text, Font.ALIGN_HCENTER);
        textV(ui, W / 2, py + 60, "You will return to the PS2 browser.", 0.46, COLORS.textDim, Font.ALIGN_HCENTER);
        hints([["cross", "Exit"], ["circle", "Cancel"]], px + pw - 24, py + ph - 21, ui);
    }
}

// Settings ---------------------------------------------------------------------------

const SETTINGS_ROWS = [
    { label: "Accent color",       value: () => ACCENTS[settings.accent].name, change: (d) => { settings.accent = (settings.accent + d + ACCENTS.length) % ACCENTS.length; classicBg = null; } },
    { label: "Wallpaper",          value: () => WALLPAPERS[settings.wallpaper], change: (d) => { settings.wallpaper = (settings.wallpaper + d + WALLPAPERS.length) % WALLPAPERS.length; } },
    { label: "24-hour clock",      bool: "clock24" },
    { label: "Clock offset",       value: () => `UTC ${settings.utcOffset >= 0 ? "+" : ""}${settings.utcOffset}h`, change: (d) => { settings.utcOffset = clamp(settings.utcOffset + d, -12, 14); } },
    { label: "Lock screen on boot", bool: "lockScreen" },
    { label: "Show FPS",           bool: "showFps", after: () => Screen.setFrameCounter(settings.showFps) },
    { label: "Animations",         bool: "animations" },
];

function settingsInput() {
    if (pressed(Pads.CIRCLE) || pressed(Pads.TRIANGLE)) { closeOverlay(); return; }
    if (nav.down) setFocus = (setFocus + 1) % SETTINGS_ROWS.length;
    if (nav.up) setFocus = (setFocus + SETTINGS_ROWS.length - 1) % SETTINGS_ROWS.length;
    const row = SETTINGS_ROWS[setFocus];
    const d = nav.right || pressed(Pads.CROSS) ? 1 : (nav.left ? -1 : 0);
    if (!d) return;
    if (row.bool) settings[row.bool] = !settings[row.bool];
    else row.change(d);
    if (row.after) row.after();
}

function drawSettings() {
    const t = easeOut(sheet.step());
    scrim();
    const w = 420, h = 104 + SETTINGS_ROWS.length * 36, x = (W - w) / 2, y = (H - h) / 2 + (1 - t) * 30;
    panel(x, y, w, h, 18, COLORS.glassHi);
    textV(ui, x + 24, y + 30, "Settings", 0.8, COLORS.text);

    SETTINGS_ROWS.forEach((row, i) => {
        const cy = y + 74 + i * 36;           // row center
        const sel = i === setFocus;
        const right = x + w - 26;             // value column right edge
        if (sel) roundRect(x + 12, cy - 16, w - 24, 32, 10, rgba(255, 255, 255, 0.10));
        textV(ui, x + 26, cy, row.label, 0.52, sel ? COLORS.text : COLORS.textDim);
        if (row.bool) {
            toggle(right - 36, cy - 10, settings[row.bool], accent(settings.accent));
        } else {
            const v = row.value();
            const vw = Math.round(measure(ui, v, 0.5));
            const arrowW = 14;
            textV(ui, right - arrowW - vw, cy, v, 0.5, COLORS.text);
            if (sel) {
                textV(ui, right - arrowW - vw - 16, cy, "<", 0.5, accent(settings.accent));
                textV(ui, right - 8, cy, ">", 0.5, accent(settings.accent));
            }
            if (row.label === "Accent color") Draw.circle(right - arrowW - vw - (sel ? 30 : 14), cy, 6, accent(settings.accent), true);
        }
    });
    Draw.rect(x + 24, y + h - 42, w - 48, 1, COLORS.stroke);
    hints([["D-pad", "Change"], ["circle", "Done"]], x + w - 24, y + h - 21, ui);
}

// System monitor ------------------------------------------------------------------------

function monitorInput() {
    if (pressed(Pads.CIRCLE) || pressed(Pads.SELECT) || pressed(Pads.TRIANGLE)) closeOverlay();
}

function drawMonitor() {
    const t = easeOut(sheet.step());
    scrim();
    const w = 520, h = 350, x = (W - w) / 2, y = (H - h) / 2 + (1 - t) * 30;
    panel(x, y, w, h, 18, COLORS.glassHi);
    textV(ui, x + 24, y + 30, "System monitor", 0.8, COLORS.text);
    textV(ui, x + w - 24 - measure(ui, `Up ${upText()}`, 0.46), y + 30, `Up ${upText()}`, 0.46, COLORS.textDim);

    const rows = [
        ["EE RAM",  stats.ram,  `${kb(stats.ramUsed)} / ${kb(cpuInfo.RAMSize)}`],
        ["IOP RAM", stats.iop,  `${kb(stats.iopUsed)} / 2048 KB`],
        ["VRAM",    stats.vram, `${kb(stats.vramUsed)} / ${kb(stats.vramSize)}`],
    ];
    rows.forEach(([label, v, detail], i) => {
        const cy = y + 68 + i * 40;
        textV(ui, x + 24, cy, label, 0.5, COLORS.text);
        textV(ui, x + w - 24 - measure(ui, detail, 0.44), cy, detail, 0.44, COLORS.textDim);
        bar(x + 24, cy + 12, w - 48, 8, v, v > 0.85 ? COLORS.warn : accent(settings.accent));
    });

    const info = [
        ["CPU", `R5900 rev ${cpuInfo.revision}  -  I$ ${kb(cpuInfo.ICacheSize)}  D$ ${kb(cpuInfo.DCacheSize)}`],
        ["GPU", `GS id ${gpuInfo.id} rev ${gpuInfo.revision}`],
        ["Video", `${W}x${H}  ${canvas.interlace ? "interlaced" : "progressive"}${settings.showFps ? "  -  " + Screen.getFPS(60) + " FPS" : ""}`],
        ["Temp", stats.temp === undefined ? "Sensor not available" : `${stats.temp} C`],
        ["Pads", stats.pads.length ? stats.pads.map(p => `Port ${p + 1}`).join(", ") : "None"],
        ["Boot", System.boot_path || "unknown"],
    ];
    info.forEach(([label, value], i) => {
        const cy = y + 196 + i * 19;
        textV(ui, x + 24, cy, label, 0.44, COLORS.textDim);
        textV(ui, x + 90, cy, fit(ui, String(value), 0.44, w - 114), 0.44, COLORS.text);
    });
    Draw.rect(x + 24, y + h - 42, w - 48, 1, COLORS.stroke);
    hints([["circle", "Close"]], x + w - 24, y + h - 21, ui);
}

function upText() {
    const s = Math.floor(Timer.getTime(uptime) / 1000000);
    return `${Math.floor(s / 60)}:${pad2(s % 60)}`;
}

const OVERLAYS = {
    details:  [detailsInput,  drawDetails],
    control:  [controlInput,  drawControl],
    settings: [settingsInput, drawSettings],
    monitor:  [monitorInput,  drawMonitor],
};

// --- Launching -------------------------------------------------------------------------

let launching = null;           // { app, t, lang }
let pendingReload = null;        // file to std.reload() at the end of the frame
let pendingC = null;             // app to run in athena_demos.elf at the end of the frame

// lang "c": the app's C version, in athena_demos.elf, which loads this shell back
// when the app returns. Either way session.launching names the app, so the shell
// greets it on the way back.
function launch(app, lang = "js") {
    if (launching) return;
    launching = { app, t: 0, lang };
    session.launching = app.file;
    session.recents = [app.file, ...session.recents.filter(f => f !== app.file)].slice(0, 8);
    session.launches[app.file] = (session.launches[app.file] || 0) + 1;
    saveSession(session);
}

function drawLaunch() {
    const LEN = settings.animations ? 22 : 2;
    launching.t++;
    const t = easeOut(launching.t / LEN);
    Draw.rect(0, 0, W, H, rgba(0, 0, 0, t));
    drawTile(launching.app, W / 2, H / 2 - 20, lerp(GRID.tile, 120, t), 0, 1);
    text(ui, W / 2, H / 2 + 56, launching.app.name, 0.7, rgba(255, 255, 255, t), Font.ALIGN_HCENTER);
    if (launching.t >= LEN + 4) {
        if (launching.lang === "c") pendingC = launching.app;
        else pendingReload = launching.app.file;
    }
}

// --- Main loop -----------------------------------------------------------------------

let state = (coldBoot && settings.lockScreen) ? "lock" : "desktop";

// Start on Home (it holds the app just closed, as the most recent), or All when empty.
if (homeApps().length === 0) setTab(1);

if (returnedFrom) {
    notify(`Closed ${returnedFrom.name}`);
    const i = visibleApps().indexOf(returnedFrom);
    if (i >= 0) {
        desk.focus = i;
        desk.topRow = Math.max(0, Math.floor(i / GRID.cols) - GRID.rows + 1);
        desk.scroll.snap(desk.topRow);
    }
}

while (true) {
    frame++;
    pollInput();
    if (frame % 30 === 0) refreshStats();

    if (!launching) {
        if (state === "lock") lockInput();
        else if (overlay) OVERLAYS[overlay][0]();
        else desktopInput();
    }

    Screen.clear(Color.new(0, 0, 0));
    drawWallpaper();

    if (state === "lock") {
        drawLock();
    } else {
        drawDesktop();
        if (overlay) OVERLAYS[overlay][1]();
    }

    drawToast();
    if (launching) drawLaunch();

    // Cold-boot fade-in from black
    const fade = lock.fade.step();
    if (fade < 0.99) Draw.rect(0, 0, W, H, rgba(0, 0, 0, 1 - fade));
    if (coldBoot) lock.fade.set(1);

    Screen.flip();

    if (pendingReload) {
        // Outer scope on purpose: std.reload() never returns.
        if (pendingReload === "main.js") { session.launching = "main.js"; saveSession(session); }
        Screen.clear(Color.new(0, 0, 0));
        Screen.flip();
        std.reload(pendingReload);
    }
    if (pendingC) {
        Screen.clear(Color.new(0, 0, 0));
        Screen.flip();
        System.loadELF(System.boot_path.replace(/\/$/, "") + "/" + DEMOS_ELF,
            ["--app=" + pendingC.id, "--return=athena.elf", "--return-script=main.js"]);
    }
}
