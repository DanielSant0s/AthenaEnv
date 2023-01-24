Sound.setVolume(100);
Sound.setADPCMVolume(0, 100);
let audio = Sound.loadADPCM("sfx/interface.adp");
let stop = Sound.loadADPCM("sfx/stop.adp");
//let nokia = Sound.loadADPCM("sfx/nokia.adp");
Sound.loadPlay("sfx/gotye.ogg");

let repeat = false;
let pad = 0;
let oldpad = 0;

while(true){
    oldpad = pad;
    pad = Pads.get();

    if(Pads.check(pad, Pads.START) && !Pads.check(oldpad, Pads.START)) {
        if(Sound.isPlaying()) {
            Sound.pause();
        } else {
            Sound.resume();
        }
    }

    if(Pads.check(pad, Pads.CROSS)){
        Sound.playADPCM(0, audio);
    } else if(Pads.check(pad, Pads.SQUARE)) {
        Sound.playADPCM(0, stop);
    } else if(Pads.check(pad, Pads.SELECT)) {
        Sound.stop();
    }
}