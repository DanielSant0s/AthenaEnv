// {"name": "Websockets", "author": "Daniel Santos", "version": "05112023", "file": "websockets.js"}

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

console.log("Athena network system\n");

IOP.reset();

IOP.loadModule("SMAP");

Network.init();

console.log(JSON.stringify(Network.getConfig()));

let req = new Request();

let ws = new WebSocket("wss://echo.websocket.org");

console.log(ab2str(ws.recv()));

ws = null;

std.gc();

Network.deinit();

System.sleep(300);

std.reload("main.js");