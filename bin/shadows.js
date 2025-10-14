// {"name": "Skinning demo", "author": "Daniel Santos", "version": "04072023", "icon": "render_icon.png", "file": "skinning.js"}

const font = new Font("default");
font.scale = 0.6f;

font.outline = 1.0f;
font.outline_color = Color.new(0, 0, 0);

Screen.setFrameCounter(true);

const canvas = Screen.getMode();

canvas.zbuffering = true;
canvas.psmz = Screen.Z16S;

Screen.setMode(canvas);

Screen.initBuffers();

const draw_buffer = Screen.getBuffer(Screen.DRAW_BUFFER);
const depth_buffer = Screen.getBuffer(Screen.DEPTH_BUFFER);

Render.init();

Render.setView(60.0, 1.0, 4000.0);

// Shadow render target (offscreen)
const shadowRT = new Image();
shadowRT.filter = LINEAR;
shadowRT.renderable = true;
shadowRT.bpp = 32;
shadowRT.texWidth = 256;
shadowRT.texHeight = 256;
shadowRT.width = 256;
shadowRT.height = 256;
shadowRT.endx = 256;
shadowRT.endy = 256;
shadowRT.lock();

// Shadow texture (outside render/ root)
const blobTex = new Image("simple_atlas.png");
blobTex.filter = LINEAR;

// Change your root folder to "render" so we can work with file path magic :p
os.chdir("render");

const sky = new Image("sky.png");
sky.width = canvas.width;
sky.height = canvas.height;

const skin_anims = new AnimCollection("Twerk.gltf");
const gltf_skin = new RenderData("Twerk.gltf");
gltf_skin.accurate_clipping = true;
gltf_skin.face_culling = Render.CULL_FACE_BACK;
gltf_skin.pipeline = Render.PL_DEFAULT;

const skin_object = new RenderObject(gltf_skin);
skin_object.rotation = {x:Math.PI/2, y:0.0, z:0.0};

skin_object.playAnim(skin_anims[4], false);

const world = ODE.World();
const space = ODE.Space();
const jgroup = ODE.JointGroup();

// Shadow projector for skinned character
const projSkin = new Shadows.Projector(shadowRT);
projSkin.setSize(2.0, 2.0);
projSkin.setGrid(12, 12);
// Share light direction between projector and shadow camera
const lightDir = { x: 0.0f, y: 1.0f, z: 1.0f };
projSkin.setLightDir(lightDir.x, lightDir.y, lightDir.z);
projSkin.setBias(-0.02);
projSkin.setColor(0.0, 0.0, 0.0, 0.65);
projSkin.setBlend(Shadows.SHADOW_BLEND_DARKEN);
projSkin.enableRaycast(space, 4, 12.0);
projSkin.setLightOffset(1.0);
projSkin.setUVRect(1.0, 0.0, 0.0, 1.0);

Screen.switchContext();
Screen.setBuffer(Screen.DRAW_BUFFER, shadowRT);
Screen.switchContext();

Screen.flush();

const scene = new RenderData("scene.gltf");
scene.face_culling = Render.CULL_FACE_NONE;
scene.pipeline = Render.PL_DEFAULT;

scene.getTexture(0).filter = LINEAR;

const scene_object = new RenderObject(scene);
// ODE space for projector raycasts against the scene

const scene_collision = ODE.GeomRenderObject(space, scene_object);

const box = new RenderData("box_bump.gltf");
box.face_culling = Render.CULL_FACE_NONE;

//box.getTexture(0).filter = LINEAR;
//box.getTexture(1).filter = LINEAR;

const box_object = new RenderObject(box);
box_object.position = {x:2.0, y:0.2, z:2.0};
box_object.scale = {x:0.2, y:0.2, z:0.2};

Camera.position(0.0f, 0.0f, 20.0f);

Camera.target(0.0f, 1.0f, 0.0f);
Camera.orbit(0.0f, 0.5f);

const light = Lights.new();
Lights.set(light, Lights.DIRECTION, 0.0,  1.0, 1.0);
Lights.set(light, Lights.AMBIENT,   0.12, 0.15, 0.2);
Lights.set(light, Lights.DIFFUSE,   0.5, 0.5, 0.5);
Lights.set(light, Lights.SPECULAR,  1.0, 1.0, 0.0);

let pad = Pads.get();
let lx = null;
let ly = null;
let rx = null;
let ry = null;

let savedlx = 0.0f;
let savedly = 0.0f;

let savedrx = 0.0f;
let savedry = 0.0f;
let savedrz = 0.0f;

let ee_info = System.getCPUInfo();

let free_mem = `RAM: ${Math.floor(System.getMemoryStats().used / 1048576)}MB / ${Math.floor(ee_info.RAMSize / 1048576)}MB`;
let free_vram = Screen.getMemoryStats();

