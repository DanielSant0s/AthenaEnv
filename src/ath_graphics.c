#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "include/graphics.h"
#include "include/fntsys.h"
#include "ath_env.h"

static bool asyncDelayed = true;

volatile int imgThreadResult = 1;
unsigned char* imgThreadData = NULL;
uint32_t imgThreadSize = 0;

static u8 imgThreadStack[4096] __attribute__((aligned(16)));

// Extern symbol 
extern void *_gp;

static int imgThread(void* data)
{
	char* text = (char*)data;
	int file = open(text, O_RDONLY, 0777);
	bool delayed = asyncDelayed;
	uint16_t magic;
	read(file, &magic, 2);
	close(file);
	GSTEXTURE* image = NULL;
	if (magic == 0x4D42) image =      loadbmp(text, delayed);
	else if (magic == 0xD8FF) image = loadjpeg(text, false, delayed);
	else if (magic == 0x5089) image = loadpng(text, delayed);
	else 
	{
		imgThreadResult = 1;
		ExitDeleteThread();
		return 0;
	}
	char* buffer = (char*)malloc(16);
	memset(buffer, 0, 16);
	sprintf(buffer, "%i", (int)image);
	imgThreadData = (unsigned char*)buffer;
	imgThreadSize = strlen(buffer);
	imgThreadResult = 1;
	ExitDeleteThread();
	return 0;
}


duk_ret_t athena_loadimgasync(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 1 && argc != 2) return duk_generic_error(ctx, "threadLoadImage takes 1 or 2 arguments");
	char* text = (char*)(duk_get_string(ctx, 0));
	if (argc == 2) asyncDelayed = duk_get_boolean(ctx, 1);
	
	ee_thread_t thread_param;
	
	thread_param.gp_reg = &_gp;
    thread_param.func = (void*)imgThread;
    thread_param.stack = (void *)imgThreadStack;
    thread_param.stack_size = sizeof(imgThreadStack);
    thread_param.initial_priority = 16;
	int thread = CreateThread(&thread_param);
	if (thread < 0)
	{
		imgThreadResult = -1;
		return 0;
	}
	
	imgThreadResult = 0;
	StartThread(thread, (void*)text);
	return 0;
}

duk_ret_t athena_getloadstate(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if(argc != 0) return duk_generic_error(ctx, "getLoadState takes no arguments");
	duk_push_int(ctx, (uint32_t)imgThreadResult);
	return 1;
}

duk_ret_t athena_getloaddata(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if(argc != 0) return duk_generic_error(ctx, "getLoadData takes no arguments");
	if (imgThreadData != NULL){
		duk_push_lstring(ctx, (const char*)imgThreadData, imgThreadSize);
		free(imgThreadData);
		imgThreadData = NULL;
		return 1;
	} else return 0;
}

duk_ret_t athena_loadimg(duk_context *ctx){
	int argc = duk_get_top(ctx);
	const char* text = duk_get_string(ctx, 0);
	int file = open(text, O_RDONLY, 0777);
	bool delayed = true;
	if (argc == 2) delayed = duk_get_boolean(ctx, 1);
	uint16_t magic;
	read(file, &magic, 2);
	close(file);
	GSTEXTURE* image = NULL;
	if (magic == 0x4D42) image =      loadbmp(text, delayed);
	else if (magic == 0xD8FF) image = loadjpeg(text, false, delayed);
	else if (magic == 0x5089) image = loadpng(text, delayed);
	else return duk_generic_error(ctx, "loadImage %s (invalid magic).", text);

	duk_push_pointer(ctx, (void*)(image));
	return 1;
}

duk_ret_t athena_imgfree(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 1) return duk_generic_error(ctx, "freeImage takes 1 argument");
	
	GSTEXTURE* source = (GSTEXTURE*)(duk_get_pointer(ctx, 0));

	UnloadTexture(source);

	free(source->Mem);
	source->Mem = NULL;
	
	// Free texture CLUT
	if(source->Clut != NULL)
	{
		free(source->Clut);
		source->Clut = NULL;
	}

	free(source);
	source = NULL;

	return 0;
}


