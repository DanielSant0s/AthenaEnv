// {"name": "Task sample", "author": "Daniel Santos", "version": "04232025", "icon": "render_icon.png", "file": "task.js"}

let counter = 0;

const timer = Timer.new();
let time = 0;

let counting = true;

const thread = Threads.new(() => {
    while (true) {
        if (counting) {
            if (Timer.getTime(timer) > time) {
                time = Timer.getTime(timer) + 2000000; // 2 seconds, since timer uses microsseconds
    
                counter++;
            }
        }
    }
});

//throw SyntaxError("receba");

const font = new Font("default");

const pad = Pads.get();

pad.setEventHandler();

Pads.newEvent(Pads.CROSS, Pads.JUST_PRESSED, () => { 
    counting ^= 1;
});

thread.start();

Screen.display(() => {
    font.print(0, 0, `Hello from main thread! Counter from aux thread: ${counter}`);
});