const gray = Color.new(180, 180, 220, 128);

Screen.setParam(Screen.ALPHA_TEST_ENABLE, false);
Screen.setParam(Screen.ALPHA_TEST_METHOD, Screen.ALPHA_LESS);
Screen.setParam(Screen.ALPHA_TEST_REF, 0x80);

Screen.setParam(Screen.DEPTH_TEST_ENABLE, true);
Screen.setParam(Screen.DEPTH_TEST_METHOD, Screen.DEPTH_GEQUAL);

let switch_anim = false;

while(true) {
    Screen.clear(gray);
    Camera.update();
    pad.update();

    lx = ((pad.lx > 25 || pad.lx < -25)? pad.lx : 0) / 1024.0f;
    ly = ((pad.ly > 25 || pad.ly < -25)? pad.ly : 0) / 1024.0f;

    if (lx != 0 || ly != 0) {
        Camera.orbit(lx, ly);
    }
    savedlx = savedlx - lx;
    savedly = savedly - ly;

    rx = ((pad.rx > 25 || pad.rx < -25)? pad.rx : 0) / 1024.0f;
    ry = ((pad.ry > 25 || pad.ry < -25)? pad.ry : 0) / 1024.0f;
    savedrx = savedrx - rx;
    savedry = savedry + ry;


    if(pad.pressed(Pads.R2)){
        Camera.zoom(0.2f);
    } else if(pad.pressed(Pads.L2)){
        Camera.zoom(-0.2f);
    }
    
    if(pad.justPressed(Pads.TRIANGLE)) {
        os.chdir("..");
        std.reload("main.js");
    }

    if (pad.justPressed(Pads.CROSS)) {
        if (switch_anim) {
            skin_object.playAnim(skin_anims["twerk"], true);
        } else {
            skin_object.playAnim(skin_anims["capoeira"], true);
        }

        switch_anim ^= 1;
    }

    // Offscreen pass: render skin into shadowRT from light-aligned camera
    const _mainCam = Camera.save();
    Screen.switchContext();
    Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);
    Draw.rect(0, 0, shadowRT.width, shadowRT.height, Color.new(0, 0, 0, 0));
    Screen.setParam(Screen.DEPTH_TEST_ENABLE, true);
    Screen.setParam(Screen.DEPTH_TEST_METHOD, Screen.DEPTH_GEQUAL);
    Render.setView(60.0, 1.0, 4000.0, shadowRT.width, shadowRT.height);
    // Build light camera looking at the skinned character
    {
        let tx = skin_object.position.x;
        let ty = skin_object.position.y; // aim slightly above ground
        let tz = skin_object.position.z + 0.1f;
        // normalize lightDir
        let len = Math.sqrt(lightDir.x*lightDir.x + lightDir.y*lightDir.y + lightDir.z*lightDir.z);
        let dx = (len > 0)? lightDir.x/len : 0.0f;
        let dy = (len > 0)? lightDir.y/len : 1.0f;
        let dz = (len > 0)? lightDir.z/len : 0.0f;
        // place camera some distance opposite the light directionS
        const dist = 1.0f;
        let px = tx - dx * dist;
        let py = ty - dy * dist;
        let pz = tz - dz * dist;
        Camera.position(px, py, pz);
        Camera.target(tx, ty, tz);
        Camera.update();
    }
    gltf_skin.texture_mapping = false;
    gltf_skin.shade_model = Render.SHADE_FLAT;
    gltf_skin.pipeline = Render.PL_NO_LIGHTS;
    skin_object.render();
    // Restore main camera and context
    Camera.restore(_mainCam);
    Camera.update();
    Screen.switchContext();

    Screen.flush();
    
    Render.setView(60.0, 1.0, 4000.0);

    Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);

    sky.draw(0, 0);

    Screen.setParam(Screen.DEPTH_TEST_ENABLE, true);
    Screen.setParam(Screen.DEPTH_TEST_METHOD, Screen.DEPTH_GEQUAL);

    scene_object.render();
    box_object.render();

    // Projector follows the skinned character on XZ, projected on ground (y=0)
    projSkin.position = { x: skin_object.position.x, y: 0.0f, z: skin_object.position.z-1.0f };
    projSkin.render();
    // step ODE (harmless if no dynamics)
    world.stepWithContacts(space, jgroup, 0.01f);

    gltf_skin.texture_mapping = true;
    gltf_skin.shade_model = Render.SHADE_GOURAUD;
    gltf_skin.pipeline = Render.PL_DEFAULT;
    skin_object.render();

    Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);

    font.print(10, 10, Screen.getFPS(360) + " FPS | " + free_mem + " | Free VRAM: " + free_vram + "KB");
    font.print(10, 25, gltf_skin.size + " Vertices");

    Screen.flip();
}