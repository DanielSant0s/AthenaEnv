//System.threadCopyFile(src, dst + "/" + src);
//var prog = System.getFileProgress(); 

file_manager = new App();
file_manager.data = [0, 0, 0, 1];
file_manager.comp = 0;

var root = [{name:"cdfs:", size:0, dir:true}, 
            {name:"host:", size:0, dir:true}, 
            {name:"mass:", size:0, dir:true}, 
            {name:"mc0:", size:0, dir:true},
            {name:"mc1:", size:0, dir:true}];

var path = "";
var file = root;

var src = "";
var srcfile = "";
var dst = "";
var fileop = 0;

file_manager.process = function() {
    if(file_manager.data[2] == 1){
        file_manager.data[1] = process_list_commands(file_manager.data[1], range(5));
    }
    if(file_manager.data[3] == 1){
        file_manager.data[0] = process_list_commands(file_manager.data[0], file);
        if(file_manager.data[0] > 20){
            file_manager.comp = -(file_manager.data[0]-20)
        } else {
            file_manager.comp = 0;
        }
    }
    if ((Pads.check(pad, PAD_TRIANGLE) && !Pads.check(oldpad, PAD_TRIANGLE))){
        var idxof = path.lastIndexOf("/");
        if(idxof != -1){
            path = path.slice(0, idxof);
            file = System.listDir(path);
        } else {
            path = "";
            file = root;
        };
    };
    if ((Pads.check(pad, PAD_CROSS) && !Pads.check(oldpad, PAD_CROSS)) || keyboard.hole){
        if(file_manager.data[3] == 1){
            if(file[file_manager.data[0]].dir){
                if(!keyboard.hole){
                    if(path == ""){
                        path = file[file_manager.data[0]].name;
                    } else {
                        path = path + "/" + file[file_manager.data[0]].name;
                    }
                    
                }
                file = System.listDir(path);

            } else if(file[file_manager.data[0]].name.endsWith(".js")){
                System.currentDir(path + "/");
                dofile(file[file_manager.data[0]].name);
            } else if(file[file_manager.data[0]].name.endsWith(".elf")){
                System.loadELF(path + "/"+ file[file_manager.data[0]].name);
            }
        }
        if(file_manager.data[2] == 1){
            switch (file_manager.data[1]) {
                case 0:
                    fileop = 0;
                    src = path + "/" + file[file_manager.data[0]].name;
                    srcfile = file[file_manager.data[0]].name;
                    file_manager.data[2] ^= 1;
                    file_manager.data[3] ^= 1;
                    file = System.listDir(path);
                    break;
                case 1:
                    fileop = 1;
                    src = path + "/" + file[file_manager.data[0]].name;
                    srcfile = file[file_manager.data[0]].name;
                    file_manager.data[2] ^= 1;
                    file_manager.data[3] ^= 1;
                    file = System.listDir(path);
                    break;
                case 2:
                    dst = path + "/" + srcfile;
                    switch (fileop){
                        case 0:
                            System.copyFile(src, dst);
                            break;
                        case 1:
                            System.moveFile(src, dst);
                            break;
                    };
                    file_manager.data[2] ^= 1;
                    file_manager.data[3] ^= 1;
                    file = System.listDir(path);
                    break;
                case 3:
                    console.log("Hole " + keyboard.hole +"\n");
                    if(keyboard.hole){
                        System.rename(path + "/"+file[file_manager.data[0]].name, path + "/"+keyboard.getinput());
                        keyboard.hole = false;
                        file_manager.data[2] ^= 1;
                        file_manager.data[3] ^= 1;
                        file = System.listDir(path);
                    } else {
                        console.log(path+keyboard.getinput()+"\n");
                        keyboard.invoke(file_manager.id);
                    }
                    break;
                case 4:
                    System.removeFile(path + "/" + file[file_manager.data[0]].name);
                    file_manager.data[2] ^= 1;
                    file_manager.data[3] ^= 1;
                    file = System.listDir(path);
                    break;
            }
        }
    };
    if (Pads.check(pad, PAD_R1) && !Pads.check(oldpad, PAD_R1)){
        file_manager.data[2] ^= 1;
        file_manager.data[3] ^= 1;
    };
};

file_manager.gfx = new Window();
file_manager.gfx.t = "File Manager"
file_manager.gfx.x = 0;
file_manager.gfx.y = 0;
file_manager.gfx.w = 640;
file_manager.gfx.h = 448;

render_filelist = function() {
    Graphics.drawRect(file_manager.gfx.x, file_manager.gfx.y+(20*(file_manager.data[0]+2+file_manager.comp)), file_manager.gfx.w, 20, Color.new(64, 0, 128, 64));
    Graphics.drawRect(file_manager.gfx.x, file_manager.gfx.y+(20), file_manager.gfx.w, 20, Color.new(64, 64, 64, 64));
    printCentered(file_manager.gfx.x+(file_manager.gfx.w/2), file_manager.gfx.y+(3+(20.0)), 0.55, path);
    for (var i = 0; i < file.length; i++) {
        if(i+file_manager.comp < 21 && i+file_manager.comp >= 0){
            Font.fmPrint(file_manager.gfx.x+10, file_manager.gfx.y+(3+(20.0*(i+2+file_manager.comp))), 0.55, 
            file[i].name);
            if(!file[i].dir){
                Font.fmPrint(file_manager.gfx.x+200, file_manager.gfx.y+(3+(20.0*(i+2+file_manager.comp))), 0.55, 
                String(file[i].size));
            }
        }
    };
    if(file_manager.data[2] == 1) {
        Graphics.drawRect(file_manager.gfx.x+file_manager.gfx.w-75, file_manager.gfx.y+40, 75, 20*5, Color.new(0, 0, 0, 100));
        Graphics.drawRect(file_manager.gfx.x+file_manager.gfx.w-75, file_manager.gfx.y+20+20*(file_manager.data[1]+1), 75, 20, Color.new(64, 0, 128, 64));
        Font.fmPrint(file_manager.gfx.x+file_manager.gfx.w-75+5, file_manager.gfx.y+(23+(20.0*(1))), 0.55, "Copy");
        Font.fmPrint(file_manager.gfx.x+file_manager.gfx.w-75+5, file_manager.gfx.y+(23+(20.0*(2))), 0.55, "Move");
        Font.fmPrint(file_manager.gfx.x+file_manager.gfx.w-75+5, file_manager.gfx.y+(23+(20.0*(3))), 0.55, "Paste");
        Font.fmPrint(file_manager.gfx.x+file_manager.gfx.w-75+5, file_manager.gfx.y+(23+(20.0*(4))), 0.55, "Rename");
        Font.fmPrint(file_manager.gfx.x+file_manager.gfx.w-75+5, file_manager.gfx.y+(23+(20.0*(5))), 0.55, "Delete");
    };
};

file_manager.gfx.add(render_filelist);

/*
render_progress = function() {
    printCentered(fileman.x-20+(fileman.w/2), fileman.y+35.0, 0.5, "Copying " + src + " to " + dst);
    printCentered(fileman.x-20+(fileman.w/2), fileman.y+140.0, 0.5, String(Math.round((prog.current/prog.final)*100))+"%");
    prog = System.getFileProgress();
    Graphics.drawRect(fileman.x+20, fileman.y+(fileman.h-60), (fileman.w-40)*(prog.current/prog.final), 20, Color.new(64, 0, 128, 64));
};

fileman.add(render_progress);
*/