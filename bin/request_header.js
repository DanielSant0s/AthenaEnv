// {"name": "Header request", "author": "Daniel Santos", "version": "05112023", "file": "request_header.js"}


Screen.log("Athena network system\n");

IOP.reset();

IOP.loadDefaultModule(IOP.pads);
IOP.loadDefaultModule(IOP.network);

Network.init();

Screen.log(JSON.stringify(Network.getConfig()));

let req = new Request();
req.followlocation = true;
req.headers = ["upgrade-insecure-requests: 1",
               "sec-fetch-dest: document",
               "sec-fetch-mode: navigate"];


Screen.log(req.head("https://raw.githubusercontent.com/DanielSant0s/brewstore-db/main/brew_data.json").text);

Network.deinit();

System.sleep(300);