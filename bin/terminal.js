// {"name": "Terminal", "author": "Daniel Santos", "version": "04102023", "file": "terminal.js", "bin": "athena_cli.elf"}

function resetCommandLine(str) {
    for (let i = str.length; i > 0; i--) {
        Console.setCoords(Console.getX()-1, Console.getY());
        Console.print("");
    }
}


function printPrompt(user, device, cur_path) {
    Console.setFontColor(0xFF0080);
    Console.print(`${user}@${device}:${cur_path}$ `);
    Console.setFontColor(0xFFFFFFFF);
}

class CommandLineInterface {
    constructor() {
        this.str = "";
        this.ptr = 0;
        this.history = {cmds:[], ptr:0, backup:false};
    }

    handleArrowRight() {
        Console.setCursorColor(0);
        Console.print("");
    
        this.ptr++;
    
        resetCommandLine(this.str);
    
        Console.print(this.str.slice(0, this.ptr));
        let x_bak = Console.getX();
        Console.print(this.str.slice(this.ptr, this.str.length));
        let x_cur_bak = Console.getX();
        Console.setCursorColor(0xFFFFFF);
        Console.setCoords(x_bak, Console.getY());
        Console.print("");
        Console.setCursor(false);
        Console.setCoords(x_cur_bak, Console.getY());
        Console.print("");
        Console.setCursor(true);
    }

    handleArrowLeft() {
        Console.setCursorColor(0);
        Console.print("");
    
        this.ptr--;
    
        for (let i = this.str.length; i > this.ptr; i--) {
            Console.setCoords(Console.getX() - 1, Console.getY());
            Console.print("");
        }
    
        let x_bak = Console.getX();
        Console.print(this.str.slice(this.ptr, this.str.length));
        let x_cur_bak = Console.getX();
        Console.setCursorColor(0xFFFFFF);
        Console.setCoords(x_bak, Console.getY());
        Console.print("");
        Console.setCursor(false);
        Console.setCoords(x_cur_bak, Console.getY());
        Console.print("");
        Console.setCursor(true);
    }

    handleArrowUp() {
        if (this.str != "" && this.history.ptr == this.history.cmds.length) {
            this.history.cmds.push(this.str);
        }
    
        this.history.ptr--;
    
        if (this.history.ptr < this.history.cmds.length) {
            Console.setCursorColor(0);
            Console.print("");
            resetCommandLine(this.str);
            Console.setCursorColor(0xFFFFFF);
            Console.print(this.history.cmds[this.history.ptr]);
            this.str = this.history.cmds[this.history.ptr];
        }
    }

    handleArrowDown() {
        this.history.ptr++;
    
        if (this.history.ptr < this.history.cmds.length) {
            Console.setCursorColor(0);
            Console.print("");
            resetCommandLine(this.str);
            Console.setCursorColor(0xFFFFFF);
            Console.print(this.history.cmds[this.history.ptr]);
            this.str = this.history.cmds[this.history.ptr];
        } else if (!this.history.backup) {
            Console.setCursorColor(0);
            Console.print("");
            resetCommandLine(this.str);
            Console.setCursorColor(0xFFFFFF);
            Console.print("");
            this.str = "";
        }
    }

    handleBackspace() {
        if (this.str.length > 0) {
            Console.setCursorColor(0);
            Console.print("");
    
            resetCommandLine(this.str);
    
            Console.setCursorColor(0xFFFFFF);
    
            Console.setCursor(false);
            Console.print(this.str.slice(0, this.ptr-1));
            let x_bak = Console.getX();
            Console.print(this.str.slice(this.ptr, this.str.length));
            let x_cur_bak = Console.getX();
            Console.setCursor(true);
            Console.setCoords(x_bak, Console.getY());
            Console.print("");
            Console.setCursor(false);
            Console.setCoords(x_cur_bak, Console.getY());
            Console.print("");
            Console.setCursor(true);
    
            this.str = this.str.slice(0, this.ptr-1) + this.str.slice(this.ptr, this.str.length);
    
            this.ptr--;
        }
    }

    handleReturn() {
        resetCommandLine(this.str);
        Console.print(this.str);

        Console.setCursor(false);
        Console.print(" \n");
    }

    putChar(ch) {
        resetCommandLine(this.str);
            
        let c = String.fromCharCode(ch);

        Console.setCursor(false);
        Console.print(this.str.slice(0, this.ptr));
        Console.print(c);
        let x_bak = Console.getX();
        Console.print(this.str.slice(this.ptr, this.str.length));
        let x_cur_bak = Console.getX();
        Console.setCursor(true);
        Console.setCoords(x_bak, Console.getY());
        Console.print("");
        Console.setCursor(false);
        Console.setCoords(x_cur_bak, Console.getY());
        Console.print("");
        Console.setCursor(true);

        this.str = this.str.slice(0, this.ptr) + c + this.str.slice(this.ptr, this.str.length);

        this.ptr++;
    }

    resetParameters() {
        Console.setCursor(true);
        this.history.backup = false;
        this.ptr = 0;
        this.str = "";
    }
};

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

let old_char = 0;
let cur_char = 0;

let cmds = System.listDir("usr/bin").map(file => file.name.replace(".js", ""));
let cmd_found = false;

let cur_path = null;

const cli = new CommandLineInterface();

while(true) {
    cur_path = System.currentDir();
    printPrompt(user, device, cur_path);

    while(cur_char != RETURN) {
        old_char = cur_char;
        cur_char = Keyboard.get();

        if (cur_char == VK_RIGHT && old_char == VK_ACTION && cli.ptr < cli.str.length) {
            cli.handleArrowRight();

        } else if (cur_char == VK_LEFT && old_char == VK_ACTION && cli.ptr > 0) {
            cli.handleArrowLeft();

        }  else if (cur_char == VK_UP && old_char == VK_ACTION && cli.history.ptr > 0) {
            cli.handleArrowUp();

        } else if (cur_char == VK_DOWN && old_char == VK_ACTION && cli.history.ptr < cli.history.cmds.length) {
            cli.handleArrowDown();

        } else if (cur_char == BACKSPACE){
            cli.handleBackspace();

        } else if(cur_char == RETURN) {
            cli.handleReturn();
        } else if(cur_char != VK_ACTION && old_char != VK_ACTION){
            cli.putChar(cur_char);
        }
    }

    if (Console.getY() > 27) {
        Console.clear();
    }


    cli.history.cmds.push(cli.str);
    cli.history.ptr = cli.history.cmds.length;
    let command = cli.str.replace("\n", "").split(" ");

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

    cli.resetParameters();
    cmd_found = false;
    cur_char = 0;
}

