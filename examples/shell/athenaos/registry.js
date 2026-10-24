// AthenaOS app registry.
//
// Every folder under apps/ with an app.json and a main.js is an app:
//   apps/pong/app.json  {"name": "Pong", "author": "...", "version": "...",
//                        "icon": "pong_icon.png", "category": "games", "blurb": "...", "exit": "START"}
//   apps/pong/main.js
// `icon`, `category`, `blurb` and `exit` are optional; "js": {...} / "lua": {...}
// override fields for one language. Apps run from the boot folder, like the
// shell: `icon` and every asset path are relative to it.

export const CATEGORIES = [
    { id: "all",     label: "All",      rgb: [104, 112, 255] },
    { id: "games",   label: "Games",    rgb: [255,  95, 120] },
    { id: "3d",      label: "3D",       rgb: [ 40, 160, 255] },
    { id: "2d",      label: "2D",       rgb: [ 40, 210, 160] },
    { id: "media",   label: "Media",    rgb: [255, 170,  40] },
    { id: "network", label: "Network",  rgb: [ 90, 200, 255] },
    { id: "input",   label: "Input",    rgb: [170,  90, 255] },
    { id: "system",  label: "System",   rgb: [150, 160, 180] },
];

export const categoryOf = (id) => CATEGORIES.find(c => c.id === id) || CATEGORIES[CATEGORIES.length - 1];

export const APPS_DIR = "apps";
// Every C app, run with --app=<id> (examples/common/demos_main.c).
export const DEMOS_ELF = "athena_demos.elf";

function readMeta(dir) {
    const text = std.loadFile(`${dir}/app.json`);
    if (!text) return null;
    try {
        const meta = JSON.parse(text);
        return meta.js ? Object.assign(meta, meta.js) : meta;
    } catch (e) {
        console.log(`[athenaos] bad ${dir}/app.json: ${e}`);
        return null;
    }
}

export function scanApps() {
    const apps = [];
    System.listDir(APPS_DIR).forEach(entry => {
        if (!entry.dir || entry.name[0] === ".") return;
        const dir = `${APPS_DIR}/${entry.name}`;
        const file = `${dir}/main.js`;
        if (!std.exists(file)) return;

        const meta = readMeta(dir);
        if (!meta || !meta.name) return;

        apps.push({
            id: entry.name,
            name: meta.name,
            author: meta.author || "Unknown",
            version: meta.version || "",
            file,
            c: std.exists(`${dir}/main.c`) && std.exists(DEMOS_ELF),
            iconPath: (meta.icon && std.exists(meta.icon)) ? meta.icon : null,
            icon: null,           // Image, loaded lazily by the shell
            category: categoryOf(meta.category || "system").id,
            blurb: meta.blurb || "",
            exit: meta.exit || "TRIANGLE",
        });
    });
    apps.sort((a, b) => a.name.localeCompare(b.name));
    return apps;
}

export function initials(name) {
    const words = name.replace(/[^A-Za-z0-9 ]/g, " ").split(" ").filter(w => w.length);
    if (words.length === 0) return "?";
    if (words.length === 1) return words[0].slice(0, 2).toUpperCase();
    return (words[0][0] + words[1][0]).toUpperCase();
}
