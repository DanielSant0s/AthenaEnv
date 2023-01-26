Sound.setVolume(100);
Sound.setVolume(100, 0);
let audio = Sound.load("sfx/interface.adp");
console.log("interface.adp duration:", Sound.duration(audio));
let stop = Sound.load("sfx/stop.adp");
console.log("stop.adp duration:", Sound.duration(stop));
let gotye = Sound.load("sfx/gotye.ogg");
console.log("gotye.ogg duration:", Sound.duration(gotye));
Sound.play(gotye);

let nokia_tune = Sound.load("sfx/tune.wav");
console.log("tune.wav duration:", Sound.duration(nokia_tune));

System.sleep(10);
Sound.pause(gotye);
System.sleep(1);
Sound.play(nokia_tune);

let repeat = false;
let pad = 0;
let oldpad = 0;

while(true){
    Screen.clear(Color.new(64, 0, 128));
    oldpad = pad;
    pad = Pads.get();

    if(Pads.check(pad, Pads.START) && !Pads.check(oldpad, Pads.START)) {
        if(Sound.isPlaying()) {
            Sound.pause(nokia_tune);
        } else {
            Sound.resume(gotye);
        }
    }

    if(Pads.check(pad, Pads.CROSS)){
        Sound.play(audio, 0);
    } else if(Pads.check(pad, Pads.SQUARE)) {
        Sound.play(stop, 0);
    } else if(Pads.check(pad, Pads.SELECT)) {
        Sound.deinit();
    }
    Screen.flip();
}