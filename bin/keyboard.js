keyboard = new App();
keyboard.data = {};
keyboard.data.x_pointer = 0;
keyboard.data.y_pointer = 0;
keyboard.data.charset = "'1234567890-=qwertyuiop[]+asdfghjkl~?!#\\zxcvbnm,.:;/";
keyboard.invoke_app = -1;
keyboard.minimized = true;
keyboard.hole = false;
var kbd_arr = [0, 0];

var text_buffer = "";

keyboard.getinput = function() {
    return text_buffer;

}

keyboard.invoke = function(app_id) {
    keyboard.invoke_app = app_id;
    while(keyboard.id != act_app){
        act_app = rotate_app_list();
    };
    keyboard.minimized = false;

}

keyboard.process = function() {
    kbd_arr = process_matrix_commands(keyboard.data.x_pointer, keyboard.data.y_pointer, 13, 4);
    keyboard.data.x_pointer = kbd_arr[0];
    keyboard.data.y_pointer = kbd_arr[1];
    if (Pads.check(pad, PAD_CROSS) && !Pads.check(oldpad, PAD_CROSS)){
        text_buffer += keyboard.data.charset[kbd_arr[0]+(kbd_arr[1]*13)];
    }
    if (Pads.check(pad, PAD_START) && !Pads.check(oldpad, PAD_START)){
        keyboard.minimized = true;
        while(keyboard.invoke_app != act_app){
            act_app = rotate_app_list();
        };
        keyboard.hole = true;
    }
}

keyboard.graphics.draw = function() {
    Graphics.drawRect(0, 350, 640, 98, Color.new(0, 0, 0, 100));
    Graphics.drawRect(215+(19.2*keyboard.data.x_pointer), 
    352+(20*keyboard.data.y_pointer), 
    20, 20, Color.new(64, 0, 128, 100));
    printCentered(50, 420, 0.55, text_buffer);
    printCentered(320, 350+(3+(20.0*(0))), 0.55, "' 1 2 3 4 5 6 7 8 9 0 - =");
    printCentered(320, 350+(3+(20.0*(1))), 0.55, "q w e r t y u i o p [ ] +");
    printCentered(320, 350+(3+(20.0*(2))), 0.55, "a s d f g h j k l ~ ? ! #");
    printCentered(320, 350+(3+(20.0*(3))), 0.55, "\\ z x c v b n m , .  : ; /");
}