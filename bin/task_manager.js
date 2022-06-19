task_manager = new App();
task_manager.data = 0;

var tasklist = Tasks.get();

task_manager.process = function() {
    task_manager.data = process_list_commands(task_manager.data, tasklist);
    if (Pads.check(pad, PAD_SQUARE) && !Pads.check(oldpad, PAD_SQUARE)){
        Tasks.kill(task_manager.data);
    }
}

task_manager.gfx = new Window(40, 15, 350, 200, "Task manager");

render_tasklist = function() {
    Graphics.drawRect(task_manager.gfx.x, task_manager.gfx.y+(20*(task_manager.data+1)), task_manager.gfx.w, 20, Color.new(64, 0, 128, 64));
    for (var i = 0; i < tasklist.length; i++) {
        tasklist = Tasks.get();
        Font.fmPrint(task_manager.gfx.x+10, task_manager.gfx.y+(3+(20.0*(i+1))), 0.55, 
        i + " " + tasklist[i].name + " Status: " + tasklist[i].status);
    };
};

task_manager.gfx.add(render_tasklist);