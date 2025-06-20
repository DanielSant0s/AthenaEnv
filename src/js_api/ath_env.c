#include <assert.h>
#include <sys/fcntl.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>

#include <ath_env.h>

#include <memory.h>

#define TRUE 1

#define JSFILE_NOTFOUND -5656

JSModuleDef *athena_push_module(JSContext* ctx, JSModuleInitFunc *func, const JSCFunctionListEntry *func_list, int len, const char* module_name){
    JSModuleDef *m;
    m = JS_NewCModule(ctx, module_name, func);
    if (!m)
        return NULL;
    JS_AddModuleExportList(ctx, m, func_list, len);

	dbgprintf("AthenaEnv: %s module pushed at 0x%x\n", module_name, m);
    return m;
}

static int qjs_eval_buf(JSContext *ctx, const void *buf, int buf_len,
                    const char *filename, int eval_flags)
{
    JSValue val;
    int ret;

    if ((eval_flags & JS_EVAL_TYPE_MASK) == JS_EVAL_TYPE_MODULE) {
        /* for the modules, we compile then run to be able to set
           import.meta */
        val = JS_Eval(ctx, buf, buf_len, filename,
                      eval_flags | JS_EVAL_FLAG_COMPILE_ONLY);
        if (!JS_IsException(val)) {
            js_module_set_import_meta(ctx, val, TRUE, TRUE);
            val = JS_EvalFunction(ctx, val);
        }
    } else {
        val = JS_Eval(ctx, buf, buf_len, filename, eval_flags);
    }
	
    if (JS_IsException(val)) {
        ret = -1;
    } else {
        ret = 0;
    }

	JS_FreeValue(ctx, val);
    return ret;
}

