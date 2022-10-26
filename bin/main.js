var osdsys_font = new Font();

var pad = Pads.get();
var oldpad = pad;

var list_ptr = 0;

var demo_list = ["hello.js", "pads.js", "render.js"]

Display.setVSync(false);

while(true){
    Display.clear();
    oldpad = pad;
    pad = Pads.get();

    osdsys_font.color = Color.new(128, 128, 128);
    osdsys_font.print(80, 15, "Athena project: Demo menu");

    if(Pads.check(pad, PAD_UP) && !Pads.check(oldpad, PAD_UP)){
        if(list_ptr > 0){
            list_ptr--;
        } else {
            list_ptr = demo_list.length - 1;
        }
    }

    if(Pads.check(pad, PAD_DOWN) && !Pads.check(oldpad, PAD_DOWN)){
        if(list_ptr < demo_list.length-1){
            list_ptr++;
        } else {
            list_ptr = 0;
        }
    }

    if(Pads.check(pad, PAD_CROSS) && !Pads.check(oldpad, PAD_CROSS)){
        dofile(demo_list[list_ptr]);
    }
 
    for(var i = 0; i < demo_list.length; i++){
        osdsys_font.color = Color.new(128, 128, 128, (i == list_ptr? 128 : 64))
        osdsys_font.print(80, 80+(i*35), demo_list[i]);
    }
    

    Display.flip();
}
