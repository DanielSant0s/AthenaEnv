#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "include/render.h"
#include "ath_env.h"

duk_ret_t athena_initrender(duk_context *ctx) {
	int argc = duk_get_top(ctx);
  	if (argc != 1) return duk_generic_error(ctx, "wrong number of arguments.");
	float aspect = duk_get_number(ctx, 0);
  	init3D(aspect);
	return 0;
}

duk_ret_t athena_loadobj(duk_context *ctx){
	int argc = duk_get_top(ctx);
	#ifndef SKIP_ERROR_HANDLING
	if (argc != 2 && argc != 1) return duk_generic_error(ctx, "wrong number of arguments");
	#endif
	const char *file_tbo = duk_get_string(ctx, 0); //Model filename
	
	// Loading texture
    GSTEXTURE* text = NULL;
	if(argc == 2) text = (GSTEXTURE*)(duk_get_pointer(ctx, 1));
	
	model* res_m = loadOBJ(file_tbo, text);


	// Push model object into Lua stack
	duk_push_pointer(ctx, (void*)res_m);
	return 1;
}


duk_ret_t athena_freeobj(duk_context *ctx) {
	int argc = duk_get_top(ctx);
#ifndef SKIP_ERROR_HANDLING
	if (argc != 1) return duk_generic_error(ctx, "wrong number of arguments");
#endif
	
	model* m = (model*)(duk_get_pointer(ctx, 0));

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

	return 0;
}

duk_ret_t athena_drawobj(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 7) return duk_generic_error(ctx, "wrong number of arguments");
	model* m = (model*)duk_get_pointer(ctx, 0);
	float pos_x = duk_get_number(ctx, 1);
	float pos_y = duk_get_number(ctx, 2);
	float pos_z = duk_get_number(ctx, 3);
	float rot_x = duk_get_number(ctx, 4);
	float rot_y = duk_get_number(ctx, 5);
	float rot_z = duk_get_number(ctx, 6);
	
	drawOBJ(m, pos_x, pos_y, pos_z, rot_x, rot_y, rot_z);

	return 0;
}


duk_ret_t athena_drawbbox(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 8) return duk_generic_error(ctx, "wrong number of arguments");
	model* m = (model*)duk_get_pointer(ctx, 0);
	float pos_x = duk_get_number(ctx, 1);
	float pos_y = duk_get_number(ctx, 2);
	float pos_z = duk_get_number(ctx, 3);
	float rot_x = duk_get_number(ctx, 4);
	float rot_y = duk_get_number(ctx, 5);
	float rot_z = duk_get_number(ctx, 6);
	Color color = (Color)duk_get_uint(ctx, 7);
	
	draw_bbox(m, pos_x, pos_y, pos_z, rot_x, rot_y, rot_z, color);

	return 0;
}

DUK_EXTERNAL duk_ret_t dukopen_render(duk_context *ctx) {
	const duk_function_list_entry module_funcs[] = {
	    { "init",           		athena_initrender,	DUK_VARARGS },
	  	{ "loadOBJ",           		athena_loadobj,		DUK_VARARGS },
	    { "drawOBJ",           		athena_drawobj,		DUK_VARARGS },
		{ "drawBbox",           	athena_drawbbox,	DUK_VARARGS },
	    { "freeOBJ",           		athena_freeobj,		DUK_VARARGS },
	  {NULL, NULL, 0}
	};

  duk_push_object(ctx);  /* module result */
  duk_put_function_list(ctx, -1, module_funcs);

  return 1;  /* return module value */
}


duk_ret_t athena_lightnumber(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 1) return duk_generic_error(ctx, "wrong number of arguments");

	int lightcount = duk_get_int(ctx, 0);

	setLightQuantity(lightcount);

	return 0;
}

duk_ret_t athena_createlight(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 8) return duk_generic_error(ctx, "wrong number of arguments");
	int id = duk_get_int(ctx, 0);
	float dir_x = duk_get_number(ctx, 1);
	float dir_y = duk_get_number(ctx, 2);
	float dir_z = duk_get_number(ctx, 3);
	float r = duk_get_number(ctx, 4);
	float g = duk_get_number(ctx, 5);
	float b = duk_get_number(ctx, 6);
	int type = duk_get_number(ctx, 7);
	
	
	createLight(id, dir_x, dir_y, dir_z, type, r, g, b);

	return 0;
}

DUK_EXTERNAL duk_ret_t dukopen_lights(duk_context *ctx) {
	const duk_function_list_entry module_funcs[] = {
	  { "set",  	athena_createlight,		DUK_VARARGS },
	  { "create", 	athena_lightnumber,		DUK_VARARGS },
	  { NULL, NULL, 0 }
	};

  duk_push_object(ctx);  /* module result */
  duk_put_function_list(ctx, -1, module_funcs);

  return 1;  /* return module value */
}


duk_ret_t athena_camposition(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 3) return duk_generic_error(ctx, "wrong number of arguments");
	float x = duk_get_number(ctx, 0);
	float y = duk_get_number(ctx, 1);
	float z = duk_get_number(ctx, 2);
	
	setCameraPosition(x, y, z);

	return 0;
}


duk_ret_t athena_camrotation(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 3) return duk_generic_error(ctx, "wrong number of arguments");
	float x = duk_get_number(ctx, 0);
	float y = duk_get_number(ctx, 1);
	float z = duk_get_number(ctx, 2);
	
	setCameraRotation(x, y, z);

	return 0;
}

DUK_EXTERNAL duk_ret_t dukopen_camera(duk_context *ctx) {
	const duk_function_list_entry module_funcs[] = {
	  { "position",  	athena_camposition,		DUK_VARARGS },
	  { "rotation", 	athena_camrotation,		DUK_VARARGS },
	  { NULL, NULL, 0 }
	};

  duk_push_object(ctx);  /* module result */
  duk_put_function_list(ctx, -1, module_funcs);

  return 1;  /* return module value */
}


void athena_render_init(duk_context* ctx){
	push_athena_module(dukopen_render,   	 "Render");
	push_athena_module(dukopen_lights,   	 "Lights");
	push_athena_module(dukopen_camera,   	 "Camera");

	duk_push_uint(ctx, LIGHT_AMBIENT);
	duk_put_global_string(ctx, "AMBIENT");

	duk_push_uint(ctx, LIGHT_DIRECTIONAL);
	duk_put_global_string(ctx, "DIRECTIONAL");

}