// {"name": "Sockets demo", "author": "Daniel Santos", "version": "04072023","file": "sockets.js"}

IOP.reset();

IOP.loadDefaultModule(IOP.network);

Network.init();

const font = new Font("default");
font.scale = 0.7;

var nw_config = Network.getConfig();

var s = new Socket(AF_INET, SOCK_STREAM);

const host = Network.getHostbyName("www.google.com");

console.log(host);

s.connect(host, 80);
s.send("GET / HTTP/1.1\r\nHost:www.google.com\r\n\r\n");
const msg = s.recv(1024);
console.log(msg);
s.close();

for(var i = 0; i < 1250; i++){
    Screen.clear();
    font.print(5, 250, "IP Address: " + nw_config.ip);
    font.print(5, 265, "Netmask: " + nw_config.netmask);
    font.print(5, 280, "Gateway: " + nw_config.gateway);
    font.print(5, 295, "DNS: " + nw_config.dns);

    //font.print(5, 120, "Message: " + msg);

    Screen.flip();
}

Network.deinit();

std.reload("main.js");