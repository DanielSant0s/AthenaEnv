// {"name": "Sockets demo", "author": "Daniel Santos", "version": "04072023","file": "sockets.js"}

Network.init();

var nw_config = Network.getConfig();

var s = new Socket(AF_INET, SOCK_STREAM);
s.connect("192.168.1.2", 65432);
var msg = "Hello from the emulated PS2!";
s.send(msg);
msg = s.recv(1024);
console.log(msg);
s.close();

for(var i = 0; i < 1250; i++){
    Screen.clear();
    lml_font.print(5, 250, "IP Address: " + nw_config.ip);
    lml_font.print(5, 265, "Netmask: " + nw_config.netmask);
    lml_font.print(5, 280, "Gateway: " + nw_config.gateway);
    lml_font.print(5, 295, "DNS: " + nw_config.dns);

    lm_font.print(5, 120, "Message: " + msg);

    Screen.flip();
}

Network.deinit();