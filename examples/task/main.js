
import { AppUI, COLORS } from 'athenaos/app.js';

Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);

const ui = new AppUI({ title: "Task sample", category: "system",
                       hints: [["cross", "Pause counters"], ["square", "Drop objects"]] });

const timerMutex = new Mutex();

let counter = 0;
let pulse_counter = 0;

const timer = Timer.new();
let time = 0;

let counting = true;

const pulse_thread = new Thread(() => {
    if (counting) {
        pulse_counter++;
    }
}, "Thread: Pulse counter");

const thread = new Thread(() => {
    while (true) {
        timerMutex.lock();

        if (counting) {
            if (Timer.getTime(timer) > time) {
                time = Timer.getTime(timer) + 2000000; // 2 seconds, since timer uses microsseconds
    
                counter++;
            }
        }

        //timerMutex.unlock();
    }
}, "Thread: Loop counter");

timerMutex.lock();

thread.start();

//throw SyntaxError("receba"); // throw de teste

const font = new Font("default");

const pad = Pads.get();

pad.setEventHandler();

Pads.newEvent(Pads.CROSS, Pads.JUST_PRESSED, () => { 
    counting ^= 1;
});

Pads.newEvent(Pads.SQUARE, Pads.JUST_PRESSED, () => { 
    globalThis.activeObjects = null;
});

let exit_requested = false;

Pads.newEvent(Pads.TRIANGLE, Pads.JUST_PRESSED, () => { 
    exit_requested = true;
});

os.setInterval(() => {
    pulse_thread.start();
}, 1000);

console.log(JSON.stringify(Thread.list()));

Screen.display(() => {
    ui.background();
    const { x, y, w } = ui.content;
    ui.panel(x, y, w, 150);
    ui.heading(x + 20, y + 22, "Main thread");
    ui.textV(x + 20, y + 46, "Hello from main thread!", 0.56);
    ui.heading(x + 20, y + 78, "Aux threads");
    ui.textV(x + 20, y + 102, `Loop counter (every 2 s): ${counter}`, 0.5);
    ui.textV(x + 20, y + 124, `Pulse counter (os.setInterval): ${pulse_counter}`, 0.5);
    ui.textV(x + w - 20, y + 22, counting ? "RUNNING" : "PAUSED", 0.42, counting ? COLORS.ok : COLORS.warn, Font.ALIGN_RIGHT);
    ui.chrome();

    timerMutex.unlock();

    if (exit_requested) {
        thread.stop();
        std.reload("main.js");
    }
});
