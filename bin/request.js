Network.init();

var response = Network.get("www.google.com/search?q=speed+test");
console.log(response);

Network.deinit();