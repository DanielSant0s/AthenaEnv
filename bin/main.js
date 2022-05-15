Font.fmLoad();

var oldpad = Pads.get();
var pad = Pads.get();

var wallpaper = Graphics.loadImage("owl.png");

function getStringSize(string, scale){
    return string.length * (scale*15);
};

function range(end) {
    var arr = [];
    for (var i = 0; i < end; i++) {
      arr.push(i);
    }
    return arr;
};

function printCentered(x, y, scale, string){
    Font.fmPrint(x-(getStringSize(string, scale)/2), y, scale, string);
};

dofile("app_system.js");

dofile("file_manager.js");
dofile("task_manager.js");

var apps = [file_manager, task_manager];
var apps_idx = range(2);

while(true){
    oldpad = pad;
    pad = Pads.get();
    Display.clear(Color.new(0, 0, 0));
    if (Pads.check(pad, PAD_R3) && !Pads.check(oldpad, PAD_R3)){
        act_app++;
        apps.push(apps.shift());
        apps_idx.push(apps_idx.shift());
        if(act_app >= apps.length){
            act_app = 0;
        }
        apps[apps_idx[act_app]].minimized = false;
    };

    if (Pads.check(pad, PAD_L3) && !Pads.check(oldpad, PAD_L3)){
        apps[apps_idx[act_app]].minimized = true;
        act_app++;
        apps.push(apps.shift());
        apps_idx.push(apps_idx.shift());
        if(act_app >= apps.length){
            act_app = 0;
        }
    };

    Graphics.drawImage(wallpaper, 0.0, 0.0);

    Font.fmPrint(520.0, 430.0, 0.45, "AthenaOS proto");

    //prog = System.getFileProgress();

    for(var i = (apps.length-1); i >= 0; i--){
        apps[i].run();
    }
    
    Display.flip();
};
