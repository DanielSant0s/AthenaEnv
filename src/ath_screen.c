#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "include/graphics.h"
#include "include/fntsys.h"
#include "ath_env.h"

static JSValue athena_set_clear_color(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	static Color clear_color = GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x80, 0x00);
  	JS_ToInt32(ctx, &clear_color, argv[0]);
  	js_set_clear_color((uint64_t)clear_color);
  	return JS_UNDEFINED;
}

static JSValue athena_displayfunc(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	js_set_render_loop_func(JS_DupValue(ctx, argv[0]));
	return JS_UNDEFINED;
}

static JSValue athena_flip(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
  flipScreen();
  return JS_UNDEFINED;
}

static JSValue athena_clear(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	Color color = GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x80, 0x00);
  	if (argc == 1) JS_ToInt32(ctx, &color, argv[0]);
	clearScreen(color);
	return JS_UNDEFINED;
}


static JSValue athena_vblank(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	graphicWaitVblankStart();
	return JS_UNDEFINED;
}

static JSValue athena_vsync(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	setVSync(JS_ToBool(ctx, argv[0]));
	return JS_UNDEFINED;
}

static JSValue athena_fcount(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	toggleFrameCounter(JS_ToBool(ctx, argv[0]));
	return JS_UNDEFINED;
}

static JSValue athena_getFreeVRAM(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	return JS_NewUint32(ctx, (uint32_t)(getFreeVRAM()));
}


static JSValue athena_getFPS(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int time_intvl;
	JS_ToInt32(ctx, &time_intvl, argv[0]);
	return JS_NewFloat32(ctx, FPSCounter(time_intvl));
}

static JSValue athena_getvmode(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	GSGLOBAL *gsGlobal = getGSGLOBAL();
	JSValue obj = JS_NewObject(ctx);

	JS_DefinePropertyValueStr(ctx, obj, "mode", JS_NewInt32(ctx, gsGlobal->Mode), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "width", JS_NewInt32(ctx, gsGlobal->Width), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "height", JS_NewInt32(ctx, gsGlobal->Height), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "psm", JS_NewInt32(ctx, gsGlobal->PSM), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "interlace", JS_NewInt32(ctx, gsGlobal->Interlace), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "field", JS_NewInt32(ctx, gsGlobal->Field), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "psmz", JS_NewInt32(ctx, gsGlobal->PSMZ), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "zbuffering", JS_NewBool(ctx, gsGlobal->ZBuffering), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "double_buffering", JS_NewBool(ctx, gsGlobal->DoubleBuffering), JS_PROP_C_W_E);

	return obj;
}

static JSValue athena_setvmode(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int width, height, psm, mode, interlace, field, psmz = GS_PSMZ_16S;
	bool zbuffering = false, double_buffering = true;
	uint32_t pass_count;
	JSValue val;

	val = JS_GetPropertyStr(ctx, argv[0], "mode");
	JS_ToInt32(ctx, &mode, val);
	JS_FreeValue(ctx, val);

	val = JS_GetPropertyStr(ctx, argv[0], "width");
	JS_ToInt32(ctx, &width, val);
	JS_FreeValue(ctx, val);

	val = JS_GetPropertyStr(ctx, argv[0], "height");
	JS_ToInt32(ctx, &height, val);
	JS_FreeValue(ctx, val);

	val = JS_GetPropertyStr(ctx, argv[0], "psm");
	JS_ToInt32(ctx, &psm, val);
	JS_FreeValue(ctx, val);

	val = JS_GetPropertyStr(ctx, argv[0], "interlace");
	JS_ToInt32(ctx, &interlace, val);
	JS_FreeValue(ctx, val);

	val = JS_GetPropertyStr(ctx, argv[0], "field");
	JS_ToInt32(ctx, &field, val);
	JS_FreeValue(ctx, val);

	val = JS_GetPropertyStr(ctx, argv[0], "psmz");
	JS_ToInt32(ctx, &psmz, val);
	JS_FreeValue(ctx, val);

	val = JS_GetPropertyStr(ctx, argv[0], "zbuffering");
	zbuffering = JS_ToBool(ctx, val);
	JS_FreeValue(ctx, val);

	val = JS_GetPropertyStr(ctx, argv[0], "double_buffering");
	double_buffering = JS_ToBool(ctx, val);
	JS_FreeValue(ctx, val);

	val = JS_GetPropertyStr(ctx, argv[0], "pass_count");
	JS_ToUint32(ctx, &pass_count, val);
	JS_FreeValue(ctx, val);

	setVideoMode(mode, width, height, psm, interlace, field, zbuffering, psmz, double_buffering, pass_count);
	return JS_UNDEFINED;
}

