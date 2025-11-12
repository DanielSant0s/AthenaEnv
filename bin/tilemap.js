
const pad = Pads.get();

const spriteListVerts = [
    {x: 0,   y: 0, zindex: 1, u1: 0, v1: 0, w: 64, h: 64, u2: 64, v2: 64, r: 128, g:128, b:128, a:128},
    {x: 64,  y: 0, zindex: 1, u1: 0, v1: 0, w: 64, h: 64, u2: 64, v2: 64, r: 128, g:128, b:128, a:128},
    {x: 128, y: 0, zindex: 1, u1: 0, v1: 0, w: 64, h: 64, u2: 64, v2: 64, r: 128, g:128, b:128, a:128},
    {x: 192, y: 0, zindex: 1, u1: 0, v1: 0, w: 64, h: 64, u2: 64, v2: 64, r: 128, g:128, b:128, a:128},

    {x: 0,   y: 64, zindex: 1, u1: 64, v1: 0, w: 64, h: 64, u2: 128, v2: 64, r: 128, g:128, b:128, a:128},
    {x: 64,  y: 64, zindex: 1, u1: 64, v1: 0, w: 64, h: 64, u2: 128, v2: 64, r: 128, g:128, b:128, a:128},
    {x: 128, y: 64, zindex: 1, u1: 64, v1: 0, w: 64, h: 64, u2: 128, v2: 64, r: 128, g:128, b:128, a:128},
    {x: 192, y: 64, zindex: 1, u1: 64, v1: 0, w: 64, h: 64, u2: 128, v2: 64, r: 128, g:128, b:128, a:128},

    {x: 0,   y: 128, zindex: 1, u1: 0, v1: 64, w: 64, h: 64, u2: 64, v2: 128, r: 128, g:128, b:128, a:128},
    {x: 64,  y: 128, zindex: 1, u1: 0, v1: 64, w: 64, h: 64, u2: 64, v2: 128, r: 128, g:128, b:128, a:128},
    {x: 128, y: 128, zindex: 1, u1: 0, v1: 64, w: 64, h: 64, u2: 64, v2: 128, r: 128, g:128, b:128, a:128},
    {x: 192, y: 128, zindex: 1, u1: 0, v1: 64, w: 64, h: 64, u2: 64, v2: 128, r: 128, g:128, b:128, a:128},

    {x: 0,   y: 192, zindex: 1, u1: 64, v1: 64, w: 64, h: 64, u2: 128, v2: 128, r: 128, g:128, b:128, a:128},
    {x: 64,  y: 192, zindex: 1, u1: 64, v1: 64, w: 64, h: 64, u2: 128, v2: 128, r: 128, g:128, b:128, a:128},
    {x: 128, y: 192, zindex: 1, u1: 64, v1: 64, w: 64, h: 64, u2: 128, v2: 128, r: 128, g:128, b:128, a:128},
    {x: 192, y: 192, zindex: 1, u1: 64, v1: 64, w: 64, h: 64, u2: 128, v2: 128, r: 128, g:128, b:128, a:128},
];

const spriteListData = {
    sprites: spriteListVerts,
    textures: ["simple_atlas.png"],
    materials: [{texture_index: 0, blend_mode: Screen.BLEND_DEFAULT, end_offset:spriteListVerts.length-1}]
};

const spriteList = new TileMap(spriteListData);

const tex = spriteList.textures[0];

Screen.setParam(Screen.DEPTH_TEST_ENABLE, true);

TileMap.init();

while(true) {
    Screen.clear(Color.new(255, 255, 255));
    pad.update();

    TileMap.begin();

    if(pad.justPressed(Pads.TRIANGLE)) {
        std.reload("main.js");
    }

    spriteList.render(50, 50);

    Screen.flip();
}