// AthenaOS app SDK -- gives every app the same chrome as the shell.
//
//   import { AppUI, exitToShell } from 'athenaos/app.js';
//
//   const ui = new AppUI({ title: "Render demo", category: "3d",
//                          hints: [["cross", "Light"], ["D-pad", "Model"]] });
//   while (true) {
//       pad.update();
//       if (ui.homeRequested(pad)) break;   // Triangle, same in every app
//       ui.background();                     // skip in full-screen apps (3D, video...)
//       ...draw the app inside ui.content...
//       ui.hud(["60 FPS", "1234 tris"]);     // optional stats card
//       ui.chrome();                         // top bar + footer hints
//       Screen.flip();
//   }
//   exitToShell();                           // from the outermost scope
//
// Layout (640x448): top bar 0-26, content ui.content, footer H-34..H.

import { rgba, accent as accentColor, mix, COLORS, ACCENTS, roundRect, panel, vgradient, hints } from 'athenaos/gfx.js';
import { categoryOf } from 'athenaos/registry.js';
import { loadSettings } from 'athenaos/storage.js';
import { text, textV, measure, fit, wrap, margin, clockText, BAR_H, FOOTER_H } from 'athenaos/ui.js';

export const SHELL = "main.js";

// std.reload() never returns; call this from the outermost scope of the app
// (not from a nested callback) so nothing stays referenced.
export function exitToShell() {
    Screen.clear(Color.new(0, 0, 0));
    Screen.flip();
    std.reload(SHELL);
}

let sharedFont = null;
export function uiFont() {
    if (!sharedFont) sharedFont = new Font("fonts/Segoe UI.ttf");
    return sharedFont;
}

export { COLORS, rgba };

export class AppUI {
    // title     - shown in the top bar
    // category  - registry category id, colors the top bar dot
    // hints     - [[glyph, label], ...] for the footer; Home is appended
    // home      - footer label for the home control, or null to hide it
    constructor({ title, category = "system", hints: hintList = [], home = ["triangle", "Home"] }) {
        const mode = Screen.getMode();
        this.W = mode.width;
        this.H = mode.height;
        this.M = margin(this.W);
        this.settings = loadSettings();
        this.font = uiFont();
        this.title = title;
        this.category = categoryOf(category);
        this.hintList = hintList;
        this.home = home;
        this.status = "";
        this.content = { x: this.M, y: BAR_H + 14, w: this.W - 2 * this.M, h: this.H - BAR_H - FOOTER_H - 28 };
    }

    accent(a = 1.0) { return accentColor(this.settings.accent, a); }

    homeRequested(pad) { return pad.justPressed(Pads.TRIANGLE); }

    // --- Drawing -----------------------------------------------------------

    // Same backdrop as the shell's "Midnight" wallpaper, tinted by the accent.
    background() {
        const top = mix([18, 20, 30], ACCENTS[this.settings.accent % ACCENTS.length].rgb, 0.12);
        Screen.clear(Color.new(0, 0, 0));
        vgradient(0, 0, this.W, this.H, rgba(top[0], top[1], top[2]), rgba(4, 4, 8));
    }

    text(x, y, str, scale = 0.5, color = COLORS.text, align) { text(this.font, x, y, str, scale, color, align); }
    textV(x, cy, str, scale = 0.5, color = COLORS.text, align) { textV(this.font, x, cy, str, scale, color, align); }
    measure(str, scale = 0.5) { return measure(this.font, str, scale); }
    fit(str, scale, maxW) { return fit(this.font, str, scale, maxW); }
    wrap(str, scale, maxW) { return wrap(this.font, str, scale, maxW); }

    panel(x, y, w, h, r = 14) { panel(x, y, w, h, r, COLORS.glassHi); }

    // Section heading inside the content area.
    heading(x, cy, str) { this.textV(x, cy, str.toUpperCase(), 0.38, this.accent()); }

    // Translucent stats card; top-left under the bar by default, or pinned to
    // the right edge / just above the footer.
    hud(lines, { x = this.M, y = BAR_H + 10, right = false, bottom = false } = {}) {
        if (!lines.length) return;
        const scale = 0.40, lh = 15;
        const w = Math.round(Math.max(...lines.map(l => this.measure(l, scale)))) + 20;
        const h = lines.length * lh + 10;
        if (right) x = this.W - this.M - w;
        if (bottom) y = this.H - FOOTER_H - 10 - h;
        roundRect(x, y, w, h, 8, rgba(0, 0, 0, 0.45));
        lines.forEach((l, i) => this.textV(x + 10, y + 5 + lh / 2 + i * lh, l, scale, i === 0 ? COLORS.text : COLORS.textDim));
    }

