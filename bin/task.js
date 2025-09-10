// {"name": "Task sample", "author": "Daniel Santos", "version": "04232025", "icon": "render_icon.png", "file": "task.js"}

Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);

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

globalThis.activeObjects = [thread, pulse_thread]; // keep it in memory, since the code execution will be asynchronous and independent from GC

Pads.newEvent(Pads.CROSS, Pads.JUST_PRESSED, () => { 
    counting ^= 1;
});

Pads.newEvent(Pads.TRIANGLE, Pads.JUST_PRESSED, () => { 
    globalThis.activeObjects = null;
});

os.setInterval(() => {
    pulse_thread.start();
}, 1000);

console.log(JSON.stringify(Thread.list()));

Screen.display(() => {
    font.print(0, 0, `Hello from main thread! Counter from aux thread: ${counter}`);
    font.print(0, 100, `Pulse counter: ${pulse_counter}`);

    timerMutex.unlock();
});
