if(args !== undefined) {

    var load_pkg_db = (fname) => {
        return JSON.parse(loadFile(fname));
    }

    var req = new Request();
    req.followlocation = true;
    req.useragent = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Ubuntu Chromium/37.0.2062.94 Chrome/37.0.2062.94 Safari/537.36";
    req.headers = ["upgrade-insecure-requests: 0",
                   "sec-fetch-dest: document",
                   "sec-fetch-mode: navigate",
                   "sec-fetch-user: ?1",
                   "sec-fetch-site: same-origin",
                   "sec-ch-ua-mobile: ?0",
                   "accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7",
                   'sec-ch-ua-platform: ^\^"Linux^\^"',
                   "sec-ch-ua-mobile: ?0",
    ];

    if (args[0] == "update") {
        Console.print("Updating package lists...\n");
        req.download("https://raw.githubusercontent.com/DanielSant0s/AthenaPM-db/main/packages.json", "packages.json");

        while(!req.ready(5)) {
            console.log("Waiting!\n");
        }

        let pkgs = load_pkg_db("packages.json");

        pkgs.forEach(pkg => {
            Console.print(`${pkg.name}\n`);
        });
    
    } else if (args[0] == "install") {
    
    }

    req = null;
}