    header(status = this.status) {
        const cy = BAR_H / 2;
        Draw.rect(0, 0, this.W, BAR_H, rgba(0, 0, 0, 0.45));
        const c = this.category.rgb;
        Draw.circle(this.M + 4, cy, 4, rgba(c[0], c[1], c[2]), true);
        this.textV(this.M + 14, cy, this.title, 0.48, COLORS.text);

        let x = this.W - this.M;
        const clock = clockText(this.settings);
        x -= this.measure(clock, 0.48);
        this.textV(x, cy, clock, 0.48, COLORS.text);
        if (status) {
            x -= 16 + this.measure(status, 0.44);
            this.textV(x, cy, status, 0.44, COLORS.textDim);
        }
    }

    footer(hintList = this.hintList, left = "") {
        const y = this.H - FOOTER_H, cy = y + FOOTER_H / 2;
        Draw.rect(0, y, this.W, FOOTER_H, rgba(0, 0, 0, 0.45));
        const list = this.home ? [...hintList, this.home] : hintList;
        const leftW = left ? Math.round(this.measure(left, 0.44)) + 20 : 0;
        hints(list, this.W - this.M, cy, this.font, this.W - 2 * this.M - leftW);
        if (left) this.textV(this.M, cy, left, 0.44, COLORS.textDim);
    }

    // Chrome for full-screen media: shown while the pad is in use, hidden
    // after ~4 s idle so it does not cover the picture. Call every frame.
    autoChrome(pad, hintList = this.hintList, left = "") {
        this.idle = (pad.btns !== 0 || Math.abs(pad.lx) > 64 || Math.abs(pad.ly) > 64) ? 0 : (this.idle || 0) + 1;
        if (this.idle < 240) this.chrome(hintList, left);
    }

    // Top bar + footer. Disables depth test so it also draws over 3D scenes.
    chrome(hintList = this.hintList, left = "") {
        const depth = Screen.getParam(Screen.DEPTH_TEST_ENABLE);
        if (depth) Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);
        this.header();
        this.footer(hintList, left);
        if (depth) Screen.setParam(Screen.DEPTH_TEST_ENABLE, depth);
    }

    // One-shot frame for long blocking work (asset loading, network calls).
    loading(message, detail = "") {
        this.background();
        const cy = this.H / 2;
        this.textV(this.W / 2, cy - 10, message, 0.62, COLORS.text, Font.ALIGN_HCENTER);
        if (detail) this.textV(this.W / 2, cy + 16, detail, 0.44, COLORS.textDim, Font.ALIGN_HCENTER);
        this.chrome([], "");
        Screen.flip();
    }
}

// Scrolling text console with the standard chrome, for demos that mostly log.
//   const term = new Terminal({ title: "Keyboard demo", category: "input", subtitle: "..." });
//   term.log("hello");
//   while (true) { pad.update(); if (term.homeRequested(pad)) break; term.draw(); }
export class Terminal extends AppUI {
    constructor(opts) {
        super(opts);
        this.subtitle = opts.subtitle || "";
        this.lines = [];
        this.maxLines = 20;
        Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);
    }

    log(...args) {
        const str = args.map(a => (typeof a === "string") ? a : JSON.stringify(a)).join(" ");
        console.log(str);
        const maxW = this.content.w - 24;
        str.split("\n").forEach(line => {
            // Wrap on width; hard-split words that are longer than a line (JSON, URLs).
            this.wrap(line.replace(/\t/g, "    "), 0.42, maxW).forEach(w => {
                while (this.measure(w, 0.42) > maxW && w.length > 1) {
                    let n = w.length;
                    while (n > 1 && this.measure(w.slice(0, n), 0.42) > maxW) n = Math.floor(n * 0.9);
                    this.lines.push(w.slice(0, n));
                    w = w.slice(n);
                }
                this.lines.push(w);
            });
            if (!line.length) this.lines.push("");
        });
        if (this.lines.length > this.maxLines) this.lines.splice(0, this.lines.length - this.maxLines);
    }

    clear() { this.lines = []; }

    // Draws without flipping, so callers can add their own overlay.
    render() {
        this.background();
        const { x, y, w, h } = this.content;
        if (this.subtitle) this.textV(x, y + 6, this.subtitle, 0.46, COLORS.textDim);
        const top = this.subtitle ? y + 20 : y;
        roundRect(x, top, w, h - (top - y), 10, rgba(0, 0, 0, 0.35));
        this.lines.forEach((line, i) => this.textV(x + 12, top + 14 + i * 15, line, 0.42, COLORS.text));
        this.chrome();
    }

    draw() {
        this.render();
        Screen.flip();
    }
}