duk_ret_t athena_drawimg(duk_context *ctx){
  	int argc = duk_get_top(ctx);
	if (argc != 3 && argc != 4) return duk_generic_error(ctx, "drawImage takes 3 or 4 arguments");
  	GSTEXTURE* source = (GSTEXTURE*)(duk_get_pointer(ctx, 0));
	float x = duk_get_number(ctx, 1);
	float y = duk_get_number(ctx, 2);
	Color color = 0x80808080;
  	if (argc == 4) color = (Color)duk_get_uint(ctx, 3);

	drawImage(source, x, y, source->Width, source->Height, 0.0f, 0.0f, source->Width, source->Height, color);
	return 0;
}


duk_ret_t athena_drawimg_rotate(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 4 && argc != 5) return duk_generic_error(ctx, "drawRotateImage takes 4 or 5 arguments");
    GSTEXTURE* source = (GSTEXTURE*)(duk_get_pointer(ctx, 0));
	float x = duk_get_number(ctx, 1);
	float y = duk_get_number(ctx, 2);
	float radius = duk_get_number(ctx, 3);
	Color color = 0x80808080;
	if (argc == 5) color = (Color)duk_get_uint(ctx, 4);

	drawImageRotate(source, x, y, source->Width, source->Height, 0.0f, 0.0f, source->Width, source->Height, radius, color);

	return 0;
}


duk_ret_t athena_drawimg_scale(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 5 && argc != 6) return duk_generic_error(ctx, "drawScaleImage takes 5 or 6 arguments");
    GSTEXTURE* source = (GSTEXTURE*)(duk_get_pointer(ctx, 0));
	float x = duk_get_number(ctx, 1);
	float y = duk_get_number(ctx, 2);
	float width = duk_get_number(ctx, 3);
	float height = duk_get_number(ctx, 4);
	Color color = 0x80808080;
	if (argc == 6) color = (Color)duk_get_uint(ctx, 5);

	drawImage(source, x, y, width, height, 0.0f, 0.0f, source->Width, source->Height, color);

	return 0;
}

duk_ret_t athena_drawimg_part(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 7 && argc != 8) return duk_generic_error(ctx, "drawPartialImage takes 7 or 8 arguments");
    GSTEXTURE* source = (GSTEXTURE*)(duk_get_pointer(ctx, 0));
	float x = duk_get_number(ctx, 1);
	float y = duk_get_number(ctx, 2);
	float startx = duk_get_number(ctx, 3);
	float starty = duk_get_number(ctx, 4);
	float endx = duk_get_number(ctx, 5);
	float endy = duk_get_number(ctx, 6);
	Color color = 0x80808080;
	if (argc == 8) color = (Color)duk_get_uint(ctx, 7);
	
	drawImage(source, x, y, source->Width, source->Height, startx, starty, endx, endy, color);

	return 0;
}

duk_ret_t athena_drawimg_full(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 10 && argc != 11) return duk_generic_error(ctx, "drawPartialImage takes 10 or 11 arguments");
    GSTEXTURE* source = (GSTEXTURE*)(duk_get_pointer(ctx, 0));
	float x = duk_get_number(ctx, 1);
	float y = duk_get_number(ctx, 2);
	float startx = duk_get_number(ctx, 3);
	float starty = duk_get_number(ctx, 4);
	float endx = duk_get_number(ctx, 5);
	float endy = duk_get_number(ctx, 6);
	float width = duk_get_number(ctx, 7);
	float height = duk_get_number(ctx, 8);
	float angle = duk_get_number(ctx, 9);
	Color color = 0x80808080;
	if (argc == 11) color = (Color)duk_get_uint(ctx, 10);

	drawImageRotate(source, x, y, width, height, startx, starty, endx, endy, angle, color);

	return 0;
}


duk_ret_t athena_width(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 1) return duk_generic_error(ctx, "getImageWidth takes a single argument");
    GSTEXTURE* source = (GSTEXTURE*)(duk_get_pointer(ctx, 0));
	duk_push_number(ctx, source->Width);
	return 1;
}


duk_ret_t athena_height(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 1) return duk_generic_error(ctx, "getImageHeight takes a single argument");
    GSTEXTURE* source = (GSTEXTURE*)(duk_get_pointer(ctx, 0));
	duk_push_number(ctx, source->Height);
	return 1;
}

