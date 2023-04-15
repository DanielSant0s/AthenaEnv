// {"name": "Terminal", "author": "Daniel Santos", "version": "04102023", "file": "terminal.js", "bin": "athena_cli.elf"}
IOP.reset();
IOP.loadDefaultModule(IOP.hdd);
IOP.loadDefaultModule(IOP.cdfs);
IOP.loadDefaultModule(IOP.memcard);
IOP.loadDefaultModule(IOP.usb_mass);
IOP.loadDefaultModule(IOP.pads);
IOP.loadDefaultModule(IOP.network);
IOP.loadDefaultModule(IOP.keyboard);

Network.init();
Keyboard.init();

Keyboard.setBlockingMode(1);

globalThis.user = "user";
globalThis.device = "ps2";


const BACKSPACE = 7;
const RETURN = 10;
let str = "";
let cur_char = 0;

let cmds = System.listDir("usr/bin").map(file => file.name.replace(".js", ""));
let cmd_found = false;

while(true) {
    Console.setFontColor(0xFF00FF00);
    Console.print(`${user}@${device}:~$ `);
    Console.setFontColor(0xFFFFFFFF);

    while(cur_char != RETURN) {

        cur_char = Keyboard.get();

        if(cur_char == RETURN) {
            Console.setCursor(false);
            Console.print(" ");
        }

        let c = String.fromCharCode(cur_char);
        str += c;
        Console.print(c);
    }

    if (Console.getY() > 27) {
        Console.clear();
    }


    let command = str.replace("\n", "").split(" ");

    cmds.forEach(cmd => {
        globalThis.args = [];
        if(cmd == command[0]) {
            cmd_found = true;
            for(let i = 1; i < command.length; i++) {
                globalThis.args[i-1] = command[i];
            }

            std.loadScript(System.boot_path + "usr/bin/" + cmd + ".js");
            std.gc();
            args = undefined;
        }
    });

    if(!cmd_found && command[0] != "") {
        Console.print(`${command[0]}: command not found\n`);
    }

    Console.setCursor(true);
    str = "";
    cur_char = 0;
}


System.sleep(999999999);