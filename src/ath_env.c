#include <assert.h>
#include <sys/fcntl.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>

#include "ath_env.h"

#define TRUE 1

//ctx stands for duk JavaScript virtual machine stack and values.
duk_context *ctx;

void athena_new_function(duk_context *ctx, duk_c_function func, const char* name){
    duk_push_c_function(ctx, func, DUK_VARARGS);
    duk_put_global_string(ctx, name);
}

float get_obj_float(duk_context* ctx, duk_idx_t idx, const char* key){
	duk_push_this(ctx);
    duk_get_prop_string(ctx, idx, key);
	return duk_to_number(ctx, idx);
}

uint32_t get_obj_uint(duk_context* ctx, duk_idx_t idx, const char* key){
	duk_push_this(ctx);
    duk_get_prop_string(ctx, idx, key);
	return duk_to_uint(ctx, idx);
}

int get_obj_int(duk_context* ctx, duk_idx_t idx, const char* key){
	duk_push_this(ctx);
    duk_get_prop_string(ctx, idx, key);
	return duk_to_int(ctx, idx);
}

bool get_obj_boolean(duk_context* ctx, duk_idx_t idx, const char* key){
	duk_push_this(ctx);
    duk_get_prop_string(ctx, idx, key);
	return duk_to_boolean(ctx, idx);
}

void push_athena_module(duk_c_function func, const char *key){
	printf("AthenaEnv: Pushing %s module...\n", key);
	duk_push_c_function(ctx, func, 0);
    duk_call(ctx, 0);
    duk_put_global_string(ctx, key);
}

JSModuleDef *athena_push_module(JSContext* ctx, JSModuleInitFunc *func, const JSCFunctionListEntry *func_list, int len, const char* module_name){
    JSModuleDef *m;
    m = JS_NewCModule(ctx, module_name, func);
    if (!m)
        return NULL;
    JS_AddModuleExportList(ctx, m, func_list, len);

	printf("AthenaEnv: %s module pushed at 0x%x\n", module_name, m);
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
        js_std_dump_error(ctx);
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
				"import * as std from 'std';\n"
                "import * as os from 'os';\n"
				"import * as Color from 'Color';\n"
				"import * as Screen from 'Screen';\n"
				"import * as Draw from 'Draw';\n"
				"import * as Sound from 'Sound';\n"
				"import * as Timer from 'Timer';\n"
				"import * as Tasks from 'Tasks';\n"
				"import * as Pads from 'Pads';\n"
				"import * as Network from 'Network';\n"
				"import * as Socket from 'Socket';\n"
				"import * as Font from 'Font';\n"
				"import * as Image from 'Image';\n"
				"import * as Render from 'Render';\n"
				"import * as Lights from 'Lights';\n"
				"import * as Camera from 'Camera';\n"
                "globalThis.std = std;\n"
                "globalThis.os = os;\n"
				"globalThis.Color = Color;\n"
				"globalThis.Screen = Screen;\n"
				"globalThis.Draw = Draw;\n"
				"globalThis.Sound = Sound;\n"
				"globalThis.Timer = Timer;\n"
				"globalThis.Tasks = Tasks;\n"
				"globalThis.Pads = Pads;\n"
				"globalThis.Network = Network;\n"
				"globalThis.Socket = Socket.Socket;\n"
				"globalThis.Font = Font.Font;\n"
				"globalThis.NEAREST = 0;\n"
				"globalThis.LINEAR = 1;\n"
				"globalThis.VRAM = false;\n"
				"globalThis.RAM = true;\n"
				"globalThis.Image = Image.Image;\n"
				"globalThis.Render = Render;\n"
				"globalThis.Lights = Lights;\n"
				"globalThis.Camera = Camera;\n";
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
	char fnbuf[256];

	snprintf(fnbuf, sizeof(fnbuf), "%s", filename);

	fnbuf[sizeof(fnbuf) - 1] = (char) 0;

	f = fopen(fnbuf, "r");
	if (!f) {
		fprintf(stderr, "failed to open source file: %s\n", filename);
		fflush(stderr);
		return -1;
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
    return ctx;
}

const char* runScript(const char* script, bool isBuffer)
{
    const char *qjserr = "[qjs error]";
    printf("\nStarting AthenaEnv...\n");
    JSRuntime *rt = JS_NewRuntime(); if (!rt) { return qjserr; }
    js_std_set_worker_new_context_func(JS_NewCustomContext);
    js_std_init_handlers(rt);
    JSContext *ctx = JS_NewCustomContext(rt); if (!ctx) { return qjserr; }

	athena_color_init(ctx);
	athena_screen_init(ctx);
	athena_render_init(ctx);
	athena_shape_init(ctx);
	athena_sound_init(ctx);
	athena_timer_init(ctx);
	athena_task_init(ctx);
	athena_image_init(ctx);
	athena_pads_init(ctx);
	athena_network_init(ctx);
	athena_socket_init(ctx);
	athena_font_init(ctx);


    int s = qjs_handle_file(ctx, script, NULL);
    if (s < 0) { return qjserr; }
    return NULL;
}
