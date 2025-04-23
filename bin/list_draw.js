// {"name": "2D List Draw", "author": "Daniel Santos", "version": "02282025", "icon": "render_icon.png", "file": "list_draw.js"}

const pad = Pads.get();

function hslToRgb(h, s, l) {
    h = h % 360;
    s /= 100;
    l /= 100;

    const c = (1 - Math.abs(2 * l - 1)) * s;
    const x = c * (1 - Math.abs((h / 60) % 2 - 1));
    const m = l - c / 2;
    let r = 0, g = 0, b = 0;

    if (h < 60) [r, g, b] = [c, x, 0];
    else if (h < 120) [r, g, b] = [x, c, 0];
    else if (h < 180) [r, g, b] = [0, c, x];
    else if (h < 240) [r, g, b] = [0, x, c];
    else if (h < 300) [r, g, b] = [x, 0, c];
    else [r, g, b] = [c, 0, x];

    return {
        r: Math.round((r + m) * 255),
        g: Math.round((g + m) * 255),
        b: Math.round((b + m) * 255)
    };
}

function generateFractalPoints(width, height, maxIterations = 50) {
    const pointListVerts = [];

    for (let py = 0; py < height; py++) {
        for (let px = 0; px < width; px++) {
            const x0 = (px / width) * 3.5 - 2.5;
            const y0 = (py / height) * 2.0 - 1.0;
            let x = 0.0, y = 0.0;
            let iteration = 0;

            while (x * x + y * y <= 4 && iteration < maxIterations) {
                const xtemp = x * x - y * y + x0;
                y = 2 * x * y + y0;
                x = xtemp;
                iteration++;
            }

            let rgba;
            if (iteration === maxIterations) {
                rgba = Color.new(0, 0, 0);
            } else {
                const hue = (iteration / maxIterations) * 360;
                const { r, g, b } = hslToRgb(hue, 100, 50);
                rgba = Color.new(r, g, b);
            }

            pointListVerts.push({ x: px, y: py, rgba });
        }
    }

    return pointListVerts;
}

const pointList = Draw.list(Draw.PRIM_TYPE_POINT, Draw.SHADE_FLAT, generateFractalPoints(64, 64));

const tileImage = new Image("simple_atlas.png");
tileImage.filter = NEAREST;

const spriteListVerts = [
    {x: 0,   y: 0, u1: 0, v1: 0, w: 64, h: 64, u2: 64, v2: 64, rgba: Color.new(128, 128, 128)},
    {x: 64,  y: 0, u1: 0, v1: 0, w: 64, h: 64, u2: 64, v2: 64, rgba: Color.new(128, 128, 128)},
    {x: 128, y: 0, u1: 0, v1: 0, w: 64, h: 64, u2: 64, v2: 64, rgba: Color.new(128, 128, 128)},
    {x: 192, y: 0, u1: 0, v1: 0, w: 64, h: 64, u2: 64, v2: 64, rgba: Color.new(128, 128, 128)},

    {x: 0,   y: 64, u1: 64, v1: 0, w: 64, h: 64, u2: 128, v2: 64, rgba: Color.new(128, 128, 128)},
    {x: 64,  y: 64, u1: 64, v1: 0, w: 64, h: 64, u2: 128, v2: 64, rgba: Color.new(128, 128, 128)},
    {x: 128, y: 64, u1: 64, v1: 0, w: 64, h: 64, u2: 128, v2: 64, rgba: Color.new(128, 128, 128)},
    {x: 192, y: 64, u1: 64, v1: 0, w: 64, h: 64, u2: 128, v2: 64, rgba: Color.new(128, 128, 128)},

    {x: 0,   y: 128, u1: 0, v1: 64, w: 64, h: 64, u2: 64, v2: 128, rgba: Color.new(128, 128, 128)},
    {x: 64,  y: 128, u1: 0, v1: 64, w: 64, h: 64, u2: 64, v2: 128, rgba: Color.new(128, 128, 128)},
    {x: 128, y: 128, u1: 0, v1: 64, w: 64, h: 64, u2: 64, v2: 128, rgba: Color.new(128, 128, 128)},
    {x: 192, y: 128, u1: 0, v1: 64, w: 64, h: 64, u2: 64, v2: 128, rgba: Color.new(128, 128, 128)},

    {x: 0,   y: 192, u1: 64, v1: 64, w: 64, h: 64, u2: 128, v2: 128, rgba: Color.new(128, 128, 128)},
    {x: 64,  y: 192, u1: 64, v1: 64, w: 64, h: 64, u2: 128, v2: 128, rgba: Color.new(128, 128, 128)},
    {x: 128, y: 192, u1: 64, v1: 64, w: 64, h: 64, u2: 128, v2: 128, rgba: Color.new(128, 128, 128)},
    {x: 192, y: 192, u1: 64, v1: 64, w: 64, h: 64, u2: 128, v2: 128, rgba: Color.new(128, 128, 128)},
];

const spriteList = Draw.list(Draw.PRIM_TYPE_SPRITE, Draw.SHADE_FLAT, spriteListVerts, tileImage);

while(true) {
    Screen.clear(Color.new(255, 255, 255));
    pad.update();

    if(pad.justPressed(Pads.TRIANGLE)) {
        std.reload("main.js");
    }

    pointList.draw(256, 0);
    spriteList.draw(0, 0);

    Screen.flip();
}