static int qjs_handle_fh(JSContext *ctx, FILE *f, const char *filename, const char *bytecode_filename) {
	char *buf = NULL;
	size_t bufsz;
	size_t bufoff;
	size_t got;
	int rc;
	int retval = -1;

	buf = (char *) malloc(1024);
	if (!buf) {
		if (buf) {
			free(buf);
			buf = NULL;
		}
		return retval;
	}
	bufsz = 1024;
	bufoff = 0;

	/* Read until EOF, avoid fseek/stat because it won't work with stdin. */
	for (;;) {
		size_t avail;

		avail = bufsz - bufoff;
		if (avail < 1024) {
			size_t newsz;
			char *buf_new;

			newsz = bufsz + (bufsz >> 2) + 1024;  /* +25% and some extra */
			if (newsz < bufsz) {
				if (buf) {
					free(buf);
					buf = NULL;
				}
				return retval;
			}
			buf_new = (char *) realloc(buf, newsz);
			if (!buf_new) {
				if (buf) {
					free(buf);
					buf = NULL;
				}
				return retval;
			}
			buf = buf_new;
			bufsz = newsz;
		}

		avail = bufsz - bufoff;

		got = fread((void *) (buf + bufoff), (size_t) 1, avail, f);

		if (got == 0) {
			break;
		}
		bufoff += got;
	}

	buf[bufoff++] = 0;
        js_std_add_helpers(ctx, 0, NULL);
        { // make 'std' and 'os' visible to non module code
            const char *str = 
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

				#ifdef ATHENA_NETWORK
				"import * as Network from 'Network';\n"
				"import * as Request from 'Request';\n"
				"import * as Socket from 'Socket';\n"
				"import * as SocketConst from 'SocketConst';\n"
				"import * as WebSocket from 'WebSocket';\n"
				"globalThis.Network = Network;\n"
				"globalThis.AF_INET = SocketConst.AF_INET;\n"
				"globalThis.SOCK_STREAM = SocketConst.SOCK_STREAM;\n"
				"globalThis.SOCK_DGRAM = SocketConst.SOCK_DGRAM;\n"
				"globalThis.SOCK_RAW = SocketConst.SOCK_RAW;\n"
				"globalThis.Socket = Socket.Socket;\n"
				"globalThis.Request = Request.Request;\n"
				"globalThis.WebSocket = WebSocket.WebSocket;\n"
				#endif

				#ifdef ATHENA_GRAPHICS
				"import * as Color from 'Color';\n"
				"import * as Screen from 'Screen';\n"
				"import * as Draw from 'Draw';\n"
				"import * as Font from 'Font';\n"
				"import * as Image from 'Image';\n"
				"import * as ImageList from 'ImageList';\n"
				"import * as Render from 'Render';\n"
				"import * as RenderData from 'RenderData';\n"
				"import * as RenderObject from 'RenderObject';\n"
				"import * as AnimCollection from 'AnimCollection';\n"
				"import * as Lights from 'Lights';\n"
				"import * as Camera from 'Camera';\n"
				"globalThis.Color = Color;\n"
				"globalThis.Screen = Screen;\n"
				"globalThis.Draw = Draw;\n"

				"globalThis.Font = Font.Font;\n"

				"globalThis.NEAREST = 0;\n"
				"globalThis.LINEAR = 1;\n"
				"globalThis.VRAM = false;\n"
				"globalThis.RAM = true;\n"
				"globalThis.Image = Image.Image;\n"
				"globalThis.ImageList = ImageList.ImageList;\n"

				"globalThis.Render = Render;\n"
				"globalThis.RenderData = RenderData.RenderData;\n"
				"globalThis.RenderObject = RenderObject.RenderObject;\n"

				"globalThis.AnimCollection = AnimCollection.AnimCollection;\n"

				"globalThis.Lights = Lights;\n"

				"globalThis.Camera = Camera;\n"

				#endif

				"import * as std from 'std';\n"
                "import * as os from 'os';\n"
				"import * as Timer from 'Timer';\n"
				"import * as Threads from 'Threads';\n"
				"import * as Pads from 'Pads';\n"
				"import * as System from 'System';\n"
				"import * as IOP from 'IOP';\n"
				"import * as Archive from 'Archive';\n"
				"import * as Vector2 from 'Vector2';\n"
				"import * as Vector3 from 'Vector3';\n"
				"import * as Vector4 from 'Vector4';\n"
				"import * as Matrix4 from 'Matrix4';\n"
				"import * as Physics from 'Physics';\n"

                "globalThis.std = std;\n"
                "globalThis.os = os;\n"
				"globalThis.Timer = Timer;\n"
				"globalThis.Threads = Threads;\n"
				"globalThis.Pads = Pads;\n"
				"globalThis.System = System;\n"
				"globalThis.Archive = Archive;\n"
				"globalThis.IOP = IOP;\n"
				"globalThis.Vector2 = Vector2.Vector2;\n"
				"globalThis.Vector3 = Vector3.Vector3;\n"
				"globalThis.Vector4 = Vector4.Vector4;\n"
				"globalThis.Matrix4 = Matrix4.Matrix4;\n"
				"globalThis.Physics = Physics;\n";

				
            rc = qjs_eval_buf(ctx, str, strlen(str), "<input>", JS_EVAL_TYPE_MODULE);
            if (rc != 0) { return retval; }
        }
	rc = qjs_eval_buf(ctx, (void *) buf, bufoff - 1, filename, JS_EVAL_TYPE_MODULE);
	free(buf);
	buf = NULL;
	if (rc != 0) { return retval; } else { return 0; }
	return retval;
}

static int qjs_handle_file(JSContext *ctx, const char *filename, const char *bytecode_filename) {
	FILE *f = NULL;
	int retval;

	f = fopen(filename, "r");
	if (!f) {
		//fprintf(stderr, "failed to open source file: %s\n", filename);
		//fflush(stderr);
		return JSFILE_NOTFOUND;
	}

	retval = qjs_handle_fh(ctx, f, filename, bytecode_filename);

	fclose(f);
	return retval;
}

