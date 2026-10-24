
import { AppUI, COLORS, rgba, exitToShell } from 'athenaos/app.js';

const ui = new AppUI({ title: "Audio player", category: "media",
                       hints: [["START", "Play/Pause"], ["L1", "Prev"], ["R1", "Next"], ["D-pad", "Seek"], ["square", "Rewind"]] });

Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);

Sound.setVolume(100);

const sounds = System.listDir("/sounds").map(file => file.name)
    .filter(str => str.endsWith(".ogg") || str.endsWith(".wav"))
    .map(file => ({ name: file.replace(".ogg", "").replace(".wav", ""), file }));

let current = 0;
let track = Sound.Stream("sounds/" + sounds[current].file);
let duration = track.length;

track.play();

let pad = Pads.get();

const icons = { play: new Image("amp/play.png"), pause: new Image("amp/pause.png"),
                next: new Image("amp/next.png"), back: new Image("amp/back.png") };
for (const k in icons) {
    icons[k].width /= 4;
    icons[k].height /= 4;
    icons[k].filter = LINEAR;
}

function msToSecondMin(ms) {
    let minutes = Math.floor(ms / 60000)
    let seconds = Math.round(ms / 1000)

    seconds -= minutes * 60

    if (seconds < 10) {
        return minutes + ":0" + seconds
    } else {
        return minutes + ":" + seconds
    }
}

function switchTrack(index) {
    track.pause();
    current = (index + sounds.length) % sounds.length;
    let new_track = Sound.Stream("sounds/" + sounds[current].file);
    duration = new_track.length;
    new_track.play();
    let old_track = track;
    track = new_track;
    old_track.free();
}

function drawIcon(img, cx, cy, color) {
    img.color = color;
    img.draw(cx - img.width / 2, cy - img.height / 2);
}

while(true){
    pad.update();

    if(ui.homeRequested(pad)) {
        break;
    }

    if(pad.justPressed(Pads.START)) {
        if(track.playing()) {
            track.pause();
        } else {
            track.play();
        }
    }

    if(pad.justPressed(Pads.L1)) switchTrack(current - 1);
    if(pad.justPressed(Pads.R1)) switchTrack(current + 1);

    if(pad.justPressed(Pads.RIGHT) && track.playing()) {
        track.position = track.position + 5000;
    } else if(pad.justPressed(Pads.LEFT) && track.playing()) {
        track.position = track.position - 5000;
    }

    if(pad.justPressed(Pads.SQUARE)) {
        track.rewind();
    }

    ui.background();
    const { x, y, w, h } = ui.content;

    // Playlist
    const listW = 200;
    ui.panel(x, y, listW, h);
    ui.heading(x + 16, y + 20, `Playlist - ${sounds.length} tracks`);
    sounds.forEach((s, i) => {
        const ry = y + 40 + i * 30;
        if (i === current) {
            Draw.rect(x + 8, ry, listW - 16, 26, rgba(255, 255, 255, 0.10));
            Draw.rect(x + 8, ry, 3, 26, ui.accent());
        }
        ui.textV(x + 20, ry + 13, ui.fit(s.name, 0.46, listW - 40), 0.46, i === current ? COLORS.text : COLORS.textDim);
    });

    // Now playing
    const px = x + listW + 16, pw = w - listW - 16, cx = Math.round(px + pw / 2);
    ui.panel(px, y, pw, h);
    ui.heading(px + 20, y + 20, track.playing() ? "Now playing" : "Paused");
    ui.textV(cx, y + 110, ui.fit(sounds[current].name, 0.8, pw - 40), 0.8, COLORS.text, Font.ALIGN_HCENTER);

    const position = track.position;
    const t = duration ? Math.min(position / duration, 1) : 0;
    const bx = px + 30, bw = pw - 60, by = y + 170;
    Draw.rect(bx, by, bw, 6, rgba(255, 255, 255, 0.15));
    Draw.rect(bx, by, bw * t, 6, ui.accent());
    Draw.circle(bx + bw * t, by + 3, 7, COLORS.text, true);
    ui.textV(bx, by + 24, msToSecondMin(position), 0.44, COLORS.textDim);
    ui.textV(bx + bw, by + 24, msToSecondMin(duration), 0.44, COLORS.textDim, Font.ALIGN_RIGHT);

    const iy = y + 260;
    drawIcon(icons.back, cx - 90, iy, rgba(255, 255, 255, 0.8));
    Draw.circle(cx, iy, 30, ui.accent(), true);
    drawIcon(track.playing() ? icons.pause : icons.play, cx, iy, COLORS.text);
    drawIcon(icons.next, cx + 90, iy, rgba(255, 255, 255, 0.8));

    ui.chrome();
    Screen.flip();
}

track.pause();
track.free();
exitToShell();
