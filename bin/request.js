// {"name": "Network demo", "author": "Daniel Santos", "version": "04072023", "file": "request.js"}

function decodeUTF16LE(binaryStr) {
    var cp = [];
    for( var i = 0; i < binaryStr.length; i+=2) {
        cp.push( 
             binaryStr.charCodeAt(i) |
            ( binaryStr.charCodeAt(i+1) << 8 )
        );
    }

    return String.fromCharCode.apply( String, cp );
}

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

req.asyncGet("https://raw.githubusercontent.com/DanielSant0s/brewstore-db/main/brew_data.json");

while(!req.ready(2)) {
    Screen.log("Waiting... " + req.getAsyncSize() + " bytes transfered");
    System.sleep(2); 
}

Screen.log(decodeUTF16LE(req.getAsyncData()));

Network.deinit();

System.sleep(300);
