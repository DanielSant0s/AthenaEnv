// {"name": "Pong", "author": "Daniel Santos", "version": "04252023","file": "pong.js"}
Sound.setVolume(100);
Sound.setVolume(100, 0);

let sounds = {
    unpause:Sound.load("pong/unpause.adp"),
    score:Sound.load("pong/score.adp"),
    over:Sound.load("pong/over.adp")
}

const screen = Screen.getMode();

const dark_gray = Color.new(32, 32, 32);
const gray = Color.new(64, 64, 64);
const white = Color.new(255, 255, 255);

function rectRect(rect1, rect2) {
    return rect1.x < rect2.x + rect2.w && rect1.x + rect1.w > rect2.x && rect1.y < rect2.y + rect2.h && rect1.h + rect1.y > rect2.y;
}

let font = new Font("fonts/minecraft.ttf");
font.color = gray;

let bigfont = new Font("fonts/minecraft.ttf");
bigfont.color = white;
bigfont.scale = 2.0f;

class Paddle {
    x = 285.0f;
    y = 400.0f;
    w = 70;
    h = 10;
    speed = 4;
    bak_x = this.x;
    bak_y = this.y;
    bak_speed = this.speed;
    color = Color.new(255, 255, 255);

    constructor(x, y) {
        x === undefined? this.x += 0 : this.x = x;
        y === undefined? this.y += 0 : this.y = y;
    }

    moveLeft() {
        if (this.x > 0) {
            this.x -= this.speed;
        }
    }

    moveRight() {
        if (this.x < screen.width - this.w) {
            this.x += this.speed;
        }
    } 

    reset() {
        this.x = this.bak_x;
        this.y = this.bak_y;
        this.speed = this.bak_speed;
    }

    draw() {
        Draw.rect(this.x, this.y, this.w, this.h, this.color);
    }
};

const DIRECTION_UP = -1;
const DIRECTION_DOWN = 1;

const DIRECTION_LEFT = -1;
const DIRECTION_RIGHT = 1;

class Ball {
    size = 16;
    x = 320-(this.size/2);
    y = 224-(this.size/2);
    w = this.size;
    h = this.size;
    bak_x = this.x;
    bak_y = this.y;
    speed = 2;
    bak_speed = this.speed;
    v_direction;
    h_direction;
    color = Color.new(255, 255, 255);

    constructor(x, y, size) {
        x === undefined? this.x += 0 : this.x = x;
        y === undefined? this.y += 0 : this.y = y;
        size === undefined? this.w += 0 : this.w = size;
        size === undefined? this.h += 0 : this.h = size;
        size === undefined? this.size += 0 : this.size = size;
    }

    move() {
        if (this.x >= (screen.width - this.size)) {
            this.h_direction = DIRECTION_LEFT;
        } else if (this.x <= 0) {
            this.h_direction = DIRECTION_RIGHT;
        }

        if (this.y <= 0) {
            this.v_direction = DIRECTION_DOWN;
        }

        this.x += (this.h_direction * this.speed);
        this.y += (this.v_direction * this.speed);
    }

    collide(paddle) {
        let col = rectRect(this, paddle);
        if(col) {
            this.v_direction = DIRECTION_UP;
        }

        return col;
    }

    escape() {
        return this.y >= screen.height;
    }

    randomDirection() {
        this.v_direction = Math.random() < 0.5 ? -1 : 1;
        this.h_direction = Math.random() < 0.5 ? -1 : 1;
    }

    reset() {
        this.x = this.bak_x;
        this.y = this.bak_y;
        this.speed = this.bak_speed;
        this.randomDirection();
    }

    draw() {
        Draw.rect(this.x, this.y, this.size, this.size, this.color);
    }
};

const player = new Paddle();
const ball = new Ball();
ball.randomDirection();

let pad = Pads.get();

let paused = true;
let game_over = false;

let score = 0;
let score_threshold = 5;

const over = {
    text:"GAME OVER",
    width: bigfont.getTextSize("GAME OVER").width,
    height: bigfont.getTextSize("GAME OVER").height*bigfont.scale,
};

const pause = {
    text:"PRESS START",
    width: bigfont.getTextSize("PRESS START").width,
    height: bigfont.getTextSize("PRESS START").height*bigfont.scale,
};

while(true) {
    pad.update();

    Screen.clear(dark_gray);

    if(pad.justPressed(Pads.START)) {
        paused = (!paused);
        if(game_over) {
            ball.reset();
            player.reset();
            score = 0;
            score_threshold = 5;
        }
        game_over = false;

        Sound.play(sounds.unpause);
    }

    if(!paused) {
        if(pad.pressed(Pads.LEFT) || pad.rx < -50) {
            player.moveLeft();
        }
    
        if(pad.pressed(Pads.RIGHT) || pad.rx > 50){
            player.moveRight();
        }

        if (ball.collide(player)) {
            score++;
            Sound.play(sounds.score);
            if(score > score_threshold) {
                score_threshold *= 2;
                ball.speed++;
                player.speed++;
            }
        }

        if(ball.escape()) {
            game_over = true;
            paused = true;
            Sound.play(sounds.over);
        }

        ball.move();
    }

    if(game_over) {
        bigfont.print(320-(over.width/2), 224-((over.height/2)), over.text);
    } else if (paused) {
        bigfont.print(320-(pause.width/2), 224-((pause.height/2)), pause.text);
    }


    font.print(600-font.getTextSize(String(score)).width, 10, String(score));
    player.draw();
    ball.draw();

    Screen.flip();
}