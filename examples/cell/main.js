
import { AppUI, COLORS, rgba, exitToShell } from 'athenaos/app.js';

// Menus use the standard chrome; gameplay owns the screen.
const ui = new AppUI({ title: "Cell.io", category: "games" });
const MENU = ["Play", "Options", "Quit"];

Screen.setFrameCounter(true);
Screen.setVSync(false);

const transparent = Color.new(255, 255, 255, 40);
const purple = Color.new(64, 0, 128);
const white = Color.new(255, 255, 255);

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
    if(hollow) Draw.circle(coords[0], coords[1], radius, white, false);
}

function circleCircleColl(c1, c2) {
    const distX = c1.x - c2.x;
    const distY = c1.y - c2.y;
    const dist = Math.sqrtf( (distX*distX) + (distY*distY) );

    return dist <= c1.r + c2.r;
}

let main_menu_ptr = 0;

const pad = Pads.get();

Screen.clearColor(purple);

pad.setEventHandler();

Screen.display(() => {
    if(ui.homeRequested(pad)){
        exitToShell();
    }

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


                    let enemies_qt = Math.randomi(8, 15);

                    while(enemies_qt > enemies.length){
                        let color = Color.new(Math.randomi(0, 255), Math.randomi(0, 255), Math.randomi(0, 255));
                        let enemy = {color:color, x:Math.randomi(0, 640), y:Math.randomi(0, 448), r:Math.randomi(5, 75)};

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
                    exitToShell();
                    break;
            }
        }

        if (game_state == MAIN_MENU) {
            ui.background();
            const { x, y } = ui.content;

            font.scale = (1.6);
            font.color = player.color;
            font.print(x, y + 10, "Cell.io");

            MENU.forEach((label, i) => {
                const cy = y + 130 + i * 44;
                if (i === main_menu_ptr) {
                    Draw.rect(x, cy - 18, 260, 36, rgba(255, 255, 255, 0.10));
                    Draw.rect(x, cy - 18, 3, 36, ui.accent());
                }
                ui.textV(x + 18, cy, label, 0.7, i === main_menu_ptr ? COLORS.text : COLORS.textDim);
            });

            ui.chrome([["D-pad", "Select"], ["cross", "Confirm"]]);
        }

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
                        player.r += enemies[i].r/2
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

        ui.hud([`${Screen.getFPS(360)} FPS`, `Size ${Math.round(player.r)}  |  ${enemies.length} cells left`]);

    } else if(game_state == GAME_OVER){
        font.scale = (2);
        font.color = Color.new(128, 128, 128);
        font.align = Font.ALIGN_HCENTER;
        font.print(ui.W / 2, 190, ((!enemies.length)? "VICTORY!" : "GAME OVER"));
        font.align = Font.ALIGN_NONE;
        ui.chrome([["cross", "Menu"]]);

        if(pad.justPressed(Pads.CROSS)){
            game_state = MAIN_MENU;
            enemies = [];
        }

    }
});
