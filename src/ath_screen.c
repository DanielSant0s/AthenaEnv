#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "include/graphics.h"
#include "include/fntsys.h"
#include "ath_env.h"

duk_ret_t athena_flip(duk_context *ctx){
  flipScreen();
  return 0;
}

duk_ret_t athena_clear(duk_context *ctx){
	int argc = duk_get_top(ctx);
	Color color = GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x80, 0x00);
  if (argc == 1) color = duk_get_uint(ctx, 0);
	clearScreen(color);
	return 0;
}


duk_ret_t athena_vblank(duk_context *ctx){
	graphicWaitVblankStart();
	return 0;
}

duk_ret_t athena_vsync(duk_context *ctx){
	setVSync(duk_get_boolean(ctx, 0));
	return 0;
}

duk_ret_t athena_getFreeVRAM(duk_context *ctx){
	duk_push_int(ctx, (uint32_t)(getFreeVRAM()));
	return 1;
}


duk_ret_t athena_getFPS(duk_context *ctx){
	duk_push_number(ctx, (uint32_t)(FPSCounter(duk_get_int(ctx, 0))));
	return 1;
}


duk_ret_t athena_setvmode(duk_context *ctx){
	int argc = duk_get_top(ctx);
	s16 mode = (s16)duk_get_int(ctx, 0);
	int width = duk_get_int(ctx, 1);
	int height = duk_get_int(ctx, 2);
	int psm = duk_get_int(ctx, 3);
	s16 interlace = (s16)duk_get_int(ctx, 4);
	s16 field = (s16)duk_get_int(ctx, 5);
	bool zbuffering = false;
	int psmz = GS_PSMZ_16S;
	if(argc == 8){
		zbuffering = duk_get_boolean(ctx, 6);
		psmz = duk_get_int(ctx, 7);
	}
	setVideoMode(mode, width, height, psm, interlace, field, zbuffering, psmz);
	return 0;
}


DUK_EXTERNAL duk_ret_t dukopen_screen(duk_context *ctx) {
  const duk_function_list_entry module_funcs[] = {
    { "flip",             athena_flip,         		      0 },
    { "clear",            athena_clear,    		DUK_VARARGS },
	{ "getFreeVRAM",      athena_getFreeVRAM,			  0 },
	{ "getFPS",           athena_getFPS,				  1 },
    { "waitVblankStart",  athena_vblank,             	  0 },
	{ "setVSync",  		  athena_vsync,              	  1 },
    { "setMode",          athena_setvmode, 		DUK_VARARGS },
    { NULL, NULL, 0 }
  };

  duk_push_object(ctx);  /* module result */
  duk_put_function_list(ctx, -1, module_funcs);

  return 1;  /* return module value */
}

void athena_screen_init(duk_context* ctx){
	push_athena_module(dukopen_screen, 	    "Display");
	
	duk_push_uint(ctx, GS_MODE_NTSC);
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
	duk_put_global_string(ctx, "Z16S");

}