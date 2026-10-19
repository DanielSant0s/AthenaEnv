// {"name": "Network demo", "author": "Daniel Santos", "version": "04072023", "file": "request.js", "category": "network", "blurb": "HTTPS GET with TLS verification.", "exit": "TRIANGLE"}

import { Network, Request } from 'athena_network.erl';
import { Terminal, exitToShell } from 'athenaos/app.js';

const term = new Terminal({ title: "Network demo", category: "network", subtitle: "HTTPS GET with TLS verification" });

term.log("Resetting IOP and loading the network driver...");
term.draw();

IOP.reset();

IOP.loadModule("padman"); // as in sockets.js: keep the pad driver after the reset so Home works
IOP.loadModule("SMAP");

Network.init();

term.log(JSON.stringify(Network.getConfig()));
term.log("GET https://raw.githubusercontent.com/DanielSant0s/brewstore-db/main/brew_data.json");
term.draw();

let req = new Request();

req.verifyTLS = true;
req.followlocation = true;
req.headers = ["upgrade-insecure-requests: 1",
               "sec-fetch-dest: document",
               "sec-fetch-mode: navigate"];

let result = req.get("https://raw.githubusercontent.com/DanielSant0s/brewstore-db/main/brew_data.json");

term.log(result.text);

Network.deinit();

const pad = Pads.get();
while (true) {
    pad.update();
    if (term.homeRequested(pad)) break;
    term.draw();
}

exitToShell();
