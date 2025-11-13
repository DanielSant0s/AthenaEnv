const pad = Pads.get();

const MAX_PARTICLES = 256;
const INITIAL_PARTICLES = 32;
const TILE_SIZE = 16;
const texturePath = "simple_atlas.png";

let activeCount = INITIAL_PARTICLES;

function buildDescriptor(endIndex) {
    return new TileMap.Descriptor({
        textures: [texturePath],
        materials: [{
            texture_index: 0,
            blend_mode: Screen.alphaEquation(Screen.ZERO_RGB, Screen.SRC_RGB, Screen.SRC_ALPHA, Screen.DST_RGB, 0),
            end_offset: endIndex,
        }],
    });
}

let descriptor = buildDescriptor(activeCount - 1);
let spriteBuffer = TileMap.SpriteBuffer.create(MAX_PARTICLES);
let tileMap = new TileMap.Instance({ descriptor, spriteBuffer });

const layout = TileMap.layout;
const stride = layout.stride;
const offsets = layout.offsets;
let spriteView = new DataView(tileMap.getSpriteBuffer());

const particles = Array.from({ length: MAX_PARTICLES }, () => ({
    alive: false,
    pos: { x: 0, y: 0 },
    vel: { x: 0, y: 0 },
    size: TILE_SIZE,
    life: 0,
    color: 0,
}));

function setFloat(idx, offset, value) {
    spriteView.setFloat32(idx * stride + offset, value, true);
}

function setUint(idx, offset, value) {
    spriteView.setUint32(idx * stride + offset, value >>> 0, true);
}

function setParticleInactive(idx) {
    particles[idx].alive = false;
    setUint(idx, offsets.a, 0);
}

function initParticle(idx, x, y, velX, velY) {
    const particle = particles[idx];
    particle.alive = true;
    particle.pos.x = x;
    particle.pos.y = y;
    particle.vel.x = velX;
    particle.vel.y = velY;
    particle.life = 1.5 + Math.random() * 1.5;
    particle.size = 12 + Math.random() * 10;
    particle.color = 120 + Math.floor(Math.random() * 120);

    setFloat(idx, offsets.x, x);
    setFloat(idx, offsets.y, y);
    setFloat(idx, offsets.zindex, 0.5);
    setFloat(idx, offsets.w, particle.size);
    setFloat(idx, offsets.h, particle.size);

    setFloat(idx, offsets.u1, 0);
    setFloat(idx, offsets.v1, 0);
    setFloat(idx, offsets.u2, 32);
    setFloat(idx, offsets.v2, 32);

    setUint(idx, offsets.r, particle.color);
    setUint(idx, offsets.g, 240);
    setUint(idx, offsets.b, 255);
    setUint(idx, offsets.a, 255);
}

function spawnBurst(count) {
    for (let i = 0; i < count; i++) {
        const idx = particles.findIndex((p, index) => index < activeCount && !p.alive);
        if (idx === -1) return;
        const speed = 30 + Math.random() * 90;
        const angle = Math.random() * Math.PI * 2;
        const vx = Math.cos(angle) * speed;
        const vy = Math.sin(angle) * speed;
        initParticle(idx, 220, 140, vx, vy);
    }
}

function resizeActiveCount(delta) {
    const newCount = Math.max(1, Math.min(MAX_PARTICLES, activeCount + delta));
    if (newCount === activeCount) return;

    if (newCount < activeCount) {
        for (let i = newCount; i < activeCount; i++) {
            setParticleInactive(i);
        }
    }

    activeCount = newCount;

    const newDescriptor = buildDescriptor(activeCount - 1);
    const newBuffer = new ArrayBuffer(activeCount * stride);
    const copyLen = Math.min(spriteBuffer.byteLength, newBuffer.byteLength);
    new Uint8Array(newBuffer).set(new Uint8Array(spriteBuffer, 0, copyLen));

    descriptor = newDescriptor;
    spriteBuffer = newBuffer;
    tileMap = new TileMap.Instance({ descriptor, spriteBuffer });
    spriteView = new DataView(tileMap.getSpriteBuffer());
}

let time = 0;
Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);
TileMap.init();

while (true) {
    Screen.clear(Color.new(70, 5, 150));
    pad.update();

    TileMap.begin();

    if (pad.justPressed(Pads.TRIANGLE)) {
        std.reload("tilemap_particles.js");
    }

    if (pad.justPressed(Pads.CROSS)) {
        spawnBurst(8);
    }

    if (pad.justPressed(Pads.SQUARE)) {
        resizeActiveCount(32);
    }

    if (pad.justPressed(Pads.CIRCLE)) {
        resizeActiveCount(-32);
    }

    time += 0.016;

    for (let i = 0; i < activeCount; i++) {
        const particle = particles[i];
        if (!particle.alive) continue;

        particle.life -= 0.016;
        if (particle.life <= 0) {
            setParticleInactive(i);
            continue;
        }

        particle.vel.y += 30 * 0.016;
        particle.vel.x *= 0.98;
        particle.vel.y *= 0.98;

        particle.pos.x += particle.vel.x * 0.016;
        particle.pos.y += particle.vel.y * 0.016;

        setFloat(i, offsets.x, particle.pos.x);
        setFloat(i, offsets.y, particle.pos.y);

        const fade = Math.max(0, particle.life / 3);
        const alpha = Math.floor(255 * fade);
        setUint(i, offsets.a, alpha);
    }

    tileMap.render(0, 0);
    Screen.flip();
}
