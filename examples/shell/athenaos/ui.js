// AthenaOS shared UI helpers: text layout, clock and layout metrics.
// Used by the shell (main.js) and by apps through athenaos/app.js, so both
// render text and time the same way.

import { textTop } from 'athenaos/gfx.js';

// --- Layout ---------------------------------------------------------------

// Left/right margin shared by the shell and every app (48px at 640 wide).
export const margin = (width) => Math.round(width * 0.075);

export const BAR_H = 26;        // top app/status bar
export const FOOTER_H = 34;     // bottom hint bar in apps

// --- Text -----------------------------------------------------------------

export function text(font, x, y, str, scale, color, align = Font.ALIGN_NONE) {
    font.scale = scale;
    font.color = color;
    font.align = align;
    font.print(x, y, str);
    font.align = Font.ALIGN_NONE;
}

// Same as text(), but vertically centered on `cy` (see textTop in gfx.js).
export function textV(font, x, cy, str, scale, color, align = Font.ALIGN_NONE) {
    text(font, Math.round(x), textTop(cy, scale), str, scale, color, align);
}

export function measure(font, str, scale) {
    font.scale = scale;
    return font.getTextSize(str).width;
}

export function fit(font, str, scale, maxW) {
    if (measure(font, str, scale) <= maxW) return str;
    while (str.length > 1 && measure(font, str + "..", scale) > maxW) str = str.slice(0, -1);
    return str.trim() + "..";
}

export function wrap(font, str, scale, maxW) {
    const lines = [];
    let line = "";
    str.split(" ").forEach(word => {
        const next = line ? line + " " + word : word;
        if (line && measure(font, next, scale) > maxW) { lines.push(line); line = word; }
        else line = next;
    });
    if (line) lines.push(line);
    return lines;
}

// --- Clock ----------------------------------------------------------------

const DAYS = ["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"];
const MONTHS = ["January", "February", "March", "April", "May", "June", "July",
                "August", "September", "October", "November", "December"];
export const pad2 = (n) => (n < 10 ? "0" : "") + n;

// The console clock shifted by the user's offset; read with getUTC* methods.
export const now = (settings) => new Date(Date.now() + settings.utcOffset * 3600000);

export function clockText(settings, d = now(settings)) {
    const h = d.getUTCHours(), m = pad2(d.getUTCMinutes());
    if (settings.clock24) return `${pad2(h)}:${m}`;
    return `${(h % 12) || 12}:${m} ${h < 12 ? "AM" : "PM"}`;
}

export const dateText = (settings, d = now(settings)) =>
    `${DAYS[d.getUTCDay()]}, ${MONTHS[d.getUTCMonth()]} ${d.getUTCDate()}`;
