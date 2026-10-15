// {"name": "Physics demo", "author": "Daniel Santos", "version": "04072023", "icon": "render_icon.png", "file": "physics_collision.js"}

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

const world = Physics.World();

const jgroup = Physics.JointGroup();

world.setGravity(0, -9.81f, 0);

const space = Physics.Space();

const skin_anims = new AnimCollection("Twerk.gltf");
const gltf_skin = new RenderData("Twerk.gltf");
gltf_skin.accurate_clipping = true;
gltf_skin.face_culling = Render.CULL_FACE_BACK;
gltf_skin.pipeline = Render.PL_DEFAULT;

const skin_object = new RenderObject(gltf_skin);
skin_object.position = {x:0.0, y:8.0, z:0.0};
skin_object.rotation = {x:Math.PI/2, y:0.0, z:0.0};

// Collision below is a primitive GeomSphere, not GeomRenderObject -- it
// never reads gltf_skin's own vertex data, so freeing the float source
// (bone matrices drive the animation, not this data) is safe here.
gltf_skin.freeze();

const skin_sphere = Physics.GeomSphere(undefined, 1.0f);
const skin_col_transform = Physics.GeomTransform(space, skin_sphere);
skin_sphere.setPosition(0, -0.5f, 0);


const skin_body = Physics.Body(world);
skin_col_transform.setBody(skin_body);

skin_object.setCollision(skin_col_transform);
skin_object.setPhysics(skin_body);

skin_object.playAnim(skin_anims[4], false);

const scene = new RenderData("scene.gltf");
scene.face_culling = Render.CULL_FACE_BACK;
scene.accurate_clipping = true;
scene.pipeline = Render.PL_DEFAULT;

scene.getTexture(0).filter = LINEAR;

const scene_object = new RenderObject(scene);
// GeomRenderObject builds its trimesh BVH from scene's positions before freeze()
const scene_collision = Physics.GeomRenderObject(space, scene_object);
scene.freeze();

scene_object.setCollision(scene_collision);

const box = new RenderData("box_bump.gltf");
box.face_culling = Render.CULL_FACE_BACK;

const box_object = new RenderObject(box);
box_object.position = {x:2.0, y:0.2, z:2.0};
box_object.scale = {x:0.2, y:0.2, z:0.2};

const box_col = Physics.GeomRenderObject(space, box_object);
box.freeze();
box_object.setCollision(box_col);

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

    font.print(10, 10, Screen.getFPS(360) + " FPS");

    world.stepWithContacts(space, jgroup, 0.01f);

    Screen.flip();
}
