
import { Network, Socket } from 'athena_network.erl';
import { Terminal, exitToShell } from 'athenaos/app.js';

const term = new Terminal({ title: "Sockets demo", category: "network", subtitle: "Raw TCP socket to www.google.com:80" });

term.log("Resetting IOP and loading the network driver...");
term.draw();

IOP.reset();

IOP.loadModule("padman");
IOP.loadModule("SMAP");

Network.init();

var nw_config = Network.getConfig();

term.log("IP Address: " + nw_config.ip);
term.log("Netmask: " + nw_config.netmask);
term.log("Gateway: " + nw_config.gateway);
term.log("DNS: " + nw_config.dns);
term.draw();

var s = new Socket(Socket.AF_INET, Socket.SOCK_STREAM);

const host = Network.getHostbyName("www.google.com");

term.log("www.google.com -> " + host);
term.draw();

s.connect(host, 80);
s.send("GET / HTTP/1.1\r\nHost:www.google.com\r\n\r\n");
const msg = s.recv(1024);
term.log(msg);
s.close();

Network.deinit();

const pad = Pads.get();
while (true) {
    pad.update();
    if (term.homeRequested(pad)) break;
    term.draw();
}

exitToShell();
