// {"name": "Physics demo", "author": "Daniel Santos", "version": "04072023", "icon": "render_icon.png", "file": "physics_collision.js"}

const font = new Font("default");
font.scale = 0.55f;
font.outline = 1.0f;
font.outline_color = Color.new(0, 0, 0);

Screen.setFrameCounter(true);

const canvas = Screen.getMode();
canvas.zbuffering = true;
canvas.psmz = Screen.Z16S;
Screen.setMode(canvas);

Render.init();
Render.setView(60.0, 0.8, 4000.0);

// Change root folder to "render" for asset loading
os.chdir("render");

const sky = new Image("sky.png");
sky.width = canvas.width;
sky.height = canvas.height;

// --- Physics World & Broadphase Space ---
const world = Physics.World();
const jgroup = Physics.JointGroup();
const space = Physics.Space();

world.setGravity(0.0f, -18.0f, 0.0f);

// --- Static Environment (Courtyard TriMesh) ---
const scene = new RenderData("scene.gltf");
scene.face_culling = Render.CULL_FACE_BACK;
scene.accurate_clipping = true;
scene.pipeline = Render.PL_DEFAULT;
scene.getTexture(0).filter = LINEAR;

const scene_object = new RenderObject(scene);
const scene_collision = Physics.GeomRenderObject(space, scene_object);
scene.freeze();
scene_object.setCollision(scene_collision);

// --- Player Character (3rd-Person Walking Simulator) ---
const skin_anims = new AnimCollection("Twerk.gltf");
const gltf_skin = new RenderData("Twerk.gltf");
gltf_skin.accurate_clipping = true;
gltf_skin.face_culling = Render.CULL_FACE_BACK;
gltf_skin.pipeline = Render.PL_DEFAULT;
gltf_skin.freeze();

const skin_object = new RenderObject(gltf_skin);

const PLAYER_RADIUS = 0.34f;
const PLAYER_START = { x: 0.0f, y: 1.0f, z: 4.5f };

const player_geom = Physics.GeomSphere(space, PLAYER_RADIUS);
const player_body = Physics.Body(world);
player_body.setMassSphere(2.5f, PLAYER_RADIUS);
player_geom.setBody(player_body);
player_body.setPosition(PLAYER_START.x, PLAYER_START.y, PLAYER_START.z);

let playerYaw = Math.PI; // Facing -Z toward the crate pyramid
let walkCycle = 0.0f;
let currentAnim = "idle";
const emotes = ["samba", "gangnam", "hiphop", "twerk", "capoeira"];
let emoteIdx = -1;
let emoteActive = false;
const RIGHT_FOOT_BONE = 22; // mixamorig:RightFoot in Twerk.gltf
const RIGHT_TOE_BONE  = 23; // mixamorig:RightToeBase in Twerk.gltf
let animTimer = 0;
let jumpWindup = 0;
let jumpCooldown = 0;
let kickPhase = 0;
let kickHits = 0;
let kickCooldown = 0;
const kickHitBodies = [false, false, false, false, false, false, false, false, false, false, false, false];
let statusText = "WALKING SIMULATOR READY";
let statusTimer = 120;

skin_object.position = { x: PLAYER_START.x, y: PLAYER_START.y - PLAYER_RADIUS, z: PLAYER_START.z };
skin_object.rotation = { x: Math.PI / 2, y: 0.0f, z: playerYaw };
skin_object.playAnim(skin_anims["idle"] || skin_anims["twerk"], true);

// --- Dynamic Physics Crates (Pyramid + Side Crates) ---
const box = new RenderData("box_bump.gltf");
box.face_culling = Render.CULL_FACE_BACK;
box.freeze();

