// {"name": "Audio player", "author": "Daniel Santos", "version": "04072023", "icon": "sap_icon.png", "file": "sound.js"}

const segoe_ui = new Font("fonts/Segoe UI.ttf");

const purple = Color.new(64, 0, 128);

Sound.setVolume(100);

let timer = Timer.new();

const sound_table = System.listDir("/sounds").map(file => file.name).filter(str => str.endsWith(".ogg") || str.endsWith(".wav") );

const sounds = Array();

sound_table.forEach(name => {
    let sound_name = name.replace(".ogg", "").replace(".wav", "");
    sounds.push({name:sound_name, file:name, size:segoe_ui.getTextSize(sound_name)});
});

let cur_sound = 0;

var track = Sound.load("sounds/" + sounds[0].file);
var duration = Sound.duration(track);
Sound.play(track);
Timer.reset(timer);

let repeat = false;
let pad = Pads.get();

const icons = {play:new Image("amp/play.png"), pause:new Image("amp/pause.png"), next:new Image("amp/next.png"), back:new Image("amp/back.png")};
icons.play.color = purple;
icons.play.width /= 3;
icons.play.height /= 3;

icons.pause.color = purple;
icons.pause.width /= 3;
icons.pause.height /= 3;

icons.next.color = purple;
icons.next.width /= 3;
icons.next.height /= 3;

icons.back.color = purple;
icons.back.width /= 3;
icons.back.height /= 3;

let playing = true;

var cur_duration = 0.0f;

let text_size = null;

console.log(JSON.stringify(Tasks.get()));

while(true){
    Screen.clear(Color.new(32, 32, 32));
    pad.update();

    if(pad.justPressed(Pads.START)) {
        if(Sound.isPlaying()) {
            Sound.pause(track);
            Timer.pause(timer);
            playing = false;
        } else {
            Sound.resume(track);
            Timer.resume(timer);
            playing = true;
        }
    }

    if(pad.justPressed(Pads.LEFT)) {
        cur_duration = 0;
        Sound.pause(track);
        sounds.unshift(sounds.pop());
        let new_track = Sound.load("sounds/" + sounds[0].file);
        duration = Sound.duration(new_track);
        Timer.reset(timer);
        Sound.play(new_track);
        let old_track = track;
        track = new_track;
        Sound.free(old_track);
    }

    if(pad.justPressed(Pads.RIGHT)) {
        cur_duration = 0;
        Sound.pause(track);
        sounds.push(sounds.shift());
        let new_track = Sound.load("sounds/" + sounds[0].file);
        duration = Sound.duration(new_track);
        Timer.reset(timer);
        Sound.play(new_track);
        let old_track = track;
        track = new_track;
        Sound.free(old_track);
    }

    if(pad.justPressed(Pads.TRIANGLE)) {
        break;
    }

    if(playing) {
        icons.pause.draw(320 - icons.play.width/2, 224 - icons.play.height/2 + 100);
    } else {
        icons.play.draw(320 - icons.play.width/2, 224 - icons.play.height/2 + 100);
    }

    if (cur_duration < 1) {
        cur_duration = (Math.floor(Timer.getTime(timer))/duration);
    }

    segoe_ui.print(320 - sounds[0].size.width/2, 224 - sounds[0].size.height/2, sounds[0].name);

    Draw.rect(120, 260, 400, 8, purple);
    Draw.rect(120, 260, 400*cur_duration, 8, Color.new(127, 0, 255));

    icons.next.draw(320 - icons.next.width/2 + 150, 224 - icons.next.height/2 + 100);
    icons.back.draw(320 - icons.back.width/2 - 150, 224 - icons.back.height/2 + 100);
    
    Screen.flip();
}

Timer.destroy(timer);
Sound.deinit();
