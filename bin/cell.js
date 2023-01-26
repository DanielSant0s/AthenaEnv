var font = new Font("fonts/LEMONMILK-Regular.otf");
font.scale = (2);

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

function drawCell(coords, radius, color, hollow){
    Draw.circle(coords[0], coords[1], radius, color);
    if(hollow) Draw.circle(coords[0], coords[1], radius, Color.new(255, 255, 255), false);
}

function circleCircleColl(c1, c2) {
    var distX = c1.x - c2.x;
    var distY = c1.y - c2.y;
    var dist = Math.sqrtf( (distX*distX) + (distY*distY) );

    if (dist <= c1.r + c2.r) {
        return true;
    }
    return false;
}

function randint(min, max) {
    min = Math.ceilf(min);
    max = Math.floorf(max);
    return Math.floorf(Math.random() * (max - min + 1)) + min;
}

var main_menu_ptr = 0;

var oldpad = Pads.get();
var pad = oldpad;

while(running){
    Screen.clear(Color.new(64, 0, 128));

    oldpad = pad;
    pad = Pads.get();

    if(game_state == MAIN_MENU){
        if(Pads.check(pad, Pads.UP) && !Pads.check(oldpad, Pads.UP)){
            if(main_menu_ptr > 0){
                main_menu_ptr--;
            }
        }

        if(Pads.check(pad, Pads.DOWN) && !Pads.check(oldpad, Pads.DOWN)){
            if(main_menu_ptr < 2){
                main_menu_ptr++;
            }
        }

        if(Pads.check(pad, Pads.CROSS) && !Pads.check(oldpad, Pads.CROSS)){
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

                    font.scale = (1);

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

        var s_lines = null;
        var e_lines = null;
        for(var i = -1500; i < 1501; i+=50){

            s_lines = World2Screen({x:i, y:-1500}, camera);
            if(s_lines[0] > 0 && s_lines[0] < 640) {
                e_lines = World2Screen({x:i, y:1500}, camera);
                Draw.line(s_lines[0], s_lines[1], e_lines[0], e_lines[1], Color.new(255, 255, 255, 40));
            }

            s_lines = World2Screen({x:-1500, y:i}, camera);
            if(s_lines[1] > 0 && s_lines[1] < 448) {
                e_lines = World2Screen({x: 1500, y:i}, camera);
                Draw.line(s_lines[0], s_lines[1], e_lines[0], e_lines[1], Color.new(255, 255, 255, 40));
            }
        }

        if(Pads.check(pad, Pads.LEFT) && player.x > -1500){
            player.x-=4;
            camera.x-=4;
        }
        if(Pads.check(pad, Pads.RIGHT) && player.x < 1500){
            player.x+=4;
            camera.x+=4;
        }
        if(Pads.check(pad, Pads.UP) && player.y > -1500){
            player.y-=4;
            camera.y-=4;
        }
        if(Pads.check(pad, Pads.DOWN) && player.y < 1500){
            player.y+=4;
            camera.y+=4;
        }
    
        for(var i = 0; i < enemies.length; i++){
            var enemy_coords = World2Screen(enemies[i], camera);
            if((enemy_coords[0]+enemies[i].r) > 0 && (enemy_coords[0]-enemies[i].r) < 640 && 
               (enemy_coords[1]+enemies[i].r) > 0 && (enemy_coords[1]-enemies[i].r) < 448 ){
                drawCell(enemy_coords, enemies[i].r, enemies[i].color, false);
                if (circleCircleColl(enemies[i], player)) {
                    if (enemies[i].r < player.r) {
                        player.r += enemies[i].r/2.0f;
                        enemies.splice(i, 1);
                        if (!enemies.length){
                            game_state = GAME_OVER;
                            font.scale = (2);
                        }
                    } else if ((enemies[i].r > player.r)) {
                        game_state = GAME_OVER;
                        font.scale = (2);
                    }
                }

            }

        }
    
        drawCell(World2Screen(player, camera), player.r, player.color, true);

        font.print(10, 10, Screen.getFPS(360) + " FPS");

    } else if(game_state == GAME_OVER){
        font.color = Color.new(128, 128, 128);
        font.print(150, 200, ((!enemies.length)? "VICTORY!" : "GAME OVER"));

        if(Pads.check(pad, Pads.CROSS) && !Pads.check(oldpad, Pads.CROSS)){
            game_state = MAIN_MENU;
            enemies = [];
        }

    }

    Screen.flip();
}

font = null;