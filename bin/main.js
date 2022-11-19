console.log('Hello, QuickJS!');
let s = Date.now();
let ti = Date.now();

let test = Color.new(128, 0, 255);
console.log('Color module test - R:' + Color.getR(test) + ' G: ' + Color.getG(test) + ' B: ' + Color.getB(test));


Screen.setVSync(false);

while (Date.now() - s < 10000){
    Screen.clear(test);

    Draw.rect(50, 50, 150, 150, Color.new(128, 128, 128));

    Draw.circle(300, 300, 25, Color.new(255, 0, 0));

    let fps = Screen.getFPS(360);
    if(Date.now() - ti > 360){
        console.log(fps + " FPS");
        ti = Date.now();
    }
    
    Screen.flip();
};
console.log('Done');