const CRATE_HALF = 0.38f;
const CRATE_SIZE = CRATE_HALF * 2.0f;
const crateSpawns = [
    // Bottom row of pyramid
    { x: -0.82f, y: CRATE_HALF + 0.02f, z: -2.5f },
    { x:  0.00f, y: CRATE_HALF + 0.02f, z: -2.5f },
    { x:  0.82f, y: CRATE_HALF + 0.02f, z: -2.5f },
    // Middle row of pyramid
    { x: -0.41f, y: CRATE_HALF * 3.0f + 0.05f, z: -2.5f },
    { x:  0.41f, y: CRATE_HALF * 3.0f + 0.05f, z: -2.5f },
    // Top crate of pyramid
    { x:  0.00f, y: CRATE_HALF * 5.0f + 0.08f, z: -2.5f },
    // Extra side crates
    { x: -2.60f, y: CRATE_HALF + 0.02f, z:  0.5f },
    { x:  2.60f, y: CRATE_HALF + 0.02f, z:  0.5f }
];

const crates = [];
for (let i = 0; i < crateSpawns.length; i++) {
    const sp = crateSpawns[i];
    const obj = new RenderObject(box);
    obj.scale = { x: CRATE_HALF, y: CRATE_HALF, z: CRATE_HALF };
    obj.position = { x: sp.x, y: sp.y, z: sp.z };

    const geom = Physics.GeomBox(space, CRATE_SIZE, CRATE_SIZE, CRATE_SIZE);
    const body = Physics.Body(world);
    body.setMassBox(0.8f, CRATE_SIZE, CRATE_SIZE, CRATE_SIZE);
    geom.setBody(body);
    body.setPosition(sp.x, sp.y, sp.z);
    obj.setPhysics(body);

    crates.push({ obj, geom, body, spawn: sp });
}

// --- Dynamic Rolling / Throwable Monkey Spheres ---
const monkeyTex = new Image("monkey.png");
const monkeyData = new RenderData("monkey.obj", monkeyTex);
monkeyData.face_culling = Render.CULL_FACE_BACK;
monkeyData.freeze();

const BALL_RADIUS = 0.45f;
const ballSpawns = [
    { x: -1.8f, y: 1.5f, z: 1.8f },
    { x:  1.8f, y: 1.5f, z: 1.8f },
    { x:  0.0f, y: 2.5f, z: 0.2f }
];

const balls = [];
let nextBallIdx = 0;
for (let i = 0; i < ballSpawns.length; i++) {
    const sp = ballSpawns[i];
    const obj = new RenderObject(monkeyData);
    obj.scale = { x: BALL_RADIUS, y: BALL_RADIUS, z: BALL_RADIUS };
    obj.position = { x: sp.x, y: sp.y, z: sp.z };

    const geom = Physics.GeomSphere(space, BALL_RADIUS);
    const body = Physics.Body(world);
    body.setMassSphere(0.7f, BALL_RADIUS);
    geom.setBody(body);
    body.setPosition(sp.x, sp.y, sp.z);
    obj.setPhysics(body);

    balls.push({ obj, geom, body, spawn: sp });
}

function resetSandbox() {
    const identityRot = [
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f
    ];

    player_body.setPosition(PLAYER_START.x, PLAYER_START.y, PLAYER_START.z);
    player_body.setLinearVel(0.0f, 0.0f, 0.0f);
    player_body.setAngularVel(0.0f, 0.0f, 0.0f);
    player_body.setRotation(identityRot);
    playerYaw = Math.PI;
    jumpWindup = 0;
    kickPhase = 0;

    for (let i = 0; i < crates.length; i++) {
        const c = crates[i];
        c.body.setPosition(c.spawn.x, c.spawn.y, c.spawn.z);
        c.body.setLinearVel(0.0f, 0.0f, 0.0f);
        c.body.setAngularVel(0.0f, 0.0f, 0.0f);
        c.body.setRotation(identityRot);
    }

    for (let i = 0; i < balls.length; i++) {
        const b = balls[i];
        b.body.setPosition(b.spawn.x, b.spawn.y, b.spawn.z);
        b.body.setLinearVel(0.0f, 0.0f, 0.0f);
        b.body.setAngularVel(0.0f, 0.0f, 0.0f);
        b.body.setRotation(identityRot);
    }

    statusText = "SANDBOX RESET!";
    statusTimer = 90;
}

// --- Third-Person Follow Camera State ---
let camYaw = 0.0f;
let camPitch = 0.38f;
let camDist = 6.2f;
let camTargetX = PLAYER_START.x;
let camTargetY = PLAYER_START.y + 0.8f;
let camTargetZ = PLAYER_START.z;