duk_ret_t athena_filters(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 2) return duk_generic_error(ctx, "setImageFilters takes 2 arguments");
    GSTEXTURE* source = (GSTEXTURE*)(duk_get_pointer(ctx, 0));
	source->Filter = duk_get_number(ctx, 1);
	return 0;
}

duk_ret_t athena_rect(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 5) return duk_generic_error(ctx, "drawRect takes 5 arguments");
	float x = duk_get_number(ctx, 0);
	float y = duk_get_number(ctx, 1);
    float width = duk_get_number(ctx, 2);
    float height = duk_get_number(ctx, 3);
    Color color =  duk_get_uint(ctx, 4);

	drawRect(x, y, width, height, color);

	return 0;
}

duk_ret_t athena_line(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 5) return duk_generic_error(ctx, "drawLine takes 5 arguments");
	float x = duk_get_number(ctx, 0);
	float y = duk_get_number(ctx, 1);
    float x2 = duk_get_number(ctx, 2);
    float y2 = duk_get_number(ctx, 3);
    Color color = duk_get_uint(ctx, 4);

	drawLine(x, y, x2, y2, color);

	return 0;
}

duk_ret_t athena_pixel(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 3) return duk_generic_error(ctx, "drawPixel takes 3 arguments");
	float x = duk_get_number(ctx, 0);
	float y = duk_get_number(ctx, 1);
    Color color = duk_get_uint(ctx, 2);

	drawPixel(x, y, color);
	return 0;
}

duk_ret_t athena_triangle(duk_context *ctx){
	int argc = duk_get_top(ctx);

	if (argc != 7 && argc != 9) return duk_generic_error(ctx, "drawTriangle takes 7 or 9 arguments");

	float x = duk_get_number(ctx, 0);
	float y = duk_get_number(ctx, 1);
    float x2 = duk_get_number(ctx, 2);
    float y2 = duk_get_number(ctx, 3);
	float x3 = duk_get_number(ctx, 4);
    float y3 = duk_get_number(ctx, 5);
    Color color =  duk_get_uint(ctx, 6);

	if (argc == 7) drawTriangle(x, y, x2, y2, x3, y3, color);

	if (argc == 9) {
		Color color2 =  duk_get_uint(ctx, 7);
		Color color3 =  duk_get_uint(ctx, 8);
		drawTriangle_gouraud(x, y, x2, y2, x3, y3, color, color2, color3);
	}
	return 0;
}

duk_ret_t athena_quad(duk_context *ctx){
	int argc = duk_get_top(ctx);

	if (argc != 9 && argc != 12) return duk_generic_error(ctx, "drawQuad takes 9 or 12 arguments");

	float x = duk_get_number(ctx, 0);
	float y = duk_get_number(ctx, 1);
    float x2 = duk_get_number(ctx, 2);
    float y2 = duk_get_number(ctx, 3);
	float x3 = duk_get_number(ctx, 4);
    float y3 = duk_get_number(ctx, 5);
	float x4 = duk_get_number(ctx, 6);
    float y4 = duk_get_number(ctx, 7);
    Color color = duk_get_uint(ctx, 8);

	if (argc == 9) drawQuad(x, y, x2, y2, x3, y3, x4, y4, color);

	if (argc == 12) {
		Color color2 = duk_get_uint(ctx, 9);
		Color color3 = duk_get_uint(ctx, 10);
		Color color4 = duk_get_uint(ctx, 11);
		drawQuad_gouraud(x, y, x2, y2, x3, y3, x4, y4, color, color2, color3, color4);
	}
	return 0;
}

duk_ret_t athena_circle(duk_context *ctx){
	int argc = duk_get_top(ctx);

	if (argc != 4 && argc != 5) return duk_generic_error(ctx, "drawCircle takes 4 or 5 arguments");

	float x = duk_get_number(ctx, 0);
	float y = duk_get_number(ctx, 1);
    float radius = duk_get_number(ctx, 2);
    Color color = duk_get_uint(ctx, 3);

	bool filling = (argc == 5);
    int filled = filling? duk_get_number(ctx, 4) : 1;

	drawCircle(x, y, radius, color, filled);

	return 0;
}

