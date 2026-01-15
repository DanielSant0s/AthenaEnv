Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);

const img = new Image("dash/bg.png");

let x = 640;

while(true) {
    Screen.clear();
    
    x -= 1;
    img.draw(x, 0);
    
    
    Screen.flip();
}