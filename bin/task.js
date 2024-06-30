// {"name": "BUGGED", "author": "Daniel Santos", "version": "05112023","file": "task.js"}

Tasks.new(() => {
    for(let i = 0; i < 10000; i++) {
        Screen.log("Hello, world!");
    }
    
});

for(let i = 0; i < 10000; i++) {
    Screen.log(i);
    System.sleep(1);
}