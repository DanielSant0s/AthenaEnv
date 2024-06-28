// {"name": "Network demo", "author": "Daniel Santos", "version": "04072023", "file": "request.js"}

//Screen.log("Athena network system\n");

IOP.reset();

//IOP.loadDefaultModule(IOP.pads);
IOP.loadDefaultModule(IOP.network);

Network.init();

//Screen.log(JSON.stringvify(Network.getConfig()));

let req = new Request();
req.followlocation = true;
req.headers = ["upgrade-insecure-requests: 1",
               "sec-fetch-dest: document",
               "sec-fetch-mode: navigate"];

let result = req.get("https://raw.githubusercontent.com/DanielSant0s/brewstore-db/main/brew_data.json");

console.log(result.text);

Network.deinit();

System.sleep(300);