static JSContext *JS_NewCustomContext(JSRuntime *rt)
{
    JSContext *ctx;
    ctx = JS_NewContext(rt);
    if (!ctx)
        return NULL;
    /* system modules */
    js_init_module_std(ctx, "std");
    js_init_module_os(ctx, "os");

	JS_AddIntrinsicBigFloat(ctx); 
	JS_AddIntrinsicBigDecimal(ctx);
	JS_AddIntrinsicOperators(ctx);
	JS_EnableBignumExt(ctx, TRUE);

	athena_system_init(ctx);
	athena_iop_init(ctx);
	athena_archive_init(ctx);
	athena_timer_init(ctx);
	athena_task_init(ctx);
	athena_pads_init(ctx);
	athena_vector_init(ctx);
	athena_vector4_init(ctx);
	athena_matrix_init(ctx);
	athena_physics_init(ctx);

	#ifdef ATHENA_AUDIO
	athena_sound_init(ctx);
	#endif

	#ifdef ATHENA_GRAPHICS
	athena_color_init(ctx);
	athena_image_init(ctx);
	athena_imagelist_init(ctx);
	athena_font_init(ctx);
	athena_shape_init(ctx);
	athena_screen_init(ctx);
	athena_render_init(ctx);
	athena_lights_init(ctx);
	athena_3dcamera_init(ctx);
	athena_anim_3d_init(ctx);
	#endif

	#ifdef ATHENA_KEYBOARD
	athena_keyboard_init(ctx);
	#endif

	#ifdef ATHENA_MOUSE
	athena_mouse_init(ctx);
	#endif

	#ifdef ATHENA_CAMERA
	athena_camera_init(ctx);
	#endif

	#ifdef ATHENA_NETWORK
	athena_network_init(ctx);
	athena_request_init(ctx);
	athena_socket_init(ctx);
	athena_ws_init(ctx); 
	#endif

    return ctx;
}

static char error_buf[4096];

void destroy_vm(JSContext* ctx) {
	JSRuntime* rt = JS_GetRuntime(ctx);
	js_std_free_handlers(rt);
	JS_FreeContext(ctx);
	JS_FreeRuntime(rt);
}

static jmp_buf vm_reset_buf;

jmp_buf *get_reset_buf() {
	return &vm_reset_buf;
}

const char* run_script(const char* script, bool isBuffer)
{
	size_t memoryLimit = (GetMemorySize() - get_used_memory()) >> 1;

    dbgprintf("\nStarting AthenaEnv...\n");
    JSRuntime *rt = JS_NewRuntime(); if (!rt) { return "AthenaError: Runtime creation"; }
    js_std_set_worker_new_context_func(JS_NewCustomContext);
    js_std_init_handlers(rt);

	JS_SetMemoryLimit(rt, memoryLimit);
	JS_SetGCThreshold(rt, memoryLimit - 4194304);

    JSContext *ctx = JS_NewCustomContext(rt); if (!ctx) { return "AthenaError: Context creation"; }

	JS_SetModuleLoaderFunc(rt, NULL, js_module_loader, NULL);

	while (!bootlogo_finished() && boot_logo) {
		usleep(1000);
	}

    int s = qjs_handle_file(ctx, script, NULL);

    if (s < 0) { 
		if (s == JSFILE_NOTFOUND) {
			sprintf(error_buf, "AthenaError: Fail when opening %s\n"
							   "\nTip: If you are on PCSX2, check Host filesystem!\n", script);
		} else {
			JSValue exception_val = JS_GetException(ctx);
			const char* exception = JS_ToCString(ctx, exception_val);
			JSValue stack_val = JS_GetPropertyStr(ctx, exception_val, "stack");
			const char* stack = JS_ToCString(ctx, stack_val);
			JS_FreeValue(ctx, exception_val);
			JS_FreeValue(ctx, stack_val);

			strcpy(error_buf, exception);
			strcat(error_buf, "\n");
			strcat(error_buf, stack);
		}
		
		destroy_vm(ctx);

		return error_buf; 
	} else {
		s = js_std_loop(ctx);

		if (s < 0) {
			JSValue exception_val = JS_GetException(ctx);
			const char* exception = JS_ToCString(ctx, exception_val);
			JSValue stack_val = JS_GetPropertyStr(ctx, exception_val, "stack");
			const char* stack = JS_ToCString(ctx, stack_val);
			JS_FreeValue(ctx, exception_val);
			JS_FreeValue(ctx, stack_val);

			strcpy(error_buf, exception);
			strcat(error_buf, "\n");
			strcat(error_buf, stack);

			destroy_vm(ctx);

			return error_buf; 
		}
	}
	
	destroy_vm(ctx);
    return NULL;
}