DUK_EXTERNAL duk_ret_t dukopen_graphics(duk_context *ctx) {
  const duk_function_list_entry module_funcs[] = {
	{ "threadLoadImage",    athena_loadimgasync,	DUK_VARARGS },
	{ "getLoadState",       athena_getloadstate,			  0 },
  	{ "getLoadData",     	athena_getloaddata,			      0 },
    { "loadImage",       	athena_loadimg,		 	DUK_VARARGS },
	{ "freeImage", 	     	athena_imgfree,		 			  1 },
    { "drawImage", 	     	athena_drawimg,		 	DUK_VARARGS },
	{ "drawRotateImage", 	athena_drawimg_rotate,  DUK_VARARGS },
	{ "drawScaleImage",  	athena_drawimg_scale,   DUK_VARARGS },
	{ "drawPartialImage",	athena_drawimg_part,    DUK_VARARGS },
	{ "drawImageExtended",	athena_drawimg_full,    DUK_VARARGS },
	{ "getImageWidth",		athena_width,    				  1 },
	{ "getImageHeight",		athena_height,    				  1 },
	{ "setImageFilters",	athena_filters,    			   	  2 },
  	{ "drawPixel",          athena_pixel,					  3 },
  	{ "drawRect",           athena_rect,					  5 },
  	{ "drawLine",           athena_line,					  5 },
	{ "drawTriangle",       athena_triangle,		DUK_VARARGS },
	{ "drawQuad",        	athena_quad,			DUK_VARARGS },
  	{ "drawCircle",         athena_circle,			DUK_VARARGS },
    { NULL, NULL, 0 }
  };

  duk_push_object(ctx);  /* module result */
  duk_put_function_list(ctx, -1, module_funcs);

  return 1;  /* return module value */
}


duk_ret_t athena_fontload(duk_context *ctx){
	if (duk_get_top(ctx) != 1) return duk_generic_error(ctx, "wrong number of arguments"); 
	const char* path = duk_get_string(ctx, 0);
	GSFONT* font = loadFont(path);
	if (font == NULL) return duk_generic_error(ctx, "Error loading font (invalid magic).");
	duk_push_pointer(ctx, (void*)(font));
	return 1;
}

duk_ret_t athena_print(duk_context *ctx) {
	int argc = duk_get_top(ctx);
	if (argc != 5 && argc != 6) return duk_generic_error(ctx, "wrong number of arguments");
	GSFONT* font = (GSFONT*)duk_get_pointer(ctx, 0);
    float x = duk_get_number(ctx, 1);
	float y = duk_get_number(ctx, 2);
    float scale =  duk_get_number(ctx, 3);
    const char* text = duk_get_string(ctx, 4);
	Color color = 0x80808080;
	if (argc == 6) color = duk_get_int(ctx, 5);
	printFontText(font, text, x, y, scale, color);
	return 0;
}

duk_ret_t athena_fontunload(duk_context *ctx){
	int argc = duk_get_top(ctx); 
	if (argc != 1) return duk_generic_error(ctx, "wrong number of arguments"); 
	GSFONT* font = (GSFONT*)duk_get_pointer(ctx, 0);
	unloadFont(font);
	return 0;
}

duk_ret_t athena_ftinit(duk_context *ctx){
	if (duk_get_top(ctx) != 0) return duk_generic_error(ctx, "wrong number of arguments"); 
	fntInit();
	return 0;
}

duk_ret_t athena_ftload(duk_context *ctx){
	if (duk_get_top(ctx) != 1) return duk_generic_error(ctx, "wrong number of arguments"); 
	const char* fontpath = duk_get_string(ctx, 0);
	int fntHandle = fntLoadFile(fontpath);
    duk_push_int(ctx, fntHandle);
	return 1;
}

duk_ret_t athena_ftSetPixelSize(duk_context *ctx) {
	if (duk_get_top(ctx) != 3) return duk_generic_error(ctx, "wrong number of arguments"); 
	int fontid = duk_get_int(ctx, 0);
	int width = duk_get_number(ctx, 1); 
	int height = duk_get_number(ctx, 2); 
	fntSetPixelSize(fontid, width, height);
	return 0;
}


duk_ret_t athena_ftSetCharSize(duk_context *ctx) {
	if (duk_get_top(ctx) != 3) return duk_generic_error(ctx, "wrong number of arguments"); 
	int fontid = duk_get_int(ctx, 0);
	int width = duk_get_int(ctx, 1); 
	int height = duk_get_int(ctx, 2); 
	fntSetCharSize(fontid, width, height);
	return 0;
}

