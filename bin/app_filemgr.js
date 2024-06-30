// {"name": "File manager", "author": "Daniel Santos", "version": "04072023", "icon": "lfm_icon.png", "file": "app_filemgr.js"}

var font = new Font("default");

font.scale = 0.55f;

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
    //font.scale = scale;
    font.print(x-(getStringSize(string, scale)/2), y-12, string);
};


function process_list_commands(control_var, list){
    if (control_var >= list.length){
        control_var = 0;
    }
    if (control_var < 0){
        control_var = list.length-1;
    }
    if (pad.justPressed(Pads.DOWN)){
        control_var++;
    }
    if (pad.justPressed(Pads.UP)){
        control_var--;
    }
    return control_var;
};

function process_matrix_commands(x_var, y_var, x_limit, y_limit){
    if ((x_var*y_var) >= (x_limit*y_limit)){
        x_var = 0;
        y_var = 0;
    };
    if (x_var >= x_limit){
        x_var = 0;
        y_var++;
    };
    if (y_var >= y_limit){
        y_var = 0;
    };
    if (x_var < 0){
        x_var = x_limit-1;
        y_var--;
    }
    if (y_var < 0){
        y_var = y_limit-1;
    }
    if ((x_var*y_var) < 0){
        x_var = x_limit-1;
        y_var = y_limit-1;
    }
    if (pad.justPressed(Pads.RIGHT)){
        x_var++;
    }
    if (pad.justPressed(Pads.LEFT)){
        x_var--;
    }
    if (pad.justPressed(Pads.DOWN)){
        y_var++;
    }
    if (pad.justPressed(Pads.UP)){
        y_var--;
    }
    return [x_var, y_var];
};

var registered_apps = 0;
var act_app = 0;

function Window(x, y, w, h, t) {
    x === undefined? this.x = 0 : this.x = x;
    y === undefined? this.y = 0 : this.y = y;
    w === undefined? this.w = 640 : this.w = w;
    h === undefined? this.h = 448 : this.h = h;
    t === undefined? this.t = "Title" : this.t = t;

    this.elm_list = [];

    this.add = function(func) { this.elm_list.push(func) };

    this.del = function(func) { 
        idx = this.elm_list.indexOf(func);
        if (idx > -1) {
            this.elm_list = this.elm_list.splice(idx, 1);
          }
    };

    this.draw = function() {
        Draw.rect(this.x, this.y, this.w, 20, Color.new(64, 0, 128, 128));
        Draw.rect(this.x, this.y+20, this.w, this.h, Color.new(40, 40, 40, 128));
        printCentered(this.x+(this.w/2), this.y+5, 0.5, this.t);
        for (var i = 0; i < this.elm_list.length; i++){
            this.elm_list[i]();
        }
    };
};

function App(){
    this.init = function() { return; };
    this.process = function() { return; };
    this.gfx = new Window();
    this.minimized = false;
    this.started = false;
    this.data = null;

    this.id = registered_apps;
    registered_apps++;

    this.run = function() {
        if (!this.started){
            this.init();
            this.started = true;
        };

        if(this.id == act_app){
            this.process();
        };

        if(!this.minimized){
            this.gfx.draw();
        };
    };
}

var file_manager = new App();
file_manager.data = [0, 0, 0, 1];
file_manager.comp = 0;

