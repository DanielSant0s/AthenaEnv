task_manager = new App();
task_manager.data = 0;

var tasklist = Tasks.get();

task_manager.process = function() {
    task_manager.data = process_list_commands(task_manager.data, tasklist);
    if (Pads.check(pad, PAD_SQUARE) && !Pads.check(oldpad, PAD_SQUARE)){
        Tasks.kill(task_manager.data);
    }
}

task_manager.graphics = new Window(40, 15, 350, 200, "Task manager");

render_tasklist = function() {
    Graphics.drawRect(task_manager.graphics.x, task_manager.graphics.y+(20*(task_manager.data+1)), task_manager.graphics.w, 20, Color.new(64, 0, 128, 64));
    for (var i = 0; i < tasklist.length; i++) {
        tasklist = Tasks.get();
        Font.fmPrint(task_manager.graphics.x+10, task_manager.graphics.y+(3+(20.0*(i+1))), 0.55, 
        i + " " + tasklist[i].name + " Status: " + tasklist[i].status);
    };
};

task_manager.graphics.add(render_tasklist);