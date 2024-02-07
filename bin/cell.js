// {"name": "Cell.io", "author": "Daniel Santos", "version": "04072023", "icon": "cell_icon.png", "file": "cell.js"}

Screen.setFrameCounter(true);
Screen.setVSync(false);

let font = new Font("fonts/LEMONMILK-Regular.otf");
font.scale = (2);

const MAIN_MENU = 0;
const RUN_GAME = 1;
const PAUSE_MENU = 2;
const GAME_OVER = 3;

let game_state = MAIN_MENU;

let running = true;

let player = {x:0, y:0, r:25, color:Color.new(128, 100, 255)};

let camera = {x:-320, y:-224};

let enemies = [];

function World2Screen(rect, camera){
    return [rect.x-camera.x, rect.y-camera.y]
}

function drawCell(coords, radius, color, hollow){
    Draw.circle(coords[0], coords[1], radius, color);
    if(hollow) Draw.circle(coords[0], coords[1], radius, Color.new(255, 255, 255), false);
}

function circleCircleColl(c1, c2) {
    let distX = c1.x - c2.x;
    let distY = c1.y - c2.y;
    let dist = Math.sqrtf( (distX*distX) + (distY*distY) );

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

let main_menu_ptr = 0;

let pad = Pads.get();

const transparent = Color.new(255, 255, 255, 40);
const purple = Color.new(64, 0, 128);

Screen.clearColor(purple);

pad.setEventHandler();

Screen.display(() => {
    if(game_state == MAIN_MENU){
        if(pad.justPressed(Pads.UP)){
            if(main_menu_ptr > 0){
                main_menu_ptr--;
            }
        }

        if(pad.justPressed(Pads.DOWN)){
            if(main_menu_ptr < 2){
                main_menu_ptr++;
            }
        }

        if(pad.justPressed(Pads.CROSS)){
            switch(main_menu_ptr){
                case 0:
                    game_state = RUN_GAME;

                    player.x = 0;
                    player.y = 0;
                    player.r = 25;

                    camera.x = -320;
                    camera.y = -224;


                    let enemies_qt = randint(8, 15);

                    while(enemies_qt > enemies.length){
                        let color = Color.new(randint(0, 255), randint(0, 255), randint(0, 255));
                        let enemy = {color:color, x:randint(0, 640), y:randint(0, 448), r:randint(5, 75)};

                        let found_collision = false;

                        for(let j = 0; j < enemies.length; j++){
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

        let s_lines = null;
        let e_lines = null;
        for(let i = -1500; i < 1501; i+=50){

            s_lines = World2Screen({x:i, y:-1500}, camera);
            if(s_lines[0] > 0 && s_lines[0] < 640) {
                e_lines = World2Screen({x:i, y:1500}, camera);
                Draw.line(s_lines[0], s_lines[1], e_lines[0], e_lines[1], transparent);
            }

            s_lines = World2Screen({x:-1500, y:i}, camera);
            if(s_lines[1] > 0 && s_lines[1] < 448) {
                e_lines = World2Screen({x: 1500, y:i}, camera);
                Draw.line(s_lines[0], s_lines[1], e_lines[0], e_lines[1], transparent);
            }
        }

        if(pad.pressed(Pads.LEFT) && player.x > -1500){
            player.x-=4;
            camera.x-=4;
        }
        if(pad.pressed(Pads.RIGHT) && player.x < 1500){
            player.x+=4;
            camera.x+=4;
        }
        if(pad.pressed(Pads.UP) && player.y > -1500){
            player.y-=4;
            camera.y-=4;
        }
        if(pad.pressed(Pads.DOWN) && player.y < 1500){
            player.y+=4;
            camera.y+=4;
        }
    
        for(let i = 0; i < enemies.length; i++){
            let enemy_coords = World2Screen(enemies[i], camera);
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

        if(pad.justPressed(Pads.CROSS)){
            game_state = MAIN_MENU;
            enemies = [];
        }

    }
});
