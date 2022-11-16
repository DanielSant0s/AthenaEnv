console.log('Hello, QuickJS!');
let s = Date.now();

let test = Color.new(128, 0, 255);
console.log('Color module test - R:' + Color.getR(test) + ' G: ' + Color.getG(test) + ' B: ' + Color.getB(test));

while (Date.now() - s < 10000){
    Screen.clear(test);
    Screen.flip();
};
console.log('Done');


