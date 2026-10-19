// AthenaOS app registry.
//
// Any .js file in the boot folder whose first line is a JSON comment is an app:
//   // {"name": "Pong", "author": "...", "version": "...", "file": "pong.js",
//   //  "icon": "pong_icon.png", "category": "games", "blurb": "...", "exit": "START"}
// `icon`, `category`, `blurb` and `exit` are optional.

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

function readHeader(file) {
    const fd = std.open(file, "r");
    if (!fd) return null;
    const line = fd.getline();
    fd.close();
    if (!line) return null;
    const json = line.replace(/^\s*\/\/\s*/, "");
    if (json[0] !== "{") return null;
    try {
        return JSON.parse(json);
    } catch (e) {
        console.log(`[athenaos] bad app header in ${file}: ${e}`);
        return null;
    }
}

const SHELL_FILES = ["main.js", "UI.js"];

export function scanApps() {
    const apps = [];
    System.listDir().forEach(entry => {
        const file = entry.name;
        if (entry.dir || !file.toLowerCase().endsWith(".js") || SHELL_FILES.includes(file)) return;

        const meta = readHeader(file);
        if (!meta || !meta.name) return;

        apps.push({
            name: meta.name,
            author: meta.author || "Unknown",
            version: meta.version || "",
            file: meta.file || file,
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