var root = [{name:"cdfs:/", size:0, dir:true}, 
            {name:"host:/", size:0, dir:true}, 
            {name:"mass:/", size:0, dir:true}, 
            {name:"mc0:/", size:0, dir:true},
            {name:"mc1:/", size:0, dir:true}];

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

    if ((pad.justPressed(Pads.TRIANGLE))){
        var idxof = path.lastIndexOf("/");
        if(path[idxof-1] != ":" && idxof != -1){
            path = path.slice(0, idxof);
            idxof = path.lastIndexOf("/");
            path = path.slice(0, idxof+1);
            file = System.listDir(path);
        } else {
            path = "";
            file = root;
        }
    }

    if ((pad.justPressed(Pads.CROSS))){
        if(file_manager.data[3] == 1){
            if(file[file_manager.data[0]].dir){
                if(path == ""){
                    path = file[file_manager.data[0]].name;
                } else {
                    path = path + file[file_manager.data[0]].name + "/";
                }
                file = System.listDir(path);

            } else if(file[file_manager.data[0]].name.endsWith(".js")){
                System.loadELF(System.boot_path + "/athena.elf", [path + "/" + file[file_manager.data[0]].name]);
            } else if(file[file_manager.data[0]].name.endsWith(".elf") || file[file_manager.data[0]].name.endsWith(".ELF")){
                System.loadELF(path + "/"+ file[file_manager.data[0]].name);
            } else if(file[file_manager.data[0]].name.endsWith(".zip")){
                Archive.extractAll(path + "/"+ file[file_manager.data[0]].name);
                file = System.listDir(path); // Update file list
            } else if(file[file_manager.data[0]].name.endsWith(".tar.gz")){
                Archive.untar(path + "/"+ file[file_manager.data[0]].name);
                file = System.listDir(path); // Update file list
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
                    break;
                case 4:
                    System.removeFile(path + "/" + file[file_manager.data[0]].name);
                    file_manager.data[2] ^= 1;
                    file_manager.data[3] ^= 1;
                    file = System.listDir(path);
                    break;
            }
        }
    }

    if (pad.justPressed(Pads.R1)){
        file_manager.data[2] ^= 1;
        file_manager.data[3] ^= 1;
    }
}

file_manager.gfx = new Window();
file_manager.gfx.t = "File Manager"

let render_filelist = function() {
    Draw.rect(file_manager.gfx.x, file_manager.gfx.y+(20*(file_manager.data[0]+2+file_manager.comp)), file_manager.gfx.w, 20, Color.new(64, 0, 128, 64));
    Draw.rect(file_manager.gfx.x, file_manager.gfx.y+(20), file_manager.gfx.w, 20, Color.new(64, 64, 64, 64));
    printCentered(file_manager.gfx.x+(file_manager.gfx.w/2), file_manager.gfx.y+(3+(20)), 0.55f, path);
    for (var i = 0; i < file.length; i++) {
        if(i+file_manager.comp < 20 && i+file_manager.comp >= 0){
            //font.scale = 0.55f;
            font.print(file_manager.gfx.x+10, file_manager.gfx.y+(3+(19*(i+2+file_manager.comp)))-12,
            file[i].name);
            if(!file[i].dir){
                font.print(file_manager.gfx.x+200, file_manager.gfx.y+(3+(19*(i+2+file_manager.comp)))-12,
                String(file[i].size));
            }
        }
    };
    if(file_manager.data[2] == 1) {
        //font.scale = 0.55f;
        Draw.rect(file_manager.gfx.x+file_manager.gfx.w-75, file_manager.gfx.y+40, 75, 20*5, Color.new(0, 0, 0, 100));
        Draw.rect(file_manager.gfx.x+file_manager.gfx.w-75, file_manager.gfx.y+20+20*(file_manager.data[1]+1), 75, 20, Color.new(64, 0, 128, 64));
        font.print(file_manager.gfx.x+file_manager.gfx.w-75+5, file_manager.gfx.y+(23+(20*(1)))-12, "Copy");
        font.print(file_manager.gfx.x+file_manager.gfx.w-75+5, file_manager.gfx.y+(23+(20*(2)))-12, "Move");
        font.print(file_manager.gfx.x+file_manager.gfx.w-75+5, file_manager.gfx.y+(23+(20*(3)))-12, "Paste");
        font.print(file_manager.gfx.x+file_manager.gfx.w-75+5, file_manager.gfx.y+(23+(20*(4)))-12, "Rename");
        font.print(file_manager.gfx.x+file_manager.gfx.w-75+5, file_manager.gfx.y+(23+(20*(5)))-12, "Delete");
    };
};

file_manager.gfx.add(render_filelist);

var pad = Pads.get();

while(true){
    pad.update();
    Screen.clear(Color.new(0, 0, 0));

    file_manager.run();

    if(pad.justPressed(Pads.CIRCLE)) {
        break;
    }
    
    Screen.flip();
};

System.loadELF(System.boot_path + "/athena_pkd.elf");