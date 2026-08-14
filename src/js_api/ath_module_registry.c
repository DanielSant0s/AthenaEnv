#include <ath_env.h>

typedef JSModuleDef *(*athena_module_init_fn)(JSContext *);

typedef struct {
    athena_module_init_fn init;
} athena_js_module_entry_t;

static const athena_js_module_entry_t athena_js_modules[] = {
    { athena_system_init },
    { athena_iop_init },
    { athena_archive_init },
    { athena_timer_init },
    { athena_task_init },
    { athena_mutex_init },
    { athena_pads_init },
    { athena_vector_init },
    { athena_vector4_init },
    { athena_matrix_init },
    { athena_box2d_init },
#ifdef ATHENA_AUDIO
    { athena_sound_init },
#endif
#ifdef ATHENA_GRAPHICS
    { athena_color_init },
    { athena_image_init },
    { athena_imagelist_init },
    { athena_font_init },
    { athena_shape_init },
    { athena_screen_init },
    { athena_render_init },
    { athena_lights_init },
    { athena_3dcamera_init },
    { athena_anim_3d_init },
    { athena_shadows_init },
    { athena_tilemap_init },
#endif
#ifdef ATHENA_ODE
    { athena_ode_init },
#endif
#ifdef ATHENA_KEYBOARD
    { athena_keyboard_init },
#endif
#ifdef ATHENA_MOUSE
    { athena_mouse_init },
#endif
#ifdef ATHENA_REMOTE
    { athena_remote_init },
#endif
#ifdef ATHENA_CAMERA
    { athena_camera_init },
#endif
#ifdef ATHENA_NETWORK
    { athena_network_init },
    { athena_request_init },
    { athena_socket_init },
    { athena_ws_init },
#endif
#ifdef ATHENA_NATIVE_COMPILER
    { athena_native_init },
#endif
#ifdef ATHENA_MPEG_VIDEO
    { athena_mpeg_init },
#endif
};

void athena_js_register_modules(JSContext *ctx) {
    for (unsigned int i = 0; i < sizeof(athena_js_modules) / sizeof(athena_js_modules[0]); i++) {
        if (athena_js_modules[i].init)
            athena_js_modules[i].init(ctx);
    }
}

const char *athena_js_globals_prelude(void) {
    static const char prelude[] =
#ifdef ATHENA_AUDIO
        "import * as Sound from 'Sound';\n"
        "globalThis.Sound = Sound;\n"
#endif
#ifdef ATHENA_CAMERA
        "import * as Camera from 'Camera';\n"
        "globalThis.Camera = Camera;\n"
#endif
#ifdef ATHENA_KEYBOARD
        "import * as Keyboard from 'Keyboard';\n"
        "globalThis.Keyboard = Keyboard;\n"
#endif
#ifdef ATHENA_MOUSE
        "import * as Mouse from 'Mouse';\n"
        "globalThis.Mouse = Mouse;\n"
#endif
#ifdef ATHENA_REMOTE
        "import * as Remote from 'Remote';\n"
        "globalThis.Remote = Remote;\n"
#endif
#ifdef ATHENA_NETWORK
        "import * as Network from 'Network';\n"
        "import * as Request from 'Request';\n"
        "import * as Socket from 'Socket';\n"
        "import * as WebSocket from 'WebSocket';\n"
        "globalThis.Network = Network;\n"
        "globalThis.Socket = Socket.Socket;\n"
        "globalThis.Request = Request.Request;\n"
        "globalThis.WebSocket = WebSocket.WebSocket;\n"
#endif
#ifdef ATHENA_GRAPHICS
        "import * as Shadows from 'Shadows';\n"
        "import * as Color from 'Color';\n"
        "import * as Screen from 'Screen';\n"
        "import * as Draw from 'Draw';\n"
        "import * as Font from 'Font';\n"
        "import * as Image from 'Image';\n"
        "import * as ImageList from 'ImageList';\n"
        "import * as Render from 'Render';\n"
        "import * as RenderData from 'RenderData';\n"
        "import * as RenderObject from 'RenderObject';\n"
        "import * as RenderBatch from 'RenderBatch';\n"
        "import * as RenderSceneNode from 'RenderSceneNode';\n"
        "import * as RenderAsyncLoader from 'RenderAsyncLoader';\n"
        "import * as AnimCollection from 'AnimCollection';\n"
        "import * as Lights from 'Lights';\n"
        "import * as Camera from 'Camera';\n"
        "globalThis.Color = Color;\n"
        "globalThis.Screen = Screen;\n"
        "globalThis.Draw = Draw;\n"
        "globalThis.Font = Font.Font;\n"
        "globalThis.NEAREST = 0;\n"
        "globalThis.LINEAR = 1;\n"
        "globalThis.Image = Image.Image;\n"
        "globalThis.ImageList = ImageList.ImageList;\n"
        "globalThis.Render = Render;\n"
        "globalThis.RenderData = RenderData.RenderData;\n"
        "globalThis.RenderObject = RenderObject.RenderObject;\n"
        "globalThis.Batch = RenderBatch.Batch;\n"
        "globalThis.SceneNode = RenderSceneNode.SceneNode;\n"
        "globalThis.AsyncLoader = RenderAsyncLoader.AsyncLoader;\n"
        "globalThis.AnimCollection = AnimCollection.AnimCollection;\n"
        "globalThis.Lights = Lights;\n"
        "globalThis.Camera = Camera;\n"
        "globalThis.Shadows = Shadows;\n"
        "import TileMap from 'TileMap';\n"
        "globalThis.TileMap = TileMap;\n"
#endif
#ifdef ATHENA_MPEG_VIDEO
        "import * as Video from 'Video';\n"
        "globalThis.Video = Video.Video;\n"
#endif
#ifdef ATHENA_ODE
        "import * as ODE from 'ODE';\n"
        "globalThis.ODE = ODE;\n"
#endif
        "import * as std from 'std';\n"
        "import * as os from 'os';\n"
        "import * as Timer from 'Timer';\n"
        "import * as Pads from 'Pads';\n"
        "import * as System from 'System';\n"
        "import * as IOP from 'IOP';\n"
        "import * as Archive from 'Archive';\n"
        "import * as Vector2 from 'Vector2';\n"
        "import * as Vector3 from 'Vector3';\n"
        "import * as Vector4 from 'Vector4';\n"
        "import * as Matrix4 from 'Matrix4';\n"
        "import * as Box2D from 'Box2D';\n"
        "globalThis.std = std;\n"
        "globalThis.os = os;\n"
        "globalThis.Timer = Timer;\n"
        "globalThis.Pads = Pads;\n"
        "globalThis.System = System;\n"
        "globalThis.Archive = Archive;\n"
        "globalThis.IOP = IOP;\n"
        "globalThis.Vector2 = Vector2.Vector2;\n"
        "globalThis.Vector3 = Vector3.Vector3;\n"
        "globalThis.Vector4 = Vector4.Vector4;\n"
        "globalThis.Matrix4 = Matrix4.Matrix4;\n"
        "globalThis.Box2D = Box2D;\n"
        "import Thread from 'Thread';\n"
        "globalThis.Thread = Thread;\n"
        "import Mutex from 'Mutex';\n"
        "globalThis.Mutex = Mutex;\n"
#ifdef ATHENA_NATIVE_COMPILER
        "import * as Native from 'Native';\n"
        "globalThis.Native = Native;\n"
#endif
        ;

    return prelude;
}
