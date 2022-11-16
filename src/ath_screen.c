#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "include/graphics.h"
#include "include/fntsys.h"
#include "ath_env.h"

static JSValue athena_flip(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
  flipScreen();
  return 0;
}

static JSValue athena_clear(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	Color color = GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x80, 0x00);
  if (argc == 1) JS_ToInt32(ctx, &color, argv[0]);
	clearScreen(color);
	return 0;
}


static JSValue athena_vblank(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	graphicWaitVblankStart();
	return 0;
}

static JSValue athena_vsync(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	setVSync(JS_ToBool(ctx, argv[0]));
	return 0;
}

static JSValue athena_getFreeVRAM(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	return JS_NewUint32(ctx, (uint32_t)(getFreeVRAM()));
}


static JSValue athena_getFPS(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int time_intvl;
	JS_ToInt32(ctx, &time_intvl, argv[0]);
	return JS_NewFloat64(ctx, FPSCounter(time_intvl));
}


static JSValue athena_setvmode(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int width, height, psm, psmz = GS_PSMZ_16S;
	s16 mode, interlace, field;
	bool zbuffering = false;

	JS_ToInt32(ctx, &mode, argv[0]);
	JS_ToInt32(ctx, &width, argv[1]);
	JS_ToInt32(ctx, &height, argv[2]);
	JS_ToInt32(ctx, &psm, argv[3]);
	JS_ToInt32(ctx, &interlace, argv[4]);
	JS_ToInt32(ctx, &field, argv[5]);
	if(argc == 8){
		zbuffering = JS_ToBool(ctx, argv[6]);
		JS_ToInt32(ctx, &psmz, argv[7]);
	}
	setVideoMode(mode, width, height, psm, interlace, field, zbuffering, psmz);
	return 0;
}

static const JSCFunctionListEntry module_funcs[] = {
    JS_CFUNC_DEF("flip", 0, athena_flip),
    JS_CFUNC_DEF("clear", -1, athena_clear),
	JS_CFUNC_DEF("getFreeVRAM", 0, athena_getFreeVRAM),
	JS_CFUNC_DEF("getFPS", 1, athena_getFPS),
    JS_CFUNC_DEF("waitVblankStart", 0, athena_vblank),
	JS_CFUNC_DEF("setVSync", 1, athena_vsync),
    JS_CFUNC_DEF("setMode", -1, athena_setvmode)
};

static int screen_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs));
}

JSModuleDef *athena_screen_init(JSContext* ctx){
	return athena_push_module(ctx, screen_init, module_funcs, countof(module_funcs), "Screen");
	
	/*duk_push_uint(ctx, GS_MODE_NTSC);
	duk_put_global_string(ctx, "NTSC");

	duk_push_uint(ctx, GS_MODE_DTV_480P);
	duk_put_global_string(ctx, "_480p");

	duk_push_uint(ctx, GS_MODE_PAL);
	duk_put_global_string(ctx, "PAL");

	duk_push_uint(ctx, GS_MODE_DTV_576P);
	duk_put_global_string(ctx, "_576p");

	duk_push_uint(ctx, GS_MODE_DTV_720P);
	duk_put_global_string(ctx, "_720p");

	duk_push_uint(ctx, GS_MODE_DTV_1080I);
	duk_put_global_string(ctx, "_1080i");

	duk_push_uint(ctx, GS_INTERLACED);
	duk_put_global_string(ctx, "INTERLACED");

	duk_push_uint(ctx, GS_NONINTERLACED);
	duk_put_global_string(ctx, "NONINTERLACED");

	duk_push_uint(ctx, GS_FIELD);
	duk_put_global_string(ctx, "FIELD");

	duk_push_uint(ctx, GS_FRAME);
	duk_put_global_string(ctx, "FRAME");

	duk_push_uint(ctx, GS_PSM_CT32);
	duk_put_global_string(ctx, "CT32");

	duk_push_uint(ctx, GS_PSM_CT24);
	duk_put_global_string(ctx, "CT24");

	duk_push_uint(ctx, GS_PSM_CT16);
	duk_put_global_string(ctx, "CT16");

	duk_push_uint(ctx, GS_PSM_CT16S);
	duk_put_global_string(ctx, "CT16S");

	duk_push_uint(ctx, GS_PSMZ_32);
	duk_put_global_string(ctx, "Z32");

	duk_push_uint(ctx, GS_PSMZ_24);
	duk_put_global_string(ctx, "Z24");

	duk_push_uint(ctx, GS_PSMZ_16);
	duk_put_global_string(ctx, "Z16");

	duk_push_uint(ctx, GS_PSMZ_16S);
	duk_put_global_string(ctx, "Z16S");*/

}