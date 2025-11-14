// {"name": "Async Network", "author": "Daniel Santos", "version": "04072023", "file": "async_request.js"}

IOP.reset();

IOP.loadModule("SMAP");

Network.init();

console.log(JSON.stringify(Network.getConfig()));

let req = new Request();

req.verifyTLS = true;
req.headers = ["upgrade-insecure-requests: 1",
               "sec-fetch-dest: document",
               "sec-fetch-mode: navigate"];

req.asyncGet("https://raw.githubusercontent.com/DanielSant0s/brewstore-db/main/brew_data.json");
 
while(!req.ready(2)) {
    console.log("Waiting... " + req.getAsyncSize() + " bytes transfered");
    System.sleep(1); 
}

let result = req.getAsyncData();

console.log(result);

Network.deinit();

System.sleep(300);

console.log("loading main.js");

std.reload("main.js");
