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

Render.init();

Render.setView(60.0, 1.0, 4000.0);

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

const scene = new RenderData("scene.gltf");
scene.face_culling = Render.CULL_FACE_NONE;
scene.pipeline = Render.PL_DEFAULT;

scene.getTexture(0).filter = LINEAR;

const scene_object = new RenderObject(scene);

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

    Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);

    sky.draw(0, 0);

    Screen.setParam(Screen.DEPTH_TEST_ENABLE, true);
    Screen.setParam(Screen.DEPTH_TEST_METHOD, Screen.DEPTH_GEQUAL);

    scene_object.render();
    box_object.render();
    skin_object.render();

    Screen.setParam(Screen.DEPTH_TEST_ENABLE, false);

    font.print(10, 10, Screen.getFPS(360) + " FPS | " + free_mem + " | Free VRAM: " + free_vram + "KB");
    font.print(10, 25, gltf_skin.size + " Vertices");

    Screen.flip();
}