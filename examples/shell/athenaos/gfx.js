// AthenaOS drawing kit: palette, shapes, animation helpers and button glyphs.
// Colors use the GS convention: RGB 0-255, alpha 0-128 (128 = opaque).

export const rgba = (r, g, b, a = 1.0) => Color.new(r, g, b, Math.round(a * 128));

export const ACCENTS = [
    { name: "Indigo",  rgb: [104, 112, 255] },
    { name: "Azure",   rgb: [ 40, 160, 255] },
    { name: "Mint",    rgb: [ 40, 210, 160] },
    { name: "Amber",   rgb: [255, 170,  40] },
    { name: "Rose",    rgb: [255,  80, 130] },
    { name: "Violet",  rgb: [170,  90, 255] },
];

export function accent(index, a = 1.0) {
    const c = ACCENTS[index % ACCENTS.length].rgb;
    return rgba(c[0], c[1], c[2], a);
}

export function mix(c1, c2, t) {
    return [
        Math.round(c1[0] + (c2[0] - c1[0]) * t),
        Math.round(c1[1] + (c2[1] - c1[1]) * t),
        Math.round(c1[2] + (c2[2] - c1[2]) * t),
    ];
}

export const COLORS = {
    text:      rgba(255, 255, 255, 1.0),
    textDim:   rgba(255, 255, 255, 0.62),
    textFaint: rgba(255, 255, 255, 0.38),
    glass:     rgba( 18,  20,  32, 0.62),
    glassHi:   rgba( 40,  44,  64, 0.70),
    stroke:    rgba(255, 255, 255, 0.10),
    shadow:    rgba(  0,   0,   0, 0.30),
    scrim:     rgba(  0,   0,   0, 0.55),
    ok:        rgba( 70, 210, 120, 1.0),
    warn:      rgba(255, 190,  60, 1.0),
    bad:       rgba(255,  90,  90, 1.0),
};

// --- Animation ------------------------------------------------------------

export const clamp = (v, lo, hi) => (v < lo ? lo : (v > hi ? hi : v));
export const lerp = (a, b, t) => a + (b - a) * t;
export const easeOut = (t) => 1 - Math.pow(1 - clamp(t, 0, 1), 3);

// Critically-damped follower: call step() once per frame, read .v.
export class Spring {
    constructor(v = 0, speed = 0.25) { this.v = v; this.target = v; this.speed = speed; }
    set(target) { this.target = target; return this; }
    snap(v) { this.v = v; this.target = v; return this; }
    step() {
        this.v += (this.target - this.v) * this.speed;
        if (Math.abs(this.target - this.v) < 0.01) this.v = this.target;
        return this.v;
    }
}

// --- Shapes ---------------------------------------------------------------

// Unit quarter-arcs, one table per segment count (more segments for bigger radii).
const ARCS = [];
for (let steps = 2; steps <= 8; steps++) {
    const arc = [];
    for (let i = 0; i <= steps; i++) {
        const a = (i / steps) * (Math.PI / 2);
        arc.push([Math.cos(a), Math.sin(a)]);
    }
    ARCS[steps] = arc;
}
const arcFor = (r) => ARCS[clamp(Math.round(r / 4), 2, 6)];

// Quarter-circle fan centered at (cx, cy), sx/sy pick the quadrant (+1/-1).
function corner(cx, cy, r, sx, sy, color) {
    const arc = arcFor(r);
    for (let i = 0; i < arc.length - 1; i++) {
        Draw.triangle(cx, cy,
            cx + sx * arc[i][0] * r, cy + sy * arc[i][1] * r,
            cx + sx * arc[i + 1][0] * r, cy + sy * arc[i + 1][1] * r,
            color);
    }
}

// Pieces meet on whole pixels; fractional edges leave 1px seams on the GS.
function snapBox(x, y, w, h, r) {
    x = Math.round(x); y = Math.round(y); w = Math.round(w); h = Math.round(h);
    r = Math.round(Math.min(r, w / 2, h / 2));
    return [x, y, w, h, r];
}

// Non-overlapping pieces, so translucent fills blend evenly.
export function roundRect(x, y, w, h, r, color) {
    [x, y, w, h, r] = snapBox(x, y, w, h, r);
    if (r < 1) { Draw.rect(x, y, w, h, color); return; }
    Draw.rect(x + r, y, w - 2 * r, h, color);
    Draw.rect(x, y + r, r, h - 2 * r, color);
    Draw.rect(x + w - r, y + r, r, h - 2 * r, color);
    corner(x + r,     y + r,     r, -1, -1, color);
    corner(x + w - r, y + r,     r,  1, -1, color);
    corner(x + r,     y + h - r, r, -1,  1, color);
    corner(x + w - r, y + h - r, r,  1,  1, color);
}

// Rounded rect with a vertical gradient: solid caps + gradient middle band.
// Same corner geometry as roundRect, so a ring() around it stays concentric.
export function roundGradient(x, y, w, h, r, top, bottom) {
    [x, y, w, h, r] = snapBox(x, y, w, h, r);
    Draw.rect(x + r, y, w - 2 * r, r, top);
    corner(x + r,     y + r, r, -1, -1, top);
    corner(x + w - r, y + r, r,  1, -1, top);
    Draw.quad(x, y + r, x + w, y + r, x, y + h - r, x + w, y + h - r, top, top, bottom, bottom);
    Draw.rect(x + r, y + h - r, w - 2 * r, r, bottom);
    corner(x + r,     y + h - r, r, -1, 1, bottom);
    corner(x + w - r, y + h - r, r,  1, 1, bottom);
}

