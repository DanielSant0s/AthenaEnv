// AthenaOS persistence: small JSON files next to the shell.
// Writes are best-effort -- booting from read-only media (CD/DVD) just keeps
// everything in memory for the current session.

const DIR = "athenaos/";

export function readJSON(name, fallback) {
    try {
        const text = std.loadFile(DIR + name);
        if (text) return Object.assign({}, fallback, JSON.parse(text));
    } catch (e) {
        console.log(`[athenaos] could not read ${name}: ${e}`);
    }
    return Object.assign({}, fallback);
}

export function writeJSON(name, value) {
    try {
        const f = std.open(DIR + name, "w");
        if (!f) return false;
        f.puts(JSON.stringify(value));
        f.close();
        return true;
    } catch (e) {
        console.log(`[athenaos] could not write ${name}: ${e}`);
        return false;
    }
}

export const DEFAULT_SETTINGS = {
    accent: 0,
    wallpaper: 0,        // index into WALLPAPERS (see main.js)
    clock24: true,
    utcOffset: 0,        // hours added to the console clock
    lockScreen: true,    // show the lock screen on cold boot
    showFps: false,
    animations: true,
};

export const DEFAULT_SESSION = {
    launching: null,     // app started from the shell; null means cold boot
    recents: [],         // most recent first
    launches: {},        // file -> launch count
    pinned: ["apps/starfox/main.js", "apps/render/main.js", "apps/physics_collision/main.js", "apps/sound/main.js", "apps/app_filemgr/main.js"],
};

export const loadSettings = () => readJSON("settings.json", DEFAULT_SETTINGS);
export const saveSettings = (s) => writeJSON("settings.json", s);
export const loadSession = () => readJSON("session.json", DEFAULT_SESSION);
export const saveSession = (s) => writeJSON("session.json", s);
