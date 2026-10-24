
import { Network, Request, WebSocket } from 'athena_network.erl';
import { Terminal, exitToShell } from 'athenaos/app.js';

function ab2str(ab) {
    return String.fromCharCode.apply(null, new Uint8Array(ab));
}

function str2ab(str) {
    var buf = new ArrayBuffer(str.length);
    var bufView = new Uint8Array(buf);
    for (var i=0, strLen=str.length; i < strLen; i++) {
      bufView[i] = str.charCodeAt(i);
    }
    return buf;
  }

const term = new Terminal({ title: "Websockets", category: "network", subtitle: "wss://echo.websocket.org" });

term.log("Resetting IOP and loading the network driver...");
term.draw();

IOP.reset();

IOP.loadModule("padman"); // as in sockets.js: keep the pad driver after the reset so Home works
IOP.loadModule("SMAP");

Network.init();

term.log(JSON.stringify(Network.getConfig()));
term.log("Connecting to wss://echo.websocket.org ...");
term.draw();

let req = new Request();

let ws = new WebSocket("wss://echo.websocket.org");

term.log(ab2str(ws.recv()));

ws = null;

std.gc();

Network.deinit();

const pad = Pads.get();
while (true) {
    pad.update();
    if (term.homeRequested(pad)) break;
    term.draw();
}

exitToShell();
