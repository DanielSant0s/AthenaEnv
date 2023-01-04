#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "include/render.h"
#include "ath_env.h"

static JSValue athena_initrender(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
  	if (argc != 1) return JS_ThrowSyntaxError(ctx, "wrong number of arguments.");
	double aspect;
	JS_ToFloat64(ctx, &aspect, argv[0]);
  	init3D(aspect);
	return JS_UNDEFINED;
}

static JSValue athena_loadobj(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	JSImageData *image;
	model* res_m;

	#ifndef SKIP_ERROR_HANDLING
	if (argc != 2 && argc != 1) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	#endif
	const char *file_tbo = JS_ToCString(ctx, argv[0]); //Model filename
	
	// Loading texture
	if(argc == 2) {
		image = JS_GetOpaque2(ctx, argv[1], get_img_class_id());
		res_m = loadOBJ(file_tbo, &(image->tex));
	} else {
		res_m = loadOBJ(file_tbo, NULL);
	}

	return JS_NewUint32(ctx, res_m);
}


static JSValue athena_freeobj(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
#ifndef SKIP_ERROR_HANDLING
	if (argc != 1) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
#endif
	
	model* m;
	JS_ToUint32(ctx, &m, argv[0]);

    free(m->idxList);
	m->idxList = NULL;
	free(m->positions);
	m->positions = NULL;
    free(m->colours);
	m->colours = NULL;
    free(m->normals);
	m->normals = NULL;
    free(m->texcoords);
	m->texcoords = NULL;
	free(m->bounding_box);
	m->bounding_box = NULL;
	
	free(m);
	m = NULL;

	return JS_UNDEFINED;
}

static JSValue athena_drawobj(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 7) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");

	double pos_x, pos_y, pos_z, rot_x, rot_y, rot_z;
	model* m;
	JS_ToUint32(ctx, &m, argv[0]);

	JS_ToFloat64(ctx, &pos_x, argv[1]);
	JS_ToFloat64(ctx, &pos_y, argv[2]);
	JS_ToFloat64(ctx, &pos_z, argv[3]);
	JS_ToFloat64(ctx, &rot_x, argv[4]);
	JS_ToFloat64(ctx, &rot_y, argv[5]);
	JS_ToFloat64(ctx, &rot_z, argv[6]);
	
	drawOBJ(m, pos_x, pos_y, pos_z, rot_x, rot_y, rot_z);

	return JS_UNDEFINED;
}


static JSValue athena_drawbbox(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 8) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");

	double pos_x, pos_y, pos_z, rot_x, rot_y, rot_z;
	Color color;
	model* m;
	JS_ToUint32(ctx, &m, argv[0]);

	JS_ToFloat64(ctx, &pos_x, argv[1]);
	JS_ToFloat64(ctx, &pos_y, argv[2]);
	JS_ToFloat64(ctx, &pos_z, argv[3]);
	JS_ToFloat64(ctx, &rot_x, argv[4]);
	JS_ToFloat64(ctx, &rot_y, argv[5]);
	JS_ToFloat64(ctx, &rot_z, argv[6]);
	JS_ToUint32(ctx, &color, argv[7]);
	
	draw_bbox(m, pos_x, pos_y, pos_z, rot_x, rot_y, rot_z, color);

	return JS_UNDEFINED;
}

static const JSCFunctionListEntry render_funcs[] = {
    JS_CFUNC_DEF( "init",      1,     		 athena_initrender),
  	JS_CFUNC_DEF( "loadOBJ",   2,        		athena_loadobj ),
    JS_CFUNC_DEF( "drawOBJ",   7,        		athena_drawobj ),
	JS_CFUNC_DEF( "drawBbox",  8,         	    athena_drawbbox),
    JS_CFUNC_DEF( "freeOBJ",   1,        		athena_freeobj ),
};


static JSValue athena_lightnumber(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 1) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");

	int lightcount;
	JS_ToInt32(ctx, &lightcount, argv[0]);

	setLightQuantity(lightcount);

	return JS_UNDEFINED;
}

static JSValue athena_createlight(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 8) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");

	double dir_x, dir_y, dir_z, r, g, b;
	int id, type;

	JS_ToInt32(ctx, &id, argv[0]);
	JS_ToFloat64(ctx, &dir_x, argv[1]);
	JS_ToFloat64(ctx, &dir_y, argv[2]);
	JS_ToFloat64(ctx, &dir_z, argv[3]);
	JS_ToFloat64(ctx, &r, argv[4]);
	JS_ToFloat64(ctx, &g, argv[5]);
	JS_ToFloat64(ctx, &b, argv[6]);
	JS_ToInt32(ctx, &type, argv[7]);
	
	createLight(id, dir_x, dir_y, dir_z, type, r, g, b);

	return JS_UNDEFINED;
}

static const JSCFunctionListEntry light_funcs[] = {
  JS_CFUNC_DEF( "set",  	8, athena_createlight),
  JS_CFUNC_DEF( "create", 	1, athena_lightnumber),
  JS_PROP_INT32_DEF("AMBIENT", LIGHT_AMBIENT, JS_PROP_CONFIGURABLE ),
  JS_PROP_INT32_DEF("DIRECTIONAL", LIGHT_DIRECTIONAL, JS_PROP_CONFIGURABLE ),
};


static JSValue athena_camposition(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 3) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	double x, y, z;
	JS_ToFloat64(ctx, &x, argv[0]);
	JS_ToFloat64(ctx, &y, argv[1]);
	JS_ToFloat64(ctx, &z, argv[2]);
	
	setCameraPosition(x, y, z);

	return JS_UNDEFINED;
}


static JSValue athena_camrotation(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 3) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	double x, y, z;
	JS_ToFloat64(ctx, &x, argv[0]);
	JS_ToFloat64(ctx, &y, argv[1]);
	JS_ToFloat64(ctx, &z, argv[2]);
	
	setCameraRotation(x, y, z);

	return JS_UNDEFINED;
}

static const JSCFunctionListEntry camera_funcs[] = {
  JS_CFUNC_DEF("position", 3, athena_camposition),
  JS_CFUNC_DEF("rotation", 3, athena_camrotation),
};

static int render_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, render_funcs, countof(render_funcs));
}

static int light_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, light_funcs, countof(light_funcs));
}

static int camera_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, camera_funcs, countof(camera_funcs));
}

JSModuleDef *athena_render_init(JSContext* ctx){
	athena_push_module(ctx, render_init, render_funcs, countof(render_funcs), "Render");
	athena_push_module(ctx, light_init, light_funcs, countof(light_funcs), "Lights");
    return athena_push_module(ctx, camera_init, camera_funcs, countof(camera_funcs), "Camera");
}