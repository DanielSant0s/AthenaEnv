const pad = Pads.get();

const TILE_SIZE = 16;
const MAX_SPRITES = 1024;
const texturePath = "simple_atlas.png";

const descriptor = new TileMap.Descriptor({
    textures: [texturePath],
    materials: [{
        texture_index: 0,
        blend_mode: Screen.alphaEquation(Screen.ZERO_RGB, Screen.SRC_RGB, Screen.SRC_ALPHA, Screen.DST_RGB, 0),
        end_offset: MAX_SPRITES - 1,
    }],
});

const spriteBuffer = TileMap.SpriteBuffer.create(MAX_SPRITES);
const tileMap = new TileMap.Instance({ descriptor, spriteBuffer });

const layout = TileMap.layout;
const stride = layout.stride;
const offsets = layout.offsets;
const spriteView = new DataView(tileMap.getSpriteBuffer());

const slots = Array.from({ length: MAX_SPRITES }, () => ({ alive: false, life: 0, phase: 0, speed: 0, target: { x: 0, y: 0 } }));

function setFloat(idx, offset, value) {
    spriteView.setFloat32(idx * stride + offset, value, true);
}

function setUint(idx, offset, value) {
    spriteView.setUint32(idx * stride + offset, value >>> 0, true);
}

function hideSprite(slotIndex) {
    setFloat(slotIndex, offsets.w, 0);
    setFloat(slotIndex, offsets.h, 0);
    setUint(slotIndex, offsets.a, 0);
    slots[slotIndex].alive = false;
}

function initSlot(slotIndex, x, y) {
    const slot = slots[slotIndex];
    slot.alive = true;
    slot.life = 4 + Math.random() * 6;
    slot.phase = Math.random() * Math.PI * 2;
    slot.speed = 0.5 + Math.random();
    slot.target.x = x;
    slot.target.y = y;

    setFloat(slotIndex, offsets.x, x);
    setFloat(slotIndex, offsets.y, y);
    setFloat(slotIndex, offsets.zindex, 1);
    setFloat(slotIndex, offsets.w, TILE_SIZE);
    setFloat(slotIndex, offsets.h, TILE_SIZE);

    const atlasX = (Math.random() > 0.5 ? 64 : 0);
    const atlasY = (Math.random() > 0.5 ? 64 : 0);
    setFloat(slotIndex, offsets.u1, atlasX);
    setFloat(slotIndex, offsets.v1, atlasY);
    setFloat(slotIndex, offsets.u2, atlasX + 64);
    setFloat(slotIndex, offsets.v2, atlasY + 64);

    const tint = 120 + Math.floor(Math.random() * 120);
    setUint(slotIndex, offsets.r, tint);
    setUint(slotIndex, offsets.g, 220 - tint / 2);
    setUint(slotIndex, offsets.b, 200);
    setUint(slotIndex, offsets.a, 255);
}

function spawnSprite(x, y) {
    const freeIndex = slots.findIndex((slot) => !slot.alive);
    if (freeIndex === -1) return;
    initSlot(freeIndex, x, y);
}

function spawnRandom() {
    const x = 40 + Math.random() * 600;
    const y = 40 + Math.random() * 400;
    spawnSprite(x, y);
}

function removeOldest() {
    const idx = slots.findIndex((slot) => slot.alive);
    if (idx !== -1) hideSprite(idx);
}

function removeRandom() {
    const aliveIndices = slots.map((slot, idx) => (slot.alive ? idx : -1)).filter((idx) => idx !== -1);
    if (!aliveIndices.length) return;
    const idx = aliveIndices[(Math.random() * aliveIndices.length) | 0];
    hideSprite(idx);
}

let time = 0;
let spawnAccumulator = 0;

Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);
TileMap.init();

while (true) {
    Screen.clear(Color.new(200, 200, 200));
    pad.update();

    TileMap.begin();

    if (pad.justPressed(Pads.TRIANGLE)) {
        std.reload("tilemap_dynamic.js");
    }

    if (pad.justPressed(Pads.CROSS)) {
        spawnRandom();
    }

    if (pad.justPressed(Pads.CIRCLE)) {
        removeRandom();
    }

    time += 0.016;
    spawnAccumulator += 0.016;

    if (spawnAccumulator > 0.35) {
        spawnRandom();
        spawnAccumulator = 0;
    }

    const aliveIndices = slots.map((slot, idx) => (slot.alive ? idx : -1)).filter((idx) => idx !== -1);
    const updates = Math.min(10, aliveIndices.length);
    for (let i = 0; i < updates; i++) {
        const pickIndex = (Math.random() * aliveIndices.length) | 0;
        const pick = aliveIndices.splice(pickIndex, 1)[0];
        const slot = slots[pick];

        slot.life -= 0.016;
        slot.phase += slot.speed * 0.05;

        const wobbleX = Math.sin(slot.phase) * 6;
        const wobbleY = Math.cos(slot.phase * 1.3) * 4;
        setFloat(pick, offsets.x, slot.target.x + wobbleX);
        setFloat(pick, offsets.y, slot.target.y + wobbleY);

        const pulse = (Math.sin(time * 3 + pick * 0.25) + 1) * 0.5;
        const alpha = 160 + Math.floor(pulse * 80);
        setUint(pick, offsets.a, alpha);

        if (slot.life <= 0) {
            hideSprite(pick);
        }
    }

    tileMap.render(20, 20);

    Screen.flip();
}
