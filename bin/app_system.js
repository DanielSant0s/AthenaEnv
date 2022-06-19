
function process_list_commands(control_var, list){
    if (control_var >= list.length){
        control_var = 0;
    }
    if (control_var < 0){
        control_var = list.length-1;
    }
    if (Pads.check(pad, PAD_DOWN) && !Pads.check(oldpad, PAD_DOWN)){
        control_var++;
    }
    if (Pads.check(pad, PAD_UP) && !Pads.check(oldpad, PAD_UP)){
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
    if (Pads.check(pad, PAD_RIGHT) && !Pads.check(oldpad, PAD_RIGHT)){
        x_var++;
    }
    if (Pads.check(pad, PAD_LEFT) && !Pads.check(oldpad, PAD_LEFT)){
        x_var--;
    }
    if (Pads.check(pad, PAD_DOWN) && !Pads.check(oldpad, PAD_DOWN)){
        y_var++;
    }
    if (Pads.check(pad, PAD_UP) && !Pads.check(oldpad, PAD_UP)){
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
        Graphics.drawRect(this.x, this.y, this.w, 20, Color.new(64, 0, 128, 128));
        Graphics.drawRect(this.x, this.y+20.0, this.w, this.h, Color.new(40, 40, 40, 128));
        printCentered(this.x+(this.w/2), this.y+5.0, 0.5, this.t);
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