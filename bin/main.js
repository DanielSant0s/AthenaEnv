console.log('Hello, QuickJS!');
let s = Date.now();
while (Date.now() - s < 10000);
console.log('Done');
