pkg_man = new App();
pkg_man.data = {ptr:0};
pkg_man.comp = 0;

var apps_list = [];

function find_apps(apps, lst, path, exc){
    if(lst !== null){
        for(var i = 0; i < lst.length; i++){
            if(lst[i].name.endsWith(".elf") || lst[i].name == "app.json"){
                var excluded = false;
                for(var j = 0; j < exc.length; j++){
                    if((lst[i].name == exc[j].name) || path.includes(exc[j].name)){
                        excluded = true;
                    };
                };
                if(!excluded){
                    apps.push({name: lst[i].name, path: path});
                };
            };
            if(lst[i].dir && lst[i].name != "." && lst[i].name != ".."){
                find_apps(apps, System.listDir(path + lst[i].name), path + lst[i].name + "/", exc);
            };
        };
    };
};

pkg_man.init = function() {

    var root = [{name:"cdfs:", size:0, dir:true},
                {name:"host:", size:0, dir:true},
                {name:"mass:", size:0, dir:true},
                {name:"mc0:", size:0, dir:true}, 
                {name:"mc1:", size:0, dir:true}];

    find_apps(apps_list, root, "", readJSON("exclusions.json"));
}

pkg_man.process = function() {
    pkg_man.data.ptr = process_list_commands(pkg_man.data.ptr, apps_list)
    if ((Pads.check(pad, PAD_CROSS) && !Pads.check(oldpad, PAD_CROSS))){
        if(apps_list[pkg_man.data.ptr].name.endsWith(".json")){
            System.currentDir(apps_list[pkg_man.data.ptr].path + "/");
            dofile(apps_list[pkg_man.data.ptr].name);
        } else if(apps_list[pkg_man.data.ptr].name.endsWith(".elf")){
            System.loadELF(apps_list[pkg_man.data.ptr].path + apps_list[pkg_man.data.ptr].name);
        }
    };
};

pkg_man.gfx = new Window();
pkg_man.gfx.t = "App Manager"

render_ui = function() {
    if(pkg_man.data.ptr > 20){
        pkg_man.comp = -(pkg_man.data.ptr-20)
    } else {
        pkg_man.comp = 0;
    }
    Graphics.drawRect(pkg_man.gfx.x, pkg_man.gfx.y+(20*(pkg_man.data.ptr+2+pkg_man.comp)), pkg_man.gfx.w, 20, Color.new(64, 0, 128, 64));
    for (var i = 0; i < apps_list.length; i++) {
        if(i+pkg_man.comp < 21 && i+pkg_man.comp >= 0){
            Font.fmPrint(pkg_man.gfx.x+10, pkg_man.gfx.y+(3+(20.0*(i+2+pkg_man.comp))), 0.55, 
            apps_list[i].name);
        }
    };
    return
};

pkg_man.gfx.add(render_ui);
