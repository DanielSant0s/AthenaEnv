// {"name": "Header request", "author": "Daniel Santos", "version": "05112023", "file": "request_header.js", "category": "network", "blurb": "HTTP HEAD request and response headers.", "exit": "TRIANGLE"}

import { Network, Request } from 'athena_network.erl';
import { Terminal, exitToShell } from 'athenaos/app.js';

const term = new Terminal({ title: "Header request", category: "network", subtitle: "HTTP HEAD request" });

term.log("Resetting IOP and loading the network driver...");
term.draw();

IOP.reset();

IOP.loadModule("padman");
IOP.loadModule("SMAP");

Network.init();

term.log(JSON.stringify(Network.getConfig()));
term.log("HEAD https://raw.githubusercontent.com/DanielSant0s/brewstore-db/main/brew_data.json");
term.draw();

let req = new Request();
req.followlocation = true;
req.headers = ["upgrade-insecure-requests: 1",
               "sec-fetch-dest: document",
               "sec-fetch-mode: navigate"];


term.log(req.head("https://raw.githubusercontent.com/DanielSant0s/brewstore-db/main/brew_data.json").text);

Network.deinit();

const pad = Pads.get();
while (true) {
    pad.update();
    if (term.homeRequested(pad)) break;
    term.draw();
}

exitToShell();