static char* str_buf = NULL;
static size_t str_len = 0;
static size_t buf_len = 0;

#define SCRLOG_LENGTH 72

static JSValue athena_scrlog(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	unsigned int old_len, len = 0;
	const char* in_str = JS_ToCStringLen(ctx, &len, argv[0]);
	
	if(!str_buf) {
		str_buf = malloc(512);
		buf_len = 512;
		memset(str_buf, 0, buf_len);
		fntLoadDefault(NULL);
	}

	old_len = str_len;
	str_len += len + 1;

	while (buf_len < str_len) {
		buf_len += 512;
		str_buf = realloc(str_buf, buf_len);
	}

	if (old_len > 0) {
		str_buf[old_len-1] = '\n';
	}

	strcat(str_buf, in_str);

	clearScreen(GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x80, 0x00));

	fntSetCharSize(0, FNTSYS_CHAR_SIZE*64*0.8f, FNTSYS_CHAR_SIZE*64*0.8f);

	if (str_len > 0) {
		fntRenderString(0, 0, 0, 0, 640, 448, str_buf, 0x80808080);
	}

	flipScreen();

	return JS_UNDEFINED;
}

static JSValue athena_cls(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (str_len > 0) {
		memset(str_buf, 0, str_len);
		str_len = 0;
	}

	return JS_UNDEFINED;
}

static const JSCFunctionListEntry module_funcs[] = {
    JS_CFUNC_DEF("flip", 0, athena_flip),
    JS_CFUNC_DEF("clear", 1, athena_clear),
	JS_CFUNC_DEF("getFreeVRAM", 0, athena_getFreeVRAM),
	JS_CFUNC_DEF("getFPS", 1, athena_getFPS),
    JS_CFUNC_DEF("waitVblankStart", 0, athena_vblank),
	JS_CFUNC_DEF("setVSync", 1, athena_vsync),
	JS_CFUNC_DEF("setFrameCounter", 1, athena_fcount),
	JS_CFUNC_DEF("getMode", 0, athena_getvmode),
    JS_CFUNC_DEF("setMode", 1, athena_setvmode),
	JS_CFUNC_DEF("log", 1, athena_scrlog),
	JS_CFUNC_DEF("cls", 0, athena_cls),

	JS_CFUNC_DEF("clearColor", 1, athena_set_clear_color),
	JS_CFUNC_DEF("display", 1, athena_displayfunc),

	JS_PROP_INT32_DEF("NTSC", GS_MODE_NTSC, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("DTV_480p", GS_MODE_DTV_480P, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("PAL", GS_MODE_PAL, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("DTV_576p", GS_MODE_DTV_576P, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("DTV_720p", GS_MODE_DTV_720P, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("DTV_1080i", GS_MODE_DTV_1080I, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("INTERLACED", GS_INTERLACED, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("PROGRESSIVE", GS_NONINTERLACED, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("FIELD", GS_FIELD, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("FRAME", GS_FRAME, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("CT32", GS_PSM_CT32, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("CT24", GS_PSM_CT24, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("CT16", GS_PSM_CT16, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("CT16S", GS_PSM_CT16S, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("Z32", GS_PSMZ_32, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("Z24", GS_PSMZ_24, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("Z16", GS_PSMZ_16, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("Z16S", GS_PSMZ_16S, JS_PROP_CONFIGURABLE),
};

static int screen_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs));
}

JSModuleDef *athena_screen_init(JSContext* ctx){
	return athena_push_module(ctx, screen_init, module_funcs, countof(module_funcs), "Screen");
}