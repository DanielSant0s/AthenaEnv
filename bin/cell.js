var font = new Font("fonts/LEMONMILK-Regular.otf");
font.setScale(2);

const MAIN_MENU = 0;
const RUN_GAME = 1;
const PAUSE_MENU = 2;
const GAME_OVER = 3;

var game_state = MAIN_MENU;

var running = true;

var player = {x:0, y:0, r:25, color:Color.new(128, 100, 255)};

var camera = {x:-320, y:-224};

var enemies = [];

function World2Screen(rect, camera){
    return [rect.x-camera.x, rect.y-camera.y]
}

function circleCircleColl(c1, c2) {
    var distX = c1.x - c2.x;
    var distY = c1.y - c2.y;
    var dist = Math.sqrt( (distX*distX) + (distY*distY) );

    if (dist <= c1.r + c2.r) {
        return true;
    }
    return false;
}

function randint(min, max) {
    min = Math.ceil(min);
    max = Math.floor(max);
    return Math.floor(Math.random() * (max - min + 1)) + min;
}

var main_menu_ptr = 0;

var oldpad = Pads.get();
var pad = oldpad;

while(running){
    Display.clear(Color.new(64, 0, 128));

    oldpad = pad;
    pad = Pads.get();

    if(game_state == MAIN_MENU){
        if(Pads.check(pad, PAD_UP) && !Pads.check(oldpad, PAD_UP)){
            if(main_menu_ptr > 0){
                main_menu_ptr--;
            }
        }

        if(Pads.check(pad, PAD_DOWN) && !Pads.check(oldpad, PAD_DOWN)){
            if(main_menu_ptr < 2){
                main_menu_ptr++;
            }
        }

        if(Pads.check(pad, PAD_CROSS) && !Pads.check(oldpad, PAD_CROSS)){
            switch(main_menu_ptr){
                case 0:
                    game_state = RUN_GAME;

                    player.x = 0;
                    player.y = 0;
                    player.r = 25;

                    camera.x = -320;
                    camera.y = -224;


                    enemies_qt = randint(8, 15);

                    while(enemies_qt > enemies.length){
                        var color = Color.new(randint(0, 255), randint(0, 255), randint(0, 255));
                        var enemy = {color:color, x:randint(0, 640), y:randint(0, 448), r:randint(5, 75)};

                        var found_collision = false;

                        for(var j = 0; j < enemies.length; j++){
                            if (circleCircleColl(enemy, enemies[j])) {
                                found_collision = true;
                            }
                        }

                        if (!found_collision && !circleCircleColl(enemy, player)){
                            enemies.push(enemy);
                        }
                    }

                    break;
                case 2:
                    running = false;
                    break;
            }
        }

        font.color = Color.new(128, 128, 128, (main_menu_ptr == 0? 128 : 64));
        font.print(50, 120, "Play");

        font.color = Color.new(128, 128, 128, (main_menu_ptr == 1? 128 : 64));
        font.print(50, 190, "Options");

        font.color = Color.new(128, 128, 128, (main_menu_ptr == 2? 128 : 64));
        font.print(50, 260, "Quit");

    } else if(game_state == RUN_GAME){
        for(var i = 20; i < 448; i+=50){
            drawLine(0, i, 640, i, Color.new(255, 255, 255, 40));
        }

        for(var i = 20; i < 640; i+=50){
            drawLine(i, 0, i, 448, Color.new(255, 255, 255, 40));
        }

        if(Pads.check(pad, PAD_LEFT)){
            player.x-=4;
            camera.x-=4;
        }
        if(Pads.check(pad, PAD_RIGHT)){
            player.x+=4;
            camera.x+=4;
        }
        if(Pads.check(pad, PAD_UP)){
            player.y-=4;
            camera.y-=4;
        }
        if(Pads.check(pad, PAD_DOWN)){
            player.y+=4;
            camera.y+=4;
        }
    
        for(var i = 0; i < enemies.length; i++){
            drawCircle(World2Screen(enemies[i], camera)[0], World2Screen(enemies[i], camera)[1], enemies[i].r, enemies[i].color);
            if (circleCircleColl(enemies[i], player)) {
                if (enemies[i].r < player.r) {
                    player.r += enemies[i].r/2;
                    enemies.splice(i, 1);
                    if (!enemies.length){
                        game_state = GAME_OVER;
                    }
                } else if ((enemies[i].r > player.r)) {
                    game_state = GAME_OVER;
                }
            }
        }
    
        drawCircle(World2Screen(player, camera)[0], World2Screen(player, camera)[1], player.r, player.color);
        drawCircle(World2Screen(player, camera)[0], World2Screen(player, camera)[1], player.r, Color.new(255, 255, 255), false);

    } else if(game_state == GAME_OVER){
        font.color = Color.new(128, 128, 128);
        font.print(150, 200, ((!enemies.length)? "VICTORY!" : "GAME OVER"));

        if(Pads.check(pad, PAD_CROSS) && !Pads.check(oldpad, PAD_CROSS)){
            game_state = MAIN_MENU;
            enemies = [];
        }

    }

    Display.flip();
}

font = null;
Duktape.gc();