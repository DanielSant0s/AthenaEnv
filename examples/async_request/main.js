
import { Network, Request } from 'athena_network.erl';
import { Terminal, exitToShell } from 'athenaos/app.js';

const term = new Terminal({ title: "Async Network", category: "network", subtitle: "Non-blocking HTTPS download" });

term.log("Resetting IOP and loading the network driver...");
term.draw();

IOP.reset();

IOP.loadModule("padman"); // as in sockets.js: keep the pad driver after the reset so Home works
IOP.loadModule("SMAP");

Network.init();

term.log(JSON.stringify(Network.getConfig()));

let req = new Request();

req.verifyTLS = true;
req.headers = ["upgrade-insecure-requests: 1",
               "sec-fetch-dest: document",
               "sec-fetch-mode: navigate"];

req.asyncGet("https://raw.githubusercontent.com/DanielSant0s/brewstore-db/main/brew_data.json");

// The download runs in the background, so the UI keeps drawing meanwhile.
while(!req.ready(2)) {
    term.status = `${req.getAsyncSize()} bytes`;
    term.draw();
}

term.status = "";
term.log(`Done: ${req.getAsyncSize()} bytes transferred`);
term.log(req.getAsyncData());

Network.deinit();

const pad = Pads.get();
while (true) {
    pad.update();
    if (term.homeRequested(pad)) break;
    term.draw();
}

exitToShell();