duk_ret_t athena_ftprint(duk_context *ctx) {
	int argc = duk_get_top(ctx);
	if (argc != 7 && argc != 8) return duk_generic_error(ctx, "wrong number of arguments");
	int fontid = duk_get_int(ctx, 0);
    int x = duk_get_int(ctx, 1);
	int y = duk_get_int(ctx, 2);
	int alignment = duk_get_int(ctx, 3);
	int width = duk_get_int(ctx, 4); 
	int height = duk_get_int(ctx, 5); 
    const char* text = duk_get_string(ctx, 6);
	Color color = 0x80808080;
	if (argc == 8) color = duk_get_uint(ctx, 7);
	fntRenderString(fontid, x, y, alignment, width, height, text, color);
	return 0;
}

duk_ret_t athena_ftunload(duk_context *ctx){
	int argc = duk_get_top(ctx); 
	if (argc != 1) return duk_generic_error(ctx, "wrong number of arguments"); 
	int fontid = duk_get_int(ctx, 0);
	fntRelease(fontid);
	return 0;
}


duk_ret_t athena_ftend(duk_context *ctx) {
	int argc = duk_get_top(ctx);
	if (argc != 0) return duk_generic_error(ctx, "wrong number of arguments");
	fntEnd();
	return 0;
}

duk_ret_t athena_fmload(duk_context *ctx) {
	int argc = duk_get_top(ctx);
	if (argc != 0) return duk_generic_error(ctx, "wrong number of arguments");
	loadFontM();
	return 0;
}

duk_ret_t athena_fmprint(duk_context *ctx) {
	int argc = duk_get_top(ctx);
	//if (argc != 3) return duk_generic_error(ctx, "wrong number of arguments");
    float x = duk_get_number(ctx, 0);
	float y = duk_get_number(ctx, 1);
    float scale =  duk_get_number(ctx, 2);
    const char* text = duk_get_string(ctx, 3);
	Color color = 0x80808080;
	if (argc == 5) color =  duk_get_int(ctx, 4);
	printFontMText(text, x, y, scale, color);
	return 0;
}

duk_ret_t athena_fmunload(duk_context *ctx) {
	int argc = duk_get_top(ctx);
	if (argc != 0) return duk_generic_error(ctx, "wrong number of arguments");
	unloadFontM();
	return 0;
}


DUK_EXTERNAL duk_ret_t dukopen_font(duk_context *ctx) {
	const duk_function_list_entry module_funcs[] = {
		//FreeType functions
		{ "ftInit",            		athena_ftinit,				0 },
		{ "ftLoad",            		athena_ftload,				1 },
		{ "ftSetPixelSize",    		athena_ftSetPixelSize,		3 },
		{ "ftSetCharSize", 	   		athena_ftSetCharSize,		3 },
		{ "ftPrint",         		athena_ftprint,	  DUK_VARARGS },
		{ "ftUnload",           	athena_ftunload,			1 },
		{ "ftEnd",           	    athena_ftend,				0 },
		//gsFont function
		{ "load",	                athena_fontload,			1 },
		{ "print",                  athena_print,	  DUK_VARARGS },
		{ "unload",                	athena_fontunload,			1 },
		//gsFontM function
		{ "fmLoad",            		athena_fmload,				0 }, 
		{ "fmPrint",           		athena_fmprint,	  DUK_VARARGS }, 
		{ "fmUnload",         	    athena_fmunload,			0 }, 
		{ NULL, NULL, 0 }
	};

  duk_push_object(ctx);  /* module result */
  duk_put_function_list(ctx, -1, module_funcs);

  return 1;  /* return module value */
}


void athena_graphics_init(duk_context* ctx){
	push_athena_module(dukopen_graphics,   "Graphics");
	push_athena_module(dukopen_font,   	  	   "Font");

	duk_push_uint(ctx, GS_FILTER_NEAREST);
	duk_put_global_string(ctx, "NEAREST");

	duk_push_uint(ctx, GS_FILTER_LINEAR);
	duk_put_global_string(ctx, "LINEAR");
}
