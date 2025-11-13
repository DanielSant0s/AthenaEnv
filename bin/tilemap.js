const pad = Pads.get();

const tilesX = 6;
const tilesY = 4;
const tileSize = 48;
const texture = "simple_atlas.png";

const spriteDefs = [];
for (let y = 0; y < tilesY; y++) {
    for (let x = 0; x < tilesX; x++) {
        const atlasX = (x % 2) * 64;
        const atlasY = (y % 2) * 64;
        spriteDefs.push({
            x: x * tileSize,
            y: y * tileSize,
            w: tileSize,
            h: tileSize,
            zindex: 1,
            u1: atlasX,
            v1: atlasY,
            u2: atlasX + 64,
            v2: atlasY + 64,
            r: 180,
            g: 180,
            b: 180,
            a: 200,
        });
    }
}

const descriptor = new TileMap.Descriptor({
    textures: [texture],
    materials: [{
        texture_index: 0,
        blend_mode: Screen.alphaEquation(Screen.ZERO_RGB, Screen.SRC_RGB, Screen.SRC_ALPHA, Screen.DST_RGB, 0),
        end_offset: spriteDefs.length - 1,
    }],
});

const spriteBuffer = TileMap.SpriteBuffer.fromObjects(spriteDefs);
const tileMap = new TileMap.Instance({ descriptor, spriteBuffer });

const layout = TileMap.layout;
const stride = layout.stride;
const offsets = layout.offsets;
const spriteView = new DataView(tileMap.getSpriteBuffer());

const basePositions = spriteDefs.map(({ x, y }) => ({ x, y }));

function setFloat(idx, offset, value) {
    spriteView.setFloat32(idx * stride + offset, value, true);
}

function setUint(idx, offset, value) {
    spriteView.setUint32(idx * stride + offset, value >>> 0, true);
}

function updateBuffer(time) {
    for (let i = 0; i < basePositions.length; i++) {
        const base = basePositions[i];
        const row = Math.floor(i / tilesX);
        const col = i % tilesX;
        const wave = Math.sin(time + col * 0.5) * 6;
        const lift = Math.cos(time * 0.8 + row * 0.6) * 4;
        setFloat(i, offsets.x, base.x + wave);
        setFloat(i, offsets.y, base.y + lift);

        const pulse = Math.sin(time + i * 0.15) * 0.5 + 0.5;
        const hue = 120 + Math.floor(pulse * 100);
        setUint(i, offsets.r, hue);
        setUint(i, offsets.g, 200 - hue / 2);
        setUint(i, offsets.b, 220);
    }
}

let time = 0;

Screen.setParam(Screen.DEPTH_TEST_ENABLE, true);
TileMap.init();

while (true) {
    Screen.clear(Color.new(200, 200, 200));
    pad.update();

    TileMap.begin();

    if (pad.justPressed(Pads.TRIANGLE)) {
        std.reload("main.js");
    }

    time += 0.05;
    updateBuffer(time);

    tileMap.render(80, 60);

    Screen.flip();
}