export function panel(x, y, w, h, r = 12, fill = COLORS.glass) {
    roundRect(x + 2, y + 4, w, h, r, COLORS.shadow);
    roundRect(x, y, w, h, r, fill);
    Draw.line(x + r, y, x + w - r, y, COLORS.stroke);
}

// Focus ring drawn as an outer rounded rect behind the item.
export function ring(x, y, w, h, r, color, t = 3) {
    roundRect(x - t, y - t, w + 2 * t, h + 2 * t, r + t, color);
}

export function vgradient(x, y, w, h, top, bottom) {
    Draw.quad(x, y, x + w, y, x, y + h, x + w, y + h, top, top, bottom, bottom);
}

export function gradient4(x, y, w, h, tl, tr, bl, br) {
    Draw.quad(x, y, x + w, y, x, y + h, x + w, y + h, tl, tr, bl, br);
}

export function bar(x, y, w, h, t, fill, track = rgba(255, 255, 255, 0.12)) {
    roundRect(x, y, w, h, h / 2, track);
    const fw = Math.max(h, w * clamp(t, 0, 1));
    roundRect(x, y, fw, h, h / 2, fill);
}

export function toggle(x, y, on, color) {
    roundRect(x, y, 36, 20, 10, on ? color : rgba(255, 255, 255, 0.20));
    Draw.circle(x + (on ? 26 : 10), y + 10, 7, COLORS.text, true);
}

// --- Text ------------------------------------------------------------------
// Font.print() takes the top of the em box and draws on a baseline below it.
// Cap-height center lands ~15.5px * scale - 2 under y (measured on hardware),
// so this returns the y that vertically centers a line on `cy`.
export const textTop = (cy, scale) => Math.round(cy - 15.5 * scale + 2);

// --- Controller glyphs ----------------------------------------------------

const GLYPH = {
    cross:    [124, 178, 232],
    circle:   [255, 102, 102],
    square:   [255, 105, 248],
    triangle: [ 64, 226, 160],
};

const PILL_SCALE = 0.40;
const LABEL_SCALE = 0.46;

function glyphWidth(kind, font) {
    if (GLYPH[kind]) return 16;
    font.scale = PILL_SCALE;
    return font.getTextSize(kind).width + 12;
}

// Draws a PlayStation button glyph vertically centered on `cy`, returns its width.
export function glyph(kind, x, cy, font) {
    x = Math.round(x); cy = Math.round(cy);
    if (GLYPH[kind]) {
        const c = GLYPH[kind];
        const col = rgba(c[0], c[1], c[2], 1.0);
        Draw.circle(x + 8, cy, 8, rgba(255, 255, 255, 0.16), true);
        switch (kind) {
            case "cross":
                Draw.line(x + 5, cy - 3, x + 11, cy + 3, col);
                Draw.line(x + 11, cy - 3, x + 5, cy + 3, col);
                break;
            case "circle":
                Draw.circle(x + 8, cy, 4, col, false);
                break;
            case "square":
                Draw.line(x + 5, cy - 3, x + 11, cy - 3, col);
                Draw.line(x + 11, cy - 3, x + 11, cy + 3, col);
                Draw.line(x + 11, cy + 3, x + 5, cy + 3, col);
                Draw.line(x + 5, cy + 3, x + 5, cy - 3, col);
                break;
            case "triangle":
                Draw.line(x + 8, cy - 4, x + 12, cy + 3, col);
                Draw.line(x + 12, cy + 3, x + 4, cy + 3, col);
                Draw.line(x + 4, cy + 3, x + 8, cy - 4, col);
                break;
        }
        return 16;
    }
    // Text pill: L1, R1, START, SELECT, D-pad...
    const w = glyphWidth(kind, font);
    roundRect(x, cy - 8, w, 16, 8, rgba(255, 255, 255, 0.16));
    font.scale = PILL_SCALE;
    font.color = COLORS.text;
    font.print(x + 6, textTop(cy, PILL_SCALE), kind);
    return w;
}

// Row of [glyph, label] hints, right-aligned at `right`, centered on `cy`.
// With `maxW`, gaps and then label size shrink until the row fits.
export function hints(list, right, cy, font, maxW = Infinity) {
    let gap = 18, labelScale = LABEL_SCALE;
    const glyphs = list.map(([g]) => glyphWidth(g, font));
    const measureRow = () => {
        font.scale = labelScale;
        const widths = list.map(([, label], i) => glyphs[i] + 6 + font.getTextSize(label).width);
        return [widths, widths.reduce((a, b) => a + b + gap, -gap)];
    };
    let [widths, total] = measureRow();
    if (total > maxW) { gap = 10; [widths, total] = measureRow(); }
    while (total > maxW && labelScale > 0.34) { labelScale -= 0.02; [widths, total] = measureRow(); }

    let x = right - total;
    list.forEach(([g, label], i) => {
        const gw = glyph(g, x, cy, font);
        font.scale = labelScale;
        font.color = COLORS.textDim;
        font.print(Math.round(x + gw + 6), textTop(cy, labelScale), label);
        x += widths[i] + gap;
    });
}
