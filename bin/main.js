/*Network.init("192.168.1.10", "255.255.255.0", "192.168.0.1", "192.168.0.1");

var nw_config = Network.getConfig();

var s = new Socket(AF_INET, SOCK_STREAM);
s.connect("192.168.1.2", 65432);
var msg = new Buffer("Hello from the emulated PS2!")
s.send(msg);
msg = new TextDecoder().decode(s.recv(1024))
console.log(msg);

s.close();*/

var lm_font = new Font("fonts/LEMONMILK-Regular.otf");
var lml_font = new Font("fonts/LEMONMILK-Light.otf");
lml_font.setScale(0.35);

var pad = Pads.get();
var oldpad = pad;

var list_ptr = 0;

var demo_list = ["hello.js", "pads.js", "render.js", "cell.js"];

Display.setVSync(false);

var vsync = false;

while(true){
    Display.clear();
    oldpad = pad;
    pad = Pads.get();

    lm_font.color = Color.new(128, 128, 128);
    lm_font.print(80, 15, "Athena Demo Gallery");

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
        lml_font.setScale(0.35);
    }

    if(Pads.check(pad, PAD_R3) && !Pads.check(oldpad, PAD_R3)){
        vsync = !vsync;
        Display.setVSync(vsync);
    }
 
    for(var i = 0; i < demo_list.length; i++){
        lm_font.color = Color.new(128, 128, 128, (i == list_ptr? 128 : 64))
        lm_font.print(80, 80+(i*35), demo_list[i]);
    }

    lml_font.print(5, 380, "Cross - Run demo");
    lml_font.print(5, 395, "Up/Down - Switch demo");
    lml_font.print(5, 410, "R3 - " + (vsync? "Disable" : "Enable") + " frame limiter");

    lml_font.print(5, 250, "IP Address: " + nw_config.ip);
    lml_font.print(5, 265, "Netmask: " + nw_config.netmask);
    lml_font.print(5, 280, "Gateway: " + nw_config.gateway);
    lml_font.print(5, 295, "DNS: " + nw_config.dns);

    Display.flip();
}
