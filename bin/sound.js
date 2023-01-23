Sound.setVolume(100);
Sound.setADPCMVolume(0, 100);
let audio = Sound.loadADPCM("sfx/interface.adp");
let stop = Sound.loadADPCM("sfx/stop.adp");
let nokia = Sound.loadADPCM("sfx/nokia.adp");

let pad = 0;

while(true){
    pad = Pads.get();
    if(Pads.check(pad, Pads.CROSS)){
        Sound.playADPCM(0, audio);
    } else if(Pads.check(pad, Pads.SQUARE)) {
        Sound.playADPCM(0, stop);
    } else if(Pads.check(pad, Pads.START)) {
        Sound.playADPCM(0, nokia);
    }
}