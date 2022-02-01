#include "ath_env.h"
#include "include/sound.h"

duk_ret_t athena_setformat(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 3) return duk_generic_error(ctx, "setFormat takes 3 arguments");
	sound_setformat(duk_get_int(ctx, 0), duk_get_int(ctx, 1), duk_get_int(ctx, 2));
	return 0;
}

duk_ret_t athena_setvolume(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 1) return duk_generic_error(ctx, "setVolume takes only 1 argument");
	sound_setvolume(duk_get_int(ctx, 0));
	return 0;
}

duk_ret_t athena_setadpcmvolume(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 2) return duk_generic_error(ctx, "setADPCMVolume takes 2 arguments");
	sound_setadpcmvolume(duk_get_int(ctx, 0), duk_get_int(ctx, 1));
	return 0;
}

duk_ret_t athena_loadadpcm(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 1) return duk_generic_error(ctx, "loadADPCM takes only 1 argument");
	duk_push_pointer(ctx, (void*)sound_loadadpcm(duk_get_string(ctx, 0)));
	return 1;
}

duk_ret_t athena_playadpcm(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 2) return duk_generic_error(ctx, "playADPCM takes 2 arguments");
	sound_playadpcm(duk_get_int(ctx, 0), (audsrv_adpcm_t *)duk_get_pointer(ctx, 1));
	return 0;
}

DUK_EXTERNAL duk_ret_t dukopen_sound(duk_context *ctx) {
	const duk_function_list_entry module_funcs[] = {
		{ "setFormat",      						athena_setformat,           3 },
		{ "setVolume",      				   		athena_setvolume,           1 },
		{ "setADPCMVolume",      				    athena_setadpcmvolume,      2 },
		{ "loadADPCM",      					    athena_loadadpcm,           1 },
		{ "playADPCM",      						athena_playadpcm,           2 },
		{ NULL, NULL, 0 }
	};

  duk_push_object(ctx);  /* module result */
  duk_put_function_list(ctx, -1, module_funcs);

  return 1;  /* return module value */
}


void athena_sound_init(duk_context* ctx){
	push_athena_module(dukopen_sound,   	  "Sound");

}