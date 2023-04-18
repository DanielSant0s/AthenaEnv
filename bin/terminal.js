// {"name": "Terminal", "author": "Daniel Santos", "version": "04102023", "file": "terminal.js", "bin": "athena_cli.elf"}

function reset_cmd(str) {
    for (let i = str.length; i > 0; i--) {
        Console.setCoords(Console.getX()-1, Console.getY());
        Console.print("");
    }
}

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

const VK_ACTION = 27;
const VK_RIGHT = 41;
const VK_LEFT = 42;
const VK_DOWN = 43;
const VK_UP = 44;
const BACKSPACE = 7;
const RETURN = 10;

let str = "";
let str_ptr = 0;
let old_char = 0;
let cur_char = 0;

let cmds = System.listDir("usr/bin").map(file => file.name.replace(".js", ""));
let cmd_found = false;

let cur_path = null;

let cmd_history = [];
let history_ptr = 0;
let buf_backup = false;

while(true) {
    cur_path = System.currentDir();
    Console.setFontColor(0xFF0080);
    Console.print(`${user}@${device}:${cur_path}$ `);
    Console.setFontColor(0xFFFFFFFF);

    while(cur_char != RETURN) {
        old_char = cur_char;
        cur_char = Keyboard.get();

        if (cur_char == VK_RIGHT && old_char == VK_ACTION && str_ptr < str.length) {
            Console.setCursorColor(0);
            Console.print("");

            str_ptr++;

            reset_cmd(str);

            Console.print(str.slice(0, str_ptr));
            let x_bak = Console.getX();
            Console.print(str.slice(str_ptr, str.length));
            let x_cur_bak = Console.getX();
            Console.setCursorColor(0xFFFFFF);
            Console.setCoords(x_bak, Console.getY());
            Console.print("");
            Console.setCursor(false);
            Console.setCoords(x_cur_bak, Console.getY());
            Console.print("");
            Console.setCursor(true);

        } else if (cur_char == VK_LEFT && old_char == VK_ACTION && str_ptr > 0) {
            Console.setCursorColor(0);
            Console.print("");

            str_ptr--;

            for (let i = str.length; i > str_ptr; i--) {
                Console.setCoords(Console.getX()-1, Console.getY());
                Console.print("");
            }

            let x_bak = Console.getX();
            Console.print(str.slice(str_ptr, str.length));
            let x_cur_bak = Console.getX();
            Console.setCursorColor(0xFFFFFF);
            Console.setCoords(x_bak, Console.getY());
            Console.print("");
            Console.setCursor(false);
            Console.setCoords(x_cur_bak, Console.getY());
            Console.print("");
            Console.setCursor(true);

        }  else if (cur_char == VK_UP && old_char == VK_ACTION && history_ptr > 0) {
            if(str != "" && history_ptr == cmd_history.length) {
                cmd_history.push(str);
                buf_backup = true;
            }

            history_ptr--;

            if (history_ptr < cmd_history.length) {
                Console.setCursorColor(0);
                Console.print("");
                reset_cmd(str);
                Console.setCursorColor(0xFFFFFF);
                Console.print(cmd_history[history_ptr]);
                str = cmd_history[history_ptr];
            }

        } else if (cur_char == VK_DOWN && old_char == VK_ACTION && history_ptr < cmd_history.length) {
            history_ptr++;
            if (history_ptr < cmd_history.length) {
                Console.setCursorColor(0);
                Console.print("");
                reset_cmd(str);
                Console.setCursorColor(0xFFFFFF);
                Console.print(cmd_history[history_ptr]);
                str = cmd_history[history_ptr];
            } else if (!buf_backup){
                Console.setCursorColor(0);
                Console.print("");
                reset_cmd(str);
                Console.setCursorColor(0xFFFFFF);
                Console.print("");
                str = "";
            }

        } else if (cur_char == BACKSPACE){
            if (str.length > 0) {
                Console.setCursorColor(0);
                Console.print("");

                reset_cmd(str);

                Console.setCursorColor(0xFFFFFF);

                Console.setCursor(false);
                Console.print(str.slice(0, str_ptr-1));
                let x_bak = Console.getX();
                Console.print(str.slice(str_ptr, str.length));
                let x_cur_bak = Console.getX();
                Console.setCursor(true);
                Console.setCoords(x_bak, Console.getY());
                Console.print("");
                Console.setCursor(false);
                Console.setCoords(x_cur_bak, Console.getY());
                Console.print("");
                Console.setCursor(true);

                str = str.slice(0, str_ptr-1) + str.slice(str_ptr, str.length);

                str_ptr--;
            }
        } else if(cur_char == RETURN) {
            reset_cmd(str);
            Console.print(str);

            Console.setCursor(false);
            Console.print(" \n");
        } else if(cur_char != VK_ACTION && old_char != VK_ACTION){
            reset_cmd(str);
            
            let c = String.fromCharCode(cur_char);

            Console.setCursor(false);
            Console.print(str.slice(0, str_ptr));
            Console.print(c);
            let x_bak = Console.getX();
            Console.print(str.slice(str_ptr, str.length));
            let x_cur_bak = Console.getX();
            Console.setCursor(true);
            Console.setCoords(x_bak, Console.getY());
            Console.print("");
            Console.setCursor(false);
            Console.setCoords(x_cur_bak, Console.getY());
            Console.print("");
            Console.setCursor(true);

            str = str.slice(0, str_ptr) + c + str.slice(str_ptr, str.length);

            str_ptr++;
        }
    }

    if (Console.getY() > 27) {
        Console.clear();
    }


    cmd_history.push(str);
    history_ptr = cmd_history.length;
    let command = str.replace("\n", "").split(" ");

    if(command[0].slice(0, 2) != "./") {
        cmds.forEach(cmd => {
            globalThis.args = [];
            if(cmd == command[0]) {
                cmd_found = true;
                command.shift();
                args = command;
                std.loadScript(System.boot_path + "usr/bin/" + cmd + ".js");
                args = undefined;
            }
        });
    } else if (command[0].endsWith(".elf") || command[0].endsWith(".ELF")) {
        if (command.length < 2) {
            System.loadELF(cur_path + command[0].slice(2, command[0].length));
        } else {
            let fst_cmd = command.shift();
            System.loadELF(cur_path + fst_cmd.slice(2, fst_cmd.length), command);
        }
        
    } else if (command[0].endsWith(".irx") || command[0].endsWith(".IRX")) {
        IOP.loadModule(cur_path + command[0].slice(2, command[0].length));
        
    } else if (command[0].endsWith(".js")) {
        if (command.length < 2) {
            std.loadScript(command[0].slice(2, command[0].length));
        } else {
            let fst_cmd = command.shift();
            globalThis.args = command;
            std.loadScript(fst_cmd.slice(2, fst_cmd.length));
        }
    }


    if(!cmd_found && command[0] != "") {
        Console.print(`${command[0]}: command not found\n`);
    }

    Console.setCursor(true);
    cmd_found = false;
    buf_backup = false;
    cur_char = 0;
    str_ptr = 0;
    str = "";
}
