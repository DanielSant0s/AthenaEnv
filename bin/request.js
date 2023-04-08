// {"name": "Network demo", "author": "Daniel Santos", "version": "04072023", "file": "request.js"}


Screen.log("Athena network system\n");

IOP.reset();

IOP.loadDefaultModule(IOP.hdd);
IOP.loadDefaultModule(IOP.cdfs);
IOP.loadDefaultModule(IOP.memcard);
IOP.loadDefaultModule(IOP.usb_mass);
IOP.loadDefaultModule(IOP.pads);
IOP.loadDefaultModule(IOP.network);
IOP.loadDefaultModule(IOP.audio);

Network.init();

Screen.log(JSON.stringify(Network.getConfig()));

var response = Network.get("https://github.com");
Screen.log(response.slice(0, 4096));

Network.deinit();

System.sleep(9999999999999);