// --- Lighting ---
const light = Lights.new();
Lights.set(light, Lights.DIRECTION, 0.2f, 1.0f, 0.8f);
Lights.set(light, Lights.AMBIENT,   0.18f, 0.20f, 0.25f);
Lights.set(light, Lights.DIFFUSE,   0.65f, 0.65f, 0.60f);
Lights.set(light, Lights.SPECULAR,  1.0f, 1.0f, 0.8f);

const pad = Pads.get();
const gray = Color.new(180, 180, 220, 128);

Screen.setParam(Screen.ALPHA_TEST_ENABLE, false);
Screen.setParam(Screen.ALPHA_TEST_METHOD, Screen.ALPHA_LESS);
Screen.setParam(Screen.ALPHA_TEST_REF, 0x80);
Screen.setParam(Screen.DEPTH_TEST_ENABLE, true);
Screen.setParam(Screen.DEPTH_TEST_METHOD, Screen.DEPTH_GEQUAL);

while (true) {
    Screen.clear(gray);
    pad.update();

    if (pad.justPressed(Pads.TRIANGLE)) {
        os.chdir("..");
        std.reload("main.js");
    }

    if (pad.justPressed(Pads.START) || pad.justPressed(Pads.SELECT)) {
        resetSandbox();
    }

    // --- 1. Camera Orbit & Zoom Input (Right Stick + L2/R2) ---
    const rx = ((pad.rx > 25 || pad.rx < -25) ? pad.rx : 0) / 1024.0f;
    const ry = ((pad.ry > 25 || pad.ry < -25) ? pad.ry : 0) / 1024.0f;

    camYaw -= rx * 0.18f;
    camPitch += ry * 0.14f;
    if (camPitch < 0.08f) camPitch = 0.08f;
    if (camPitch > 1.15f) camPitch = 1.15f;

    if (pad.pressed(Pads.R2)) {
        camDist -= 0.12f;
        if (camDist < 2.8f) camDist = 2.8f;
    } else if (pad.pressed(Pads.L2)) {
        camDist += 0.12f;
        if (camDist > 13.0f) camDist = 13.0f;
    }

    // --- 2. Player Movement Input (Left Stick or D-Pad) ---
    let stickX = ((pad.lx > 25 || pad.lx < -25) ? pad.lx : 0) / 128.0f;
    let stickY = ((pad.ly > 25 || pad.ly < -25) ? pad.ly : 0) / 128.0f;

    if (pad.pressed(Pads.LEFT))  stickX = -1.0f;
    if (pad.pressed(Pads.RIGHT)) stickX =  1.0f;
    if (pad.pressed(Pads.UP))    stickY = -1.0f;
    if (pad.pressed(Pads.DOWN))  stickY =  1.0f;

    let inputMag = Math.sqrt(stickX * stickX + stickY * stickY);
    if (inputMag > 1.0f) {
        stickX /= inputMag;
        stickY /= inputMag;
        inputMag = 1.0f;
    }

    const pPos = player_body.getPosition();
    const pVel = player_body.getLinearVel();
    let px = pPos[0], py = pPos[1], pz = pPos[2];
    let vx = pVel[0], vy = pVel[1], vz = pVel[2];

    // Respawn safety if player ever falls out of bounds
    if (py < -8.0f) {
        resetSandbox();
        px = PLAYER_START.x; py = PLAYER_START.y; pz = PLAYER_START.z;
        vx = 0.0f; vy = 0.0f; vz = 0.0f;
    }

    if (jumpCooldown > 0) jumpCooldown--;
    if (kickCooldown > 0) kickCooldown--;
    if (statusTimer > 0) statusTimer--;
    if (animTimer > 0) animTimer--;

    const onFloor = (py <= PLAYER_RADIUS + 0.12f && Math.abs(vy) < 1.8f);

    // Check if the character is standing / balancing on top of a crate or ball
    let onProp = false;
    const allProps = crates.concat(balls);
    if (!onFloor && Math.abs(vy) < 2.2f && jumpCooldown < 30) {
        for (let i = 0; i < allProps.length; i++) {
            const bp = allProps[i].body.getPosition();
            const dx = px - bp[0];
            const dz = pz - bp[2];
            const dy = (py - PLAYER_RADIUS) - bp[1];
            if ((dx * dx + dz * dz) < 0.72f * 0.72f && dy > 0.15f && dy < 0.75f) {
                onProp = true;
                break;
            }
        }
    }

    const isGrounded = onFloor || onProp;
    const isBalancing = onProp || (onFloor && Math.abs(vy) > 0.30f && jumpWindup === 0 && jumpCooldown === 0);

    const isSprinting = pad.pressed(Pads.R1);
    const moveSpeed = isSprinting ? 10.5f : 6.0f;

    // Camera-relative horizontal basis vectors
    const fwdX = -Math.sin(camYaw);
    const fwdZ = -Math.cos(camYaw);
    const rightX = Math.cos(camYaw);
    const rightZ = -Math.sin(camYaw);

    const moveX = stickX * rightX + (-stickY) * fwdX;
    const moveZ = stickX * rightZ + (-stickY) * fwdZ;

    if (inputMag > 0.08f) {
        const targetVx = moveX * moveSpeed;
        const targetVz = moveZ * moveSpeed;
        const accel = isGrounded ? 0.35f : 0.12f;
        vx += (targetVx - vx) * accel;
        vz += (targetVz - vz) * accel;

        // Smoothly rotate character toward movement direction
        const targetYaw = Math.atan2(-moveX, moveZ);
        let diff = targetYaw - playerYaw;
        while (diff >  Math.PI) diff -= 2.0f * Math.PI;
        while (diff < -Math.PI) diff += 2.0f * Math.PI;
        playerYaw += diff * 0.22f;

        // Gentle camera auto-align when not manually orbiting
        if (rx === 0.0f && ry === 0.0f) {
            camYaw -= stickX * 0.012f;
        }

        walkCycle += (isSprinting ? 0.32f : 0.20f) * inputMag;
        emoteActive = false;
    } else if (isGrounded) {
        vx *= 0.72f;
        vz *= 0.72f;
    }

    // --- 3. Player Actions: Jump (CROSS), Kick (SQUARE), Throw (CIRCLE), Emote (L1) ---
    if (pad.justPressed(Pads.CROSS) && isGrounded && jumpCooldown === 0 && jumpWindup === 0) {
        jumpWindup = 26; // ~0.43s squat/wind-up before leaving the ground
        jumpCooldown = 50;
        emoteActive = false;
        const jumpClip = skin_anims["jump"] ? "jump" : "capoeira";
        skin_object.playAnim(skin_anims[jumpClip], false);
        currentAnim = jumpClip;
        animTimer = 68;
        statusText = "CHARGING JUMP...";
        statusTimer = 45;
    }

    if (jumpWindup > 0) {
        jumpWindup--;
        if (jumpWindup === 0) {
            vy = 7.4f;
            statusText = "JUMP!";
            statusTimer = 45;
        }
    }

    // SQUARE: Standing Melee Kick with wind-up & live Right Foot bone collision
    if (pad.justPressed(Pads.SQUARE) && kickCooldown === 0 && kickPhase === 0) {
        kickCooldown = 58;
        kickPhase = 1;
        kickHits = 0;
        for (let i = 0; i < kickHitBodies.length; i++) kickHitBodies[i] = false;
        emoteActive = false;
        const kickClip = skin_anims["kick"] ? "kick" : "capoeira";
        skin_object.playAnim(skin_anims[kickClip], false);
        currentAnim = kickClip;
        animTimer = 64;
        statusText = "WINDING UP KICK...";
        statusTimer = 45;
    }

    if (kickPhase > 0) {
        kickPhase++;

        // Damp horizontal movement during kick wind-up & strike so foot plants cleanly
        if (isGrounded && kickPhase < 48) {
            vx *= 0.65f;
            vz *= 0.65f;
        }

        // Active foot strike window (frames 26..45 of the Mixamo kick animation)
        if (kickPhase >= 26 && kickPhase <= 45) {
            const cz = Math.cos(playerYaw);
            const sz = Math.sin(playerYaw);
            const faceX = -sz;
            const faceZ =  cz;

            // Read live Mixamo RightToeBase (bone 23) & RightFoot (bone 22) transforms
            let sx = -0.06f, sy = 0.98f, szBone = -1.15f;
            if (skin_object.bones && skin_object.bones[RIGHT_TOE_BONE]) {
                const mToe = skin_object.bones[RIGHT_TOE_BONE].transform.toArray();
                sx = mToe[3];
                sy = mToe[7];
                szBone = mToe[11];
                if (skin_object.bones[RIGHT_FOOT_BONE]) {
                    const mFoot = skin_object.bones[RIGHT_FOOT_BONE].transform.toArray();
                    sx = 0.6f * sx + 0.4f * mFoot[3];
                    sy = 0.6f * sy + 0.4f * mFoot[7];
                    szBone = 0.6f * szBone + 0.4f * mFoot[11];
                }
            }

            // Transform skeleton-space foot coordinates into world space
            const footX = px + (sx * cz - sy * sz);
            const footY = (py - PLAYER_RADIUS) - szBone;
            const footZ = pz + (sx * sz + sy * cz);

            for (let i = 0; i < allProps.length; i++) {
                if (kickHitBodies[i]) continue;

                const b = allProps[i].body;
                const bp = b.getPosition();
                const dx = bp[0] - footX;
                const dy = bp[1] - footY;
                const dz = bp[2] - footZ;
                const horizDistSq = dx * dx + dz * dz;

                // Match horizontal foot proximity and leg sweep vertical band
                if (horizDistSq < 1.15f * 1.15f && Math.abs(dy) < 1.25f) {
                    const horizDist = Math.sqrt(horizDistSq) + 0.001f;
                    // Blend kick direction between character's forward kick arc and foot-to-object contact normal
                    let dirX = faceX * 0.65f + (dx / horizDist) * 0.35f;
                    let dirZ = faceZ * 0.65f + (dz / horizDist) * 0.35f;
                    const dirLen = Math.sqrt(dirX * dirX + dirZ * dirZ) + 0.001f;
                    dirX /= dirLen;
                    dirZ /= dirLen;

                    const strength = 13.5f;
                    const bVel = b.getLinearVel();
                    b.setLinearVel(
                        bVel[0] + dirX * strength,
                        bVel[1] + 5.2f,
                        bVel[2] + dirZ * strength
                    );
                    b.setAngularVel(
                        dirZ * 10.0f,
                        4.0f,
                        -dirX * 10.0f
                    );

                    kickHitBodies[i] = true;
                    kickHits++;
                    statusText = `KICK IMPACT! (${kickHits} HIT)`;
                    statusTimer = 60;
                }
            }
        }

        if (kickPhase >= 52) {
            if (kickHits === 0) {
                statusText = "KICK!";
                statusTimer = 30;
            }
            kickPhase = 0;
        }
    }

    // CIRCLE: Throw / Bowl a Monkey Sphere forward
    if (pad.justPressed(Pads.CIRCLE)) {
        const b = balls[nextBallIdx];
        nextBallIdx = (nextBallIdx + 1) % balls.length;

        const faceX = -Math.sin(playerYaw);
        const faceZ =  Math.cos(playerYaw);
        const spawnX = px + faceX * 0.95f;
        const spawnY = (py - PLAYER_RADIUS) + 1.05f;
        const spawnZ = pz + faceZ * 0.95f;

        b.body.setPosition(spawnX, spawnY, spawnZ);
        b.body.setLinearVel(faceX * 15.5f + vx * 0.4f, 2.8f, faceZ * 15.5f + vz * 0.4f);
        b.body.setAngularVel(faceZ * 12.0f, 4.0f, -faceX * 12.0f);

        statusText = "MONKEY BALL LAUNCHED!";
        statusTimer = 60;
    }

    // L1: Cycle Mixamo Dance Emotes
    if (pad.justPressed(Pads.L1)) {
        emoteIdx = (emoteIdx + 1) % emotes.length;
        const chosenEmote = emotes[emoteIdx];
        emoteActive = true;
        animTimer = 0;
        skin_object.playAnim(skin_anims[chosenEmote], true);
        currentAnim = chosenEmote;
        statusText = `DANCE: ${chosenEmote.toUpperCase()}`;
        statusTimer = 60;
    }

    // Keep animation looping smoothly based on movement state
    if (animTimer === 0) {
        let desiredAnim = "idle";
        if (!isGrounded) {
            desiredAnim = skin_anims["fall"] ? "fall" : "idle";
        } else if (isBalancing && inputMag < 0.35f && skin_anims["balance"]) {
            desiredAnim = "balance";
        } else if (inputMag > 0.08f) {
            desiredAnim = isSprinting ? "run" : "walk";
        } else if (emoteActive) {
            desiredAnim = emotes[emoteIdx];
        }
        if (!skin_anims[desiredAnim]) {
            desiredAnim = "idle";
        }
        if (currentAnim !== desiredAnim || !skin_object.isPlayingAnim()) {
            skin_object.playAnim(skin_anims[desiredAnim], true);
            currentAnim = desiredAnim;
        }
    }

    // Apply player velocity & lock player body rotation
    player_body.setLinearVel(vx, vy, vz);
    player_body.setAngularVel(0.0f, 0.0f, 0.0f);

    // --- 4. Step Physics World & Resolve Collisions ---
    const contacts = world.stepWithContacts(space, jgroup, 0.016f);

    // Sync player visual mesh
    const newPos = player_body.getPosition();
    px = newPos[0]; py = newPos[1]; pz = newPos[2];

    skin_object.position = { x: px, y: py - PLAYER_RADIUS, z: pz };
    skin_object.rotation = { x: Math.PI / 2, y: 0.0f, z: playerYaw };

    // --- 5. Update 3rd-Person Follow Camera ---
    camTargetX += (px - camTargetX) * 0.18f;
    camTargetY += ((py - PLAYER_RADIUS + 1.15f) - camTargetY) * 0.14f;
    camTargetZ += (pz - camTargetZ) * 0.18f;

    const horizDist = camDist * Math.cos(camPitch);
    const camX = camTargetX + Math.sin(camYaw) * horizDist;
    const camY = Math.max(0.55f, camTargetY + Math.sin(camPitch) * camDist);
    const camZ = camTargetZ + Math.cos(camYaw) * horizDist;

    Camera.target(camTargetX, camTargetY, camTargetZ);
    Camera.position(camX, camY, camZ);
    Camera.update();

    // --- 6. Render Pass ---
    Render.begin();

    Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);
    sky.draw(0, 0);

    Screen.setParam(Screen.DEPTH_TEST_ENABLE, true);
    Screen.setParam(Screen.DEPTH_TEST_METHOD, Screen.DEPTH_GEQUAL);

    scene_object.render();

    for (let i = 0; i < crates.length; i++) {
        crates[i].obj.render();
    }

    for (let i = 0; i < balls.length; i++) {
        balls[i].obj.render();
    }

    skin_object.render();

    // --- 7. HUD Overlay ---
    Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);

    const stateLabel = !isGrounded
        ? (currentAnim === "jump" ? "JUMPING" : "FALLING")
        : (kickPhase > 0
            ? "KICKING"
            : (isBalancing && inputMag < 0.35f
                ? "BALANCING"
                : (inputMag > 0.08f
                    ? (isSprinting ? "SPRINTING" : "WALKING")
                    : (emoteActive ? "DANCING" : "IDLE"))));
    font.print(10, 10, `${Screen.getFPS(360)} FPS | ${stateLabel} (${currentAnim.toUpperCase()}) | Contacts: ${contacts.length}`);
    font.print(10, 26, "L-Stick/DPad: Walk (+R1 Run) | R-Stick: Camera | X: Jump");
    font.print(10, 42, "SQUARE: Kick | CIRCLE: Throw Ball | L1: Dance | START: Reset");

    if (statusTimer > 0) {
        font.print(10, 62, `>> ${statusText}`);
    }

    Screen.flip();
}
